# 06 — The connection is closed the instant a CGI starts

**Where:** `src/Multiplexing/Engine.cpp` → `_writeClient()`, `prepareResponse()`,
`is_cgi()`, `is_in_cgi_list()`, `handleClient()`

## Symptom

Every CGI request ends with no response at all:

```
$ curl -s -o /dev/null -w '%{http_code}\n' --max-time 5 http://127.0.0.1:1027/directory/youpi.bla
000
```

Minimum Evaluation tests **7, 16, 16b, 17, 18 and 31** fail; test 7 reports
`(missing: 200 OK)` and the streaming ones time out.

## Cause

```cpp
void Multiplexer::_writeClient(int fd)
{
    ...
    if (is_cgi(it->second.parsed_request.getRequestPath()))
    {
        handleClient(fd);          // forks the CGI, registers the output pipe
        disableWrite(fd);
        _removeClient(fd);         // <-- closes the client socket immediately
        return;
    }
```

`handleClient()` starts the child and remembers `_cgi_pipes[pipe] = fd`, then
`_removeClient(fd)` closes `fd` and erases the `Client`. When the CGI output
finally arrives the loop does `_clients[client_fd].response.append(...)`, which
**default-constructs a brand new `Client`** in the map (`operator[]`) whose
`fd` is `-1`, and `enableWrite(client_fd)` finds nothing to arm. The answer is
written into an object nobody will ever send, and the socket has been closed
for a while already.

Worse, `fd` has been returned to the OS: the next `accept()` can hand out the
same number, and the resurrected entry then aliases a completely different
client.

Three smaller problems live in the same code path:

* `is_cgi()` uses `path.find(".")` — the **first** dot anywhere in the URI — and
  matches it against a hard-coded list `{.py .sh .pl .php .bla}`. A request for
  `/dir.d/index.htm` is treated as a CGI with extension `.d/index.htm`, and a
  `cgi_extension` configured for anything outside that list is ignored. The
  configuration file is the only thing that should decide this.
* `handleClient()` picks its server by comparing the `Host` header against
  `listen_port_str[0]` and its location by `path.find(loc_path) == 0` — a
  second, different and weaker copy of the routing already implemented in
  `Router`.
* The dispatcher already decides "this is a CGI" and returns a response with
  `CGI_RESPONSE` mode, but nothing ever looks at that flag.

## Fix

The decision comes from the dispatcher, and the CGI keeps the connection open
until its answer is complete.

### 1. `prepareResponse()` — the single place where a CGI is started

```cpp
void Multiplexer::prepareResponse(Client &client)
{
    if (client.response_prepared)
        return;

    const Server_block& server = which_server(client.port);
    Response response = Dispatcher::dispatch(client, server);

    client.response_prepared = true;
    if (!response.isCGI())
    {
        client.response = response.toString();
        return;
    }
    if (startCgi(client, server))
        return;

    WARN() << "Multiplexer::prepareResponse: cgi start failed fd=" << client.fd;
    client.response = AMethod::buildErrorResponse(HTTP_502_BAD_GATEWAY, "Bad Gateway").toString();
}
```

### 2. `startCgi()` — routing is done by `Router`, not by a second copy

```cpp
bool Multiplexer::startCgi(Client& client, const Server_block& server)
{
    const Location_Config* location =
        Router::resolveLocation(client.parsed_request.getRequestPath(), server);

    if (location == NULL)
        return false;

    CGI cgi(client, *location, server);

    if (!cgi.isRunnable() || !cgi.execute())
        return false;

    client.cgi.pid = cgi.get_pid();
    client.cgi.stdin_fd = cgi.get_input_fd();
    client.cgi.stdout_fd = cgi.get_output_fd();
    client.cgi.running = true;
    client.cgi.last_activity = time(NULL);
    openCgiBodySource(client);
    registerCgiPipes(client);
    INFO() << "Multiplexer::startCgi: started pid=" << client.cgi.pid
           << " script=" << cgi.get_script() << " client fd=" << client.fd;
    return true;
}
```

```cpp
void Multiplexer::registerCgiPipes(Client& client)
{
    _cgi_pipes[client.cgi.stdout_fd] = client.fd;
    addFd(client.cgi.stdout_fd, EPOLLIN);
    _cgi_pipes[client.cgi.stdin_fd] = client.fd;
    addFd(client.cgi.stdin_fd, EPOLLOUT);
}
```

### 3. `_writeClient()` — a running CGI keeps the socket alive

```cpp
void Multiplexer::_writeClient(int fd)
{
    Client* client = findClient(fd);

    if (client == NULL)
        return;

    prepareResponse(*client);
    if (!sendResponse(fd, *client))
        return;
    if (client->stream_file_fd != -1)
    {
        sendStreaming(fd, *client);
        return;
    }
    if (client->cgi.running)
    {
        disableWrite(fd);
        return;
    }
    _removeClient(fd);
}
```

An empty send buffer no longer means "we are done": while `cgi.running` is set,
`EPOLLOUT` is simply switched off and the connection waits for the next piece of
CGI output to arm it again ([14](14-cgi-output-is-not-http.md)).

