FOnline ThirdParty pruning notes

This vendored copy is libbacktrace from https://github.com/ianlancetaylor/libbacktrace. The project publishes no
releases and GCC imports it from the branch head, so the copy tracks `master` (commit
0b9b49cf4a2c9229fc052d6716e1528b2f23e91a). The engine uses it on Linux only, to turn native addresses into function,
file and line through `backtrace_pcinfo` / `backtrace_syminfo`; unwinding stays with the bundled LLVM libunwind, so the
unwinding and printing sources stay out. When updating from upstream, keep only the files listed below.

Kept paths:
- LICENSE
- README.md
- backtrace.h
- internal.h
- filenames.h
- atomic.c
- dwarf.c
- elf.c
- fileline.c
- mmap.c
- mmapio.c
- posix.c
- sort.c
- state.c

Removed paths:
- config/
- aclocal.m4, compile, config.guess, config.sub, configure, configure.ac, filetype.awk, install-sh, ltmain.sh,
  Makefile.am, Makefile.in, missing, move-if-change, test-driver, .gitignore
- config.h.in, backtrace-supported.h.in, install-debuginfo-for-buildid.sh.in
- alloc.c, read.c (the malloc and read fallbacks of mmap.c and mmapio.c)
- backtrace.c, simple.c, nounwind.c, print.c (unwinding and printing)
- macho.c, pecoff.c, xcoff.c, unknown.c (other object formats)
- allocfail.c, allocfail.sh, btest.c, edtest.c, edtest2.c, instrumented_alloc.c, mdtest.c, mtest.c, stest.c,
  strippedtest.c, test_format.c, testlib.c, testlib.h, ttest.c, unittest.c, xztest.c, zstdtest.c, ztest.c,
  Isaac.Newton-Opticks.txt (tests)

Added files (FOnline):
- config.h and backtrace-supported.h stand in for the files upstream generates with configure. They carry what
  configure detects on Linux with glibc, less the checks only the tests use (zlib, zstd, liblzma: elf.c decompresses
  compressed debug sections and MiniDebugInfo with its own code).

Allocator hook: none. mmap.c allocates with mmap directly, which keeps symbol resolution usable from a crash handler;
the memory stays with the process-lifetime state the engine creates once.
