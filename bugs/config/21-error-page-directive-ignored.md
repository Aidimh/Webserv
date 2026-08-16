# 21 — The `error_page` directive is parsed and then ignored

**Where:** `src/Response/Dispatcher.cpp` → `setErrorPageBody()`

## Symptom

```nginx
server {
    error_page 404 /my_404.html;      # parsed, stored… and never read
}
```

The configured page is never served. Instead the server always looks for
`www/error_pages/404.html` **relative to the working directory it was started
from**, so running

```bash
cd /somewhere/else && /home/aazzaoui/webFork/webserv EngineX.conf
```

produces a bare status line with no body at all, and logs:

```
[WARN] Dispatcher::setErrorPageBody: error page not found path=www/error_pages/404.html
```

## Cause

```cpp
static void setErrorPageBody(Response& response)
{
    if (response.getStatusCode() < 400)
        return;

    std::ostringstream path;
    path << "www/error_pages/" << response.getStatusCode() << ".html";
    ...
```

The function knows the status code and nothing else. It has no access to the
`Server_block`, so `Server_block::error_pages` — filled in by
`parse_error_pages()` — is unreachable from here, and the path is hard-coded and
relative.

`Location_Config::error_pages` exists too and is never filled by any parser, so
per-location error pages are not implemented at all.

## Fix

Pass the server down, look the code up in the configuration, and fall back to
the built-in directory only when nothing is configured. Finding the file and
loading it are separate jobs.

```cpp
/*
 * "error_page 404 /my_404.html;" names a URI, so it is resolved under the
 * server root exactly like any other static file.
 */
static std::string configuredErrorPage(const Server_block& server, int statusCode)
{
    std::map<int, std::string>::const_iterator it = server.error_pages.find(statusCode);

    if (it == server.error_pages.end())
        return "";
    return joinPath(server.root, it->second);
}
```

```cpp
static std::string defaultErrorPage(int statusCode)
{
    std::ostringstream path;

    path << "www/error_pages/" << statusCode << ".html";
    return path.str();
}
```

```cpp
static bool loadFileInto(const std::string& path, std::string& out)
{
    std::ifstream file(path.c_str(), std::ios::binary);

    if (!file.is_open())
        return false;

    std::ostringstream page;
    page << file.rdbuf();
    out = page.str();
    return true;
}
```

```cpp
static void setErrorPageBody(Response& response, const Server_block& server)
{
    if (response.getStatusCode() < 400)
        return;

    std::string page;
    std::string path = configuredErrorPage(server, response.getStatusCode());

    if (path.empty() || !loadFileInto(path, page))
    {
        path = defaultErrorPage(response.getStatusCode());
        if (!loadFileInto(path, page))
        {
            WARN() << "Dispatcher::setErrorPageBody: no error page for status="
                   << response.getStatusCode() << ", serving the built-in body";
            return;
        }
    }
    response.setBody(page);
    response.addHeader("Content-Type", "text/html");
    DEBUG("Dispatcher") << "setErrorPageBody: loaded error page path=" << path
                        << " for status=" << response.getStatusCode();
}
```

Every call inside `Dispatcher::dispatch()` becomes
`setErrorPageBody(response, server);` — the `Server_block&` is already a
parameter of `dispatch()`, so nothing else has to change. `joinPath()` is the
helper introduced in [09](../cgi/09-cgi-script-path.md).

Returning without a body is not a failure: `AMethod::buildErrorResponse()` has
already installed a minimal HTML body and the correct `Content-Length`, so the
client always gets a complete response.

## Note

The existing comment above the function is worth keeping — it explains a
deliberate decision that is easy to "fix" wrongly later:

> Error pages are served internally, keeping the original HTTP status code.
> A 302 redirect here would turn, for example, a 404 into a successful
> redirect and would hide the real error from the client.

## Verification

```
$ curl -s -o /dev/null -w '%{http_code}\n' http://127.0.0.1:1025/nope.html
404
$ curl -s http://127.0.0.1:1025/nope.html | head -1
<!-- my_404.html -->
```

and with `error_page` absent from the configuration, the built-in
`www/error_pages/404.html` is served as before.
