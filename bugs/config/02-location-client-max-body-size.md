# 02 — `client_max_body_size` cannot be set on a location

**Where:** `src/ConfFile/Conf_file_parsing.cpp` → `parse_location_directives()`
(there is a `//todo` on the very line), `includes/multiplexing/header.hpp` →
`Location_Config`, `src/ConfFile/location_parsing.cpp`

## Symptom

Minimum Evaluation tests **21, 22, 25, 26 and 30** fail:

```
✘ FAIL  21 » POST /post_body with 200 bytes    (missing: 413 Payload Too Large)
✘ FAIL  22 » POST /post_body with 101 bytes    (missing: 413 Payload Too Large)
✘ FAIL  25 » Chunked POST /post_body 200 bytes (missing: 413 Payload Too Large)
✘ FAIL  26 » Chunked POST /post_body 101 bytes (missing: 413 Payload Too Large)
✘ FAIL  30 » Stress large POST fork 20         (missing: 413 Payload Too Large)
```

The tester configuration asks for two different limits on the same port:

```nginx
server {
    listen 1027;
    ...                                  # no limit: 100 MB uploads must pass
    location /post_body {
        client_max_body_size 100b;       # this route caps at 100 bytes
    }
}
```

With a server-wide limit only, the two requirements are mutually exclusive:
whatever value is chosen, either `/post_body` accepts 200 bytes (tests 21/22/25/26/30 fail)
or `/directory/youpi.bla` rejects the 100 MB body (tests 16/16b/17/31 fail).

## Cause

`client_max_body_size` is only dispatched in `parse_directives()` (server
scope). Inside a location block the token falls through to
`throw Error::Unknown_Directive()`, and `Location_Config` has nowhere to store
a limit.

## Fix

### 1. `Location_Config` carries the limit — `includes/multiplexing/header.hpp`

Add the two members and initialise them in the constructor added by
[03](03-uninitialised-config-fields.md):

```cpp
    bool has_max_body_size;
    long max_body_size;
```

### 2. The location parser — `src/ConfFile/location_parsing.cpp`

```cpp
void parse_location_max_body_size(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size() || Conf_File::tokens[index + 2] != ";")
        throw Error::MaxUploads();

    size_t count = Conf_File::Servers[server_index].location_count;
    Location_Config& location = Conf_File::Servers[server_index].location[count];

    location.max_body_size = parse_size_in_bytes(Conf_File::tokens[index + 1]);
    location.has_max_body_size = true;
    index += 3;
    DEBUG("ConfFile") << "parse_location_max_body_size: parsed client_max_body_size="
                      << location.max_body_size << " bytes location=" << count
                      << " server=" << server_index;
}
```

`parse_size_in_bytes()` is the shared helper from
[01](01-client-max-body-size-parsing.md), so both scopes accept exactly the
same grammar. Declare the new function in `includes/multiplexing/header.hpp`:

```cpp
void parse_location_max_body_size(size_t &index);
```

### 3. The dispatch table — `src/ConfFile/Conf_file_parsing.cpp`

The existing function also contains a copy-pasted duplicate `index` branch that
can never be reached; it is dropped here.

```cpp
void parse_location_directives(std::string& token, size_t &i)
{
    DDEBUG("ConfFile") << "parse_location_directives: dispatching token=" << token << " index=" << i;
    if (token == "root")
        parse_root_path(i);
    else if (token == "index")
        parse_location_index(i);
    else if (token == "client_max_body_size")
        parse_location_max_body_size(i);
    else if (token == "autoindex")
        parse_autoindex(i);
    else if (token == "return")
        parse_return(i);
    else if (token == "allowed_methods")
        parse_methods(i);
    else if (token == "cgi_extension")
        parse_cgi_extension(i);
    else if (token == "upload_store")
        parse_upload_store(i);
    else
        throw Error::Unknown_Directive();
}
```

### 4. Reading the limit back

The lookup lives in `ClientRequest::getServerMaxBodySize()`; its complete
replacement — which resolves the location before falling back to the server —
is in [11](11-max-body-size-first-port-only.md).

## Verification

```
$ curl -s -o /dev/null -w '%{http_code}\n' -X POST --data-binary "$(python3 -c 'print("P"*100,end="")')" http://127.0.0.1:1027/post_body
201
$ curl -s -o /dev/null -w '%{http_code}\n' -X POST --data-binary "$(python3 -c 'print("Q"*101,end="")')" http://127.0.0.1:1027/post_body
413
```

Tester, after the fix:

```
✔ PASS  21 » POST /post_body with 200 bytes
✔ PASS  22 » POST /post_body with 101 bytes exceeds limit
✔ PASS  25 » Chunked POST /post_body with 200 bytes
✔ PASS  26 » Chunked POST /post_body with 101 bytes exceeds limit
✔ PASS  30 » Stress large POST fork 20 (20/20 OK)
```
