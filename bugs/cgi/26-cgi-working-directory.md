# 26 — A CGI script runs in the server's working directory

**Where:** `src/CGI/CGI_class.cpp` → `runChild()`

## Symptom

A script that opens a file next to itself fails:

```python
# EngineX/www/cgi-bin/dir_check.py
with open("data.txt", "r") as f:
    body = f.read().strip()
```

```
$ curl -s http://127.0.0.1:1025/cgi-bin/dir_check.py
$                       # empty: the script died with FileNotFoundError
```

CGI test 6 fails with `(missing: CGI-WORKDIR-SENTINEL)`.

## Cause

`fork()` inherits the parent's working directory, and nothing changes it before
`execve()`. The script therefore resolves `data.txt` against wherever the server
was started from, not against `EngineX/www/cgi-bin/`.

Relative paths are the normal way for a CGI to reach its own data — templates,
fixtures, a small database — so the practical effect is that any script with
more than one file stops working. Every real gateway (`mod_cgi`, `fcgiwrap`,
nginx + `cgi-fcgi`) chdir's into the script directory first.

## Fix

The child moves into the script's directory before exec, and passes the script
by name from there. Splitting a path is its own small job, twice:

```cpp
static std::string directoryOf(const std::string& path)
{
    size_t slash = path.find_last_of('/');

    if (slash == std::string::npos)
        return "";
    return path.substr(0, slash);
}
```

```cpp
static std::string fileNameOf(const std::string& path)
{
    size_t slash = path.find_last_of('/');

    if (slash == std::string::npos)
        return path;
    return path.substr(slash + 1);
}
```

```cpp
/*
 * Runs in the forked child: move to the script directory, wire the pipes on
 * stdin/stdout and exec. Scripts open their data files by relative name, so
 * the working directory has to be the script's own, not the server's.
 * The child never returns from here.
 */
void CGI::runChild()
{
    char *argv[3];
    std::string directory = directoryOf(script);
    std::string scriptArgument = script;

    if (!directory.empty() && chdir(directory.c_str()) == 0)
        scriptArgument = fileNameOf(script);

    argv[0] = const_cast<char *>(interpreter.c_str());
    argv[1] = const_cast<char *>(scriptArgument.c_str());
    argv[2] = NULL;

    if (dup2(stdin_pipe[0], STDIN_FILENO) == -1 || dup2(stdout_pipe[1], STDOUT_FILENO) == -1)
        _exit(1);

    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);

    execve(interpreter.c_str(), argv, request_vars);
    _exit(1);
}
```

The relative name is only used when the `chdir` succeeded; otherwise the
absolute path is passed as before, so a directory the server cannot enter
degrades to the old behaviour instead of failing to exec.

`SCRIPT_FILENAME` and `PATH_TRANSLATED` keep the absolute path — the
environment describes the script, the working directory is where it runs.

`chdir` affects only the child process, so the server's own relative paths
(`www/error_pages/…`, the temp body directory) are untouched.

## Verification

```
$ curl -s http://127.0.0.1:1025/cgi-bin/dir_check.py
CGI-WORKDIR-SENTINEL
```
