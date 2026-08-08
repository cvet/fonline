FOnline ThirdParty pruning notes

This vendored copy is intentionally trimmed for the engine build. When updating
from upstream, remove these paths again after copying the new version.

Removed paths:
- .bcr/
- .github/
- bazel/
- doc/
- examples/
- tools/
- src/ftxui/**/*_test.cpp
- src/ftxui/**/*_fuzzer.cpp
- .bazelrc
- .clang-format
- .clang-tidy
- .gitignore
- BUILD.bazel
- MODULE.bazel
- WORKSPACE.bazel
- codecov.yml
- flake.lock
- flake.nix
- ftxui.pc.in
- iwyu.imp

Retained: CHANGELOG.md, LICENSE, README.md, CMakeLists.txt (with FOnline Patch
guards around the pruned examples/ and doc/ subdirectories), cmake/, include/,
and the non-test sources under src/. The cmake/ helpers self-guard on the
FTXUI_BUILD_* / FTXUI_ENABLE_INSTALL options the engine sets to OFF in
BuildTools/cmake/stages/ThirdParty.cmake.
