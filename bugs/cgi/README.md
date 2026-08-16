# cgi/ — running scripts

The largest group, and the one the tester leans on hardest. Starting a child,
handing it an environment and a body, reading its answer back as HTTP, and
making sure neither the script nor its death can take the server with it.

**Where these live:** `src/CGI/`, and the CGI half of
`src/Multiplexing/Engine.cpp`.

| # | Bug | Breaks |
|---|-----|--------|
| [06](06-cgi-connection-dropped.md) | The connection is closed the moment a CGI starts | 7, 16, 16b, 17, 18, 31 |
| [07](07-cgi-environment-incomplete.md) | The CGI environment is missing `SERVER_PROTOCOL` and the `HTTP_*` headers | 16, 16b, 17, 18, 31 |
| [08](08-cgi-body-lost-and-deadlock.md) | The request body is not sent to the CGI, and big bodies deadlock | 16, 16b, 17, 18, 31 |
| [09](09-cgi-script-path.md) | `extension_found` is used uninitialised and the script path loses its `/` | every CGI route |
| [12](12-sigpipe-kills-the-server.md) | A CGI that exits early kills the whole server | CGI under load |
| [13](13-cgi-timeout-never-fires.md) | The CGI timeout never runs, and would kill healthy transfers | 16, 16b, 17, 31 |
| [14](14-cgi-output-is-not-http.md) | Raw CGI output is forwarded without a status line or framing | 16, 16b, 17, 18, 31 |
| [26](26-cgi-working-directory.md) | A CGI runs in the server's working directory | CGI 6 |

Read them in order: [06](06-cgi-connection-dropped.md) introduces `CgiState`,
`startCgi()` and `releaseCgi()`, and [08](08-cgi-body-lost-and-deadlock.md),
[13](13-cgi-timeout-never-fires.md) and [14](14-cgi-output-is-not-http.md) all
build on that structure. They share the event loop with
[epoll/](../epoll/README.md) — the pipes are ordinary watched descriptors.

Back to the [index](../README.md).
