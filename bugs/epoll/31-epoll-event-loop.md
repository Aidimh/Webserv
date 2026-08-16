# 31 — The event loop scales with the number of connections: move it to `epoll`

**Where:** `src/Multiplexing/Engine.cpp` → `run()`, and every function that
touches `_pollfds`
**Also touches:** `includes/multiplexing/header.hpp` (Multiplexer members and
prototypes), `includes/Errors/Error.hpp` + `src/Multiplexing/Error_messages.cpp`
(a new `Error::Epoll`)

> **Status.** Every other file in this folder describes a defect that was
> reproduced, fixed and re-run against the tester. This one is a change of
> *mechanism*, not a repair, and it has **not** been run against the tester yet.
> The code below is written against the post-fix Engine described in
> [10](10-poll-loop-index-invalidation.md), [06](../cgi/06-cgi-connection-dropped.md),
> [13](../cgi/13-cgi-timeout-never-fires.md), [14](../cgi/14-cgi-output-is-not-http.md) and
> [23](23-connection-always-closed.md); apply those first. The checks that must
> pass before it is believed are at the bottom.

## Symptom

Nothing fails. The server answers everything correctly — it just does an amount
of work per event that grows with the number of connections it is holding.

`poll()` is handed the whole interest list on every single call. Stress 6 keeps
10 000 idle keep-alive connections open ([23](23-connection-always-closed.md));
one HTTP request arriving on one of them costs:

* 10 000 `struct pollfd` copied into the kernel,
* 10 000 file descriptors examined by the kernel,
* 10 000 copied back out,
* one `O(n)` scan of the vector to collect the handful whose `revents` are set
  ([10](10-poll-loop-index-invalidation.md)),

and then every registry call that touches the descriptor — arm `POLLOUT`,
disarm it, check registration, unregister — walks the same 10 000-entry vector
again. A single keep-alive request/response pair does five or six full
traversals to move a few hundred bytes.

## Cause

`poll()` has no memory. The interest list is an *argument*, so it is rebuilt,
transferred and rescanned from scratch on each round, and the API can only
report readiness by writing `revents` back into that same array — which is what
made the index-invalidation bug in [10](10-poll-loop-index-invalidation.md)
possible in the first place, and what forces the snapshot that fixes it.

`epoll` inverts the ownership: the interest list is kernel state, mutated once
per change with `epoll_ctl()`, and `epoll_wait()` returns *only* the ready
descriptors into a private array. The per-round cost stops depending on how many
connections are idle.

## Fix

The public shape of the Engine does not change. `run()` still loops,
`_readClient()` / `_writeClient()` / `readCgiOutput()` / `writeCgiInput()` are
untouched, and **the event masks armed are the same bits the `poll()` version
armed** — this file changes the mechanism, not the policy.

### 1. The interest list moves into the kernel

```cpp
/* Longest epoll_wait() sleep: bounded so killTimedOutCgi() and closeIdleClients() always run. */
static const int EPOLL_TIMEOUT_MS = 1000;

/* Ready events harvested per round. A leftover is not lost: level-triggered epoll re-reports it. */
static const int MAX_EVENTS = 256;
```

```cpp
Multiplexer::Multiplexer() : _epoll_fd(-1)
{
    _epoll_fd = epoll_create(MAX_EVENTS);
    if (_epoll_fd == -1)
    {
        ERR() << "Multiplexer::Multiplexer: epoll_create failed: " << strerror(errno);
        throw Error::Epoll();
    }
    fcntl(_epoll_fd, F_SETFD, FD_CLOEXEC);
    DEBUG("Multiplexer") << "Multiplexer: epoll instance fd=" << _epoll_fd;
}
```

The size argument to `epoll_create()` has been ignored by the kernel since
2.6.8, but it must still be positive. `FD_CLOEXEC` keeps the epoll descriptor
out of every CGI child — a forked script has no business holding the server's
event queue open.

```cpp
Multiplexer::~Multiplexer()
{
    for (size_t i = 0; i < _servers.size(); i++)
        delete _servers[i];
    if (_epoll_fd != -1)
        close(_epoll_fd);
}
```

