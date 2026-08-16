# 11 — Only the first `listen` port of a server is recognised

**Where:** `src/Request/ClientRequest.cpp` → `getServerMaxBodySize()`

## Symptom

A server block with two ports applies its `client_max_body_size` on the first
one only:

```nginx
server {
    listen 1025;
    listen 1026;                 # requests here get the 1 MB default instead
    client_max_body_size 2k;
}
```

A request on port 1026 that should be rejected with 413 is accepted, and one
that should be accepted can be rejected — the limit comes from an unrelated
server block, or from the fallback default.

## Cause

```cpp
for (i = 0; i < Conf_File::Servers.size(); i++)
{
    if (Conf_File::Servers[i].listen_port[0] == client.port)   // [0] only
        break;
}
```

`listen_port` is a vector precisely because a server may listen on several
ports — `which_server()` in `Engine.cpp` walks all of them. This function looks
at the first and gives up.

`listen_port[0]` is also an unchecked index: a server block with no `listen`
would read past the end. `every_server_has_listen_port()` rejects that
configuration at start-up, so it cannot happen today, but the function does not
guarantee it locally.

Then:

```cpp
size_t founded = Conf_File::Servers[i].max_body_size;
if (Conf_File::Servers[i].body_size_is_MB)  founded *= (1024 * 1024);
else if (Conf_File::Servers[i].body_size_is_KB) founded *= 1024;
```

Those two flags are never assigned anywhere in the project and are not
initialised either ([03](03-uninitialised-config-fields.md)), so the configured
limit is multiplied by 1, 1024 or 1 048 576 depending on stack contents. Since
[01](01-client-max-body-size-parsing.md) the stored value is already in bytes,
so the rescaling has to go.

## Fix

Finding the server is one job, choosing the limit is another.

```cpp
static const Server_block* findServerByPort(int port)
{
    for (size_t i = 0; i < Conf_File::Servers.size(); i++)
    {
        const Server_block& server = Conf_File::Servers[i];

        for (size_t j = 0; j < server.listen_port.size(); j++)
        {
            if (server.listen_port[j] == port)
                return &server;
        }
    }
    return NULL;
}
```

```cpp
size_t  ClientRequest::getServerMaxBodySize(Client& client)
{
    const Server_block* server = findServerByPort(client.port);

    if (server == NULL)
    {
        DDEBUG("ClientRequest") << "getServerMaxBodySize: no server listens on port=" << client.port
                                << ", using default=" << DEFAULT_MAX_BODY_SIZE << " bytes";
        return (DEFAULT_MAX_BODY_SIZE);
    }

    const Location_Config* location = Router::resolveLocation(request_path, *server);

    if (location != NULL && location->has_max_body_size)
    {
        DDEBUG("ClientRequest") << "getServerMaxBodySize: location=" << location->path
                                << " max_body_size=" << location->max_body_size << " bytes";
        return (static_cast<size_t>(location->max_body_size));
    }

    if (!server->client_max_body_found)
    {
        DDEBUG("ClientRequest") << "getServerMaxBodySize: no limit configured for port=" << client.port
                                << ", using default=" << DEFAULT_MAX_BODY_SIZE << " bytes";
        return (DEFAULT_MAX_BODY_SIZE);
    }

    DDEBUG("ClientRequest") << "getServerMaxBodySize: port=" << client.port
                            << " max_body_size=" << server->max_body_size << " bytes";
    return (static_cast<size_t>(server->max_body_size));
}
```

Resolution order is now the usual one: **location → server → default**, which is
what makes [02](02-location-client-max-body-size.md) work. The magic number
`1048576` becomes a named constant in `includes/multiplexing/header.hpp`:

```cpp
#define DEFAULT_MAX_BODY_SIZE 1048576
```

`src/Request/ClientRequest.cpp` needs the router:

```cpp
#include "../../includes/Routing/Router.hpp"
```

## Verification

```
$ curl -s -o /dev/null -w '%{http_code}\n' -X POST --data-binary @3k.bin http://127.0.0.1:1025/  # 2k limit
413
$ curl -s -o /dev/null -w '%{http_code}\n' -X POST --data-binary @3k.bin http://127.0.0.1:1026/  # same block
413
```

Before the fix the second one answered `201`.
