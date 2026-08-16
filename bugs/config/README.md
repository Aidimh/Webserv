# config/ — the configuration file

Parsing `webserv.conf` and obeying what it says. Everything here fails before a
single byte of HTTP is exchanged: the server either refuses to start, or starts
with settings the file never asked for.

**Where these live:** `src/ConfFile/`, plus the config structs in
`includes/multiplexing/header.hpp`.

| # | Bug | Breaks |
|---|-----|--------|
| [01](01-client-max-body-size-parsing.md) | `client_max_body_size 1000000;` is rejected, and the unit pointer dangles | server will not start |
| [02](02-location-client-max-body-size.md) | `client_max_body_size` cannot be set per location | 21, 22, 25, 26, 30 |
| [03](03-uninitialised-config-fields.md) | `Location_Config` / `Server_block` fields are read before being written | autoindex, root, index (random) |
| [11](11-max-body-size-first-port-only.md) | Only the first `listen` port of a server is recognised | 2nd port of any server |
| [16](16-return-directive-ignored.md) | `return 301 /x;` fails to parse and does nothing | Redirection suite |
| [21](21-error-page-directive-ignored.md) | `error_page` is parsed and never used | custom error pages |
| [22](22-upload-store-logic-inverted.md) | `upload_store` creates the directory only when it already exists | any config using it |

Start with [01](01-client-max-body-size-parsing.md): `parse_size_in_bytes()` is
written there and [02](02-location-client-max-body-size.md) reuses it.

Back to the [index](../README.md).
