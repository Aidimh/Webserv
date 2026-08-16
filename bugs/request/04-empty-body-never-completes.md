# 04 — A request with `Content-Length: 0` never gets an answer

**Where:** `src/Request/ClientRequest.cpp` → `BodyRequest()`

## Symptom

Minimum Evaluation test **19** (`POST /post_body` with `Content-Length: 0`)
times out. Reproduced with curl:

```
$ curl -sv --max-time 3 -X POST -H 'Content-Length: 0' -d '' http://127.0.0.1:1027/post_body
> POST /post_body HTTP/1.1
> Content-Length: 0
* Operation timed out after 3000 milliseconds with 0 bytes received
```

The server holds the connection open for ever. Any client that announces an
empty body — a form with no fields, a `POST` used as a trigger, a `DELETE` with
`Content-Length: 0` — hangs.

## Cause

```cpp
if (client.request.empty())
    return;
```

After `parse()` has consumed the header block it erases it from
`client.request`. For a request with no body the buffer is now empty, so
`BodyRequest()` returns immediately — before `HandleContentLength()` gets the
chance to notice that `BodySize (0) >= getContentLength() (0)` and move the
state to `DONE`.

The state stays `BODY` for ever. `_readClient()` only calls `enableWrite()` when
the state is `DONE` or `ERROR_STATE`, so `EPOLLOUT` is never armed and no
response is ever produced.

The guard is not needed at all: both handlers already cope with an empty
buffer. `HandleContentLength()` checks the completion condition first, and
`HandleTransferEncoding()` returns as soon as `find("\r\n")` fails.

## Fix

```cpp
void ClientRequest::BodyRequest(Client& client)
{
	if (!HasContentLength && !HasTransferEncoding)
	{
		DEBUG("ClientRequest") << "BodyRequest: state BODY to DONE, no body expected fd=" << client.fd;
		state = DONE;
		return;
	}
	if (HasTransferEncoding)
	{
		HandleTransferEncoding(client);
		return;
	}
	HandleContentLength(client);
}
```

The `else if` / `else` chain after an early `return` is also gone: each branch
now ends the function on its own, which is what the code already meant.

## Verification

```
$ curl -s -o /dev/null -w '%{http_code}\n' --max-time 3 -X POST -d '' http://127.0.0.1:1027/post_body
201
```

Tester:

```
✔ PASS  19 » POST /post_body with empty body
✔ PASS  23 » Chunked POST /post_body with empty body
```
