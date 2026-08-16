# 07 — The CGI environment is incomplete

**Where:** `src/CGI/CGI_class.cpp` → `build_env_vars()`

## Symptom

The subject's own `cgi_tester` refuses to run:

```
$ printf 'abc' | env -i REQUEST_METHOD=POST PATH_INFO=/x.bla CONTENT_LENGTH=3 ./cgi_tester
cgi: invalid SERVER_PROTOCOL version
```

Every `.bla` request would answer with that error text instead of the echoed
body, so Minimum Evaluation tests **16, 16b, 17 and 31** fail their body
assertion, and test **18** — which asserts the value of a request header the
CGI is supposed to see — can never pass.

## Cause

The whole environment was six variables:

```cpp
env_vars.push_back("REQUEST_METHOD=" + client.parsed_request.getMethod());
env_vars.push_back("PATH_INFO=" + client.parsed_request.getRequestPath());
env_vars.push_back("SCRIPT_FILENAME=" + script);
env_vars.push_back("CONTENT_TYPE=" + get_header_value(..., "content_type"));
env_vars.push_back("QUERY_STRING=");
env_vars.push_back("CONTENT_LENGTH=" + result);
```

* `SERVER_PROTOCOL` is missing — mandatory in RFC 3875, and `cgi_tester`
  refuses to start without it.
* `GATEWAY_INTERFACE`, `SERVER_NAME`, `SERVER_PORT`, `SCRIPT_NAME`,
  `REQUEST_URI`, `PATH_TRANSLATED` are missing.
* **No request header is exported.** RFC 3875 requires every header to be passed
  as `HTTP_<NAME>` with `-` turned into `_`. Test 18 sends
  `X-Secret-Header-For-Test: 1` and expects the CGI to have read
  `HTTP_X_SECRET_HEADER_FOR_TEST`; sessions and cookies need `HTTP_COOKIE` for
  the same reason.
* `"content_type"` is a typo: headers are stored lower-case with the dash, so
  the lookup key is `"content-type"`. `CONTENT_TYPE` was always empty.
* `QUERY_STRING` is hard-coded empty — see [15](../request/15-query-string-not-split.md).

## Fix

Building the environment is three jobs — the CGI meta-variables, the request
headers, and the conversion to `char **` — so it is three functions.

```cpp
void CGI::addEnv(const std::string& key, const std::string& value)
{
    env_vars.push_back(key + "=" + value);
}
```

```cpp
/* "X-Secret-Header-For-Test" -> "HTTP_X_SECRET_HEADER_FOR_TEST" */
static std::string toEnvName(const std::string& header)
{
    std::string name = "HTTP_";

    for (size_t i = 0; i < header.size(); i++)
    {
        if (header[i] == '-')
            name += '_';
        else
            name += static_cast<char>(toupper(static_cast<unsigned char>(header[i])));
    }
    return name;
}
```

```cpp
void CGI::addRequestHeaders(const Client& client)
{
    const std::map<std::string, std::string>& headers = client.parsed_request.getHeaders();
    std::map<std::string, std::string>::const_iterator it;

    for (it = headers.begin(); it != headers.end(); ++it)
        addEnv(toEnvName(it->first), it->second);
}
```

```cpp
void CGI::buildEnvArray()
{
    request_vars = new char *[env_vars.size() + 1];

    for (size_t i = 0; i < env_vars.size(); i++)
        request_vars[i] = strdup(env_vars[i].c_str());
    request_vars[env_vars.size()] = NULL;
}
```

```cpp
void CGI::build_env_vars(Client& client, const Server_block& server)
{
    const ClientRequest& request = client.parsed_request;

    addEnv("GATEWAY_INTERFACE", "CGI/1.1");
    addEnv("SERVER_SOFTWARE", "webserv/1.0");
    addEnv("SERVER_PROTOCOL", "HTTP/1.1");
    addEnv("SERVER_NAME", server.server_name);
    addEnv("SERVER_PORT", sizeToString(static_cast<size_t>(client.port)));
    addEnv("REQUEST_METHOD", request.getMethod());
    addEnv("REQUEST_URI", request.getRequestPath());
    addEnv("PATH_INFO", request.getRequestPath());
    addEnv("PATH_TRANSLATED", script);
    addEnv("SCRIPT_NAME", request.getRequestPath());
    addEnv("SCRIPT_FILENAME", script);
    addEnv("QUERY_STRING", request.getQueryString());
    addEnv("CONTENT_TYPE", get_header_value(request.getHeaders(), "content-type"));
    addEnv("CONTENT_LENGTH", sizeToString(request.getBodySize()));
    addEnv("REDIRECT_STATUS", "200");
    addRequestHeaders(client);
    buildEnvArray();
}
```

```cpp
static std::string sizeToString(size_t value)
{
    std::ostringstream out;

    out << value;
    return out.str();
}
```

`sizeToString` also replaces the `sprintf(buff, "%zu", ...)` into a 32-byte
stack buffer, and `CONTENT_LENGTH` keeps using `getBodySize()` — the number of
bytes actually received — which is the only correct value for a chunked
request, where no `Content-Length` header exists.

The signature gains the `Server_block` (for `SERVER_NAME`), so
`includes/multiplexing/header.hpp` declares:

```cpp
        void build_env_vars(Client& client, const Server_block& server);
        void addEnv(const std::string& key, const std::string& value);
        void addRequestHeaders(const Client& client);
        void buildEnvArray();
```

## Verification

```
$ curl -s -X POST -H 'X-Secret-Header-For-Test: 1' --data-binary 'abcde' \
       http://127.0.0.1:1027/directory/youpi.bla
11111
```

Five body bytes, five `1`s — the CGI saw the header. Tester:

```
✔ PASS  18 » Chunked 100K chars body   ('1' repeated 100000 times)
```
