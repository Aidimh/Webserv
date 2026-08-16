# 22 — `upload_store` creates the directory only when it already exists

**Where:** `src/ConfFile/location_parsing.cpp` → `parse_upload_store()`

## Symptom

Any configuration using the directive fails to load if the directory exists:

```
$ ./webserv conf.conf          # location /upload { upload_store www/upload; }
[ERROR] main: Could not create the path : www/upload
```

and silently does nothing if it does not — the server starts, and the first
upload fails at request time because the directory was never created.

There is no way to write a configuration where this directive works.

## Cause

```cpp
if (path_file_exists(Conf_File::tokens[index + 1]))
{
    if (mkdir(Conf_File::tokens[index + 1].c_str(), 777) != 0)
        throw std::runtime_error("Could not create the path : " + Conf_File::tokens[index + 1]);
}
```

Two defects:

1. **The condition is inverted.** `mkdir` is attempted when the path *exists*,
   where it can only ever fail with `EEXIST`; when the path is missing —
   the one case where creating it makes sense — the branch is skipped.

2. **`777` is decimal.** `mkdir` takes an octal mode. `777` decimal is `01411`
   octal: the sticky bit, plus `r--` for owner, `--x` for group and `--x` for
   others. Even if the call succeeded, the server could not write into the
   directory it just made. The intended value is `0755`.

## Fix

Creating a directory when needed is one job; parsing the directive is another.

```cpp
static void ensureDirectoryExists(const std::string& path)
{
    struct stat info;

    if (stat(path.c_str(), &info) == 0)
    {
        if (S_ISDIR(info.st_mode))
            return;
        throw std::runtime_error("upload_store path exists and is not a directory : " + path);
    }
    if (mkdir(path.c_str(), 0755) != 0)
    {
        DEBUG("ConfFile") << "ensureDirectoryExists: mkdir failed path=" << path
                          << ": " << strerror(errno);
        throw std::runtime_error("Could not create the path : " + path);
    }
    DEBUG("ConfFile") << "ensureDirectoryExists: created directory path=" << path;
}
```

```cpp
void parse_upload_store(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Root();
    if (Conf_File::tokens[index + 2] != ";")
        throw Error::SemiColon();

    size_t count = Conf_File::Servers[server_index].location_count;
    Location_Config& location = Conf_File::Servers[server_index].location[count];

    ensureDirectoryExists(Conf_File::tokens[index + 1]);
    location.upload_path = Conf_File::tokens[index + 1];
    index += 3;
    DEBUG("ConfFile") << "parse_upload_store: parsed upload_store=" << location.upload_path
                      << " location=" << count << " server=" << server_index;
}
```

`ensureDirectoryExists()` is now honest about all three outcomes: the directory
is there, it was created, or the configuration is wrong and the server refuses
to start.

## Note

`upload_path` is stored but no handler reads it — `POST` writes to
`root + request_uri` regardless. Making `upload_store` actually redirect writes
is a feature, not a bug fix, and is out of scope here; the parser at least stops
lying about what it did.

## Verification

```
$ rm -rf /tmp/up && ./webserv conf.conf     # location { upload_store /tmp/up; }
[INFO] main: listening on 127.0.0.1:1025
$ ls -ld /tmp/up
drwxr-xr-x 2 aazzaoui aazzaoui 40 /tmp/up
$ ./webserv conf.conf                        # second start, directory already there
[INFO] main: listening on 127.0.0.1:1025
```

Both starts succeed; before the fix the second one aborted.
