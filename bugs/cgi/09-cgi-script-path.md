# 09 — `_find_interpreter()` reads an uninitialised flag and builds a broken path

**Where:** `src/CGI/CGI_class.cpp` → `_find_interpreter()`

## Symptom

Depending on the build, a CGI request either runs a script whose path does not
exist, or is treated as a CGI when no interpreter was configured for it at all.
With the location root `/var/www` and the URI `/cgi-bin/hello.py` the script
path came out as:

```
/var/wwwcgi-bin/hello.py
```

## Cause

```cpp
int CGI::_find_interpreter(const Location_Config& conf)
{
    size_t i = 0;
    size_t pos = request_path.rfind('.');
    if (pos == std::string::npos)
        return 1;
    if (request_path.c_str() && request_path[0] == '/')
        remove_char_at(request_path, 0);          // (1)
    std::string extension = request_path.substr(pos - 1);   // (2)
    ...
    if (!extension_found)                          // (3)
        return ERROR;
    this->script = conf.root + request_path;       // (4)
```

1. The leading `/` is stripped from `request_path` — a member that
   `build_env_vars()` later exports as `PATH_INFO` and `SCRIPT_NAME`, so those
   variables lose their leading slash too.
2. `pos` was computed **before** the erase, so `substr(pos - 1)` silently
   compensates for the shift. It is correct only when the path started with
   `/`, and when `pos == 0` it underflows to `substr(SIZE_MAX)` and throws
   `std::out_of_range`.
3. `extension_found` is **never initialised** — the constructor does not touch
   it. Reading it is undefined behaviour; a garbage `true` makes the function
   report success with an empty `interpreter`, and `execve("")` then fails
   inside the child.
4. `conf.root + request_path` concatenates without a separator — and because of
   (1) the separator is exactly what was removed. `conf.root` is also the
   *location* root, which is empty whenever the location does not override it,
   so the server root is never used as a fallback.

## Fix

The URI is left alone, the path join is explicit, and the flag is initialised
in the constructor.

```cpp
std::string joinPath(const std::string& root, const std::string& path)
{
    std::string base = root;

    if (!base.empty() && base[base.size() - 1] == '/')
        base.erase(base.size() - 1);
    if (path.empty() || path[0] != '/')
        return base + "/" + path;
    return base + path;
}
```

```cpp
bool CGI::_find_interpreter(const Location_Config& conf, const Server_block& server)
{
    size_t pos = request_path.rfind('.');

    if (pos == std::string::npos)
        return false;

    std::string extension = request_path.substr(pos);

    for (size_t i = 0; i < conf.cgi_extensions.size(); i++)
    {
        if (conf.cgi_extensions[i] != extension)
            continue;
        interpreter = conf.cgi_paths[i];
        extension_found = true;
        break;
    }
    if (!extension_found)
    {
        DEBUG("CGI") << "_find_interpreter: no interpreter configured for extension=" << extension;
        return false;
    }

    std::string root = server.root;
    if (conf.has_root && !conf.root.empty())
        root = conf.root;
    script = joinPath(root, request_path);
    DEBUG("CGI") << "_find_interpreter: extension=" << extension
                 << " interpreter=" << interpreter << " script=" << script;
    return true;
}
```

```cpp
CGI::CGI(Client& client, const Location_Config& conf, const Server_block& server)
: pid(-1),
  request_path(client.parsed_request.getRequestPath()),
  extension_found(false),
  request_vars(NULL)
{
    stdin_pipe[0] = -1;
    stdin_pipe[1] = -1;
    stdout_pipe[0] = -1;
    stdout_pipe[1] = -1;
    if (!_find_interpreter(conf, server))
        return;
    build_env_vars(client, server);
}
```

```cpp
bool CGI::isRunnable() const
{
    return extension_found;
}
```

The destructor must survive an object that never got as far as
`build_env_vars()` — the original would dereference a null `request_vars`:

```cpp
CGI::~CGI()
{
    if (request_vars == NULL)
        return;
    for (size_t i = 0; request_vars[i] != NULL; i++)
        free(request_vars[i]);
    delete[] request_vars;
}
```

`remove_char_at()` has no remaining caller and is deleted.

Return type changed from `int` to `bool`: the old function returned `1` for
"no dot", `ERROR` (also `1`) for "no interpreter" and `SUCESS` (`0`) for
success, so `if (_find_interpreter(...))` read backwards. The caller now uses
`isRunnable()`.

## Verification

```
$ curl -s http://127.0.0.1:1025/cgi-bin/hello.py
CGI-GET-SENTINEL
$ grep script /path/to/webserv.log | tail -1
[DEBUG] _find_interpreter: extension=.py interpreter=/usr/bin/python3 script=/…/EngineX/www/cgi-bin/hello.py
```
