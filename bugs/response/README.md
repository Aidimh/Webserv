# response/ — answering

Which status the server picks, which methods it implements, and how the headers
are spelled. Nothing here is about *finding* the resource — that part works —
only about what is sent back once it has been found or refused.

**Where these live:** `src/Response/`, `src/Routing/`.

| # | Bug | Breaks |
|---|-----|--------|
| [05](05-known-method-must-be-405.md) | `HEAD` is answered 501 instead of 405 | 3 |
| [17](17-head-method-missing.md) | There is no `HEAD` handler | Happy Path suite |
| [18](18-response-header-casing.md) | Header names are spelled inconsistently | header assertions |
| [24](24-post-to-a-directory.md) | `POST` to a directory answers 500 | Client Body Size 2 |
| [25](25-missing-allow-header.md) | A 405 answer omits the required `Allow` header | Method Restriction 5 |

[05](05-known-method-must-be-405.md) and [17](17-head-method-missing.md) are one
story told twice: 405 is the right answer when `HEAD` is *not* allowed on a
route, and it can only be the right answer once the method is implemented at
all.

Back to the [index](../README.md).
