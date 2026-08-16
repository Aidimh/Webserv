# Known Bugs

Bugs found while adding logging. Nothing here is fixed in code — this file is the record.
Each entry says where it is, what goes wrong, and why.

---

## 1. `Server_block` fields are never initialized

**Where:** `includes/multiplexing/header.hpp:140` (`reset_flags`), `src/ConfFile/Conf_file_parsing.cpp` (`parse_config_file`)

`Server_block` has no constructor. It has a `reset_flags()` method that zeroes the flags and
`ports_count`, but `grep -rn "reset_flags"` across the whole project returns **only the definition** —
it is never called. Every `Server_block` pushed into `Conf_File::Servers` therefore starts with
garbage in all its scalar members.

**What it breaks:** `main.cpp:150` loops `while (j < Conf_File::Servers[i].ports_count)` and indexes
`Conf_File::Servers[i].listen_port[j]`. With `ports_count` holding garbage, that reads past the end of
the `listen_port` vector and tries to bind sockets that were never configured.

**Evidence** — a config with exactly one `listen` directive, logged at parse time:
```
[DEBUG] parse_listen: parsed listen port=8080 server=0 ports_count=4715
...
Error!
Bind failed!.
```
`ports_count` should be `1`. The value changes between runs (`4715`, `140730018144293`), which is the
signature of uninitialized memory. The server does not start.

**Fields affected:** `ports_count`, all the `*_found` bools, `server_has_autoindex`, `uploadLimits`,
`max_body_size`, `body_size_is_MB` / `body_size_is_KB` / `body_size_is_BT`, `index_count`.
Note `reset_flags()` does not cover `max_body_size`, `body_size_is_*`, `index_count` or `uploadLimits`
either, so calling it is necessary but not sufficient.

---

## 1b. `struct Client` is never initialized either — it corrupts the fd table

**Where:** `includes/multiplexing/header.hpp:174`, used in `src/Multiplexing/Engine.cpp` `_acceptNewClient`

Same shape as #1, different struct. `Client` has no constructor, and `_acceptNewClient` only sets
`fd`, `port` and `parsed_request.state`:

```cpp
Client client;
client.fd = client_fd;
client.port = s->get_listen_port();
client.parsed_request.state = ClientRequest::HEADERS;
```

`stream_file_fd`, `response_prepared`, `cgi_started`, `stream_buffer_size` and `stream_buffer_offset`
are left holding whatever was on the stack.

**What it breaks:** `stream_file_fd` is garbage rather than `-1`, and both `Client::reset()` and
`Multiplexer::_writeClient` are guarded on exactly that value:

```cpp
if (stream_file_fd != -1)
    close(stream_file_fd);          // closes an arbitrary descriptor
```

So the server closes a descriptor it never opened. Once that descriptor is one of the standard
streams, the next `accept()` hands it straight back out.

**Evidence** — three requests against one server; the first succeeds, then the fd table is corrupt:
```
[DEBUG] _acceptNewClient: accepted client fd=5 port=8080     <- GET /     -> 200
[DEBUG] _acceptNewClient: accepted client fd=0 port=8080     <- GET /nope -> no response
[DEBUG] _acceptNewClient: accepted client fd=0 port=8080     <- DELETE /x -> no response
```
`fd=0` is stdin. Every request after the first fails because the connection is accepted onto a
descriptor that is not a real client socket.

---

## 1c. `Socket` closes its descriptor twice

**Where:** `src/Multiplexing/Engine.cpp`, `Socket::~Socket` and `AFd::~AFd`

```cpp
AFd::~AFd()      { if (fd != -1) close(fd); }
Socket::~Socket(){ close(fd); }                 // runs first, then the base destructor runs
```

`Socket` derives from `AFd`, so destroying a `Socket` closes `fd`, and then the base destructor closes
the same `fd` again. `Socket::~Socket` also never sets `fd = -1`, so the base class guard cannot
catch it.

A double `close()` is not harmless here: if any descriptor is opened between the two calls, the second
`close()` targets a live, unrelated descriptor. This compounds #1b.

---

## 2. `client_max_body_size 1M` is parsed as 1 byte

**Where:** `src/ConfFile/Conf_file_parsing.cpp`, `parse_max_body_size`

```cpp
size_t size = Conf_File::tokens[index + 1].size();          // size of "1M" = 2
...
max_body_size = strtol(next_token(Conf_File::tokens, index).substr(0, size).c_str(), &unit, 10);
if (max_uploads_is_unit(size, index))                       // index has already moved
```

`next_token()` does `return tokens[++i]` — it **advances `index` as a side effect**. By the time
`max_uploads_is_unit(size, index)` runs, `index` no longer points where the caller thinks it does:
`tokens[index + 1]` is now `";"`, and the helper reads `";"[size - 1]` = `";"[1]`, one past the end of a
one-character string. The unit check returns false, so the `max_body_size *= 1000000` branch never runs.

