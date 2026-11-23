FOnline ThirdParty pruning notes

This vendored copy is intentionally trimmed for the engine build. When updating
from upstream, remove these paths again after copying the new version.

Upstream ships this as the "amalgamation" package: the whole library as one
`sqlite3.c` plus its public headers, so no generation step is needed.

Removed paths:
- shell.c

`shell.c` is the standalone `sqlite3` command-line tool. The engine links only
the library; use an upstream build if the CLI is needed for inspecting a
database file.

Retained files: sqlite3.c, sqlite3.h, sqlite3ext.h.

SQLite is released into the public domain, so there is no license file to keep;
the dedication is stated in the header of `sqlite3.c`.

Build configuration lives in `BuildTools/cmake/stages/ThirdParty.cmake` rather
than in patched sources — the engine compiles the amalgamation with feature
switches (threading mode, omitted subsystems, and the engine allocator hook)
instead of editing the vendored file, so this copy stays byte-identical to
upstream and carries no `(FOnline Patch)` markers.
