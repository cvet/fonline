FOnline ThirdParty pruning notes

This vendored copy is intentionally trimmed for the engine build. When updating
from upstream, remove these paths again after copying the new version.

Removed paths:
- .github/
- bindgen/
- data/
- examples/
- extra/
- test/
- .gitattributes
- .gitignore
- misc/cmake/
- misc/deflate_benchmark/
- misc/deflate_testcases/
- misc/ufbx_testcases/
- misc/users/
- misc/wasm_hasher/
- misc/*.c
- misc/*.cpp
- misc/*.h
- misc/*.hpp
- misc/*.py
- misc/*.sh

Retained files: ufbx.c, ufbx.h, misc/ufbx.natvis, misc/ufbxi.natvis, LICENSE, README.md.
