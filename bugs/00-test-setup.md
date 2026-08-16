# 00 — Test setup: the configuration and the fixtures the tester needs

Not a bug — the setup every other file assumes. The tester's README makes this
step mandatory: its `EngineX/EngineX.conf` is written in a different dialect
from the one this server parses, so it has to be translated, not copied.

## Directive translation

| Tester config | This server | Note |
|---|---|---|
| `allow_methods GET POST;` | `allowed_methods GET POST;` | |
| `cgi_pass .py python3;` | `cgi_extension .py /usr/bin/python3;` | absolute path: `execve` does not search `PATH` |
| `cgi_pass .bla <prog>;` | `cgi_extension .bla <abs path>;` | |
| `client_max_body_size 100b;` | same | needs [01](config/01-client-max-body-size-parsing.md) + [02](config/02-location-client-max-body-size.md) |
| `return 301 /new-page;` | same | needs [16](config/16-return-directive-ignored.md) |
| `root EngineX/www;` | absolute path | `parse_root()` `stat()`s the value, so it is relative to the working directory |
| `method_redirect POST /directory_post/;` | *no equivalent* | replaced by one location carrying both the index and the CGI extension |
| `client_body_in_file_only`, `delete_files`, `cgi_timeout`, `keep_alive_timeout`, `client_read_timeout`, `types { … }` | *no equivalent* | unknown directives are a fatal parse error, so they must be dropped |

`root` paths below are written for `/home/aazzaoui/webFork`; adjust the prefix
if the repository moves.

## The configuration

Port 1027 is the Minimum Evaluation suite; ports 1025 and 1026 serve the other
suites.

```nginx
server {
    listen 1027;
    host 127.0.0.1;
    server_name localhost;
    root /home/aazzaoui/webFork/EngineX/www-subject-tester;
    index index.htm;
    autoindex on;
    client_max_body_size 200M;

    location / {
        index index.htm;
        allowed_methods GET;
    }

    location /post_body {
        allowed_methods POST;
        client_max_body_size 100b;
    }

    location /directory/ {
        index youpi.bad_extension;
        allowed_methods GET POST;
        cgi_extension .bla /home/aazzaoui/webFork/EngineX/www-subject-tester/cgi_tester;
    }

    location /directory/Yeah {
        root /home/aazzaoui/webFork/EngineX;
        allowed_methods GET;
    }

    location /directory/Yeah/not_happy.bad_extension {
        root /home/aazzaoui/webFork/EngineX/www-subject-tester;
        allowed_methods GET;
    }
}

server {
    listen 1025;
    listen 1026;
    host 127.0.0.1;
    server_name localhost;
    root /home/aazzaoui/webFork/EngineX/www;
    index index.htm;
    autoindex on;
    client_max_body_size 2k;

    location / {
        cgi_extension .py /usr/bin/python3;
    }

    location /upload {
        allowed_methods GET POST DELETE;
    }

    location /get-only {
        allowed_methods GET;
    }

    location /post-only {
        allowed_methods POST;
    }

    location /old-page {
        return 301 /new-page;
    }

    location /temp-page {
        return 302 /index.htm;
    }

    location /ext-redirect {
        return 301 http://example.com;
    }

    location /autoindex-dir {
        autoindex on;
    }

    location /no-index-dir {
        autoindex off;
    }

    location /auto-with-index {
        autoindex on;
        index index.htm;
    }

    location /mapped {
        root /home/aazzaoui/webFork/EngineX/www/root-Test;
        index index.htm;
    }

    location /dir-with-index {
        root /home/aazzaoui/webFork/EngineX/www;
        index index.htm;
    }

    location /cgi-bin {
        cgi_extension .py /usr/bin/python3;
        allowed_methods POST GET;
    }
}
```

Two directives are not needed by any test but were kept in the verified
configuration to prove their fixes work — add them if you want to exercise
[21](config/21-error-page-directive-ignored.md) and
[22](config/22-upload-store-logic-inverted.md):

```nginx
    error_page 404 /my_404.html;        # server scope, alongside client_max_body_size

    location / {
        upload_store /tmp/webserv_uploads;
    }
```

### Why the routes are shaped like this

* **`location /` on 1027 allows GET only** — tests 2 and 3 require `POST /` and
  `HEAD /` to answer 405.
* **`/directory/` carries both `index youpi.bad_extension` and
  `cgi_extension .bla`** — this is how the tester's `method_redirect` pair is
  expressed here. `GET /directory/` serves the index (test 5), and
  `POST /directory/youpi.bla` runs `cgi_tester` (tests 16, 16b, 17, 31).
* **`/directory/Yeah` points at a different root** — test 14 requires 404 for
  `/directory/Yeah`, and the test's own note says "because location has
  different root than `/directory/`". `EngineX/directory/Yeah` does not exist,
  so the lookup fails as intended.
* **`/directory/Yeah/not_happy.bad_extension` points back at the real root** —
  test 15 requires that one file to be served with 200.
* **`/post_body` caps at 100 bytes while the server allows 200 MB** — the
  reason [02](config/02-location-client-max-body-size.md) exists.

## Fixtures

`EngineX/www-subject-tester/` ships complete. `EngineX/www/` does not: several
files the other suites request are missing from the tester repository and have
to be created. They are content-free by design — the assertions are about status
codes and headers.

```bash
WWW=/home/aazzaoui/webFork/EngineX/www

chmod +x "$WWW"/cgi-bin/*.py
chmod +x /home/aazzaoui/webFork/EngineX/www-subject-tester/cgi_tester

printf '<!DOCTYPE html><html><head><title>EngineX</title></head><body><h1>EngineX index</h1></body></html>\n' > "$WWW/index.htm"
printf 'body { color: #0f0; background: #000; }\n' > "$WWW/styles.css"
python3 -c "open('$WWW/large.htm','w').write('<html><body>'+('<p>filler</p>'*30000)+'</body></html>')"
printf '<html><body>my page</body></html>\n' > "$WWW/my page.htm"

mkdir -p "$WWW/empty_dir" "$WWW/upload" "$WWW/forbidden_dir"
printf 'secret\n' > "$WWW/forbidden.html"
printf 'secret\n' > "$WWW/forbidden.txt"
printf 'hidden\n' > "$WWW/forbidden_dir/inside.txt"
chmod 000 "$WWW/forbidden.html" "$WWW/forbidden.txt" "$WWW/forbidden_dir"

cat > "$WWW/cgi-bin/cookie_check.py" <<'PY'
#!/usr/bin/env python3
import os
val = os.environ.get("HTTP_COOKIE", "NO_COOKIE_ENV_VAR")
print("Content-Type: text/plain")
print("Content-Length: " + str(len(val)))
print()
print(val, end="")
PY
chmod +x "$WWW/cgi-bin/cookie_check.py"
```

The permission fixtures only work when the server does **not** run as root —
root ignores the permission bits and the 403 cases silently become 200.

## Running

```bash
cd /home/aazzaoui/webFork
make re
./webserv EngineX/EngineX.conf          # keep it running

cd /home/aazzaoui/Downloads/web-serv-Tester-main
make re
./servTester.out                        # 13 -> Minimum Evaluation, 0 -> run all
```

Non-interactively, the menu can be driven from stdin — `13` picks the suite,
`0` runs all of it, the empty line answers "press Enter", `33` returns and `14`
exits:

```bash
printf '13\n0\n\n33\n14\n' | ./servTester.out
```

Test 31 moves roughly 10 GB in each direction through localhost and takes
several minutes; the whole suite needs about 15 minutes.
