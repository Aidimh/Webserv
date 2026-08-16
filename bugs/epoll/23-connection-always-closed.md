# 23 — Every connection is closed after one response

**Where:** `src/Multiplexing/Engine.cpp` → `_writeClient()`,
`src/Response/Response.cpp` → `toString()`

## Symptom

The server behaves like HTTP/1.0 while announcing HTTP/1.1:

```
$ curl -sv http://127.0.0.1:1025/index.htm http://127.0.0.1:1025/styles.css 2>&1 | grep -E '^[<>*] (HTTP|Closing)'
> HTTP/1.1
< HTTP/1.1 200 OK
* Closing connection 0            <- server hung up
> HTTP/1.1                        <- curl has to open a second connection
```

Nothing tells the client either: no `Connection: close` header, the socket just
goes away. A client that reuses the connection — which HTTP/1.1 says it may —
loses a request and has to guess why.

Stress test 6 fails outright:

```
✘ FAIL  6 » Keep-Alive Connection Eviction Test  (missing: OLDEST_CONNECTION_CLOSED)
FAILED: keep-alive sanity probe failed on socket index 9980
```

It is also why the stress runs are so expensive: 100 000 requests means 100 000
TCP handshakes and 100 000 sockets in `TIME_WAIT`.

## Cause

`_writeClient()` ends with `_removeClient(fd)` as soon as the response buffer
drains, and `Response::toString()` never writes a `Connection` header. The
request's own `Connection:` header is parsed into the map and never read.

`Client::reset()` already does exactly the right thing — clears the buffers and
the parsed request so the object can serve another request on the same socket —
but the only call to it was immediately followed by `_removeClient()`, which
threw the object away.

## Fix

Four parts: decide, announce, reuse, and put a ceiling on it.

### 1. Decide

```cpp
/*
 * HTTP/1.1 keeps the connection open unless the client asked for a close;
 * HTTP/1.0 is the other way round.
 */
static bool clientWantsKeepAlive(const ClientRequest& request)
{
    std::map<std::string, std::string>::const_iterator it =
        request.getHeaders().find("connection");
    std::string value;

    if (it != request.getHeaders().end())
    {
        value = it->second;
        MyToLower(value);
    }
    if (request.getVersion() == "HTTP/1.0")
        return value == "keep-alive";
    return value != "close";
}
```

### 2. Announce

In `prepareResponse()`, before the response is serialised:

```cpp
        if (client.parsed_request.state == ClientRequest::ERROR_STATE
            || !clientWantsKeepAlive(client.parsed_request))
            client.close_after_response = true;
        response.addHeader("Connection",
                           client.close_after_response ? "close" : "keep-alive");
```

A request that failed to parse is not reusable — the buffer may hold the tail of
something the server could not make sense of — so an error response always
closes. A CGI answer sets the same flag when it starts, because its head is
written before the body is known and already says `Connection: close`.

`Response::toString()` keeps a fallback for responses built elsewhere:

```cpp
    if (headers.find("Connection") == headers.end())
        out << "Connection: close\r\n";
```

### 3. Reuse

Parsing what is in the buffer is now needed in two places, so it is its own
function:

```cpp
/* Parses whatever is buffered and arms the answer once the request is whole. */
void Multiplexer::advanceRequest(int fd, Client& client)
{
    client.parsed_request.parse(client);
    if (client.parsed_request.state == ClientRequest::BODY)
        client.parsed_request.BodyRequest(client);
    if (client.parsed_request.state == ClientRequest::DONE
        || client.parsed_request.state == ClientRequest::ERROR_STATE)
        enableWrite(fd);
}
```

```cpp
/*
 * The answer is out. Either the connection is reused for the next request --
 * a request that may already be sitting in the buffer -- or it is closed.
 */
void Multiplexer::finishResponse(int fd, Client& client)
{
    if (client.close_after_response || !clientWantsKeepAlive(client.parsed_request))
    {
        _removeClient(fd);
        return;
    }

    std::string pipelined;

    pipelined.swap(client.request);
    client.reset();
    client.request.swap(pipelined);
    client.last_activity = time(NULL);
    setEvents(fd, EPOLLIN);
    DDEBUG("Multiplexer") << "finishResponse: connection kept alive fd=" << fd;
    if (!client.request.empty())
        advanceRequest(fd, client);
}
```

The swap around `reset()` is what makes pipelining work: bytes of the *next*
request that arrived with the previous one are preserved and parsed immediately,
instead of being wiped and waited for for ever.

