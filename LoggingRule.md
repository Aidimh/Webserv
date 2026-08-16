# Logging Rules

Conventions for using the `Logging` class (`src/Logging/Logging.hpp`) across the codebase.
This doc defines the rules; applying them to existing files is a separate pass.

## The macros

```cpp
INFO()               // always printed, no gating
WARN()                // always printed, no gating
ERR()                 // always printed, no gating
DEBUG("ClassName")    // printed only if -d ClassName (or global -d) is active
DDEBUG("ClassName")   // printed only if -D ClassName (or global -D) is active
```

`DEBUG`/`DDEBUG` compile out completely when `DISABLE_LOGGING` is defined, so never put code
with side effects inside a log statement (e.g. `DEBUG("X") << (counter++)`).

## Which level to use

| Level | Use for | Example |
|---|---|---|
| `INFO` | Things the operator should always see: server startup, config loaded, listening on a port, shutdown. Rare, high-signal, one per lifecycle event. | `INFO() << "Listening on " << host << ":" << port;` |
| `WARN` | Something unexpected happened but the server recovered or degraded gracefully (bad request line, config falls back to default, client sent malformed data). | `WARN() << "Router: unknown location for " << path << ", falling back to default";` |
| `ERR` | A syscall or operation failed and something broke as a result (bind/accept/fork/read failure, exception caught, fatal parse error). Always include `strerror(errno)` when logging a failed syscall. | `ERR() << "Socket::setup: bind failed on " << host << ":" << port << ": " << strerror(errno);` |
| `DEBUG("Class")` | Per-class, opt-in trace of what a component is doing: entering/leaving a function, state transitions, fd lifecycle. This is the bulk of the logging in the codebase. | `DEBUG("CGI") << "execute: forked pid=" << pid << " for " << script;` |
| `DDEBUG("Class")` | Same as `DEBUG` but for noisy/high-frequency detail you don't want even when `-d Class` is on (per-byte counts, per-loop-iteration state, raw buffer contents). Only enabled with `-D Class`. | `DDEBUG("Multiplexer") << "readClient: buffer now " << buffer.size() << " bytes";` |

If you're unsure between `DEBUG` and `DDEBUG`: if it fires once per request/connection/CGI call, it's
`DEBUG`. If it fires once per loop iteration/read/poll tick, it's `DDEBUG`.

## Message format

```
DEBUG("ClassName") << "functionName: message";
```

- `ClassName` is the actual C++ class name (see table below), not the filename.
- The message always starts with the function name, followed by `: `, then the message.
- `INFO`/`WARN`/`ERR` don't take a class argument, but still prefix with the function name when
  the source isn't obvious from the message alone (e.g. inside `Engine.cpp` where many functions
  touch the same fd).
- No trailing period. No emoji. Message is one line of intent, not a sentence.
- Identifiers (fd, pid, path, size, status code) are written as `name=value`, not prose like
  "the fd which is 5". This makes messages greppable.

```cpp
// good
DEBUG("CGI") << "execute: forked pid=" << pid << " script=" << script;
DEBUG("Multiplexer") << "acceptNewClient: accepted fd=" << client_fd << " port=" << port;
ERR() << "Socket::setup: bind failed host=" << host << " port=" << port << ": " << strerror(errno);

// bad — no function name, no field names, string built by blind concatenation
DEBUG("CGI") << "Closed stdin pipe" << stdin_pipe[0] << " and stdout pipe " << stdout_pipe[1];
```
That last "bad" example is real, current code
(`src/CGI/CGI_class.cpp:90`) — it prints `Closed stdin pipe0` because there's no space before the
value. This is exactly the kind of bug the `name=value` rule prevents.

## Class name key

Use the class the log line lives in. For files that are mostly free functions (parsing, signals),
use the logical module name listed below consistently everywhere in that file.

