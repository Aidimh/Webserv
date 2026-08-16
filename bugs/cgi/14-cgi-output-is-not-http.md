# 14 — Raw CGI output is forwarded as if it were an HTTP response

**Where:** `src/Multiplexing/Engine.cpp` → the CGI read branch of `run()`

## Symptom

What a CGI writes is what the client receives, verbatim:

```
Status: 200 OK
Content-Type: text/html; charset=utf-8

HELLO WORLD
```

There is no status line, so the first line of the "response" is a header. There
is no `Content-Length` and no `Transfer-Encoding` either, so the client cannot
tell where the body ends — it has to wait for the connection to close, and a
client that keeps the connection open waits for ever.

For the 100 MB cases it is worse: the whole answer accumulates in
`client.response` in RAM before anything is sent, so 20 concurrent CGI
requests need 2 GB of memory.

## Cause

```cpp
int n = read(_pollfds[i].fd, buffer, sizeof(buffer));
if (n > 0)
    _clients[_cgi_pipes[_pollfds[i].fd]].response.append(buffer, n);
```

The CGI answer is appended to the response buffer as-is. A CGI speaks the CGI
protocol (RFC 3875): it emits a header block terminated by a blank line, where
`Status:` carries the status code, and the gateway is responsible for turning
that into an HTTP message.

## Fix

The header block is parsed once, translated into a real status line, and the
body is framed with chunked transfer encoding — the only framing available when
the length is unknown up front. It also means bytes can leave for the socket as
soon as they arrive, instead of being buffered whole.

### Header translation

```cpp
static std::vector<std::string> splitHeaderLines(const std::string& block)
{
    std::vector<std::string> lines;
    size_t start = 0;

    while (start < block.size())
    {
        size_t end = block.find('\n', start);
        std::string line = (end == std::string::npos)
                         ? block.substr(start)
                         : block.substr(start, end - start);

        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (!line.empty())
            lines.push_back(line);
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return lines;
}
```

```cpp
static bool isStatusHeader(const std::string& line)
{
    return line.size() >= 7 && strncasecmp(line.c_str(), "Status:", 7) == 0;
}
```

```cpp
/* CGI answers with "Status: 404 Not Found"; HTTP wants it on the status line. */
static std::string cgiStatusLine(const std::vector<std::string>& lines)
{
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (!isStatusHeader(lines[i]))
            continue;

        std::string value = lines[i].substr(7);
        size_t begin = value.find_first_not_of(" \t");

        if (begin != std::string::npos)
            return value.substr(begin);
    }
    return "200 OK";
}
```

```cpp
static std::string cgiForwardedHeaders(const std::vector<std::string>& lines)
{
    std::string headers;

    for (size_t i = 0; i < lines.size(); i++)
    {
        if (isStatusHeader(lines[i]))
            continue;
        headers += lines[i] + "\r\n";
    }
    return headers;
}
```

```cpp
/*
 * The answer is streamed: its size is unknown when the head is written,
 * so the body is framed with chunked transfer encoding.
 */
static std::string buildCgiResponseHead(const std::string& cgiHeaderBlock)
{
    std::vector<std::string> lines = splitHeaderLines(cgiHeaderBlock);
    std::string head;

    head += "HTTP/1.1 " + cgiStatusLine(lines) + "\r\n";
    head += cgiForwardedHeaders(lines);
    head += "Transfer-Encoding: chunked\r\n";
    head += "Connection: close\r\n";
    head += "\r\n";
    return head;
}
```

Everything the CGI set other than `Status:` — `Content-Type`, `Set-Cookie`, … —
is forwarded untouched.

### Framing

```cpp
static void appendChunk(std::string& out, const char* data, size_t size)
{
    std::ostringstream header;

    if (size == 0)
        return;
    header << std::hex << size << "\r\n";
    out += header.str();
    out.append(data, size);
    out += "\r\n";
}
```

```cpp
static size_t findHeaderBlockEnd(const std::string& buffer, size_t& terminatorSize)
{
    size_t position = buffer.find("\r\n\r\n");

    if (position != std::string::npos)
    {
        terminatorSize = 4;
        return position;
    }
    position = buffer.find("\n\n");
    if (position != std::string::npos)
    {
        terminatorSize = 2;
        return position;
    }
    return std::string::npos;
}
```