### 2. The registry: same four functions, `epoll` underneath

[10](10-poll-loop-index-invalidation.md) made one owner responsible for the
interest list. That seam is what makes this swap a local change: only these
functions know which system call is in use.

`_pollfds` is replaced by `_watched`, a mirror of what the kernel has been told.
It exists for two reasons — `epoll` offers no way to *ask* whether a descriptor
is registered or with which mask, and the mirror answers both in `O(log n)`
instead of a linear scan.

```cpp
void Multiplexer::addFd(int fd, uint32_t events)
{
    struct epoll_event entry;

    memset(&entry, 0, sizeof(entry));
    entry.events = events;
    entry.data.fd = fd;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &entry) == -1)
    {
        WARN() << "Multiplexer::addFd: epoll_ctl ADD failed fd=" << fd
               << ": " << strerror(errno);
        return;
    }
    _watched[fd] = events;
    DDEBUG("Multiplexer") << "addFd: watching fd=" << fd << " events=" << events
                          << " total=" << _watched.size();
}
```

```cpp
/*
 * EPOLL_CTL_DEL before close(), always: a descriptor inherited by a forked CGI
 * child keeps the underlying open file description alive, so close() alone does
 * NOT drop the entry, and epoll_wait() goes on reporting an fd number this
 * process has already handed to somebody else.
 */
void Multiplexer::removeFd(int fd)
{
    if (_watched.find(fd) == _watched.end())
        return;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1)
    {
        WARN() << "Multiplexer::removeFd: epoll_ctl DEL failed fd=" << fd
               << ": " << strerror(errno);
    }
    _watched.erase(fd);
    _dead_fds.insert(fd);
    DDEBUG("Multiplexer") << "removeFd: stopped watching fd=" << fd
                          << " total=" << _watched.size();
}
```

```cpp
void Multiplexer::setEvents(int fd, uint32_t events)
{
    std::map<int, uint32_t>::iterator it = _watched.find(fd);

    if (it == _watched.end() || it->second == events)
        return;                              /* nothing to say to the kernel */

    struct epoll_event entry;

    memset(&entry, 0, sizeof(entry));
    entry.events = events;
    entry.data.fd = fd;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &entry) == -1)
    {
        WARN() << "Multiplexer::setEvents: epoll_ctl MOD failed fd=" << fd
               << ": " << strerror(errno);
        return;
    }
    it->second = events;
}
```

The `it->second == events` early return matters more than it looks:
`applyCgiBackPressure()` ([14](../cgi/14-cgi-output-is-not-http.md)) re-asserts a mask
for every running CGI on every round, and `finishResponse()`
([23](23-connection-always-closed.md)) re-arms `EPOLLIN` after every response.
Under `poll()` those were vector writes; under `epoll` each one would be a
system call. Diffing against the mirror makes the no-op case free.

```cpp
bool Multiplexer::isRegistered(int fd) const
{
    return _watched.find(fd) != _watched.end();
}
```

```cpp
void Multiplexer::enableWrite(int fd)
{
    setEvents(fd, EPOLLOUT);
}
```

```cpp
void Multiplexer::disableWrite(int fd)
{
    std::map<int, uint32_t>::iterator it = _watched.find(fd);

    if (it == _watched.end())
        return;
    setEvents(fd, it->second & ~static_cast<uint32_t>(EPOLLOUT));
}
```

`enableWrite()` arms `EPOLLOUT` alone, exactly as the `poll()` version armed
`POLLOUT` alone: a client with a response in flight is not read from until
`finishResponse()` puts it back to `EPOLLIN`. `EPOLLERR` and `EPOLLHUP` are
reported whether or not they are requested, so a connection whose mask has
dropped to `0` is still noticed when it breaks.

### 3. The loop

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

One `epoll_wait()` per iteration, still the loop's only blocking point, still
bounded so the CGI and idle sweeps run on a quiet server
([13](../cgi/13-cgi-timeout-never-fires.md)).

### 4. Dispatch: the snapshot is gone, the guards are not

`collectReadyEvents()` is deleted. `epoll_wait()` already fills an array the
handlers cannot touch, so the copy [10](10-poll-loop-index-invalidation.md) had
to make by hand comes free — and no index into a mutating vector exists any
more.

