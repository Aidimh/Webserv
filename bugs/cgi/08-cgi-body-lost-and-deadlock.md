# 08 — The request body never reaches the CGI, and a big one deadlocks the server

**Where:** `src/CGI/CGI_class.cpp` → `writeToChild()`, `execute()`
**Fix lives in:** `src/Multiplexing/Engine.cpp` → `writeCgiInput()` and friends

## Symptom

Two failures with the same root.

1. **The body is empty.** `POST /directory/youpi.bla` with a body of more than
   8 bytes reaches the CGI with nothing on stdin, so the echo comes back empty.
2. **The server freezes.** With a body larger than a pipe buffer (64 KB on
   Linux) the whole process blocks — not just that client, *every* client,
   because the single-threaded event loop is stuck inside `write()`.

Minimum Evaluation tests **16, 16b, 17 and 31** (100 MB bodies) can never
finish.

## Cause

```cpp
void CGI::writeToChild()
{
    write(stdin_pipe[1], body.c_str(), body.size());
    close(stdin_pipe[1]);
}
```

with

```cpp
CGI::CGI(Client& client, const Location_Config& conf)
    : ..., body(client.parsed_request.getBody())
```

* `getBody()` returns the **in-RAM** body only. `MAX_RAM_BUFFER` is 8, so any
  body over 8 bytes has been streamed to the temp file and `getBody()` is
  empty. The one path that carries real uploads carries nothing.
* The write is a single blocking call on a blocking pipe. Beyond 64 KB the
  kernel makes the writer wait until the child reads. The child does read —
  but it also writes its answer into the other pipe, which the server is not
  draining because it is blocked in `write()`. Both processes wait for each
  other: a textbook deadlock.
* `writeToChild()` is called straight after `execute()`, before the loop ever
  waits for an event, which is what makes the block fatal instead of merely
  slow.

## Fix

The body is fed to the child from the event loop, in chunks, on a non-blocking
pipe. `writeToChild()` disappears entirely.

### 1. The parent's pipe ends are non-blocking — `CGI::execute()`

```cpp
bool CGI::execute()
{
    if (!extension_found || !openPipes())
        return false;

    pid = fork();
    if (pid == -1)
    {
        ERR() << "CGI::execute: fork failed: " << strerror(errno);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return false;
    }
    if (pid == 0)
        runChild();

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    stdin_pipe[0] = -1;
    stdout_pipe[1] = -1;
    fcntl(stdin_pipe[1], F_SETFL, O_NONBLOCK);
    fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
    DEBUG("CGI") << "execute: forked pid=" << pid << " script=" << script
                 << " input fd=" << stdin_pipe[1] << " output fd=" << stdout_pipe[0];
    return true;
}
```

```cpp
bool CGI::openPipes()
{
    if (pipe(stdin_pipe) == -1)
    {
        ERR() << "CGI::openPipes: pipe failed: " << strerror(errno);
        return false;
    }
    if (pipe(stdout_pipe) == -1)
    {
        ERR() << "CGI::openPipes: pipe failed: " << strerror(errno);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        stdin_pipe[0] = -1;
        stdin_pipe[1] = -1;
        return false;
    }
    DEBUG("CGI") << "openPipes: stdin pipe fd=" << stdin_pipe[0] << "," << stdin_pipe[1]
                 << " stdout pipe fd=" << stdout_pipe[0] << "," << stdout_pipe[1];
    return true;
}
```

```cpp
/*
 * Runs in the forked child: wire the pipes on stdin/stdout and exec.
 * The child never returns from here.
 */
void CGI::runChild()
{
    char *argv[3];

    argv[0] = const_cast<char *>(interpreter.c_str());
    argv[1] = const_cast<char *>(script.c_str());
    argv[2] = NULL;

    if (dup2(stdin_pipe[0], STDIN_FILENO) == -1 || dup2(stdout_pipe[1], STDOUT_FILENO) == -1)
        _exit(1);

    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);

    execve(interpreter.c_str(), argv, request_vars);
    _exit(1);
}
```

`O_NONBLOCK` on the parent ends does not affect the child: the read end of
`stdin_pipe` is a different open file description, so the script still sees a
normal blocking stdin.

The child also uses `_exit()` rather than `exit()`, so a failed `execve` cannot
flush the parent's `iostream` buffers a second time.

