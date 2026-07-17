FOnline ThirdParty pruning notes

This vendored copy is intentionally trimmed for the engine baker build. The
retained C++ library is also moved out of the upstream CPP/Clipper2Lib/ subtree
to this directory root: CPP/Clipper2Lib/include/ -> include/ and
CPP/Clipper2Lib/src/ -> src/ (upstream root LICENSE and README.md are kept as
is). When updating from upstream, copy the new version, remove the
upstream-relative paths listed below, then reapply that move.

Removed paths:
- CSharp/
- Delphi/
- Tests/
- CPP/BenchMark/
- CPP/DLL/
- CPP/Examples/
- CPP/GoogleTest1/
- CPP/Utils/
- CPP/Clipper2Lib/src/clipper.rectclip.cpp
- CPP/Clipper2Lib/src/clipper.triangulation.cpp
- CPP/Clipper2Lib/CMakeLists.txt
- CPP/Clipper2Lib/clipper2.pc.cmakein
- CPP/Clipper2Lib/clipper2Config.cmake.in
- CPP/Clipper2Lib/clipper2ConfigVersion.cmake.in
- CPP/Clipper2Lib/clipper2.version.in
- CPP/CMakeLists.txt
- CPP/clipper.version.in
- CPP/clipper2.pc.cmakein
- CPP/clipper2Config.cmake.in
- CPP/clipper2ConfigVersion.cmake.in
- .github/
- .gitignore
- .gitmodules
