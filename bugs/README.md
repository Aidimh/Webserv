# bugs/

One file per bug. Every file contains:

* **Where** — the file and function the defect lives in.
* **Symptom** — what the server actually does, and which tester case it breaks.
* **Cause** — why it happens.
* **Fix** — the *complete* replacement function(s), ready to drop in. Nothing is
  abbreviated: when a fix needs a helper the helper is written out too, and when
  a function was too big to do one thing it is split and every part is given.
* **Verification** — the command and the result that proves the fix.

Nothing in `src/` or `includes/` was modified. Every fix in this folder was
compiled and run against the tester in a throw-away copy of the repository
before being written down — with one stated exception,
[31](epoll/31-epoll-event-loop.md).

## Layout

Files are grouped by the part of the server they belong to. **The numbers are
stable ids, not an order**: they are the order the bugs were found, they survive
every reorganisation, and everything cross-references by number.

| Folder | What lives there | |
|--------|------------------|---|
| [config/](config/README.md) | the configuration file: parsing it, and directives that are parsed but not obeyed | 7 |
| [request/](request/README.md) | reading and validating what the client sent | 6 |
| [response/](response/README.md) | methods, status codes and headers on the way out | 5 |
| [cgi/](cgi/README.md) | starting scripts, feeding them, reading them back, killing them | 8 |
| [epoll/](epoll/README.md) | the event loop and connection lifetime | 3 |
| [session/](session/README.md) | cookies and session state | 1 |
| [build/](build/README.md) | the Makefile | 1 |

[00-test-setup.md](00-test-setup.md) and [RESULTS.md](RESULTS.md) stay at the
top: neither is a bug, one is what you need to reproduce any of this and the
other is what came out.

## Reference

Tester: `/home/aazzaoui/Downloads/web-serv-Tester-main`
Fixture + config source: `/home/aazzaoui/Downloads/web-serv-Tester-main/EngineX`

## Result after applying every fix in this folder

```
Minimum Evaluation Tests      Total: 32     Passed: 32     Failed: 0
All 13 suites                 Total: 124    Passed: 124    Failed: 0
```

Including the heavy ones: 100 MB chunked upload, 100 MB `Content-Length`
upload, 100 000 sequential requests, 6 400 concurrent redirects, 20 forks × 5 ×
100 MB through the CGI, and 10 000 simultaneous keep-alive connections.

Full breakdown, before-and-after, and what was deliberately left undone:
[RESULTS.md](RESULTS.md).

## Index

### Setup

| # | Bug | Breaks |
|---|-----|--------|
| [00](00-test-setup.md) | Test setup: the adapted `EngineX.conf` and the missing fixtures | every suite |

### [config/](config/README.md) — configuration file

| # | Bug | Breaks |
|---|-----|--------|
| [01](config/01-client-max-body-size-parsing.md) | `client_max_body_size 1000000;` is rejected, and the unit pointer dangles | server will not start |
| [02](config/02-location-client-max-body-size.md) | `client_max_body_size` cannot be set per location | 21, 22, 25, 26, 30 |
| [03](config/03-uninitialised-config-fields.md) | `Location_Config` / `Server_block` fields are read before being written | autoindex, root, index (random) |
| [11](config/11-max-body-size-first-port-only.md) | Only the first `listen` port of a server is recognised | 2nd port of any server |
| [16](config/16-return-directive-ignored.md) | `return 301 /x;` fails to parse and does nothing | Redirection suite |
| [21](config/21-error-page-directive-ignored.md) | `error_page` is parsed and never used | custom error pages |
| [22](config/22-upload-store-logic-inverted.md) | `upload_store` creates the directory only when it already exists | any config using it |

### [request/](request/README.md) — reading the client

| # | Bug | Breaks |
|---|-----|--------|
| [04](request/04-empty-body-never-completes.md) | A request with `Content-Length: 0` never gets an answer | 19 |
| [15](request/15-query-string-not-split.md) | `?query` stays glued to the file path | CGI / query suites |
| [20](request/20-temp-body-files-leak.md) | Every uploaded body is left behind in `www/upload/` | disk fills under load |
| [27](request/27-uri-validation.md) | Wrong status for traversal, `%00` and over-long URIs | Path 6, 7, 8, 10 |
| [28](request/28-chunked-trailers-rejected.md) | A chunked request with trailers is rejected as malformed | Chunked 16 |
| [29](request/29-truncated-request-treated-as-complete.md) | A request cut off mid-body is answered as if complete | Chunked 15 |

### [response/](response/README.md) — answering

| # | Bug | Breaks |
|---|-----|--------|
| [05](response/05-known-method-must-be-405.md) | `HEAD` is answered 501 instead of 405 | 3 |
| [17](response/17-head-method-missing.md) | There is no `HEAD` handler | Happy Path suite |
| [18](response/18-response-header-casing.md) | Header names are spelled inconsistently | header assertions |
| [24](response/24-post-to-a-directory.md) | `POST` to a directory answers 500 | Client Body Size 2 |
| [25](response/25-missing-allow-header.md) | A 405 answer omits the required `Allow` header | Method Restriction 5 |

### [cgi/](cgi/README.md) — running scripts

| # | Bug | Breaks |
|---|-----|--------|
| [06](cgi/06-cgi-connection-dropped.md) | The connection is closed the moment a CGI starts | 7, 16, 16b, 17, 18, 31 |
| [07](cgi/07-cgi-environment-incomplete.md) | The CGI environment is missing `SERVER_PROTOCOL` and the `HTTP_*` headers | 16, 16b, 17, 18, 31 |
| [08](cgi/08-cgi-body-lost-and-deadlock.md) | The request body is not sent to the CGI, and big bodies deadlock | 16, 16b, 17, 18, 31 |
| [09](cgi/09-cgi-script-path.md) | `extension_found` is used uninitialised and the script path loses its `/` | every CGI route |
| [12](cgi/12-sigpipe-kills-the-server.md) | A CGI that exits early kills the whole server | CGI under load |
| [13](cgi/13-cgi-timeout-never-fires.md) | The CGI timeout never runs, and would kill healthy transfers | 16, 16b, 17, 31 |
| [14](cgi/14-cgi-output-is-not-http.md) | Raw CGI output is forwarded without a status line or framing | 16, 16b, 17, 18, 31 |
| [26](cgi/26-cgi-working-directory.md) | A CGI runs in the server's working directory | CGI 6 |

### [epoll/](epoll/README.md) — the event loop

| # | Bug | Breaks |
|---|-----|--------|
| [10](epoll/10-poll-loop-index-invalidation.md) | The event loop erases from its descriptor vector while indexing it | random dropped clients |
| [23](epoll/23-connection-always-closed.md) | Every connection is closed after one response | Stress 6 |
| [31](epoll/31-epoll-event-loop.md) | The event loop rescans every connection on every round — `poll()` → `epoll` | nothing; cost per event |

### [session/](session/README.md) — session state

| # | Bug | Breaks |
|---|-----|--------|
| [30](session/30-no-session-support.md) | There is no session support | Session 1-5 |

### [build/](build/README.md) — build system

| # | Bug | Breaks |
|---|-----|--------|
| [19](build/19-makefile-header-dependencies.md) | Editing a header does not rebuild the objects that include it | silent memory corruption |

---

[31](epoll/31-epoll-event-loop.md) is the odd one out: a change of mechanism
rather than a defect repair, and the only file here whose code has **not** yet
been run against the tester. It is written to be applied last, on top of
everything else, and the rest of the folder is worded for the `epoll` engine it
produces — `addFd` / `removeFd` / `setEvents` / `isRegistered` and the `EPOLL*`
flags.
