# 03 — Configuration structs are read before they are written

**Where:** `includes/multiplexing/header.hpp` → `Location_Config`, `Server_block`

## Symptom

Undefined behaviour, so it shows up as "works on my machine": `autoindex on;`
at server level is silently ignored for some locations, a location without
`root` sometimes serves from an empty root (`/index.htm` instead of
`<root>/index.htm`), and a rebuild can change the outcome.

The reason tests 10 and 11 pass today is luck — `directory/nop/` happens to
contain a file named by the `index` directive, so the autoindex path is never
reached.

## Cause

`Location_Config` is an aggregate with no constructor:

```cpp
Location_Config obj;                       // parse_config_file()
Conf_File::Servers[server_index].location.push_back(obj);
```

`obj` is default-initialised. Its `std::string` / `std::vector` / `std::map`
members are constructed properly, but `has_index`, `has_root`, `has_autoindex`,
`cgi_paths_index` and `cgi_extns_index` are **indeterminate**. Two of them are
assigned right after the `push_back`; the three `has_*` flags never are.

Those flags are the entire override mechanism:

```cpp
// GET::isAutoindexEnabled
if (location != NULL && location->has_autoindex)     // garbage
    return location->autoindex == "on";              // "" == "on" -> false

// AMethod::resolveTarget
if (location && location->has_root && !location->root.empty())   // garbage
```

A garbage-true `has_autoindex` turns server-level `autoindex on` into `off`
for that location, because `location->autoindex` is the empty string.

`Server_block` has a constructor, but it leaves `uploadLimits`, `index_count`,
`max_body_size`, `body_size_is_MB`, `body_size_is_KB` and `body_size_is_BT`
uninitialised, and those last three are multiplied into the body-size limit
(see [01](01-client-max-body-size-parsing.md)).

## Fix

### `Location_Config` — give it a constructor

The struct is written out in full, including the members added by
[02](02-location-client-max-body-size.md) and
[16](16-return-directive-ignored.md), so it can be copied as-is:

```cpp
typedef struct Location_Config
{
    std::string root;
    std::string path;
    std::string upload_path;
    std::vector<std::string> index_files;
    std::vector<std::string> allowed_methods;
    std::vector<std::string> cgi_extensions;
    std::vector<std::string> cgi_paths;
    std::map<int, std::string> error_pages;
    std::string _return;
    std::string redirect_target;
    int         redirect_code;
    bool        has_redirect;
    std::string autoindex;
    bool has_index;
    bool has_root;
    bool has_autoindex;
    bool has_max_body_size;
    long max_body_size;
    size_t cgi_paths_index;
    size_t cgi_extns_index;

    Location_Config()
    : redirect_code(0),
      has_redirect(false),
      has_index(false),
      has_root(false),
      has_autoindex(false),
      has_max_body_size(false),
      max_body_size(0),
      cgi_paths_index(0),
      cgi_extns_index(0)
    {}

} Location_Config;
```

With the constructor in place, these two lines in `parse_config_file()` become
dead weight and can be deleted:

```cpp
Conf_File::Servers[server_index].location[...].cgi_extns_index = 0;
Conf_File::Servers[server_index].location[...].cgi_paths_index = 0;
```

### `Server_block` — complete the constructor

```cpp
        Server_block()
        {
            server_found = false;
            host_found = false;
            location_found = false;
            root_found = false;
            server_name_found = false;
            listen_found = false;
            index_found = false;
            server_has_autoindex = false;
            error_page_found = false;
            client_max_body_found = false;
            uploadLimits = false;
            ports_count = 0;
            index_count = 0;
            location_count = 0;
            max_body_size = 0;
            body_size_is_MB = false;
            body_size_is_KB = false;
            body_size_is_BT = false;
        }
```

`location_count` is still set to `-1` by `parse_config_file()` when a `server`
block opens (it is pre-incremented before the first location is pushed); the
initialiser here only guarantees a defined value if that assignment is ever
removed.

## Verification

Built with `-fsanitize=undefined` the original code reports a load of an
uninitialised `bool` in `GET::isAutoindexEnabled`; after the fix it is clean,
and a location without an explicit `autoindex` now inherits the server
setting as documented.
