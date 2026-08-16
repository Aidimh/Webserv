# 13 — The CGI timeout never runs, and would kill healthy transfers if it did

**Where:** `src/Multiplexing/Engine.cpp` → `run()` (the `cgi_timeouts` block)

## Symptom

Two opposite failures from one piece of code.

* **It never fires.** A CGI stuck in an infinite loop holds its client for ever
  on an otherwise idle server. The timeout sweep only executes when *some other*
  connection wakes the loop up.
* **It fires on healthy work.** As soon as traffic does arrive, the sweep kills
  any CGI that has been running for more than 5 seconds — including the 100 MB
  streaming uploads of tests 16, 16b, 17 and 31, which legitimately take longer.

## Cause

```cpp
int poll_ret = poll(_pollfds.data(), _pollfds.size(), -1);
```

`-1` means "block until an event". With no traffic, the sweep at the top of the
loop is simply never reached.

And the condition measures the wrong thing:

```cpp
if ((current - iter->second) > 5)          // 5 = total run time
```

`cgi_timeouts[pipe_fd]` is stamped once, at start. That is *total duration*, not
*inactivity*. A CGI streaming 100 MB is perfectly healthy at second 30; a CGI
that has produced nothing for 5 seconds is not. The two are indistinguishable
here.

The cleanup itself is also unsafe: it erases from `_pollfds` by index inside the
loop ([10](../epoll/10-poll-loop-index-invalidation.md)), and after killing the child it
leaves `_clients[client_fd].response` set through `operator[]`, which
resurrects a `Client` that may already be gone.

## Fix

The wait gets a bounded timeout so the sweep always runs, and the sweep measures
silence rather than duration.

```cpp
/* Longest epoll_wait() sleep: short enough to notice a stuck CGI on an idle server. */
static const int EPOLL_TIMEOUT_MS = 1000;
```

(`epoll_wait()` since [31](../epoll/31-epoll-event-loop.md); with `poll()` it is the same
argument in the same position.)

`last_activity` is stamped in `CgiState` every time a byte moves in either
direction — `writeCgiInput()` and `readCgiOutput()` both refresh it.

```cpp
/*
 * A CGI is killed when it stops talking, not when it takes long: streaming a
 * big body is slow but healthy, a hung script produces nothing at all.
 */
void Multiplexer::killTimedOutCgi()
{
    std::map<int, Client>::iterator it;
    time_t now = time(NULL);

    for (it = _clients.begin(); it != _clients.end(); ++it)
    {
        Client& client = it->second;

        if (!client.cgi.running || now - client.cgi.last_activity <= CGI_TIMEOUT)
            continue;
        WARN() << "Multiplexer::killTimedOutCgi: cgi pid=" << client.cgi.pid
               << " idle for " << (now - client.cgi.last_activity)
               << "s, responding status=504 client fd=" << client.fd;
        if (client.cgi.headers_done)
            client.response += "0\r\n\r\n";
        else
            client.response = AMethod::buildErrorResponse(HTTP_504_GATEWAY_TIMEOUT, "Gateway Timeout").toString();
        releaseCgi(client);
        enableWrite(client.fd);
    }
}
```

Two cases, handled differently:

* Nothing has been sent yet → a clean `504 Gateway Timeout`.
* Headers already went out → the status line cannot be taken back, so the
  chunked stream is terminated with the final `0\r\n\r\n` and the client gets a
  complete, if truncated, message instead of a hanging socket.

`releaseCgi()` ([06](06-cgi-connection-dropped.md)) kills the child, reaps it,
unregisters both pipes and closes them — so the `cgi_timeouts`, `_cgi_pids` maps
and the manual interest-list surgery all disappear. `CgiState::running` is the only
flag left to check, and it lives next to the client it belongs to.

`CGI_TIMEOUT` is the existing constant in `includes/multiplexing/header.hpp`
(`5` seconds of silence). It should eventually come from a `cgi_timeout`
directive — the tester's configuration file sets one — but the value is at least
applied to the right quantity now.

## Verification

```
$ curl -s -o /dev/null -w '%{http_code}\n' --max-time 30 http://127.0.0.1:1025/cgi-bin/sleeper.py
504                       # script sleeps, no output -> killed after 5s of silence
$ curl -s -X POST --data-binary @100mb.bin http://127.0.0.1:1027/directory/youpi.bla | wc -c
100000000                 # 11s of continuous traffic -> never killed
```

Tester:

```
✔ PASS  16  » Chunked large 100M chars body with y
✔ PASS  16b » Content-Length large 100M chars body with y
✔ PASS  17  » Chunked large 100M chars body with e
```