Both terminators are accepted: a Python script that uses `print()` emits bare
`\n`, and every script in the tester's `cgi-bin/` does exactly that.

### Reading and forwarding

```cpp
void Multiplexer::readCgiOutput(int pipe_fd)
{
    Client* client = findClientByPipe(pipe_fd);
    char buffer[CGI_CHUNK_SIZE];

    if (client == NULL)
        return;

    ssize_t count = read(pipe_fd, buffer, sizeof(buffer));
    if (count <= 0)
    {
        finishCgiOutput(*client);
        return;
    }
    client->cgi.last_activity = time(NULL);
    appendCgiPayload(*client, buffer, count);
    enableWrite(client->fd);
    DDEBUG("Multiplexer") << "readCgiOutput: read " << count << " bytes from cgi fd=" << pipe_fd;
}
```

```cpp
void Multiplexer::appendCgiPayload(Client& client, const char* data, size_t size)
{
    if (!client.cgi.headers_done)
    {
        client.cgi.header_buffer.append(data, size);
        emitCgiHeaders(client);
        return;
    }
    appendChunk(client.response, data, size);
}
```

```cpp
void Multiplexer::emitCgiHeaders(Client& client)
{
    size_t terminatorSize = 0;
    size_t end = findHeaderBlockEnd(client.cgi.header_buffer, terminatorSize);

    if (end == std::string::npos)
    {
        if (client.cgi.header_buffer.size() < MAX_HEADER_SIZE)
            return;
        end = 0;
        terminatorSize = 0;
    }

    std::string headerBlock = client.cgi.header_buffer.substr(0, end);
    std::string payload = client.cgi.header_buffer.substr(end + terminatorSize);

    client.response += buildCgiResponseHead(headerBlock);
    client.cgi.headers_done = true;
    client.cgi.header_buffer.clear();
    appendChunk(client.response, payload.data(), payload.size());
}
```

A CGI that never emits a valid header block does not hang the client: after
`MAX_HEADER_SIZE` bytes everything collected is treated as body under a default
`200 OK`.

```cpp
void Multiplexer::finishCgiOutput(Client& client)
{
    if (!client.cgi.headers_done)
    {
        std::string payload = client.cgi.header_buffer;

        client.cgi.header_buffer.clear();
        client.response += buildCgiResponseHead("");
        client.cgi.headers_done = true;
        appendChunk(client.response, payload.data(), payload.size());
    }
    client.response += "0\r\n\r\n";
    DEBUG("Multiplexer") << "finishCgiOutput: cgi answer complete client fd=" << client.fd;
    releaseCgi(client);
    enableWrite(client.fd);
}
```

### Memory stays bounded

```cpp
/* Stop draining the CGI while this much answer is still waiting for the socket. */
static const size_t CGI_MAX_PENDING = 1048576;
```

```cpp
/*
 * Nothing is read from a CGI while its answer is still piling up in memory:
 * the kernel pipe buffer becomes the queue and memory stays bounded.
 */
void Multiplexer::applyCgiBackPressure()
{
    std::map<int, Client>::iterator it;

    for (it = _clients.begin(); it != _clients.end(); ++it)
    {
        Client& client = it->second;

        if (!client.cgi.running || client.cgi.stdout_fd == -1)
            continue;
        if (client.response.size() >= CGI_MAX_PENDING)
            setEvents(client.cgi.stdout_fd, 0);
        else
            setEvents(client.cgi.stdout_fd, EPOLLIN);
    }
}
```

A slow client can no longer make the server buffer an unbounded answer: once
1 MB is queued the CGI's pipe simply stops being watched, the pipe fills, and
the script blocks in its own `write()` until the client catches up. This is what
keeps 20 concurrent 100 MB answers inside a few tens of MB of RAM.

## Verification

```
$ curl -s -i -X POST --data-binary 'hello world' http://127.0.0.1:1027/directory/youpi.bla
HTTP/1.1 200 OK
Content-Type: text/html; charset=utf-8
Transfer-Encoding: chunked
Connection: close

HELLO WORLD
```

Peak RSS during test 31 (20 forks × 100 MB) stayed under 60 MB.
