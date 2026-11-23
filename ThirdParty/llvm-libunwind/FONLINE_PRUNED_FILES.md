FOnline ThirdParty pruning notes

This vendored copy is the `libunwind/` directory of llvm-project, trimmed for the engine build. The engine uses it
only for the local unwind API on Linux - `Source/Essentials/StackTrace.cpp` walks native stacks with it, from the
current frame, from a crash handler's signal context, and from the register context a script entry saved - so the
`_Unwind_*` level-1 sources stay out: linking them would replace the unwinder C++ exceptions already use. When
updating from upstream, remove these paths again after copying the new version.

Removed paths:
- cmake/
- docs/
- test/
- CMakeLists.txt
- include/CMakeLists.txt
- include/libunwind.modulemap
- src/CMakeLists.txt
- src/Unwind-EHABI.cpp
- src/Unwind-seh.cpp
- src/Unwind-sjlj.c
- src/Unwind-wasm.c
- src/Unwind_AIXExtras.cpp
- src/UnwindLevel1.c
- src/UnwindLevel1-gcc-ext.c

Allocator hook: none. The library allocates only in `DwarfFDECache` when frames are registered at runtime through
`__register_frame`, which the engine never calls.

Include path: `include/` is offered only to `Source/Essentials/StackTrace.cpp` (`BuildTools/cmake/stages/ThirdParty.cmake`),
because it also carries an `unwind.h` that would shadow the compiler's for every other library.
