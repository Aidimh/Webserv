# Results

Every suite of `web-serv-Tester-main`, run against the server with all 30 fixes
in this folder applied. The numbers below are from the `poll()` engine; the
`epoll` migration ([31](epoll/31-epoll-event-loop.md)) came after them and has not
been measured yet.

## Before

Minimum Evaluation Tests, on the code as it stands in `main`:

| | |
|---|---|
| Passed | 12 of the 15 cases that could be run at all |
| CGI cases (7, 16, 16b, 17, 18, 31) | no response — connection closed on CGI start |
| `/post_body` limit cases (21, 22, 25, 26, 30) | 201 instead of 413 — no per-location limit |
| Case 19 (`Content-Length: 0`) | timed out, no answer ever sent |
| Case 3 (`HEAD`) | 501 instead of 405 |
| Case 12 | failed intermittently — a connection dropped by the event loop |

The server also could not load any configuration file in the repository:
`client_max_body_size 1000000;` aborted the start
([01](config/01-client-max-body-size-parsing.md)).

## After

```
★ Overall Test Results
Total: 124
✔ Passed: 124    ✘ Failed: 0
```

| # | Suite | Result |
|---|-------|--------|
| 1 | Happy Path Tests | 14 / 14 |
| 2 | Error Handling Tests | 6 / 6 |
| 3 | Client Body Size Tests | 4 / 4 |
| 4 | Method Restriction Tests | 5 / 5 |
| 5 | Redirection Tests | 5 / 5 |
| 6 | Auto Index Tests | 5 / 5 |
| 7 | Routing Tests | 6 / 6 |
| 8 | CGI Tests | 7 / 7 |
| 9 | Chunked Transfer Tests | 19 / 19 |
| 10 | Session Tests | 5 / 5 |
| 11 | Stress Tests | 6 / 6 |
| 12 | Path Tests | 10 / 10 |
| 13 | **Minimum Evaluation Tests** | **32 / 32** |

### Minimum Evaluation, case by case

```
✔ 1  » Simple GET                                  ✔ 17  » Chunked large 100M chars body with e
✔ 2  » POST to root returns 405                    ✔ 18  » Chunked 100K chars body
✔ 3  » HEAD to root returns 405                    ✔ 19  » POST /post_body with empty body
✔ 4  » Directory redirect returns 301              ✔ 20  » POST /post_body with 100 bytes
✔ 5  » Directory GET with bad extension index      ✔ 21  » POST /post_body with 200 bytes
✔ 6  » Directory GET file with bad extension       ✔ 22  » POST /post_body with 101 bytes
✔ 7  » Directory GET file with bla extension       ✔ 23  » Chunked POST /post_body empty
✔ 8  » Directory GET non-existent file             ✔ 24  » Chunked POST /post_body 100 bytes
✔ 9  » Nop directory returns 301                   ✔ 25  » Chunked POST /post_body 200 bytes
✔ 10 » Nop directory with referer                  ✔ 26  » Chunked POST /post_body 101 bytes
✔ 11 » Nop directory autoindex enabled             ✔ 27  » Stress fork 5 requests 20      (100/100)
✔ 12 » Nop directory GET file returns 200          ✔ 28  » Stress fork 20 requests 5000   (100000/100000)
✔ 13 » Nop directory GET other file 404            ✔ 29  » Stress fork 128 requests 50    (6400/6400)
✔ 14 » Yeah directory returns 404                  ✔ 30  » Stress large POST fork 20      (20/20)
✔ 15 » Yeah not happy returns 200                  ✔ 31  » Chunked fork 100M 'k' chars    (100/100)
✔ 16 » Chunked large 100M chars body with y
✔ 16b» Content-Length large 100M chars body
```

The heavy cases are the interesting ones:

* **16 / 16b / 17** — 100 MB uploaded to a CGI, chunked and with
  `Content-Length`, and the same 100 MB echoed back. ~11 s each.
* **28** — 100 000 sequential requests, every one answered.
* **29** — 6 400 requests across 128 concurrent processes, all redirected.
* **31** — 20 processes × 5 requests × 100 MB through the CGI: about 10 GB in
  each direction, 100/100 answered correctly.
* **Stress 6** — 10 000 simultaneous keep-alive connections, a recent one still
  usable for a second request, the oldest evicted under pressure.

Resource behaviour during the heavy runs:

```
$ df -h /home | tail -1     # before test 31
/dev/mapper/rl-home  30G  26G  4.6G  85% /home
$ df -h /home | tail -1     # after 10 GB streamed through
/dev/mapper/rl-home  30G  26G  4.6G  85% /home
$ ls www/upload
$                           # nothing left behind
```

Peak RSS stayed under 60 MB while 20 CGI processes streamed 100 MB answers each
— the back-pressure in [14](cgi/14-cgi-output-is-not-http.md) keeps the queued
answer bounded.

## How this was verified

Nothing in `src/`, `includes/`, `main.cpp` or `Makefile` was modified. Each fix
was applied to a throw-away copy of the repository, compiled with the project's
own flags (`-Wall -Wextra -Werror -std=c++98`), and run against the tester
before being written down here. The code in `bugs/00`–`bugs/30` is the code that
produced the numbers above.

[31](epoll/31-epoll-event-loop.md) is the exception and says so at the top of the
file: it swaps `poll()` for `epoll` and was written, not run. Its own
verification section lists what must pass before it is trusted — starting with
these same 124 cases.

Two things had to be created outside the server, both described in
[00](00-test-setup.md):

* the configuration, translated from the tester's dialect into this server's;
* the fixtures the tester requests but does not ship (`index.htm`,
  `styles.css`, `large.htm`, `my page.htm`, `empty_dir/`, `forbidden*`,
  `upload/`, `cgi-bin/cookie_check.py`).

## Not done on purpose

* **`upload_store` does not redirect writes.** The directive now parses and
  creates its directory ([22](config/22-upload-store-logic-inverted.md)), but `POST`
  still writes under `root + uri`. Making the directive actually move uploads is
  a feature decision, not a defect repair.
* **Request-body temp files still live under `www/upload/`.** They no longer
  leak ([20](request/20-temp-body-files-leak.md)), but the location is a design choice
  the server's author should make.
* **Trailer fields are parsed and discarded** ([28](request/28-chunked-trailers-rejected.md)).
* **Session ids use `rand()`** ([30](session/30-no-session-support.md)). Fine for the
  tester, not fine for anything that must resist a hostile client.
