# 30 — There is no session support

**Where:** new `includes/Session/SessionManager.hpp`, `src/Session/SessionManager.cpp`,
wired in `src/Multiplexing/Engine.cpp` → `prepareResponse()`

## Symptom

The whole Session suite fails:

```
✘ FAIL  1 » Set Cookie On First Visit Test   (missing: Set-Cookie:)
✘ FAIL  2 » Cookie Value Not Empty Test      (missing: Set-Cookie: SESSIONID=…)
✘ FAIL  4 » Invalid Session New Cookie Test  (missing: Set-Cookie:)
```

No response ever carries a `Set-Cookie` header, so a client has no identity to
send back and tests 3 and 5 have nothing to work with either.

## Cause

Not a broken function — a missing one. `struct Client` even reserves the field:

```cpp
    std::string session_id;
```

and nothing ever writes it. The subject asks for cookies and session
management; the request side works (`Cookie:` is parsed into the header map and,
after [07](../cgi/07-cgi-environment-incomplete.md), reaches a CGI as `HTTP_COOKIE`),
but the server never issues an identity of its own.

## Fix

The smallest thing that is actually a session: hand out an id, remember which
ids were handed out, and re-issue when a client presents one that was not.

Remembering matters — without it, a client could invent any id and the server
would accept it, which is exactly what test 4 checks.

### `includes/Session/SessionManager.hpp` (new file)

```cpp
#ifndef SESSIONMANAGER_HPP
#define SESSIONMANAGER_HPP

#include <map>
#include <string>
#include <ctime>

class ClientRequest;

/*
 * Minimal session tracking: the server hands every visitor an id in a cookie
 * and remembers the ids it issued, so it can tell a returning visitor from a
 * client that made one up.
 */
class SessionManager
{
    public:
        static const std::string&   cookieName();
        static std::string          readCookie(const ClientRequest& request);
        static bool                 isKnown(const std::string& id);
        static std::string          create();
        static std::string          buildSetCookieHeader(const std::string& id);

    private:
        static std::map<std::string, time_t> _sessions;
        static std::string          generateId();
        static void                 forgetExpired();
};

#endif
```

### `src/Session/SessionManager.cpp` (new file)

```cpp
#include "SessionManager.hpp"
#include "../../includes/Request/ClientRequest.hpp"
#include "../Logging/Logging.hpp"

#include <cstdlib>
#include <sstream>
#include <unistd.h>

/* A session is forgotten after this long without being seen. */
static const time_t SESSION_LIFETIME = 3600;
/* Above this many live sessions, expired ones are swept before adding more. */
static const size_t SESSION_SWEEP_THRESHOLD = 10000;

std::map<std::string, time_t> SessionManager::_sessions;

const std::string& SessionManager::cookieName()
{
    static const std::string name = "SESSIONID";

    return name;
}
```

```cpp
/*
 * "Cookie: theme=dark; SESSIONID=abc; lang=en" -> "abc"
 */
std::string SessionManager::readCookie(const ClientRequest& request)
{
    std::map<std::string, std::string>::const_iterator it =
        request.getHeaders().find("cookie");

    if (it == request.getHeaders().end())
        return "";

    const std::string& header = it->second;
    const std::string needle = cookieName() + "=";
    size_t start = 0;

    while (start < header.size())
    {
        size_t end = header.find(';', start);
        std::string pair = header.substr(start, end - start);
        size_t begin = pair.find_first_not_of(" \t");

        if (begin != std::string::npos)
        {
            pair = pair.substr(begin);
            if (pair.compare(0, needle.size(), needle) == 0)
                return pair.substr(needle.size());
        }
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return "";
}
```

```cpp
bool SessionManager::isKnown(const std::string& id)
{
    std::map<std::string, time_t>::iterator it = _sessions.find(id);

    if (it == _sessions.end())
        return false;
    if (time(NULL) - it->second > SESSION_LIFETIME)
    {
        _sessions.erase(it);
        return false;
    }
    it->second = time(NULL);
    return true;
}
```

```cpp
void SessionManager::forgetExpired()
{
    std::map<std::string, time_t>::iterator it = _sessions.begin();
    time_t now = time(NULL);

    while (it != _sessions.end())
    {
        if (now - it->second > SESSION_LIFETIME)
            _sessions.erase(it++);
        else
            ++it;
    }
}
```

```cpp
std::string SessionManager::generateId()
{
    static unsigned long counter = 0;
    std::ostringstream id;

    id << std::hex << static_cast<unsigned long>(time(NULL))
       << "-" << static_cast<unsigned long>(getpid())
       << "-" << ++counter
       << "-" << (rand() % 0xffffff);
    return id.str();
}
```

