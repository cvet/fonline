/* config.h -- FOnline configuration of the vendored copy.
   Upstream generates this file from config.h.in with configure; the engine builds libbacktrace with CMake on
   Linux only (ELF, glibc), so it carries what configure detects there, less the checks only the tests use.  */

#define BACKTRACE_ELF_SIZE 64
#define BACKTRACE_XCOFF_SIZE unused
#define HAVE_ATOMIC_FUNCTIONS 1
#define HAVE_SYNC_FUNCTIONS 1
#define HAVE_DECL_GETPAGESIZE 1
#define HAVE_DECL_STRNLEN 1
#define HAVE_DECL__PGMPTR 0
#define HAVE_DLFCN_H 1
#define HAVE_DL_ITERATE_PHDR 1
#define HAVE_FCNTL 1
#define HAVE_LINK_H 1
#define HAVE_LSTAT 1
#define HAVE_READLINK 1
#define HAVE_SYS_MMAN_H 1
#define HAVE_UNISTD_H 1

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
