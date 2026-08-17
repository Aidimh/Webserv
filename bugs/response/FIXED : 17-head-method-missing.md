# 17 — There is no `HEAD` handler

**Where:** `src/Response/MethodFactory.cpp` → `createMethod()`

## Symptom

```
$ curl -s -o /dev/null -w '%{http_code}\n' -I http://127.0.0.1:1025/index.htm
501
```

On a route with no `allowed_methods` restriction, `HEAD` should answer exactly
like `GET` but without the body. `HEAD` is the one method besides `GET` that
HTTP/1.1 requires every server to implement, and browsers, proxies and health
checks use it constantly.

## Cause

`MethodFactory::createMethod()` knows three methods:

```cpp
    if (method == "GET")    return new GET();
    if (method == "POST")   return new POST();
    if (method == "DELETE") return new DeleteMethod();
    return NULL;                       // -> 501 Not Implemented
```

Before [05](05-known-method-must-be-405.md) the request never even got this
far — the parser rejected `HEAD` outright. Now that routing sees it, a handler
is needed; without one, a location that allows `HEAD` still answers 501.

## Fix

`HEAD` is `GET` minus the payload, so it is written as exactly that instead of
being duplicated.

### `includes/Methods/HeadMethod.hpp` (new file)

```cpp
#ifndef HEADMETHOD_HPP
#define HEADMETHOD_HPP

#include "GET.hpp"

/*
 * HEAD is GET without the payload: same status, same headers, same
 * Content-Length, but the body is never written on the socket.
 */
class HeadMethod : public GET
{
    public:
        HeadMethod();
        ~HeadMethod();
        Response execute(Client& client, const Server_block& server);

    private:
        void dropPayload(Client& client, Response& response) const;
};

#endif
```

### `src/Response/HeadMethod.cpp` (new file)

```cpp
#include "HeadMethod.hpp"
#include "../Logging/Logging.hpp"

HeadMethod::HeadMethod()
{
}

HeadMethod::~HeadMethod()
{
}

/*
 * A streamed answer would keep sending the file after the headers, so the
 * streaming source is released here as well.
 */
void HeadMethod::dropPayload(Client& client, Response& response) const
{
    if (client.stream_file_fd != -1)
    {
        close(client.stream_file_fd);
        client.stream_file_fd = -1;
        client.stream_bytes_remaining = 0;
    }
    if (response.getResponseMode() == Response::STREAMING_RESPONSE)
        response.setResponseMode(Response::NORMAL_RESPONSE);
    response.dropBody();
}

Response HeadMethod::execute(Client& client, const Server_block& server)
{
    Response response = GET::execute(client, server);

    dropPayload(client, response);
    DEBUG("HeadMethod") << "execute: answered HEAD with status=" << response.getStatusCode()
                        << " and no payload fd=" << client.fd;
    return response;
}
```

Releasing the streaming descriptor matters: a large file answered by
`GET::execute()` sets `client.stream_file_fd`, and `_writeClient()` would keep
pushing the file out after the headers — the one thing `HEAD` must not do.

### `Response::dropBody()` — `src/Response/Response.cpp`

```cpp
/* Keeps Content-Length untouched: HEAD announces the size it would have sent. */
void Response::dropBody()
{
    body.clear();
}
```

Declared in `includes/Response/Response.hpp` next to `setBody`:

```cpp
        void dropBody();
```

`setBody("")` cannot be used instead: it would rewrite `Content-Length` to 0,
and the whole point of `HEAD` is that the announced length matches what `GET`
would return.

### `src/Response/MethodFactory.cpp`

```cpp
AMethod* MethodFactory::createMethod(const std::string& method)
{
    DEBUG("MethodFactory") << "createMethod: creating handler for method=" << method;
    if (method == "GET")
        return new GET();

    if (method == "HEAD")
        return new HeadMethod();

    if (method == "POST")
        return new POST();

    if (method == "DELETE")
        return new DeleteMethod();

    WARN() << "MethodFactory::createMethod: no handler for method=" << method;
    return NULL;
}
```

with `#include "HeadMethod.hpp"` added to `includes/Methods/MethodFactory.hpp`.

### `Makefile`

```make
	src/Response/HeadMethod.cpp \
```

`GET`'s members used by the subclass (`execute`) are already public, so nothing
in `GET.hpp` changes.

## Verification

```
$ curl -s -I http://127.0.0.1:1025/index.htm
HTTP/1.1 200 OK
Content-Length: 99
Content-Type: text/html

$ curl -s -o /dev/null -w '%{http_code}\n' -I http://127.0.0.1:1027/   # allowed_methods GET
405
```

Same status and same headers as `GET`, zero body bytes; still 405 where the
configuration forbids it.
