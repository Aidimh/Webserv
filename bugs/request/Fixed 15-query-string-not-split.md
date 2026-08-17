# 15 — `?query` is never split off the request path

**Where:** `src/Request/ClientRequest.cpp` → `RequestLineParser()`

## Symptom

Any URL with a query string 404s:

```
$ curl -s -o /dev/null -w '%{http_code}\n' 'http://127.0.0.1:1025/QueryString/index.htm?foo=bar'
404
```

and `QUERY_STRING` reaches every CGI empty, so scripts that read parameters
(`cgi-bin/query.py`) can never work.

## Cause

`RequestLineParser()` takes everything between the two spaces of the request
line and stores it as `request_path`:

```cpp
request_path = line.substr(first_space + 1, second_space - first_space - 1);
```

`/QueryString/index.htm?foo=bar` is a URI, not a path. `AMethod::resolveTarget()`
then appends the whole thing to the root and `stat()`s a file literally named
`index.htm?foo=bar`.

`CGI::build_env_vars()` acknowledges the gap with a hard-coded
`env_vars.push_back("QUERY_STRING=")`.

## Fix

Splitting the URI is its own step, run right after validation and before
`CleanUri()` collapses duplicate slashes.

```cpp
/*
 * "/cgi-bin/hello.py?name=42" is one token in the request line: the part
 * after '?' is the query string, it is never part of the file path.
 */
void ClientRequest::SplitQueryString(void)
{
    size_t mark = request_path.find('?');

    if (mark == std::string::npos)
    {
        query_string.clear();
        return;
    }
    query_string = request_path.substr(mark + 1);
    request_path = request_path.substr(0, mark);
}
```

Called from `RequestLineParser()`:

```cpp
    if (!RequestLineValidate())
        return;
    SplitQueryString();
    CleanUri();
    DEBUG("ClientRequest") << "RequestLineParser: parsed method=" << method
                           << " path=" << request_path << " version=" << version;
```

`includes/Request/ClientRequest.hpp` gains the member, the accessor and the
declaration:

```cpp
		void										SplitQueryString(void);
    	const std::string&							getQueryString() const;
...
	private:
		std::string									query_string;
```

```cpp
const std::string& ClientRequest::getQueryString() const {return query_string;}
```

and the two functions that maintain request state must carry it:

* `ClientRequest::operator=` — add `query_string = other.query_string;`
  next to `request_path`. Without it a copied request keeps the previous
  query string, and `Client` objects are copied on `accept()`.
* `ClientRequest::reset()` — add `query_string.clear();` next to
  `request_path.clear();`, otherwise the value leaks into the next request on a
  reused connection.

`CGI::build_env_vars()` then exports the real value
([07](../cgi/07-cgi-environment-incomplete.md)):

```cpp
    addEnv("QUERY_STRING", request.getQueryString());
```

## Verification

```
$ curl -s -o /dev/null -w '%{http_code}\n' 'http://127.0.0.1:1025/QueryString/index.htm?foo=bar&baz=qux'
200
$ curl -s 'http://127.0.0.1:1025/cgi-bin/query.py?color=blue&size=large'
color=blue&size=large
```
