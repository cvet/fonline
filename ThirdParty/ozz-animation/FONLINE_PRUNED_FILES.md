FOnline ThirdParty pruning notes

This vendored copy is based on the upstream ozz-animation 0.16.0 release tag
(commit 6cbdc790123aa4731d82e255df187b3a8a808256) and is intentionally trimmed
for the engine build. When updating from upstream, remove these paths again
after copying the new version.

Removed paths:
- .github/
- .vscode/
- build-utils/
- extern/
- howtos/
- media/
- samples/
- test/
- include/ozz/animation/offline/fbx/
- include/ozz/animation/offline/tools/
- src/animation/offline/fbx/
- src/animation/offline/gltf/
- src/animation/offline/tools/
- src/geometry/
- src/options/
- .clang-format
- .gitattributes
- .gitignore
- CMakeLists.txt
- CONTRIBUTING.md
- dashboard.html
- documentation.html

Upstream CMake files below retained source directories are also removed. The
engine builds the pinned source lists from BuildTools/cmake/stages/ThirdParty.cmake
and does not consume upstream CMake targets.
