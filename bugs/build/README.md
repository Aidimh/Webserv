# build/ — build system

Not a runtime bug, and the most dangerous file in the folder: it is the reason a
fix can look wrong when it is right.

**Where this lives:** `Makefile`.

| # | Bug | Breaks |
|---|-----|--------|
| [19](19-makefile-header-dependencies.md) | Editing a header does not rebuild the objects that include it | silent memory corruption |

Several fixes here change structs in `includes/multiplexing/header.hpp`
(`Client`, `CgiState`, `Location_Config`). Without header dependencies, half the
objects are compiled against the old layout and half against the new one, and
the resulting binary misbehaves in ways that have nothing to do with the fix
being tested. Apply this one **first**, or `make re` after every edit.

Back to the [index](../README.md).