| File(s) | `DEBUG(...)` class string |
|---|---|
| `src/Multiplexing/Engine.cpp` (`Multiplexer` methods) | `"Multiplexer"` |
| `src/Multiplexing/Engine.cpp` (`Socket`/`AFd` methods) | `"Socket"` |
| `src/CGI/CGI_class.cpp` | `"CGI"` |
| `src/Request/ClientRequest.cpp`, `RequestHelpers.cpp` | `"ClientRequest"` |
| `src/Response/Response.cpp` | `"Response"` |
| `src/Response/Dispatcher.cpp` | `"Dispatcher"` |
| `src/Response/AMethod.cpp` | `"AMethod"` |
| `src/Response/GET.cpp` | `"GET"` |
| `src/Response/POST.cpp` | `"POST"` |
| `src/Response/DeleteMethod.cpp` | `"DeleteMethod"` |
| `src/Response/MethodFactory.cpp` | `"MethodFactory"` |
| `src/Response/MultipartUploadStrategy.cpp` | `"MultipartUploadStrategy"` |
| `src/Response/BuffersStrategy.cpp` | `"BuffersStrategy"` |
| `src/Routing/Router.cpp` | `"Router"` |
| `src/ConfFile/*.cpp` | `"ConfFile"` |
| `src/Signals/signal_handling.cpp` | `"Signal"` |

Don't invent a new string per function — one class string per file/component, so `-d ClassName`
turns on everything relevant at once.

## Standard phrasing for recurring operations

The same kind of operation must read the same way everywhere it happens, so a `-d` trace is
scannable. Use these templates as-is (fill in the fields), don't rephrase them per file.

| Operation | Level | Template |
|---|---|---|
| Open fd (socket/file) succeeds | `DEBUG` | `"funcName: opened <what> fd=<fd>"` |
| Open fails | `ERR` | `"funcName: open <what> failed: " << strerror(errno)` |
| Close fd | `DEBUG` | `"funcName: closed <what> fd=<fd>"` |
| Accept | `DEBUG` | `"funcName: accepted client fd=<fd>"` |
| Fork | `DEBUG` | `"funcName: forked pid=<pid> for <purpose>"` |
| Bind/listen succeeds | `INFO` | `"funcName: listening on <host>:<port>"` |
| Bind/listen fails | `ERR` | `"funcName: listen failed on <host>:<port>: " << strerror(errno)` |
| Read from fd | `DEBUG` | `"funcName: read <n> bytes from fd=<fd>"` |
| Read fails / peer closed | `WARN` | `"funcName: read failed fd=<fd>: " << strerror(errno)` or `"funcName: peer closed fd=<fd>"` |
| Write/send to fd | `DEBUG` | `"funcName: wrote <n> bytes to fd=<fd>"` |
| Remove client/connection | `DEBUG` | `"funcName: removed client fd=<fd>"` |
| Process killed (CGI timeout etc.) | `WARN` | `"funcName: killed pid=<pid> reason=<reason>"` |
| Config value parsed/loaded | `DEBUG` | `"funcName: parsed <key>=<value>"` |
| Config validation failure | `ERR` | `"funcName: invalid config: <reason>"` |

Example of the pattern already partially in use and worth keeping
(`src/Multiplexing/Engine.cpp:224`):
```cpp
DEBUG("Multiplexer") << "acceptNewClient: accepted fd=" << client_fd;
```
vs. the destructor at `src/Multiplexing/Engine.cpp:198`, which is missing the fd label and
mismatches the template — should become:
```cpp
DEBUG("Socket") << "~Socket: closed fd=" << fd;
```

## Placement rules

- Log after the syscall/operation, once you know whether it succeeded — not before, unless you're
  logging intent for something that can hang (e.g. "about to fork").
- One log line per event. Don't split one action across two `DEBUG` calls.
- Don't log inside hot loops (`poll()` tick, per-byte copy) at `DEBUG` — use `DDEBUG` or don't log
  at all.
- Every fd/pid a log line mentions should still be traceable to a close/kill later in the same
  file — if you add an "opened" log, make sure there's a matching "closed" log on every exit path
  (including error paths), not just the happy path.
- Exceptions: log once where caught with `ERR`, don't also log at the throw site — avoids the same
  failure appearing twice in the trace.

## What not to log

- No secrets, credentials, or full request/response bodies at `DEBUG` — sizes and headers are
  fine, raw bodies are `DDEBUG` only if truly needed for a specific investigation, and should be
  removed once the bug it was added for is fixed.
- No logging in destructors of objects that are destroyed in bulk (e.g. per-tick cleanup) unless
  gated behind `DDEBUG`.
