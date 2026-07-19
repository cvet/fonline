FOnline ThirdParty pruning notes

This vendored copy is based on the upstream meshoptimizer v1.2 release tag
(commit 9d9890c73011d75920af614485296d1e03e95448) and is intentionally trimmed
for the engine build. When updating from upstream, remove these paths again
after copying the new version.

Removed paths:
- .github/
- demo/
- extern/
- gltf/
- js/
- tools/
- .clang-format
- .editorconfig
- .git-blame-ignore-revs
- .gitignore
- .prettierrc
- CMakeLists.txt
- CONTRIBUTING.md
- Makefile

The complete upstream core source directory is retained. The engine currently
builds only the allocator, vertex-cache optimizer, and vertex-fetch optimizer
translation units through BuildTools/cmake/stages/ThirdParty.cmake.
