# 16 — `return` fails to parse, and would do nothing if it did

**Where:** `src/ConfFile/location_parsing.cpp` → `parse_return()`

## Symptom

The server refuses to start on the tester's own configuration:

```
$ ./webserv EngineX.conf          # location /old-page { return 301 /new-page; }
[ERROR] main: Error
Root path isn't provided or is invalid!.
```

With the two-token form removed to get past that, the directive is accepted and
then ignored:

```
[WARN] parse_return: 'return' directive is validated but not stored, it will have no effect
$ curl -s -o /dev/null -w '%{http_code}\n' http://127.0.0.1:1025/old-page
404
```

The whole Redirection suite depends on it.

## Cause

```cpp
void parse_return(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Root();
    if (Conf_File::tokens[index + 2] != ";")
        throw Error::Root();
    WARN() << "parse_return: 'return' directive is validated but not stored, it will have no effect";
}
```

* It only accepts `return <target>;`. `return 301 /new-page;` has a token where
  the `;` is expected, so it throws — and throws `Error::Root()`, whose message
  ("Root path isn't provided or is invalid") points at the wrong directive
  entirely.
* It stores nothing. `Location_Config::_return` exists but is never assigned,
  and no code path reads it.
* It never advances `index`, so even the accepted form would leave the parser
  sitting on the same token.

## Fix

### 1. The parser — `src/ConfFile/location_parsing.cpp`

```cpp
static bool is_redirect_status(const std::string& token, int& code)
{
    for (size_t i = 0; i < token.size(); i++)
    {
        if (!isdigit(static_cast<unsigned char>(token[i])))
            return false;
    }
    code = atoi(token.c_str());
    return code >= 300 && code <= 399;
}
```

```cpp
static Location_Config& current_location(void)
{
    Server_block& server = Conf_File::Servers[server_index];

    return server.location[server.location_count];
}
```

```cpp
/*
 * Two accepted shapes, like nginx:
 *      return /new-page;             -> 302 by default
 *      return 301 /new-page;
 */
void parse_return(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Return();

    Location_Config& location = current_location();
    int code = 0;

    if (Conf_File::tokens[index + 2] == ";")
    {
        location.redirect_code = HTTP_302_FOUND;
        location.redirect_target = Conf_File::tokens[index + 1];
        index += 3;
    }
    else
    {
        if (index + 3 >= Conf_File::tokens.size() || Conf_File::tokens[index + 3] != ";")
            throw Error::Return();
        if (!is_redirect_status(Conf_File::tokens[index + 1], code))
            throw Error::Return();
        location.redirect_code = code;
        location.redirect_target = Conf_File::tokens[index + 2];
        index += 4;
    }
    if (location.redirect_target.empty())
        throw Error::Return();
    location.has_redirect = true;
    DEBUG("ConfFile") << "parse_return: parsed return " << location.redirect_code
                      << " " << location.redirect_target
                      << " location=" << Conf_File::Servers[server_index].location_count
                      << " server=" << server_index;
}
```

### 2. A dedicated error — `includes/Errors/Error.hpp`

```cpp
        class Return : public std::exception
        {
            public:
                const char* what() const throw();
        };
```

`src/Multiplexing/Error_messages.cpp`:

```cpp
const char* Error::Return::what() const throw()
{
    return ("Error\nInvalid 'return' directive. expected: return [3xx] <target>;");
}
```

### 3. The fields — `includes/multiplexing/header.hpp`

`Location_Config` gains `redirect_target`, `redirect_code` and `has_redirect`,
all initialised by the constructor in
[03](03-uninitialised-config-fields.md).

### 4. Answering the redirect — `src/Response/Dispatcher.cpp`

```cpp
static std::string redirectReason(int code)
{
    switch (code)
    {
        case HTTP_301_MOVED_PERMANENTLY:
            return "Moved Permanently";

        case HTTP_302_FOUND:
            return "Found";

        case 303:
            return "See Other";

        case 307:
            return "Temporary Redirect";

        case 308:
            return "Permanent Redirect";

        default:
            return "Found";
    }
}
```

```cpp
/*
 * A location carrying "return <code> <target>;" answers before any file is
 * looked up: the target may perfectly well not exist on this server.
 */
static Response buildConfiguredRedirect(const Location_Config& location)
{
    Response response;

    response.setStatusCode(location.redirect_code);
    response.setReasonPhrase(redirectReason(location.redirect_code));
    response.addHeader("Location", location.redirect_target);
    response.setBody("");
    DEBUG("Dispatcher") << "buildConfiguredRedirect: status=" << location.redirect_code
                        << " location=" << location.redirect_target;
    return response;
}
```

and in `Dispatcher::dispatch()`, immediately after the location is resolved and
before the method check — a redirect applies to every method, and its target
need not exist:

```cpp
    if (location->has_redirect)
        return buildConfiguredRedirect(*location);
```

## Verification

```
$ curl -s -o /dev/null -w '%{http_code} %{redirect_url}\n' http://127.0.0.1:1025/old-page
301 http://127.0.0.1:1025/new-page
$ curl -s -o /dev/null -w '%{http_code} %{redirect_url}\n' http://127.0.0.1:1025/temp-page
302 http://127.0.0.1:1025/index.htm
$ curl -s -o /dev/null -w '%{http_code} %{redirect_url}\n' http://127.0.0.1:1025/ext-redirect
301 http://example.com/
```
