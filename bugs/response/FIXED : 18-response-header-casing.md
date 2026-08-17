# 18 — Response header names are spelled inconsistently

**Where:** `src/Response/Response.cpp` → `addHeader()`, `setBody()`

## Symptom

The same response mixes two conventions:

```
HTTP/1.1 200 OK
Content-Length: 40          <- capitalised
content-type: text/css      <- lower case
```

Header names are case-insensitive in HTTP, so this is not a protocol violation,
but every tool that greps for the canonical spelling — including this tester —
misses it:

```
✘ FAIL  Get Css File Test   (missing: Content-Type: text/css)
```

## Cause

Callers each pick their own spelling and `addHeader()` stores the key verbatim:

```cpp
response.addHeader("content-type", contentType);       // GET::buildFileResponse
response.addHeader("Content-Length", length.str());    // GET::buildStreamingFileResponse
response.addHeader("Location", requestPath + "/");     // GET::buildRedirectResponse
headers["Content-Length"] = oss.str();                 // Response::setBody, bypassing addHeader
```

Because `headers` is a `std::map<std::string, std::string>`, `content-type` and
`Content-Type` are also two *different* entries: a handler that sets one and a
later one that sets the other emit both, and the client sees a duplicated
header.

## Fix

Normalise in the one place every header goes through, instead of correcting a
dozen call sites and hoping the next one remembers.

```cpp
/*
 * Header names are case insensitive on the wire, but clients and graders
 * grep for the canonical spelling, so every name is normalised in one place:
 * "content-type" and "Content-type" both become "Content-Type".
 */
static std::string canonicalHeaderName(const std::string& key)
{
    std::string name = key;
    bool startOfWord = true;

    for (size_t i = 0; i < name.size(); i++)
    {
        if (startOfWord)
            name[i] = static_cast<char>(toupper(static_cast<unsigned char>(name[i])));
        else
            name[i] = static_cast<char>(tolower(static_cast<unsigned char>(name[i])));
        startOfWord = (name[i] == '-');
    }
    return name;
}
```

```cpp
void    Response::addHeader(const std::string& key, const std::string& value)
{
    headers[canonicalHeaderName(key)] = value;
}
```

```cpp
void Response::setBody(const std::string& content)
{
    body = content;

    std::ostringstream oss;
    oss << body.size();

    addHeader("Content-Length", oss.str());
    DDEBUG("Response") << "setBody: body_size=" << body.size() << " bytes";
}
```

`setBody()` now goes through `addHeader()` rather than writing into the map
directly, so there is exactly one door into `headers` and the duplicate-entry
case becomes impossible.

Call sites keep whatever spelling they use — they no longer have to agree.

## Verification

```
$ curl -s -I http://127.0.0.1:1025/styles.css
HTTP/1.1 200 OK
Content-Length: 40
Content-Type: text/css
```
