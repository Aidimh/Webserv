# 25 — A 405 answer does not say which methods are allowed

**Where:** `src/Response/Dispatcher.cpp` → `dispatch()`

## Symptom

```
$ curl -s -i -X POST -d '' http://127.0.0.1:1025/get-only | head -3
HTTP/1.1 405 Method Not Allowed
Content-Length: 2095
Content-Type: text/html
```

Method Restriction test 5 fails with `(missing: Allow:)`.

## Cause

RFC 7231 §6.5.5 is explicit: *"The origin server MUST generate an Allow header
field in a 405 response containing a list of the target resource's currently
supported methods."* The dispatcher knows the list — it just read
`location.allowed_methods` to decide the request was not allowed — and then
throws it away:

```cpp
Response response = AMethod::buildErrorResponse(HTTP_405_METHOD_NOT_ALLOWED, "Method Not Allowed");
setErrorPageBody(response);
return response;
```

Without it the client has no way to recover except by trying methods at random.

## Fix

```cpp
/*
 * RFC 7231: a 405 answer must tell the client which methods the route does
 * accept, otherwise it has no way to recover except by guessing.
 */
static std::string allowedMethodList(const Location_Config& location)
{
    std::string list;

    for (size_t i = 0; i < location.allowed_methods.size(); i++)
    {
        if (!list.empty())
            list += ", ";
        list += location.allowed_methods[i];
    }
    if (list.empty())
        list = "GET, HEAD, POST, DELETE";
    return list;
}
```

and one line inside `dispatch()`:

```cpp
    if (!Router::isMethodAllowed(client.parsed_request.getMethod(), *location)) {
        DEBUG("Dispatcher") << "dispatch: method=" << client.parsed_request.getMethod()
                            << " not allowed on location=" << location->path
                            << ", responding status=405 fd=" << client.fd;
        Response response = AMethod::buildErrorResponse(HTTP_405_METHOD_NOT_ALLOWED, "Method Not Allowed");
        response.addHeader("Allow", allowedMethodList(*location));
        setErrorPageBody(response);
        return response;
    }
```

The empty-list fallback covers a location with no `allowed_methods` directive:
`Router::isMethodAllowed()` treats that as "everything allowed", so the only way
to reach a 405 there is an unimplemented method, and the honest answer is the
set the server actually implements.

## Verification

```
$ curl -s -i -X POST -d '' http://127.0.0.1:1025/get-only | head -2
HTTP/1.1 405 Method Not Allowed
Allow: GET
$ curl -s -i -X DELETE http://127.0.0.1:1025/post-only | head -2
HTTP/1.1 405 Method Not Allowed
Allow: POST
```
