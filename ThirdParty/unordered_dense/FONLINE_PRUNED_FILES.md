FOnline ThirdParty pruning notes

This vendored copy is intentionally trimmed for the engine build. When updating
from upstream, remove these paths again after copying the new version.

Removed paths:
- .github/
- cmake/
- data/
- doc/
- example/
- handoff/
- scripts/
- src/
- subprojects/
- test/
- .clang-format
- .gitignore
- CLAUDE.md
- CMakeLists.txt
- CODE_OF_CONDUCT.md
- CONTRIBUTING.md
- meson.build
- meson_options.txt
- requirements.txt
- xmake.lua

Retained files: `.clang-tidy`, `.fuzz-corpus-base-dir`, `include/ankerl/stl.h`,
`include/ankerl/unordered_dense.h`, `LICENSE`, and `README.md`.
