# 28 — A chunked request with trailers is rejected as malformed

**Where:** `src/Request/ClientRequest.cpp` → `HandleTransferEncoding()`

## Symptom

Chunked Transfer test 16 fails with
`(missing: HTTP/1.1 201 Created||HTTP/1.1 200 OK)`. The request is the standard
one described in RFC 7230 §4.1:

```
5;ext=1\r\n
hello\r\n
0\r\n
X-Checksum: abc123\r\n
\r\n
```

The server answers 400 Bad Request. Chunk *extensions* (`;ext=1`) are handled;
the *trailer section* after the last chunk is not.

## Cause

The terminating chunk is treated as if nothing could follow it:

```cpp
		size_t	needs = pos + 2 + ChunkSize + 2;
		...
		if (client.request.substr(pos + 2 + ChunkSize, 2) != "\r\n")
		{
			status_code = 400;          // <- "X-" instead of CRLF
			state = ERROR_STATE;
			return;
		}
		if (ChunkSize == 0)
		{
			...
			client.request.erase(0, needs);
			state = DONE;
```

For the last chunk `ChunkSize` is 0, so the code demands a CRLF immediately
after `0\r\n`. A trailer field starts with a header name instead, and the
request is thrown out. The grammar is:

```
last-chunk = "0" CRLF *( trailer-field CRLF ) CRLF
```

so both `0\r\n\r\n` and `0\r\nX-Checksum: abc123\r\n\r\n` are valid, and a
server that only accepts the first one rejects a legal request.

The same two lines also mis-measure completeness: with `ChunkSize == 0`,
`needs` says the message ends 2 bytes after `0\r\n`, which is only true when
there are no trailers.

## Fix

Finding the end of the terminating chunk becomes its own function, and the two
checks that assume "data chunk" are told to skip the last one.

```cpp
/*
 * The last chunk is "0\r\n" followed by an optional trailer section and a
 * final CRLF:   0\r\n\r\n   or   0\r\nExpires: ...\r\n\r\n
 * Returns the offset just past the end of the message, npos while incomplete.
 */
static size_t chunkedTerminatorEnd(const std::string& buffer, size_t afterSizeLine)
{
    if (buffer.size() < afterSizeLine + 2)
        return std::string::npos;
    if (buffer.compare(afterSizeLine, 2, "\r\n") == 0)
        return afterSizeLine + 2;

    size_t end = buffer.find("\r\n\r\n", afterSizeLine);

    if (end == std::string::npos)
        return std::string::npos;
    return end + 4;
}
```

Inside `HandleTransferEncoding()`, the two guards become data-chunk-only:

```cpp
		size_t	needs = pos + 2 + ChunkSize + 2;
		if (ChunkSize != 0 && client.request.length() < needs)
		{
			DDEBUG("ClientRequest") << "HandleTransferEncoding: chunk incomplete, have="
			                        << client.request.length() << " need=" << needs << " fd=" << client.fd;
			return;
		}
		if (ChunkSize != 0 && client.request.substr(pos + 2 + ChunkSize, 2) != "\r\n")
		{
			WARN() << "ClientRequest::HandleTransferEncoding: rejected status=400 chunk not terminated by CRLF fd="
			       << client.fd;
			status_code = 400;
			state = ERROR_STATE;
			return;
		}
```

and the terminating branch uses the new helper:

```cpp
		if (ChunkSize == 0)
		{
			size_t end = chunkedTerminatorEnd(client.request, pos + 2);

			if (end == std::string::npos)
			{
				DDEBUG("ClientRequest") << "HandleTransferEncoding: waiting for the trailer section fd="
				                        << client.fd;
				return;
			}
			if (TmpFileFd != -1)
			{
				DEBUG("ClientRequest") << "HandleTransferEncoding: closed temp file fd=" << TmpFileFd;
				close(TmpFileFd);
				TmpFileFd = -1;
			}
			client.request.erase(0, end);
			state = DONE;
			DEBUG("ClientRequest") << "HandleTransferEncoding: state BODY to DONE, received body_size="
			                       << BodySize << " bytes fd=" << client.fd;
			return;
		}
```

Returning `npos` while the trailer section is still arriving means a trailer
split across TCP segments is waited for instead of being mistaken for the end of
the message.

Trailer fields are discarded rather than merged into the header map — nothing in
this server consumes them, and accepting them silently is what the RFC allows.

## Verification

```
$ printf 'POST /upload/t.txt HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n5;ext=1\r\nhello\r\n0\r\nX-Checksum: abc\r\n\r\n' | nc -q1 127.0.0.1 1025 | head -1
HTTP/1.1 201 Created
```

Tester: `✔ PASS  16 » Chunked Extensions and Trailers Test`, with the other 18
cases of the suite unchanged.
