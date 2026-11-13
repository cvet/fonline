// FOnline Engine
// SPDX-License-Identifier: MIT

// Coverage applications finish through quick_exit, which does not run the
// compiler's atexit writer
#include <stdlib.h>

#if FO_CODE_COVERAGE_LLVM
extern int __llvm_profile_write_file(void);
#elif FO_CODE_COVERAGE_GCC
extern void __gcov_dump(void);
#else
#error A compiler coverage backend is required
#endif

static void FlushCoverageAtQuickExit(void) {
#if FO_CODE_COVERAGE_LLVM
  if (__llvm_profile_write_file() != 0) {
    _Exit(EXIT_FAILURE);
  }
#else
  __gcov_dump();
#endif
}

__attribute__((constructor)) static void RegisterCoverageQuickExit(void) {
  if (at_quick_exit(FlushCoverageAtQuickExit) != 0) {
    abort();
  }
}