What does *not* go away is that the array is a description of the world as it
was before the first handler ran. Two things can be wrong with an entry by the
time it is reached:

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
        return;                 /* closed earlier in this same batch */
    if (!isRegistered(fd))
        return;                 /* never registered, or unregistered since */
    if (handleServerEvent(fd, revents))
        return;
    if (handleCgiEvent(fd, revents))
        return;
    handleClientEvent(fd, revents);
}
```

`isRegistered()` alone is not enough, and was not enough under `poll()` either.
A descriptor closed by the first event in a batch is immediately available for
reuse: an `accept()` later in the same batch can be handed the same number, and
the stale event would then be applied to a brand new connection that happens to
be registered. `_dead_fds` — filled by `removeFd()`, cleared at the top of every
dispatch, so it never spans more than one batch — closes that window. Losing one
round of events for a freshly accepted descriptor costs nothing: level-triggered
`epoll` reports it again immediately.

### 5. The three handlers

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

One `accept()` per event, as before: with level triggering a backlog that is not
drained is simply reported again on the next round, which spreads a burst of
connections over several iterations instead of starving everyone else during it.

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
            closeCgiInput(*client);         /* the script stopped reading */
        else if (revents & EPOLLOUT)
            writeCgiInput(fd);
        return true;
    }
    if (revents & (EPOLLIN | EPOLLHUP | EPOLLERR))
        readCgiOutput(fd);                  /* drain first, EOF is read()==0 */
    return true;
}
```

Two orderings in there are load-bearing:

* **Read before believing a hangup.** A script that writes its answer and exits
  produces `EPOLLIN | EPOLLHUP` in the same event. The pipe still holds the
  answer; the writer is merely gone. Treating `EPOLLHUP` as "finished" and
  closing the pipe throws away the tail of every fast CGI response. Both flags
  route to `readCgiOutput()`, which ends the response on `read()` returning `0`
  — the one thing that actually means end of output.
* **`EPOLLERR` on the input pipe is the dead child.** With `SIGPIPE` ignored
  ([12](../cgi/12-sigpipe-kills-the-server.md)), a script that exits before consuming
  the body shows up here rather than as a signal, and the body source is closed
  without another `write()` being attempted.

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

Unchanged from [10](10-poll-loop-index-invalidation.md) apart from the flag
names: each step re-checks that the client still exists, because the step before
it may legitimately have closed it.

### 6. Unregister before closing

Under `poll()` the order of `close()` and unregistering was a matter of taste.
Under `epoll` it is not, for the reason spelled out in `removeFd()` above, and
this server forks: every CGI child inherits copies of the parent's descriptors,
which keeps their open file descriptions — and therefore their epoll entries —
alive past the parent's `close()`.

```cpp
void Multiplexer::_removeClient(int fd)
{
    std::map<int, Client>::iterator it = _clients.find(fd);

    if (it == _clients.end())
        return;
    releaseCgi(it->second);
    removeFd(fd);
    close(fd);
    _clients.erase(it);
    DEBUG("Multiplexer") << "_removeClient: closed client fd=" << fd;
}
```

`releaseCgi()` and `closeCgiInput()`
([06](../cgi/06-cgi-connection-dropped.md)) follow the same rule — `removeFd()` on each
pipe before its `close()`. That is the only edit those two functions need.

### 7. Header and error type

`includes/multiplexing/header.hpp` — the `poll()` includes go, `<set>` and
`<stdint.h>` arrive (`<sys/epoll.h>` is already there, twice):

```cpp
#include <sys/epoll.h>
#include <stdint.h>
#include <set>
```

