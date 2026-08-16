# 27 — Three wrong answers to a malformed URI

**Where:** `src/Request/ClientRequest.cpp` → `parse()`, `RequestLineParser()`;
`src/Response/GET.cpp` and `src/Response/POST.cpp` → `execute()`

Three distinct defects, all about judging a URI, all in the Path suite.

## Symptom

```
✘ FAIL  6  » Path Traversal Test          (missing: HTTP/1.1 404 Not Found)
✘ FAIL  7  » Encoded Path Traversal Test  (missing: HTTP/1.1 404 Not Found)
✘ FAIL  8  » Null Byte In Path Test       (missing: HTTP/1.1 400 Bad Request)
✘ FAIL  10 » Very Long Path Test          (missing: HTTP/1.1 414)
```

| Request | Answered | Expected |
|---|---|---|
| `GET /../../../etc/passwd` | 403 Forbidden | 404 Not Found |
| `GET /%2e%2e/%2e%2e/etc/passwd` | 403 Forbidden | 404 Not Found |
| `GET /index%00.htm` | 404 Not Found | 400 Bad Request |
| `GET /aaaa…` (9 000 chars) | 431 Request Header Fields Too Large | 414 URI Too Long |

The traversal attempts are *blocked* correctly in all cases — the escape is
caught by `normalizePath()` and nothing outside the root is ever opened. What is
wrong is what the client is told.

## Cause and fix, one at a time

### 1. An over-long URI is judged as an over-long header block

`parse()` looks for the end of the header block first:

```cpp
	size_t check = client.request.find("\r\n\r\n");
	if (check == std::string::npos)
	{
		if (client.request.length() > MAX_HEADER_SIZE)
		{
			this->status_code = 431;          // <- 9 000-byte URI lands here
```

The `414` branch in `RequestLineParser()` is never reached, because the request
line is never parsed. 431 is about *header fields*; the client sent one enormous
request line and needs to be told 414.

Measure the request line on its own, before judging the block:

```cpp
/*
 * An over-long URI is 414, not 431: the request line is measured on its own,
 * before the whole header block is judged too big.
 */
bool ClientRequest::RequestLineTooLong(Client& client)
{
    size_t lineEnd = client.request.find("\r\n");
    size_t length = (lineEnd == std::string::npos) ? client.request.size() : lineEnd;

    if (length <= MAX_REQUEST_LINE_SIZE)
        return false;
    WARN() << "ClientRequest::RequestLineTooLong: rejected status=414 request line length="
           << length << " exceeds limit=" << MAX_REQUEST_LINE_SIZE << " fd=" << client.fd;
    status_code = 414;
    state = ERROR_STATE;
    return true;
}
```

called from `parse()` right after the leading whitespace is trimmed:

```cpp
	if (client.request.empty())
		return;
	if (RequestLineTooLong(client))
		return;

	size_t check = client.request.find("\r\n\r\n");
```

While the first CRLF has not arrived the buffered length stands in for the line
length, so the verdict is reached without waiting for the rest of a request that
is already too long. Once the CRLF is there, a short request line is measured
correctly however large the headers behind it are.

The two limits become named constants in `includes/multiplexing/header.hpp`:

```cpp
#define MAX_URI_SIZE 2048
#define MAX_REQUEST_LINE_SIZE 2176
```

and the magic `2048` inside `RequestLineParser()` uses the name:

```cpp
	if (request_path.length() > MAX_URI_SIZE)
	{
		WARN() << "ClientRequest::RequestLineParser: rejected status=414 uri length=" << request_path.length()
		       << " exceeds limit=" << MAX_URI_SIZE;
		status_code = 414;
		state = ERROR_STATE;
		return;
	}
```

Declaration in `includes/Request/ClientRequest.hpp`:

```cpp
		bool										RequestLineTooLong(Client& client);
```

### 2. A percent-encoded NUL is not rejected

`%00` survives parsing and only fails later as "file not found". A NUL in a URI
is invalid per RFC 3986 and is the classic way to make a name check and a
filesystem call disagree — the check sees `/index%00.htm`, the C string sees
`/index`. It has to be refused as a bad request, before anything resolves it.

```cpp
/*
 * A percent-encoded NUL is the classic way to smuggle "/index%00.htm" past a
 * name check and reach "/index" on disk. RFC 3986 forbids it outright, so the
 * URI is refused before anything tries to resolve it.
 */
bool uriHasForbiddenByte(const std::string& uri)
{
    for (size_t i = 0; i < uri.size(); i++)
    {
        if (uri[i] == '\0' || static_cast<unsigned char>(uri[i]) < 0x20)
            return true;
        if (uri[i] != '%' || i + 2 >= uri.size())
            continue;
        if (uri[i + 1] == '0' && uri[i + 2] == '0')
            return true;
    }
    return false;
}
```

called in `RequestLineParser()` immediately after the length check:

```cpp
	if (uriHasForbiddenByte(request_path))
	{
		WARN() << "ClientRequest::RequestLineParser: rejected status=400 uri contains a null or control byte";
		status_code = 400;
		state = ERROR_STATE;
		return;
	}
```

Raw control characters are refused too: they cannot appear in a well-formed
request line, and letting them through only creates ways for a URI to mean two
different things.

### 3. A URI that escapes the root is 403, not 404

`AMethod::resolveTarget()` returns an empty string when `normalizePath()`
detects an escape, and both handlers turned that into 403 Forbidden. 403 says
"this exists and you may not have it" — which leaks the fact that something is
there. The URI simply does not name anything inside this server: 404.

`src/Response/GET.cpp` → `execute()`:

```cpp
    if (target.empty())
    {
        DEBUG("GET") << "execute: uri escapes the server root, responding status=404 uri=" << requestPath;
        return buildErrorResponse(404, "Not Found");
    }
```

`src/Response/POST.cpp` → `execute()`:

```cpp
    if (target.empty())
    {
        DEBUG("POST") << "execute: uri escapes the server root, responding status=404 fd=" << client.fd;
        return buildErrorResponse(404, "Not Found");
    }
```

Genuine permission failures keep their 403: those come from
`getPathType()` returning `PERMISSION_DENIED` on a file that exists but cannot
be read, which is a different branch entirely.

## Verification

```
$ printf 'GET /../../../etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n' | nc -q1 127.0.0.1 1025 | head -1
HTTP/1.1 404 Not Found
$ printf 'GET /%%2e%%2e/%%2e%%2e/etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n' | nc -q1 127.0.0.1 1025 | head -1
HTTP/1.1 404 Not Found
$ printf 'GET /index%%00.htm HTTP/1.1\r\nHost: x\r\n\r\n' | nc -q1 127.0.0.1 1025 | head -1
HTTP/1.1 400 Bad Request
$ python3 -c "…9000-char URI…"
HTTP/1.1 414 URI Too Long
```

And `/etc/passwd` was never opened in any of the four cases — that part was
already right.