### 4. `sendResponse()` — a failed send closes the connection once

```cpp
bool Multiplexer::sendResponse(int fd, Client &client)
{
    if (client.response.empty())
        return true;

    ssize_t sent = send(fd, client.response.data(), client.response.size(), MSG_NOSIGNAL);
    if (sent <= 0)
    {
        WARN() << "Multiplexer::sendResponse: send failed to client fd=" << fd
               << ": " << strerror(errno);
        _removeClient(fd);
        return false;
    }
    client.response.erase(0, sent);
    DDEBUG("Multiplexer") << "sendResponse: wrote " << sent << " bytes to fd=" << fd
                          << " remaining=" << client.response.size();
    return client.response.empty();
}
```

The caller returns immediately on `false`, so the destroyed `Client` is never
touched again.

### 5. `_removeClient()` — a client never leaves a CGI behind

```cpp
void Multiplexer::_removeClient(int fd)
{
    std::map<int, Client>::iterator it = _clients.find(fd);

    if (it == _clients.end())
        return;
    releaseCgi(it->second);
    DEBUG("Multiplexer") << "_removeClient: closed client fd=" << fd;
    removeFd(fd);
    close(fd);
    _clients.erase(it);
}
```

```cpp
void Multiplexer::releaseCgi(Client& client)
{
    closeCgiInput(client);
    if (client.cgi.stdout_fd != -1)
    {
        removeFd(client.cgi.stdout_fd);
        _cgi_pipes.erase(client.cgi.stdout_fd);
        close(client.cgi.stdout_fd);
        client.cgi.stdout_fd = -1;
    }
    if (client.cgi.pid != -1)
    {
        kill(client.cgi.pid, SIGKILL);
        waitpid(client.cgi.pid, NULL, 0);
        DEBUG("Multiplexer") << "releaseCgi: reaped cgi pid=" << client.cgi.pid;
        client.cgi.pid = -1;
    }
    client.cgi.running = false;
}
```

If the client hangs up mid-CGI the child is killed and reaped instead of being
left to write into a closed pipe — no zombies, no leaked descriptors.

### 6. Delete the dead code

`is_cgi()`, `is_in_cgi_list()` and `handleClient()` are removed together with
their declarations in `includes/multiplexing/header.hpp`. The extension list
now comes from `cgi_extension` in the configuration, through
`Router::isCGIRequest()`.

### 7. Method restriction is checked before CGI — `src/Response/Dispatcher.cpp`

In the original order a CGI extension bypassed `allowed_methods` completely:
`POST /x.py` ran the script even on a `allowed_methods GET;` location.

```cpp
    if (!Router::isMethodAllowed(client.parsed_request.getMethod(), *location)) {
        DEBUG("Dispatcher") << "dispatch: method=" << client.parsed_request.getMethod()
                            << " not allowed on location=" << location->path
                            << ", responding status=405 fd=" << client.fd;
        Response response = AMethod::buildErrorResponse(HTTP_405_METHOD_NOT_ALLOWED, "Method Not Allowed");
        setErrorPageBody(response);
        return response;
    }

    if (Router::isCGIRequest(client.parsed_request,*location) == true) {
        DEBUG("Dispatcher") << "dispatch: routing to CGI path="
                            << client.parsed_request.getRequestPath() << " fd=" << client.fd;
        Response response;
        response.setResponseMode(Response::CGI_RESPONSE);
        client.cgi_started = true;
        return response;
    }
```

### 8. Per-connection CGI state — `includes/multiplexing/header.hpp`

```cpp
struct CgiState
{
    pid_t       pid;
    int         stdin_fd;       // pipe the request body is written into
    int         stdout_fd;      // pipe the CGI answer is read from
    int         body_fd;        // temp file holding the request body, -1 when in RAM
    std::string body_buffer;    // request body bytes not forwarded yet
    std::string header_buffer;  // CGI header block being collected
    bool        headers_done;   // HTTP header block already handed to the client
    bool        running;
    time_t      last_activity;

    CgiState()
    : pid(-1),
      stdin_fd(-1),
      stdout_fd(-1),
      body_fd(-1),
      headers_done(false),
      running(false),
      last_activity(0)
    {}

    void clear()
    {
        pid = -1;
        stdin_fd = -1;
        stdout_fd = -1;
        body_fd = -1;
        body_buffer.clear();
        header_buffer.clear();
        headers_done = false;
        running = false;
        last_activity = 0;
    }
};
```

`struct Client` gains `CgiState cgi;` and `Client::reset()` gains `cgi.clear();`.

## Verification

```
$ curl -s -X POST --data-binary 'hello world' http://127.0.0.1:1027/directory/youpi.bla
HELLO WORLD
```

Tester:

```
✔ PASS  7  » Directory GET file with bla extension
✔ PASS  16 » Chunked large 100M chars body with y
✔ PASS  31 » Chunked fork large 100M chars with k (100/100 OK)
```
