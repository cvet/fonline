FOnline ThirdParty pruning notes

This vendored copy is intentionally trimmed for the engine build. When updating
from upstream, remove these paths again after copying the new version.

Removed paths:
- benchmarks/
- docs/
- examples/
- extras/
- fuzzing/
- projects/
- src/
- tests/
- tools/
- .github/
- .reuse/
- .clang-format
- .cmake-format.py
- .editorconfig
- .git-blame-ignore-revs
- .gitattributes
- .gitignore
- .mailmap
- appveyor.yml
- CMakeLists.txt
- CODE_OF_CONDUCT.md
- conanfile.py
- meson.build
- meson_options.txt
- pyproject.toml
- README.android
- SECURITY.md
- vcpkg.json

Retained files: catch_amalgamated.cpp, catch_amalgamated.hpp, LICENSE.txt,
README.md, CHANGELOG.md.
