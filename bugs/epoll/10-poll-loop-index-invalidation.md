# 10 — The event loop erases from `_pollfds` while walking it by index

**Where:** `src/Multiplexing/Engine.cpp` → `run()`

> The defect and the dispatch structure below stand as written. The mechanism
> underneath them does not: [31](31-epoll-event-loop.md) replaces `poll()` with
> `epoll`, which makes `collectReadyEvents()` unnecessary and renames the four
> registry functions. Read this file for *why* the loop is shaped this way and
> [31](31-epoll-event-loop.md) for the code that is actually in the Engine.

## Symptom

Clients are dropped at random while another connection is being closed in the
same poll round. It looks like a flaky network:

```
Failed to send request to the server.
✘ FAIL  12 » Nop directory GET file returns 200   (missing: 200 OK)
```

The very same request passes on its own — the failure only appears when
several connections are active at once, which is exactly what the stress tests
(27, 28, 29) do 100 000 times.

## Cause

`run()` walks `_pollfds` with an index while the handlers it calls erase from
that same vector:

```cpp
for (size_t i = 0; i < _pollfds.size(); i++)
{
    ...
    if (_pollfds[i].revents & POLLIN)
        _readClient(_pollfds[i].fd);          // may _removeClient -> erase at i
    if (_pollfds[i].revents & POLLOUT)
        _writeClient(_pollfds[i].fd);         // same
    if (_pollfds[i].revents & (POLLHUP | POLLERR))
        _removeClient(_pollfds[i].fd);
}
```

Once entry `i` is erased, `_pollfds[i]` is a **different** connection:

* Its `revents` are read as if they belonged to the erased one, so a healthy
  client can be handed to `_removeClient()` on someone else's `POLLHUP` — the
  dropped-connection failure above.
* `i++` then skips the entry after it, whose events go unserved this round.
* The vector may also have been reallocated by an `_acceptNewClient()` earlier
  in the same loop, so the whole `_pollfds[i]` expression is re-evaluated
  against moved storage.

The CGI branch turns the same mistake into a wrong-key erase:

```cpp
close(_pollfds[i].fd);
_pollfds.erase(_pollfds.begin() + i);
_cgi_pids.erase(_pollfds[i].fd);      // <-- reads the *next* entry
_cgi_pipes.erase(_pollfds[i].fd);     // <-- erases a live CGI's bookkeeping
```

`disableWrite(fd)` after `_removeClient(fd)` in `_writeClient()` is the third
variant: the descriptor is closed, the kernel may have handed the same number
to a brand new connection, and `POLLOUT` is cleared on that innocent client.

## Fix

Never index the live vector while handlers are running: take the ready set as a
batch first, then look every descriptor up **by value**. An event can then never
land on the wrong socket, whatever the handlers do underneath.

Under `poll()` the batch has to be copied by hand, because `revents` lives in
the vector the handlers mutate. Under `epoll` it comes for free —
`epoll_wait()` fills a private array — which is why
[31](31-epoll-event-loop.md) deletes the copy step and keeps everything else.
The dispatch below is written in its final, `epoll` form.

```cpp
void Multiplexer::dispatchEvents(struct epoll_event* events, int count)
{
    _dead_fds.clear();
    for (int i = 0; i < count; i++)
        handleEvent(events[i].data.fd, events[i].events);
}
```

```cpp
void Multiplexer::handleEvent(int fd, uint32_t revents)
{
    if (_dead_fds.find(fd) != _dead_fds.end())
        return;
    if (!isRegistered(fd))
        return;
    if (handleServerEvent(fd, revents))
        return;
    if (handleCgiEvent(fd, revents))
        return;
    handleClientEvent(fd, revents);
}
```

Those two lines are what make a stale batch safe. `isRegistered()` drops an
event for a descriptor that has since been unregistered; `_dead_fds` drops one
for a descriptor that was closed *and whose number has already been handed back
by `accept()`* inside the same batch — the case `isRegistered()` cannot see,
because the number is legitimately registered again, to somebody else.

```cpp
bool Multiplexer::handleServerEvent(int fd, uint32_t revents)
{
    for (size_t i = 0; i < _servers.size(); i++)
    {
        if (_servers[i]->get_fd() != fd)
            continue;
        if (revents & EPOLLIN)
            _acceptNewClient(_servers[i]);
        return true;
    }
    return false;
}
```

