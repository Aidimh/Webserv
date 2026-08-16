# 20 — Every uploaded body is left behind in `www/upload/`

**Where:** `src/Request/ClientRequest.cpp` → `openTempFile()`

## Symptom

`www/upload/` fills up with one file per request that had a body larger than
8 bytes:

```
$ ls www/upload | head
storage_10
storage_11
storage_12
...
```

Nothing ever removes them. Minimum Evaluation test 31 alone (20 forks × 5
requests × 100 MB) writes 10 GB through this directory; with the files kept, it
fills the disk and the run dies with `write to temp file failed: No space left
on device`.

The files are also written under the **served web root**, so a body uploaded to
one route becomes downloadable at `/upload/storage_<fd>` from another.

## Cause

```cpp
std::string FilePath = "www/upload/storage_" + intToString(ClientFd);
TmpFilePath = FilePath;

int fd = open(FilePath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
```

The name is derived from the client descriptor, so it is unique among *live*
connections but reused as soon as the descriptor number is. There is no
`unlink()` anywhere in the project: `ClientRequest::reset()` closes the
descriptor and clears the path, and `~ClientRequest()` closes the descriptor —
neither deletes the file.

Only one path ever consumes the file: `POST::handleRegularRequest()` renames it
onto the target. Every other body — CGI requests, rejected requests, requests
whose handler never reads the body — leaves it behind.

The path is also relative to the **current working directory**, not to the
configured root, so `./webserv` started from another directory creates a
`www/upload/` tree wherever it happens to be.

## Fix

Two complementary changes.

### 1. The consumer unlinks as soon as it has the descriptor

This is where the 10 GB goes: [08](../cgi/08-cgi-body-lost-and-deadlock.md) opens the
body for the CGI and immediately drops the name, so the data lives only as long
as the descriptor and the space is reclaimed the moment the request ends.

```cpp
    client.cgi.body_fd = open(request.getTmpFilePath().c_str(), O_RDONLY);
    if (client.cgi.body_fd == -1)
    {
        ERR() << "Multiplexer::openCgiBodySource: open body file failed path="
              << request.getTmpFilePath() << ": " << strerror(errno);
        return false;
    }
    /* the fd keeps the data alive, the name is not needed any more */
    unlink(request.getTmpFilePath().c_str());
```

### 2. The owner cleans up whatever is left

`ClientRequest` created the file, so `ClientRequest` removes it — on reset and
on destruction, which covers rejected requests, dropped connections and handlers
that never touched the body.

```cpp
void ClientRequest::removeTempFile()
{
    if (TmpFileFd != -1)
    {
        DEBUG("ClientRequest") << "removeTempFile: closed temp file fd=" << TmpFileFd;
        close(TmpFileFd);
        TmpFileFd = -1;
    }
    if (TmpFilePath.empty())
        return;
    if (unlink(TmpFilePath.c_str()) == 0)
        DEBUG("ClientRequest") << "removeTempFile: deleted " << TmpFilePath;
    TmpFilePath.clear();
}
```

```cpp
void ClientRequest::reset()
{
    state = HEADERS;
    method.clear();
    request_path.clear();
    query_string.clear();
    cgi_extension.clear();
    version.clear();
    headers.clear();
    cgi.clear();
    body.clear();
    status_code = 200;
    removeTempFile();
    BodySize = 0;
    ContentLength = 0;
    HasContentLength = false;
    HasTransferEncoding = false;
}
```

```cpp
ClientRequest::~ClientRequest()
{
	removeTempFile();
}
```

Declared in `includes/Request/ClientRequest.hpp`:

```cpp
		void										removeTempFile();
```

A failing `unlink` is not reported as an error: the CGI path deletes the name
first on purpose, so "already gone" is the expected outcome there.

`POST::handleRegularRequest()` needs no change — `rename()` moves the file out
of the way, so the later `unlink()` simply finds nothing.

### 3. Where the file is created

```cpp
bool ClientRequest::openTempFile(int ClientFd)
{
	struct stat meta;

	if (stat("www", &meta) != 0)
		mkdir("www", 0755);
	if (stat("www/upload", &meta) != 0)
		mkdir("www/upload", 0755);

	std::string FilePath = "www/upload/storage_" + intToString(ClientFd);
	...
```

Left as-is here because changing it is a design decision rather than a bug fix,
but it is worth doing: request bodies are private data and do not belong under a
directory the server also serves. `/tmp/webserv_body_<pid>_<fd>` — or a
`client_body_temp_path` directive — would be the usual answer, and with the
`unlink()` above the file is invisible in the filesystem for all but a few
microseconds anyway.

## Verification

```
$ df -h /home | tail -1        # before test 31
/dev/mapper/rl-home  30G  26G  4.6G  85% /home
$ df -h /home | tail -1        # after test 31 (10 GB streamed through)
/dev/mapper/rl-home  30G  26G  4.6G  85% /home
$ ls www/upload
$
```
