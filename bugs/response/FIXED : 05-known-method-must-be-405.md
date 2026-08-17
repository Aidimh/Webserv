# 05 — A known method that a location forbids is answered 501, not 405

**Where:** `src/Request/ClientRequest.cpp` → `RequestLineValidate()`
**Related:** `src/Response/Dispatcher.cpp` → `dispatch()`

## Symptom

Minimum Evaluation test **3**:

```
✘ FAIL  3 » HEAD to root returns 405   (missing: 405 Method Not Allowed)
```

```
$ curl -s -o /dev/null -w '%{http_code}\n' -I http://127.0.0.1:1027/
501
```

`location / { allowed_methods GET; }` says which methods this route accepts.
A request using a real HTTP method that is simply not in that list must be
told "not allowed **here**" (405) — 501 means "this server does not know this
method at all", which is a different statement and sends the client looking for
the wrong problem.

## Cause

```cpp
if (method != "GET" && method != "POST" && method != "DELETE")
{
    status_code = 501;
    state = ERROR_STATE;
    return (false);
}
```

The parser rejects the request outright. Once `state == ERROR_STATE` the
dispatcher short-circuits on its very first check and never resolves the
location, so `allowed_methods` is never consulted.

Two different questions are being answered by one test:

* *Is this a valid HTTP method?* — a syntax question, and the answer for
  anything else is 501 Not Implemented.
* *Is this method allowed on this route?* — a routing question, and the answer
  is 405, decided by `Router::isMethodAllowed()`.

## Fix

### 1. Recognise every standard method — `src/Request/ClientRequest.cpp`

```cpp
bool isKnownHttpMethod(const std::string& method)
{
    static const char* known[] = {"GET", "HEAD", "POST", "PUT", "DELETE",
                                  "CONNECT", "OPTIONS", "TRACE", "PATCH"};
    const size_t count = sizeof(known) / sizeof(known[0]);

    for (size_t i = 0; i < count; i++)
    {
        if (method == known[i])
            return true;
    }
    return false;
}
```

### 2. `RequestLineValidate()` only validates syntax

```cpp
bool ClientRequest::RequestLineValidate(void)
{
    if (!isKnownHttpMethod(method))
    {
        WARN() << "ClientRequest::RequestLineValidate: rejected status=501 unknown method=" << method;
        status_code = 501;
        state = ERROR_STATE;
        return (false);
    }

    if (!request_path.empty() && request_path.find("http://") == 0)
    {
        size_t path_start = request_path.find('/', 7);
        if (path_start != std::string::npos)
            request_path = request_path.substr(path_start);
        else
            request_path = "/";
    }
    if (request_path.empty() || request_path[0] != '/')
    {
        WARN() << "ClientRequest::RequestLineValidate: rejected status=400 path is empty or not absolute, path=" << request_path;
        status_code = 400;
        state = ERROR_STATE;
        return (false);
    }

    if (version != "HTTP/1.1" && version != "HTTP/1.0")
    {
        if (version.find("HTTP/") == 0)
        {
            WARN() << "ClientRequest::RequestLineValidate: rejected status=505 unsupported version=" << version;
            status_code = 505;
            state = ERROR_STATE;
            return (false);
        }
        else
        {
            WARN() << "ClientRequest::RequestLineValidate: rejected status=400 malformed version=" << version;
            status_code = 400;
            state = ERROR_STATE;
            return (false);
        }
    }

    DEBUG("ClientRequest") << "RequestLineValidate: accepted method=" << method
                           << " path=" << request_path << " version=" << version;
    return (true);
}
```

Declare the helper in `includes/Request/ClientRequest.hpp` beside the other
free helper functions:

```cpp
bool		isKnownHttpMethod(const std::string& method);
```

### 3. Routing decides the rest

A method that reaches the dispatcher and is not in `allowed_methods` gets 405
from the existing check; one that is allowed but has no handler
(`MethodFactory::createMethod` returns `NULL`) gets 501. That ordering is
established in [06](../cgi/06-cgi-connection-dropped.md), and the `HEAD` handler
itself is [17](17-head-method-missing.md).

## Verification

```
$ curl -s -o /dev/null -w '%{http_code}\n' -I http://127.0.0.1:1027/     # allowed_methods GET
405
$ curl -s -o /dev/null -w '%{http_code}\n' -I http://127.0.0.1:1025/     # no restriction -> HEAD handler
200
$ printf 'BREW / HTTP/1.1\r\nHost: x\r\n\r\n' | nc -q1 127.0.0.1 1027 | head -1
HTTP/1.1 501 Not Implemented
```

Tester: `✔ PASS  3 » HEAD to root returns 405`