```cpp
class Multiplexer
{
    private:
        std::vector<Socket *>           _servers;
        std::map<int, Client>           _clients;
        int                             _epoll_fd;
        std::map<int, uint32_t>         _watched;    /* mirror of the kernel interest list */
        std::set<int>                   _dead_fds;   /* closed during the current batch */
        std::map<int , int>             _cgi_pipes;

        void        addFd(int fd, uint32_t events);
        void        removeFd(int fd);
        void        setEvents(int fd, uint32_t events);
        bool        isRegistered(int fd) const;
        void        dispatchEvents(struct epoll_event* events, int count);
        void        handleEvent(int fd, uint32_t revents);
        bool        handleServerEvent(int fd, uint32_t revents);
        bool        handleCgiEvent(int fd, uint32_t revents);
        void        handleClientEvent(int fd, uint32_t revents);
        /* ... unchanged members and helpers ... */
};
```

`_cgi_pids` and `cgi_timeouts` are already gone with
[13](../cgi/13-cgi-timeout-never-fires.md); `_pollfds` is the last member this file
removes.

`includes/Errors/Error.hpp`, next to `Error::Poll`:

```cpp
        class Epoll : public std::exception
        {
            public:
                const char* what() const throw();
        };
```

`src/Multiplexing/Error_messages.cpp`:

```cpp
const char* Error::Epoll::what() const throw()
{
    return "Error!\nEpoll failed!.";
}
```

`Error::Poll` stays — nothing else references it, but removing a public error
type is a separate decision.

`addServer()` is the only remaining caller that changes:

```cpp
void Multiplexer::addServer(Socket *s)
{
    _servers.push_back(s);
    addFd(s->get_fd(), EPOLLIN);
}
```

## Notes

* **Level-triggered on purpose.** `EPOLLET` is not used. Every handler here does
  exactly one `recv()`/`send()`/`read()`/`write()` per event and relies on being
  told again — `sendStreaming()` sends one 4 KB chunk per `EPOLLOUT`,
  `_acceptNewClient()` accepts one connection per `EPOLLIN`. Edge triggering
  would require every one of them to loop until `EAGAIN`, and a single missed
  drain would hang that connection for ever. Level-triggered `epoll` is a drop-in
  for `poll()`; edge-triggered is a different program.
* **`EPOLLRDHUP` is deliberately not requested.** It would let the server
  distinguish a half-close from a full one, which is a behaviour change
  (`_readClient()` currently treats `recv() == 0` as end of request). Worth
  doing, not worth smuggling into a mechanism swap.
* **`epoll` is Linux-only.** `poll()` builds anywhere; this does not. If the
  project has to run on macOS, the four registry functions are precisely the
  seam to compile two ways — `kqueue` on BSD, `epoll` on Linux, nothing above
  them aware of which.
* **Subject compliance is unchanged.** `epoll_create` / `epoll_ctl` /
  `epoll_wait` are in the permitted family; there is still exactly one call to
  the multiplexer per loop iteration, every socket is still non-blocking, and no
  `read`/`write`/`recv`/`send` happens outside an event it reported.
* **`MAX_EVENTS = 256`** is a harvest size, not a limit on connections. When
  more are ready, the surplus is reported on the next round.

## Verification

Not yet run. Before this replaces the `poll()` loop, all of the following must
hold.

1. Behaviour is identical — the whole tester, unchanged:

```
$ make re && ./webserv EngineX.conf
$ ./tester ...
Minimum Evaluation Tests      Total: 32     Passed: 32     Failed: 0
All 13 suites                 Total: 124    Passed: 124    Failed: 0
```

2. The interest list is not leaking. After a full run, with the server idle, the
   number of registered descriptors must be back to one per listening socket:

```
$ ls /proc/$(pgrep -f 'webserv')/fd | wc -l
```

3. No stale entries: run the CGI suites, then confirm `epoll_wait` is not
   returning events for descriptors the process has closed —

```
$ strace -f -e trace=epoll_ctl,epoll_wait,close -p $(pgrep -f 'webserv')
```

   every `close()` on a watched descriptor must be preceded by an
   `epoll_ctl(..., EPOLL_CTL_DEL, ...)`.

4. The point of the exercise — Stress 6 (10 000 idle keep-alive connections)
   with one active client. `epoll_wait` returning a handful of events per round
   instead of a 10 000-entry scan should be visible both in `strace -c` (the
   `poll` line disappears, `epoll_wait` costs a fraction of it) and in the
   server's CPU time while the connections sit idle. Record the before/after
   numbers here once measured; do not assume the win, measure it.