`_writeClient()` ends with it:

```cpp
    if (client->cgi.running)
    {
        disableWrite(fd);
        return;
    }
    client->last_activity = time(NULL);
    finishResponse(fd, *client);
```

and `_readClient()` uses the same helper:

```cpp
    client->last_activity = time(NULL);
    client->request.append(buffer, bytesRead);
    advanceRequest(fd, *client);
```

### 4. Put a ceiling on it

Persistent connections are held until the client goes away, so a flood of idle
sockets could lock out every new client. Two sweeps prevent that.

```cpp
/* Persistent connections held at once; past it the least recently used goes. */
static const size_t MAX_CLIENTS = 4096;
/* A kept-alive connection that says nothing for this long is closed. */
static const time_t CLIENT_IDLE_TIMEOUT = 65;
```

```cpp
bool Multiplexer::isEvictable(const Client& client) const
{
    return !client.cgi.running
        && client.response.empty()
        && client.stream_file_fd == -1;
}
```

```cpp
/*
 * Under connection pressure the least recently used idle connection is
 * dropped, so a flood of persistent sockets cannot lock out new clients.
 */
void Multiplexer::evictOldestClient()
{
    std::map<int, Client>::iterator it;
    int oldest = -1;
    time_t oldestSeen = 0;

    for (it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (!isEvictable(it->second))
            continue;
        if (oldest == -1 || it->second.last_activity < oldestSeen)
        {
            oldest = it->first;
            oldestSeen = it->second.last_activity;
        }
    }
    if (oldest == -1)
        return;
    DEBUG("Multiplexer") << "evictOldestClient: closing least recently used fd=" << oldest
                         << " live clients=" << _clients.size();
    _removeClient(oldest);
}
```

```cpp
/* Frees connections that were kept alive and then went quiet. */
void Multiplexer::closeIdleClients()
{
    std::map<int, Client>::iterator it = _clients.begin();
    std::vector<int> expired;
    time_t now = time(NULL);

    while (it != _clients.end())
    {
        const Client& client = it->second;

        if (client.parsed_request.state == ClientRequest::HEADERS
            && client.request.empty()
            && isEvictable(client)
            && now - client.last_activity > CLIENT_IDLE_TIMEOUT)
            expired.push_back(it->first);
        ++it;
    }
    for (size_t i = 0; i < expired.size(); i++)
    {
        DEBUG("Multiplexer") << "closeIdleClients: idle timeout fd=" << expired[i];
        _removeClient(expired[i]);
    }
}
```

Both are careful never to touch a connection that is mid-transaction: a client
with a running CGI, a queued response or an in-flight file stream is not idle,
however long ago it last spoke. `closeIdleClients()` collects first and removes
afterwards, because `_removeClient()` erases from the map being walked.

`_acceptNewClient()` applies the ceiling:

```cpp
    client.last_activity = time(NULL);
    client.parsed_request.state = ClientRequest::HEADERS;
    _clients[client_fd] = client;
    addFd(client_fd, EPOLLIN);
    if (_clients.size() > MAX_CLIENTS)
        evictOldestClient();
```

and `run()` gains the sweep:

```cpp
        killTimedOutCgi();
        closeIdleClients();
        applyCgiBackPressure();
```

### `struct Client` and declarations

```cpp
    bool close_after_response;
    time_t last_activity;
```

`close_after_response` is reset in `Client::reset()` so a reused connection
starts neutral. New members of `Multiplexer`:

```cpp
        void                            advanceRequest(int fd, Client& client);
        void                            finishResponse(int fd, Client& client);
        void                            evictOldestClient();
        void                            closeIdleClients();
        bool                            isEvictable(const Client& client) const;
```

## Verification

Three requests over one socket:

```
request 1 HTTP/1.1 200 OK ['Connection: keep-alive']
request 2 HTTP/1.1 200 OK ['Connection: keep-alive']
request 3 HTTP/1.1 200 OK ['Connection: keep-alive']
```

The client's wish is respected in both directions:

```
$ curl -s -i -H 'Connection: close' http://127.0.0.1:1025/index.htm | grep -i '^connection'
Connection: close
$ curl -s -i http://127.0.0.1:1025/index.htm | grep -i '^connection'
Connection: keep-alive
```

Tester, 10 000 simultaneous persistent sockets:

```
✔ PASS  6 » Keep-Alive Connection Eviction Test
▶ INFO  Opened sockets: 10000/10000
```

— every socket answered, a recent one still usable for a second request, and the
oldest evicted under pressure.