```cpp
std::string SessionManager::create()
{
    if (_sessions.size() >= SESSION_SWEEP_THRESHOLD)
        forgetExpired();

    std::string id = generateId();

    _sessions[id] = time(NULL);
    DEBUG("Session") << "create: issued session id=" << id
                     << " live sessions=" << _sessions.size();
    return id;
}
```

```cpp
std::string SessionManager::buildSetCookieHeader(const std::string& id)
{
    return cookieName() + "=" + id + "; Path=/; HttpOnly";
}
```

The store is bounded: the sweep runs before growing past the threshold, so a
stress run that opens 100 000 connections cannot grow it without limit. The
counter and pid in the id make collisions impossible within a run; `rand()`
only makes an id hard to guess by hand. A session cookie that must survive a
hostile client needs a CSPRNG (`/dev/urandom`) — worth doing before this is
used for anything that matters.

### Wiring — `src/Multiplexing/Engine.cpp`

```cpp
/*
 * Every visitor gets a session id. A cookie the server never issued is not a
 * session, so it is replaced instead of being trusted.
 */
void Multiplexer::assignSession(Client &client)
{
    std::string id = SessionManager::readCookie(client.parsed_request);

    if (!id.empty() && SessionManager::isKnown(id))
    {
        client.session_id = id;
        client.session_is_new = false;
        return;
    }
    client.session_id = SessionManager::create();
    client.session_is_new = true;
}
```

`prepareResponse()` calls it before dispatching and adds the header only when
the session is new — re-sending the same cookie on every request is noise:

```cpp
    assignSession(client);

    Response response = Dispatcher::dispatch(client, server);

    client.response_prepared = true;
    if (!response.isCGI())
    {
        if (client.session_is_new)
            response.addHeader("Set-Cookie", SessionManager::buildSetCookieHeader(client.session_id));
        ...
```

A CGI answer needs the same header, and its head is built separately
([14](../cgi/14-cgi-output-is-not-http.md)), so `buildCgiResponseHead()` takes the
extra headers as a parameter:

```cpp
/* The session cookie has to reach the client whoever produced the body. */
static std::string sessionHeader(const Client& client)
{
    if (!client.session_is_new)
        return "";
    return "Set-Cookie: " + SessionManager::buildSetCookieHeader(client.session_id) + "\r\n";
}
```

```cpp
static std::string buildCgiResponseHead(const std::string& cgiHeaderBlock,
                                        const std::string& extraHeaders)
{
    std::vector<std::string> lines = splitHeaderLines(cgiHeaderBlock);
    std::string head;

    head += "HTTP/1.1 " + cgiStatusLine(lines) + "\r\n";
    head += cgiForwardedHeaders(lines);
    head += extraHeaders;
    head += "Transfer-Encoding: chunked\r\n";
    head += "Connection: close\r\n";
    head += "\r\n";
    return head;
}
```

### `struct Client` and the build

```cpp
    std::string session_id;
    bool session_is_new;
```

initialised to `false` in the constructor (keep the initialiser list in
declaration order or `-Werror=reorder` will stop the build).

`Makefile`:

```make
	-Iincludes/Session \
	...
	src/Session/SessionManager.cpp \
```

## Verification

```
$ curl -s -i http://127.0.0.1:1025/ | grep -i set-cookie
Set-Cookie: SESSIONID=6a8227f0-114d59-2-7b23f8; Path=/; HttpOnly

$ curl -s -i -H 'Cookie: SESSIONID=6a8227f0-114d59-2-7b23f8' http://127.0.0.1:1025/ | grep -ci set-cookie
0                                    # known session: no new cookie

$ curl -s -i -H 'Cookie: SESSIONID=not-real' http://127.0.0.1:1025/ | grep -ci set-cookie
1                                    # invented session: replaced

$ curl -s -H 'Cookie: SESSIONID=6a8227f0-114d59-2-7b23f8' http://127.0.0.1:1025/cgi-bin/cookie_check.py
SESSIONID=6a8227f0-114d59-2-7b23f8   # the CGI sees it in HTTP_COOKIE
```

Tester:

```
Session Tests
✔ PASS  1 » Set Cookie On First Visit Test
✔ PASS  2 » Cookie Value Not Empty Test
✔ PASS  3 » HTTP Cookie Env Var Test
✔ PASS  4 » Invalid Session New Cookie Test
✔ PASS  Session Data Stored Test
Total: 5    Passed: 5    Failed: 0
```
