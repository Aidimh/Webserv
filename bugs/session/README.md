# session/ — session state

Cookies, and the state that hangs off them. One file, because the server had no
session support at all: this is the only entry in `bugs/` that adds a feature
the tester requires rather than repairing something that misbehaves.

**Where this lives:** `src/Multiplexing/Engine.cpp` (`assignSession()`) and the
response path that sets `Set-Cookie`.

| # | Bug | Breaks |
|---|-----|--------|
| [30](30-no-session-support.md) | There is no session support | Session 1-5 |

The session ids it generates use `rand()` — enough for the tester, not enough
for a hostile client. That limitation is recorded in
[RESULTS.md](../RESULTS.md#not-done-on-purpose).

Back to the [index](../README.md).