**Effect:** the limit is silently set to `1` instead of `1000000`. Any request with a body larger than
one byte is measured against a 1-byte cap.

**Evidence:**
```
[DEBUG] parse_max_body_size: parsed client_max_body_size=1 bytes server=0
```
for a config line reading `client_max_body_size 1M;`.

---

## 3. `client_max_body_size 1000000` (no unit) crashes the server

**Where:** `src/ConfFile/Conf_file_parsing.cpp`, `parse_max_body_size` — same root cause as #2

With no unit suffix, `size` is 7 (`"1000000"`), and `max_uploads_is_unit` reads `";"[6]` — six bytes past
the end of the token. This one does not merely return the wrong answer, it faults.

**Evidence:** `./webserv <config>` with `client_max_body_size 1000000;` dumps core during parsing,
immediately after `parse_index` logs:
```
[DEBUG] parse_index: parsed index files count=1 server=0
timeout: the monitored command dumped core
```

**Status:** the helper was since changed to `max_uploads_is_unit(char unit)`, which fixes the
out-of-bounds read behind #2 and #3. The unit branch below is still wrong — see #3b.

---

## 3b. Every `client_max_body_size` with a unit now throws

**Where:** `src/ConfFile/Conf_file_parsing.cpp`, `parse_max_body_size`

With the `max_uploads_is_unit(char)` fix in place, the unit is detected correctly, but the branch
structure rejects it:

```cpp
if (*unit == 'M') { ... max_body_size *= 1000000; }   // separate if, falls through
if (*unit == 'G')
    throw Error::MaxUploads();
if (*unit == 'K')      { ... }
else if (unit == NULL) { ... }
else
    throw Error::MaxUploads();                        // 'M' lands here
```

The `M` branch is a standalone `if`, so after it runs control reaches the `K` chain. `'M'` is not
`'K'`, and `unit` is not `NULL`, so the trailing `else` throws.

**Effect:** `client_max_body_size 1M;` aborts startup:
```
[DEBUG] parse_index: parsed index files count=1 server=0
[ERROR] main: Error
Max upload value was exceeded.
```
The `M`, `G` and `K` tests need to be one `if / else if` chain. The `else if (unit == NULL)` test is
also unreachable — `unit` is the address of the null terminator when there is no suffix, never `NULL`,
so the no-unit case has to be `*unit == '\0'`.

---

## 4. `upload_store` always throws — the existence check is inverted

**Where:** `src/ConfFile/location_parsing.cpp:53`, `parse_upload_store`

```cpp
if (path_file_exists(Conf_File::tokens[index + 1]))
{
    if (mkdir(Conf_File::tokens[index + 1].c_str(), 777) != 0)
        throw std::runtime_error("Could not create the path : " + Conf_File::tokens[index + 1]);
}
```

The condition runs `mkdir` **when the directory already exists**, which always fails with `EEXIST`, so a
valid `upload_store` pointing at a real directory always throws. When the directory genuinely is
missing, nothing is created. The test is backwards — it should be `if (!path_file_exists(...))`.

**Evidence:** `upload_store /home/aazzaoui/Webserv/www/upload;` (an existing directory) aborts startup:
```
Could not create the path : /home/aazzaoui/Webserv/www/upload
```

**Also:** the mode is `777` decimal, not `0777` octal. Decimal 777 is octal 1411, which sets the sticky
bit and gives owner write-only, no read and no execute — the directory would be unusable even if the
branch were corrected.

---

## 5. The `return` directive is validated then thrown away

**Where:** `src/ConfFile/location_parsing.cpp`, `parse_return`

```cpp
void parse_return(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Root();
    if (Conf_File::tokens[index + 2] != ";")
        throw Error::Root();
}
```

The function checks the syntax and returns without storing anything, and without advancing `index`
the way every other parse function does. `Location_Config` has no field to hold a redirect target, and
nothing in the response path looks for one.

**Effect:** a `return 301 /somewhere;` in a config is silently ignored — the server serves the location
normally instead of redirecting. Since `index` is not advanced either, the surrounding token loop
re-reads the same tokens, which can lead to `return`'s arguments being interpreted as directives.

`parse_return` is reachable from both `parse_directives` (server level) and
`parse_location_directives` (location level), so both are affected.

---

## 6. CGI pipe log prints a value with no separator

**Where:** `src/CGI/CGI_class.cpp:90` and `:100`

```cpp
DEBUG("CGI") << "Closed stdin pipe" << stdin_pipe[0] << " and stdout pipe " << stdout_pipe[1];
```

Missing space after `"pipe"`, so the output reads `Closed stdin pipe0`. Cosmetic, but it makes the fd
unreadable at a glance and unsearchable. Both lines also report `stdin_pipe[0]` and `stdout_pipe[1]`
*after* those descriptors were closed, and the parent branch closes `stdin_pipe[0]` / `stdout_pipe[1]`
while the message claims the same pair as the child branch.
