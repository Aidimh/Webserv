# 24 — `POST` to a directory answers 500

**Where:** `src/Response/POST.cpp` → `handleRegularRequest()`

## Symptom

```
$ curl -s -o /dev/null -w '%{http_code}\n' -X POST --data-binary @file.bin http://127.0.0.1:1025/upload/
500
```

Client Body Size test 2 fails with `(missing: HTTP/1.1 201 Created)`, and every
upload endpoint written the normal way — `POST` to the *directory*, letting the
server name the file — is unusable.

## Cause

The target of `POST /upload/` resolves to the directory `<root>/upload/`, and
the handler treats it as a file name:

```cpp
    if (!saveBody(target, request.getBody()))
        return buildErrorResponse(500, "Internal Server Error");
```

`std::ofstream` on a directory path fails, `saveBody()` returns false, and the
client gets 500 for a perfectly valid request. The streaming branch fails the
same way: `rename(tmp, "<root>/upload/")` returns `EISDIR`.

The parent-directory checks above it do not catch the case either — the parent
of `<root>/upload/` is `<root>/upload`, which exists and is writable, so
everything looks fine right up to the write.

## Fix

The request says *where* to store the body; when that is a directory the server
supplies the name. The old 60-line function is split into the four things it
was doing: choosing the destination, validating it, and the two ways of writing
it out.

```cpp
static unsigned long nextUploadId()
{
    static unsigned long counter = 0;

    return ++counter;
}
```

```cpp
/*
 * A POST whose target is a directory means "store the body in here". The
 * request carries no file name, so one is generated.
 */
std::string POST::resolveDestination(const std::string& target) const
{
    if (getPathType(target) != DIRECTORY_PATH)
        return target;

    std::ostringstream path;

    path << target;
    if (target.empty() || target[target.size() - 1] != '/')
        path << "/";
    path << "upload_" << static_cast<long>(time(NULL)) << "_" << nextUploadId();
    return path.str();
}
```

```cpp
bool POST::destinationIsUsable(const std::string& destination, Response& failure) const
{
    PathType parent = validateParentDirectory(destination);

    if (parent == NOT_FOUND)
    {
        DEBUG("POST") << "destinationIsUsable: parent directory missing, responding status=404 target=" << destination;
        failure = buildErrorResponse(404, "Not Found");
        return false;
    }
    if (parent == PERMISSION_DENIED)
    {
        DEBUG("POST") << "destinationIsUsable: parent directory not readable, responding status=403 target=" << destination;
        failure = buildErrorResponse(403, "Forbidden");
        return false;
    }
    if (parent != DIRECTORY_PATH)
    {
        DEBUG("POST") << "destinationIsUsable: parent is not a directory, responding status=400 target=" << destination;
        failure = buildErrorResponse(400, "Bad Request");
        return false;
    }
    if (!canWrite(destination))
    {
        DEBUG("POST") << "destinationIsUsable: parent directory not writable, responding status=403 target=" << destination;
        failure = buildErrorResponse(403, "Forbidden");
        return false;
    }
    return true;
}
```

```cpp
/* Body already on disk: rename it into place and stream it back from there. */
Response POST::storeStreamedBody(Client& client, const std::string& destination) const
{
    const ClientRequest& request = client.parsed_request;
    struct stat info;

    if (stat(request.getTmpFilePath().c_str(), &info) != 0)
    {
        ERR() << "POST::storeStreamedBody: stat failed on temp file path="
              << request.getTmpFilePath() << ": " << strerror(errno);
        return buildErrorResponse(500, "Internal Server Error");
    }
    if (rename(request.getTmpFilePath().c_str(), destination.c_str()) != 0)
    {
        ERR() << "POST::storeStreamedBody: rename failed from=" << request.getTmpFilePath()
              << " to=" << destination << ": " << strerror(errno);
        return buildErrorResponse(500, "Internal Server Error");
    }

    int fd = open(destination.c_str(), O_RDONLY);
    if (fd == -1)
    {
        ERR() << "POST::storeStreamedBody: open target failed path=" << destination
              << ": " << strerror(errno);
        return buildErrorResponse(500, "Internal Server Error");
    }
    DEBUG("POST") << "storeStreamedBody: stored " << info.st_size
                  << " bytes at target=" << destination << " stream fd=" << fd;
    client.stream_file_fd = fd;
    client.stream_bytes_remaining = info.st_size;

    Response response;
    std::ostringstream length;

    length << info.st_size;
    response.setStatusCode(201);
    response.setReasonPhrase("Created");
    response.addHeader("Content-Length", length.str());
    response.addHeader("Content-Type", "text/plain");
    response.setResponseMode(Response::STREAMING_RESPONSE);
    return response;
}
```

```cpp
/* Small body still in RAM: write it out and echo it back. */
Response POST::storeMemoryBody(const Client& client, const std::string& destination) const
{
    const ClientRequest& request = client.parsed_request;

    if (!saveBody(destination, request.getBody()))
        return buildErrorResponse(500, "Internal Server Error");

    Response response;

    response.setStatusCode(201);
    response.setReasonPhrase("Created");
    response.setBody(request.getBody());
    response.addHeader("Content-Type", "text/plain");
    INFO() << "POST::storeMemoryBody: created target=" << destination
           << " size=" << request.getBody().size() << " bytes";
    return response;
}
```

```cpp
Response POST::handleRegularRequest(Client& client, const std::string& target)
{
    std::string destination = resolveDestination(target);
    Response failure;

    if (!destinationIsUsable(destination, failure))
        return failure;
    if (client.parsed_request.usesTmpFile())
        return storeStreamedBody(client, destination);
    return storeMemoryBody(client, destination);
}
```

Declarations to add to `includes/Methods/POST.hpp`:

```cpp
        std::string resolveDestination(const std::string& target) const;
        bool destinationIsUsable(const std::string& destination, Response& failure) const;
        Response storeStreamedBody(Client& client, const std::string& destination) const;
        Response storeMemoryBody(const Client& client, const std::string& destination) const;
```

The commented-out earlier version of `handleRegularRequest` still sitting below
it in the file can go with it.

## Verification

```
$ curl -s -o /dev/null -w '%{http_code}\n' -X POST --data-binary "$(python3 -c 'print("A"*2048,end="")')" http://127.0.0.1:1025/upload/
201
$ curl -s -o /dev/null -w '%{http_code}\n' -X POST --data-binary "$(python3 -c 'print("A"*2049,end="")')" http://127.0.0.1:1025/upload/
413
$ ls EngineX/www/upload
upload_1786913409_1
```

Named targets still work, so `POST /upload/delete_me.txt` followed by
`DELETE /upload/delete_me.txt` is unaffected (201 then 204).
