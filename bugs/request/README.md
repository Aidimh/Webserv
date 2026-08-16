# request/ — reading the client

Turning bytes off the socket into a request the rest of the server can trust:
headers, `Content-Length`, chunked encoding, the URI, and the temp files a body
is streamed into.

**Where these live:** `src/Request/`, and the body handling reached from
`Multiplexer::_readClient()`.

| # | Bug | Breaks |
|---|-----|--------|
| [04](04-empty-body-never-completes.md) | A request with `Content-Length: 0` never gets an answer | 19 |
| [15](15-query-string-not-split.md) | `?query` stays glued to the file path | CGI / query suites |
| [20](20-temp-body-files-leak.md) | Every uploaded body is left behind in `www/upload/` | disk fills under load |
| [27](27-uri-validation.md) | Wrong status for traversal, `%00` and over-long URIs | Path 6, 7, 8, 10 |
| [28](28-chunked-trailers-rejected.md) | A chunked request with trailers is rejected as malformed | Chunked 16 |
| [29](29-truncated-request-treated-as-complete.md) | A request cut off mid-body is answered as if complete | Chunked 15 |

The two chunked bugs ([28](28-chunked-trailers-rejected.md),
[29](29-truncated-request-treated-as-complete.md)) are opposite errors in the
same state machine: one rejects a legal request, the other accepts an
incomplete one.

Back to the [index](../README.md).
