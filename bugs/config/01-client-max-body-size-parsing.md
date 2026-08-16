# 01 — `client_max_body_size` cannot be parsed

**Where:** `src/ConfFile/Conf_file_parsing.cpp` → `parse_max_body_size()`
**Also touches:** `src/Request/ClientRequest.cpp` → `getServerMaxBodySize()`

## Symptom

A configuration file with a plain byte value refuses to start the server:

```
$ ./webserv conf.conf          # client_max_body_size 1000000;
[ERROR] main: Error
Max upload value was exceeded.
```

Every fixture configuration in the repository (`conf.conf`, `local.conf`,
`EngineX/EngineX.conf`) uses a plain byte count, so none of them can be loaded.

## Cause

Three separate defects in the same function.

1. **A plain number is rejected.** `strtol` leaves `unit` pointing at the
   terminating `'\0'`. The code only accepts `K`, `M` or `G` and falls into the
   `else throw` for every other value — including "no unit at all".

2. **`unit` dangles.** It points inside the temporary returned by
   `next_token(...).substr(0, size).c_str()`. That temporary is destroyed at the
   end of the full expression, so `unit[0]` reads freed memory. The value it
   sees is whatever the stack happens to hold.

3. **The unit is applied twice.** `getServerMaxBodySize()` multiplies the stored
   value *again* by `body_size_is_MB` / `body_size_is_KB` — two flags that are
   never assigned anywhere in the project (see [03](03-uninitialised-config-fields.md)).
   `2k` could therefore mean 2048 or 2 097 152 bytes depending on stack garbage.

`size` is also read from `tokens[index + 1]` *before* the bounds check on the
next line, so a truncated file reads past the end of the vector.

## Fix

The size grammar is one concern, the directive is another. Three small
functions replace the single tangled one.

```cpp
static bool size_unit_multiplier(const std::string& unit, long& multiplier)
{
    if (unit.empty() || unit == "b" || unit == "B")
    {
        multiplier = 1;
        return true;
    }
    if (unit == "k" || unit == "K")
    {
        multiplier = 1024L;
        return true;
    }
    if (unit == "m" || unit == "M")
    {
        multiplier = 1024L * 1024L;
        return true;
    }
    if (unit == "g" || unit == "G")
    {
        multiplier = 1024L * 1024L * 1024L;
        return true;
    }
    return false;
}
```

```cpp
static size_t count_leading_digits(const std::string& token)
{
    size_t digits = 0;

    while (digits < token.size() && isdigit(static_cast<unsigned char>(token[digits])))
        digits++;
    return digits;
}
```

```cpp
long parse_size_in_bytes(const std::string& token)
{
    size_t digits = count_leading_digits(token);
    if (digits == 0)
        throw Error::MaxUploads();

    long multiplier = 1;
    if (!size_unit_multiplier(token.substr(digits), multiplier))
        throw Error::MaxUploads();

    const long max_long = std::numeric_limits<long>::max();
    long value = strtol(token.substr(0, digits).c_str(), NULL, 10);

    if (value < 0 || value > max_long / multiplier)
        throw Error::MaxUploads();
    return value * multiplier;
}
```

```cpp
void parse_max_body_size(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size() || Conf_File::tokens[index + 2] != ";")
        throw Error::MaxUploads();

    Server_block& server = Conf_File::Servers[server_index];

    server.max_body_size = parse_size_in_bytes(Conf_File::tokens[index + 1]);
    server.client_max_body_found = true;
    index += 3;
    DEBUG("ConfFile") << "parse_max_body_size: parsed client_max_body_size="
                      << server.max_body_size
                      << " bytes server=" << server_index;
}
```

`parse_size_in_bytes` is not `static`: [02](02-location-client-max-body-size.md)
reuses it for the location-level directive. Declare it in
`includes/multiplexing/header.hpp` next to the other parser prototypes:

```cpp
long parse_size_in_bytes(const std::string& token);
```

The value stored is now always **bytes**, so the second scaling in
`getServerMaxBodySize()` must go — it is removed as part of the rewrite in
[11](11-max-body-size-first-port-only.md).

## Notes

* `index += 3` replaces `next_token(...) ; index += 2`. `next_token` advances
  the index by one as a side effect, so both forms skip
  `<directive> <value> ;` — the new one just says so out loud.
* Accepting a bare `b`/`B` suffix is what lets the tester's own value
  `client_max_body_size 100b;` be copied over unchanged.

## Verification

```
$ ./webserv test.conf     # client_max_body_size 1000000;  and  100b;  and  200M;
[INFO] parse_config_file: loaded 2 server block(s)
[INFO] main: listening on 127.0.0.1:1027
```
