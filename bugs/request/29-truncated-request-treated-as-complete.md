# 29 — A request cut off mid-body is answered as if it were complete

**Where:** `src/Multiplexing/Engine.cpp` → `_readClient()`

## Symptom

Chunked Transfer test 15 ("Chunked Premature EOF") fails with
`(missing: HTTP/1.1 400 Bad Request||HTTP/1.1 400)`.

The client announces a chunked body, sends half of it, and closes its side of
the connection. The server answers `201 Created` — for a file it only received
part of.

## Cause

```cpp
    else if (bytesRead == 0)
    {
        DEBUG("Multiplexer") << "_readClient: peer closed fd=" << fd;
        iter->second.parsed_request.state = ClientRequest::DONE;
        enableWrite(fd);
        return;
    }
```

End-of-stream is turned into `DONE` unconditionally. `DONE` means "the request
was fully received"; the dispatcher trusts it and runs the handler, which
happily stores a truncated upload or hands a truncated body to a CGI.

The distinction that matters is *where* the parser was when the peer went away:

* still reading headers, nothing buffered → the client just opened and dropped
  the connection. Nothing to answer; close it.
* headers or body incomplete → the request is truncated: 400 Bad Request.
* already `DONE` or `ERROR_STATE` → a client that finished its request and
  half-closed the socket to signal it. Answer normally.

## Fix

```cpp
/*
 * The peer closed its side. A request that was still being read is truncated,
 * and a truncated request is a bad request: answering it as if it were
 * complete would act on a body the client never finished sending.
 */
void Multiplexer::handlePeerShutdown(int fd, Client& client)
{
    ClientRequest& request = client.parsed_request;

    if (request.state == ClientRequest::HEADERS && client.request.empty())
    {
        _removeClient(fd);
        return;
    }
    if (request.state != ClientRequest::DONE && request.state != ClientRequest::ERROR_STATE)
    {
        WARN() << "Multiplexer::handlePeerShutdown: truncated request, responding status=400 fd=" << fd;
        request.setStatusCode(HTTP_400_BAD_REQUEST);
        request.state = ClientRequest::ERROR_STATE;
    }
    enableWrite(fd);
}
```

and the read path calls it:

```cpp
    if (bytesRead == 0)
    {
        DEBUG("Multiplexer") << "_readClient: peer closed fd=" << fd;
        handlePeerShutdown(fd, *client);
        return;
    }
```

Declaration in `includes/multiplexing/header.hpp`:

```cpp
        void                            handlePeerShutdown(int fd, Client& client);
```

The silent close for a connection that never sent anything matters under load:
health checks, port scanners and browser pre-connects all open and drop
connections, and answering them with 400 fills the log with noise about clients
that did nothing wrong.

## Verification

```
$ python3 - <<'EOF'
import socket
s = socket.create_connection(('127.0.0.1', 1025))
s.sendall(b'POST /upload/x.txt HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n10\r\nonly-half')
s.shutdown(socket.SHUT_WR)
print(s.recv(200).split(b'\r\n')[0].decode())
EOF
HTTP/1.1 400 Bad Request
```

and no truncated file is created.