```cpp
bool Multiplexer::handleCgiEvent(int fd, uint32_t revents)
{
    if (_cgi_pipes.find(fd) == _cgi_pipes.end())
        return false;

    Client* client = findClientByPipe(fd);
    if (client == NULL)
    {
        removeFd(fd);
        _cgi_pipes.erase(fd);
        close(fd);
        return true;
    }
    if (fd == client->cgi.stdin_fd)
    {
        if (revents & (EPOLLERR | EPOLLHUP))
            closeCgiInput(*client);
        else if (revents & EPOLLOUT)
            writeCgiInput(fd);
        return true;
    }
    if (revents & (EPOLLIN | EPOLLHUP | EPOLLERR))
        readCgiOutput(fd);
    return true;
}
```

```cpp
void Multiplexer::handleClientEvent(int fd, uint32_t revents)
{
    if (revents & EPOLLIN)
        _readClient(fd);
    if (findClient(fd) != NULL && (revents & EPOLLOUT))
        _writeClient(fd);
    if (findClient(fd) != NULL && (revents & (EPOLLHUP | EPOLLERR)))
        _removeClient(fd);
}
```

Every step re-checks that the client still exists, because the previous step
may legitimately have closed it.

```cpp
void Multiplexer::run()
{
    struct epoll_event events[MAX_EVENTS];

    INFO() << "Multiplexer::run: running, waiting for events";
    while (loop_is_true)
    {
        killTimedOutCgi();
        closeIdleClients();
        applyCgiBackPressure();

        int ready = epoll_wait(_epoll_fd, events, MAX_EVENTS, EPOLL_TIMEOUT_MS);
        if (ready < 0)
        {
            if (errno == EINTR)
                break;
            ERR() << "Multiplexer::run: epoll_wait failed: " << strerror(errno);
            throw Error::Epoll();
        }
        if (ready == 0)
            continue;
        DDEBUG("Multiplexer") << "run: epoll reported " << ready << " ready fd(s)";
        dispatchEvents(events, ready);
    }
}
```

### The registry: one owner for the interest list

The real cure for this class of bug is that no handler touches the interest list
directly. Every add/remove/modify went through hand-written loops scattered
across the file; they become four small functions —

| | |
|---|---|
| `addFd(fd, events)` | start watching a descriptor |
| `removeFd(fd)` | stop watching it, and mark it dead for this batch |
| `setEvents(fd, events)` | change what it is watched for |
| `isRegistered(fd)` | is it watched at all |

— and `enableWrite()` / `disableWrite()` become one-liners on top of
`setEvents()`. Their implementations are in
[31](31-epoll-event-loop.md#2-the-registry-same-four-functions-epoll-underneath);
this is the seam that let the whole mechanism be swapped without any handler
above noticing.

Lookup helpers used throughout, replacing repeated `_clients.find(fd)` blocks
and — importantly — replacing `_clients[fd]`, whose `operator[]` silently
inserts an empty client for an unknown descriptor:

```cpp
Client* Multiplexer::findClient(int fd)
{
    std::map<int, Client>::iterator it = _clients.find(fd);

    if (it == _clients.end())
        return NULL;
    return &it->second;
}
```

```cpp
Client* Multiplexer::findClientByPipe(int pipe_fd)
{
    std::map<int, int>::iterator it = _cgi_pipes.find(pipe_fd);

    if (it == _cgi_pipes.end())
        return NULL;
    return findClient(it->second);
}
```

Constants at the top of the file:

```cpp
/* Longest epoll_wait() sleep: bounded so the CGI and idle sweeps always run. */
static const int EPOLL_TIMEOUT_MS = 1000;

/* Ready events harvested per round; a leftover is re-reported next round. */
static const int MAX_EVENTS = 256;
```

The old loop also logged `INFO() << "running, waiting for events"` on **every
iteration** and printed three `DEBUG` lines per descriptor per round; at 100 000
requests that is more time in the logger than in the server. The banner moved
out of the loop.

## Verification

```
✔ PASS  27 » Stress fork 5 requests 20 (100/100 OK)
✔ PASS  28 » Stress fork 20 requests 5000 (100000/100000 OK)
✔ PASS  29 » Stress fork 128 requests 50 (6400/6400 OK)
✔ PASS  12 » Nop directory GET file returns 200
```

100 000 sequential requests and 6 400 concurrent ones with no dropped
connection.
