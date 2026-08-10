FOnline ThirdParty pruning notes

This vendored copy is intentionally trimmed for the engine build. When updating
from upstream, remove these paths again after copying the new version.

Removed paths:
- benchmarks/
- CMake/
- data/
- docs/
- examples/
- extras/
- fuzzing/
- src/
- tests/
- third_party/
- tools/
- .bazelrc
- .clang-format
- .clang-tidy
- .conan/
- .gitattributes
- .github/
- .gitignore
- appveyor.yml
- BUILD.bazel
- CMakeLists.txt
- CMakePresets.json
- codecov.yml
- CODE_OF_CONDUCT.md
- conanfile.py
- Doxyfile
- MAINTAINERS.md
- mdsnippets.json
- meson.build
- meson_options.txt
- MODULE.bazel
- SECURITY.md

Retained files: catch_amalgamated.cpp and catch_amalgamated.hpp are the upstream
release assets (also generated into extras/); LICENSE.txt and README.md come from
the source root; CHANGELOG.md is the upstream docs/release-notes.md.
