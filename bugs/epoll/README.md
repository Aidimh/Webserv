# epoll/ — the event loop

Who gets woken up, in what order, and how long a connection lives. These are the
bugs that do not belong to any one request: they show up as *other* requests
failing.

**Where these live:** `src/Multiplexing/Engine.cpp` → `Multiplexer`.

| # | Bug | Breaks |
|---|-----|--------|
| [10](10-poll-loop-index-invalidation.md) | The event loop erases from its descriptor vector while indexing it | random dropped clients |
| [23](23-connection-always-closed.md) | Every connection is closed after one response | Stress 6 |
| [31](31-epoll-event-loop.md) | The event loop rescans every connection on every round — `poll()` → `epoll` | nothing; cost per event |

Order matters here. [10](10-poll-loop-index-invalidation.md) is the correctness
fix and introduces the registry — one owner for the interest list, four
functions, no handler touching it directly. [23](23-connection-always-closed.md)
adds keep-alive, which is what makes the number of watched descriptors large
enough to matter. [31](31-epoll-event-loop.md) then swaps `poll()` for `epoll`
underneath the registry, and is the one file in `bugs/` that has not been run
against the tester yet.

The CGI pipes are watched by this same loop; their handlers are in
[cgi/](../cgi/README.md).

Back to the [index](../README.md).