### 2. Where the body comes from — `src/Multiplexing/Engine.cpp`

```cpp
bool Multiplexer::openCgiBodySource(Client& client)
{
    const ClientRequest& request = client.parsed_request;

    if (!request.usesTmpFile())
    {
        client.cgi.body_buffer = request.getBody();
        return true;
    }
    client.cgi.body_fd = open(request.getTmpFilePath().c_str(), O_RDONLY);
    if (client.cgi.body_fd == -1)
    {
        ERR() << "Multiplexer::openCgiBodySource: open body file failed path="
              << request.getTmpFilePath() << ": " << strerror(errno);
        return false;
    }
    /* the fd keeps the data alive, the name is not needed any more */
    unlink(request.getTmpFilePath().c_str());
    return true;
}
```

Small bodies come from RAM, large ones are read back from the temp file — the
case the old code ignored. The `unlink()` also fixes
[20](../request/20-temp-body-files-leak.md): the disk space is reclaimed the moment the
descriptor closes, so 20 concurrent 100 MB uploads leave nothing behind.

### 3. Feeding the child, one event at a time

```cpp
/* Refills the outgoing body buffer from the temp file, false once it is drained. */
static bool refillCgiBody(CgiState& cgi)
{
    char buffer[CGI_CHUNK_SIZE];

    if (cgi.body_fd == -1)
        return false;

    ssize_t count = read(cgi.body_fd, buffer, sizeof(buffer));
    if (count <= 0)
        return false;
    cgi.body_buffer.append(buffer, count);
    return true;
}
```

```cpp
void Multiplexer::writeCgiInput(int pipe_fd)
{
    Client* client = findClientByPipe(pipe_fd);

    if (client == NULL)
        return;
    if (client->cgi.body_buffer.empty() && !refillCgiBody(client->cgi))
    {
        closeCgiInput(*client);
        return;
    }

    ssize_t written = write(pipe_fd, client->cgi.body_buffer.data(), client->cgi.body_buffer.size());
    if (written <= 0)
    {
        DEBUG("Multiplexer") << "writeCgiInput: cgi stopped reading fd=" << pipe_fd;
        closeCgiInput(*client);
        return;
    }
    client->cgi.body_buffer.erase(0, written);
    client->cgi.last_activity = time(NULL);
    DDEBUG("Multiplexer") << "writeCgiInput: wrote " << written << " body bytes to cgi fd=" << pipe_fd;
}
```

Nothing is ever attempted unless the event loop reported the pipe writable, and
a short write just leaves the rest in the buffer for the next event. When the
body is exhausted the pipe is closed, which is the EOF the script waits for.

```cpp
void Multiplexer::closeCgiInput(Client& client)
{
    if (client.cgi.stdin_fd != -1)
    {
        removeFd(client.cgi.stdin_fd);
        _cgi_pipes.erase(client.cgi.stdin_fd);
        close(client.cgi.stdin_fd);
        DEBUG("Multiplexer") << "closeCgiInput: closed cgi input fd=" << client.cgi.stdin_fd
                             << " client fd=" << client.fd;
        client.cgi.stdin_fd = -1;
    }
    if (client.cgi.body_fd != -1)
    {
        close(client.cgi.body_fd);
        client.cgi.body_fd = -1;
    }
    client.cgi.body_buffer.clear();
}
```

A script that stops reading early (`write` fails with `EPIPE`) is not an error:
the input side is simply closed and the answer is still collected. That path is
only survivable because of [12](12-sigpipe-kills-the-server.md).

Constant used above, at the top of `Engine.cpp`:

```cpp
/* Read/write granularity for the CGI pipes. */
static const size_t CGI_CHUNK_SIZE = 65536;
```

## Verification

10 MB through the CGI, byte-for-byte:

```
$ time curl -s -X POST --data-binary @10mb.bin http://127.0.0.1:1027/directory/youpi.bla -o out.bin
real 0m1.084s
$ python3 -c "d=open('out.bin','rb').read(); print(len(d), d==b'Y'*len(d))"
10485760 True
```

Tester, 100 MB each and 20 forks in parallel:

```
✔ PASS  16  » Chunked large 100M chars body with y
✔ PASS  16b » Content-Length large 100M chars body with y
✔ PASS  17  » Chunked large 100M chars body with e
✔ PASS  31  » Chunked fork large 100M chars with k (100/100 OK)
```
