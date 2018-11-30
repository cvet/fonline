/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Whether Android NDK unified headers are used */
/* #undef ANDROID_UNIFIED_HEADERS */

/* bind with unsigned addrlen */
/* #undef BIND_ADDRLEN_UNSIGNED */

/* GC description */
#define DEFAULT_GC_NAME "Included Boehm (with typed GC and Parallel Mark)"

/* String of disabled features */
#define DISABLED_FEATURES "none"

/* Disable AOT Compiler */
/* #undef DISABLE_AOT */
#define DISABLE_AOT 1

/* Disable support for multiple appdomains. */
/* #undef DISABLE_APPDOMAINS */

/* Disable agent attach support */
/* #undef DISABLE_ATTACH */

/* Disable COM support */
/* #undef DISABLE_COM */

/* Disable runtime debugging support */
/* #undef DISABLE_DEBUG */

/* Disable Soft Debugger Agent. */
/* #undef DISABLE_DEBUGGER_AGENT */

/* Disable System.Decimal support */
/* #undef DISABLE_DECIMAL */

/* Disable desktop assembly loader semantics. */
/* #undef DISABLE_DESKTOP_LOADER */

/* Disable generics support */
/* #undef DISABLE_GENERICS */

/* ... */
/* #undef DISABLE_HW_TRAPS */

/* Icall tables disabled */
/* #undef DISABLE_ICALL_TABLES */

/* Disable the interpreter. */
/* #undef DISABLE_INTERPRETER */

/* Disable the JIT, only full-aot mode or interpreter will be supported by the
   runtime. */
/* #undef DISABLE_JIT */
#define DISABLE_JIT 1

/* Disable support for huge assemblies */
/* #undef DISABLE_LARGE_CODE */

/* Disable support code for the LLDB plugin. */
/* #undef DISABLE_LLDB */

/* Disable support debug logging */
/* #undef DISABLE_LOGGING */

/* Disable support for .mdb symbol files. */
/* #undef DISABLE_MDB */

/* Disable String normalization support. */
/* #undef DISABLE_NORMALIZATION */

/* Disable Performance Counters. */
/* #undef DISABLE_PERFCOUNTERS */

/* Disable P/Invoke support */
/* #undef DISABLE_PINVOKE */

/* Disables the IO portability layer */
/* #undef DISABLE_PORTABILITY */

/* Disable default profiler support */
/* #undef DISABLE_PROFILER */

/* Disable reflection emit support */
/* #undef DISABLE_REFLECTION_EMIT */

/* Disable assembly saving support in reflection emit */
/* #undef DISABLE_REFLECTION_EMIT_SAVE */

/* Disable remoting support (This disables type proxies and make com
   non-functional) */
/* #undef DISABLE_REMOTING */

/* Disable CAS/CoreCLR security */
/* #undef DISABLE_SECURITY */

/* Disable debug helpers in SGEN. */
/* #undef DISABLE_SGEN_DEBUG_HELPERS */

/* Disable gc bridge support in SGEN. */
/* #undef DISABLE_SGEN_GC_BRIDGE */

/* Disable concurrent gc support in SGEN. */
/* #undef DISABLE_SGEN_MAJOR_MARKSWEEP_CONC */

/* Disable minor=split support in SGEN. */
/* #undef DISABLE_SGEN_SPLIT_NURSERY */

/* Disable Shadow Copy for AppDomains */
/* #undef DISABLE_SHADOW_COPY */

/* Disable shared perfcounters. */
/* #undef DISABLE_SHARED_PERFCOUNTERS */

/* Disable SIMD intrinsics related optimizations. */
/* #undef DISABLE_SIMD */

/* Disable advanced SSA JIT optimizations */
/* #undef DISABLE_SSA */

/* Do not create structured crash files during unmanaged crashes */
/* #undef DISABLE_STRUCTURED_CRASH */

/* Disables the verifier */
/* #undef DISABLE_VERIFIER */

/* Enable checked build */
/* #undef ENABLE_CHECKED_BUILD */

/* Enable GC checked build */
/* #undef ENABLE_CHECKED_BUILD_GC */

/* Enable metadata checked build */
/* #undef ENABLE_CHECKED_BUILD_METADATA */

/* Enable private types checked build */
/* #undef ENABLE_CHECKED_BUILD_PRIVATE_TYPES */

/* Enable thread checked build */
/* #undef ENABLE_CHECKED_BUILD_THREAD */

/* Enable cooperative stop-the-world garbage collection. */
/* #undef ENABLE_COOP_SUSPEND */

/* Enable DTrace probes */
/* #undef ENABLE_DTRACE */

/* Extension module enabled */
/* #undef ENABLE_EXTENSION_MODULE */

/* Enable hybrid suspend for GC stop-the-world */
/* #undef ENABLE_HYBRID_SUSPEND */

/* Icall export enabled */
/* #undef ENABLE_ICALL_EXPORT */

/* Icall symbol map enabled */
/* #undef ENABLE_ICALL_SYMBOL_MAP */

/* Some VES is available at runtime */
#define ENABLE_ILGEN 1

/* Enable the LLVM back end */
/* #undef ENABLE_LLVM */

/* Runtime support code for llvm enabled */
/* #undef ENABLE_LLVM_RUNTIME */

/* Enable runtime support for Monodroid (Xamarin.Android) */
/* #undef ENABLE_MONODROID */

/* Enable runtime support for Monotouch (Xamarin.iOS and Xamarin.Mac) */
/* #undef ENABLE_MONOTOUCH */

/* Overridable allocator support enabled */
/* #undef ENABLE_OVERRIDABLE_ALLOCATORS */

/* Have GLIBC_BEFORE_2_3_4_SCHED_SETAFFINITY */
/* #undef GLIBC_BEFORE_2_3_4_SCHED_SETAFFINITY */

/* GLIBC has CPU_COUNT macro in sched.h */
#define GLIBC_HAS_CPU_COUNT 1

/* accept4 */
#define HAVE_ACCEPT4 1

/* Have access */
#define HAVE_ACCESS 1

/* Has the 'aintl' function */
/* #undef HAVE_AINTL */

/* Define to 1 if you have the <alloca.h> header file. */
#define HAVE_ALLOCA_H 1

/* Define to 1 if you have the <android/api-level.h> header file. */
/* #undef HAVE_ANDROID_API_LEVEL_H */
#define HAVE_ANDROID_API_LEVEL_H

/* Define to 1 if you have the <android/legacy_signal_inlines.h> header file.
   */
/* #undef HAVE_ANDROID_LEGACY_SIGNAL_INLINES_H */
#define HAVE_ANDROID_LEGACY_SIGNAL_INLINES_H

/* Define to 1 if you have the <android/versioning.h> header file. */
/* #undef HAVE_ANDROID_VERSIONING_H */
#define HAVE_ANDROID_VERSIONING_H

/* arc4random */
/* #undef HAVE_ARC4RANDOM */

/* ARM v5 */
/* #undef HAVE_ARMV5 */

/* ARM v6 */
/* #undef HAVE_ARMV6 */

/* ARM v7 */
/* #undef HAVE_ARMV7 */
#define HAVE_ARMV7

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Supports C99 array initialization */
#define HAVE_ARRAY_ELEM_INIT 1

/* Define to 1 if you have the <asm/sigcontext.h> header file. */
#define HAVE_ASM_SIGCONTEXT_H 1

/* Define to 1 if you have the <attr/xattr.h> header file. */
/* #undef HAVE_ATTR_XATTR_H */

/* Define to 1 if you have the `backtrace_symbols' function. */
// #define HAVE_BACKTRACE_SYMBOLS 1

/* Define to 1 if the system has the type `blkcnt_t'. */
#define HAVE_BLKCNT_T 1

/* Define to 1 if the system has the type `blksize_t'. */
#define HAVE_BLKSIZE_T 1

/* BoringTls is supported */
#define HAVE_BTLS 1

/* Define to 1 if you have the <checklist.h> header file. */
/* #undef HAVE_CHECKLIST_H */

/* Use classic Windows API support */
#define HAVE_CLASSIC_WINAPI_SUPPORT 1

/* CLOCK_MONOTONIC */
#define HAVE_CLOCK_MONOTONIC 1

/* CLOCK_MONOTONIC_COARSE */
#define HAVE_CLOCK_MONOTONIC_COARSE 1

/* Define to 1 if you have the `clock_nanosleep' function. */
#define HAVE_CLOCK_NANOSLEEP 1

/* CLOCK_REALTIME */
#define HAVE_CLOCK_REALTIME 1

/* Define to 1 if you have the <CommonCrypto/CommonDigest.h> header file. */
/* #undef HAVE_COMMONCRYPTO_COMMONDIGEST_H */

/* Define to 1 if you have the <complex.h> header file. */
#define HAVE_COMPLEX_H 1

/* Defaults to concurrent GC */
#define HAVE_CONC_GC_AS_DEFAULT 1

/* Define to 1 if you have the `confstr' function. */
#define HAVE_CONFSTR 1

/* Define to 1 if you have the <copyfile.h> header file. */
/* #undef HAVE_COPYFILE_H */

/* Define to 1 if you have the <crt_externs.h> header file. */
/* #undef HAVE_CRT_EXTERNS_H */

/* Have /dev/random */
#define HAVE_CRYPT_RNG 1

/* Define to 1 if you have the <curses.h> header file. */
/* #undef HAVE_CURSES_H */

/* Define to 1 if you have the declaration of `InterlockedAdd', and to 0 if
   you don't. */
/* #undef HAVE_DECL_INTERLOCKEDADD */

/* Define to 1 if you have the declaration of `InterlockedAdd64', and to 0 if
   you don't. */
/* #undef HAVE_DECL_INTERLOCKEDADD64 */

/* Define to 1 if you have the declaration of `InterlockedCompareExchange64',
   and to 0 if you don't. */
/* #undef HAVE_DECL_INTERLOCKEDCOMPAREEXCHANGE64 */

/* Define to 1 if you have the declaration of `InterlockedDecrement64', and to
   0 if you don't. */
/* #undef HAVE_DECL_INTERLOCKEDDECREMENT64 */

/* Define to 1 if you have the declaration of `InterlockedExchange64', and to
   0 if you don't. */
/* #undef HAVE_DECL_INTERLOCKEDEXCHANGE64 */

/* Define to 1 if you have the declaration of `InterlockedIncrement64', and to
   0 if you don't. */
/* #undef HAVE_DECL_INTERLOCKEDINCREMENT64 */

/* Define to 1 if you have the declaration of `strerror_r', and to 0 if you
   don't. */
#define HAVE_DECL_STRERROR_R 1

/* Define to 1 if you have the declaration of `__readfsdword', and to 0 if you
   don't. */
/* #undef HAVE_DECL___READFSDWORD */

/* Support for the deprecated attribute */
/* #undef HAVE_DEPRECATED */

/* Define to 1 if you have the <dirent.h> header file. */
#define HAVE_DIRENT_H 1

/* struct dirent.d_namlen */
/* #undef HAVE_DIRENT_NAME_LEN */

/* Define to 1 if you have the `dladdr' function. */
#define HAVE_DLADDR 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `dl_iterate_phdr' function. */
#define HAVE_DL_ITERATE_PHDR 1

/* dlopen-based dynamic loader available */
#define HAVE_DL_LOADER 1

/* ECHO */
#define HAVE_ECHO 1

/* Define to 1 if you have the <elf.h> header file. */
#define HAVE_ELF_H 1

/* Define to 1 if you have the `endgrent' function. */
#define HAVE_ENDGRENT 1

/* Define to 1 if you have the `endpwent' function. */
#define HAVE_ENDPWENT 1

/* Define to 1 if you have the `endusershell' function. */
#define HAVE_ENDUSERSHELL 1

/* epoll_create1 */
#define HAVE_EPOLL 1

/* Define to 1 if you have the `epoll_ctl' function. */
// #define HAVE_EPOLL_CTL 1

/* Define to 1 if you have the <execinfo.h> header file. */
// #define HAVE_EXECINFO_H 1

/* Define to 1 if you have the `execv' function. */
#define HAVE_EXECV 1

/* Define to 1 if you have the `execve' function. */
#define HAVE_EXECVE 1

/* Define to 1 if you have the `execvp' function. */
#define HAVE_EXECVP 1

/* fcopyfile */
/* #undef HAVE_FCOPYFILE */

/* struct fd_set.fds_bits */
/* #undef HAVE_FDS_BITS */

/* Define to 1 if you have the `fgetgrent' function. */
#define HAVE_FGETGRENT 1

/* Define to 1 if you have the `fgetpwent' function. */
#define HAVE_FGETPWENT 1

/* Define to 1 if you have the `finite' function. */
#define HAVE_FINITE 1

/* struct flock64 */
#define HAVE_FLOCK64 1

/* Define to 1 if you have the `fork' function. */
#define HAVE_FORK 1

/* Define to 1 if you have the <fstab.h> header file. */
#define HAVE_FSTAB_H 1

/* Define to 1 if you have the `fstatat' function. */
#define HAVE_FSTATAT 1

/* Define to 1 if you have the `fstatfs' function. */
#define HAVE_FSTATFS 1

/* Define to 1 if you have the `fstatvfs' function. */
#define HAVE_FSTATVFS 1

/* ftruncate64 */
#define HAVE_FTRUNCATE64 1

/* Define to 1 if you have the `futimens' function. */
#define HAVE_FUTIMENS 1

/* Define to 1 if you have the `futimes' function. */
#define HAVE_FUTIMES 1

/* Have getaddrinfo */
#define HAVE_GETADDRINFO 1

/* getdomainname */
#define HAVE_GETDOMAINNAME 1

/* getdomainname with size_t namelen */
/* #undef HAVE_GETDOMAINNAME_SIZET */

/* Define to 1 if you have the `getentropy' function. */
/* #undef HAVE_GETENTROPY */

/* Define to 1 if you have the `getfsstat' function. */
/* #undef HAVE_GETFSSTAT */

/* Define to 1 if you have the `getgrent' function. */
#define HAVE_GETGRENT 1

/* Define to 1 if you have the `getgrgid_r' function. */
// #define HAVE_GETGRGID_R 1

/* Define to 1 if you have the `getgrnam_r' function. */
// #define HAVE_GETGRNAM_R 1

/* Have gethostbyname */
#define HAVE_GETHOSTBYNAME 1

/* Have gethostbyname2 */
#define HAVE_GETHOSTBYNAME2 1

/* Have gethostbyname2_r */
#define HAVE_GETHOSTBYNAME2_R 1

/* Define to 1 if you have the `gethostid' function. */
#define HAVE_GETHOSTID 1

/* gethrtime */
/* #undef HAVE_GETHRTIME */

/* Have getifaddrs */
#define HAVE_GETIFADDRS 1

/* Define to 1 if you have the `getlogin_r' function. */
#define HAVE_GETLOGIN_R 1

/* getmntinfo */
/* #undef HAVE_GETMNTINFO */

/* Have getnameinfo */
#define HAVE_GETNAMEINFO 1

/* getnameinfo with signed flags */
/* #undef HAVE_GETNAMEINFO_SIGNED_FLAGS */

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* getpeereid */
/* #undef HAVE_GETPEEREID */

/* Define to 1 if you have the `getpriority' function. */
#define HAVE_GETPRIORITY 1

/* Have getprotobyname */
#define HAVE_GETPROTOBYNAME 1

/* Define to 1 if you have the `getpwent' function. */
#define HAVE_GETPWENT 1

/* Define to 1 if you have the `getpwnam_r' function. */
#define HAVE_GETPWNAM_R 1

/* Define to 1 if you have the `getpwuid_r' function. */
// #define HAVE_GETPWUID_R 1

/* Define to 1 if you have the `getrandom' function. */
/* #undef HAVE_GETRANDOM */

/* Define to 1 if you have the `getresuid' function. */
#define HAVE_GETRESUID 1

/* Define to 1 if you have the `getrlimit' function. */
#define HAVE_GETRLIMIT 1

/* Define to 1 if you have the `getrusage' function. */
#define HAVE_GETRUSAGE 1

/* char* strerror(int errnum, char *buf, size_t buflen) */
#define HAVE_GNU_STRERROR_R 1

/* Define to 1 if you have the <grp.h> header file. */
#define HAVE_GRP_H 1

/* ICANON */
#define HAVE_ICANON 1

/* Define to 1 if you have the <iconv.h> header file. */
// #define HAVE_ICONV_H 1

/* Define to 1 if you have the <ieeefp.h> header file. */
/* #undef HAVE_IEEEFP_H */

/* Have if_nametoindex */
#define HAVE_IF_NAMETOINDEX 1

/* Define to 1 if you have the `inet_aton' function. */
#define HAVE_INET_ATON 1

/* Have inet_ntop */
#define HAVE_INET_NTOP 1

/* Have inet_pton */
#define HAVE_INET_PTON 1

/* HAVE_INOTIFY */
#define HAVE_INOTIFY 1

/* inotify_add_watch */
#define HAVE_INOTIFY_ADD_WATCH 1

/* inotify_init */
#define HAVE_INOTIFY_INIT 1

/* inotify_rm_watch */
#define HAVE_INOTIFY_RM_WATCH 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* IN_EXCL_UNLINK */
#define HAVE_IN_EXCL_UNLINK 1

/* struct in_pktinfo */
#define HAVE_IN_PKTINFO 1

/* ioctl */
#define HAVE_IOCTL 1

/* Have IPPROTO_IP */
#define HAVE_IPPROTO_IP 1

/* Have IPPROTO_IPV6 */
#define HAVE_IPPROTO_IPV6 1

/* Have IPPROTO_TCP */
#define HAVE_IPPROTO_TCP 1

/* Have IPV6_PKTINFO */
#define HAVE_IPV6_PKTINFO 1

/* Have IP_DONTFRAG */
/* #undef HAVE_IP_DONTFRAG */

/* Have IP_DONTFRAGMENT */
/* #undef HAVE_IP_DONTFRAGMENT */

/* struct ip_mreqn */
#define HAVE_IP_MREQN 1

/* Have IP_MTU_DISCOVER */
#define HAVE_IP_MTU_DISCOVER 1

/* Have IP_PKTINFO */
#define HAVE_IP_PKTINFO 1

/* Have IP_PMTUDISC_DO */
#define HAVE_IP_PMTUDISC_DO 1

/* Define to 1 if you have the `isfinite' function. */
#define HAVE_ISFINITE 1

/* isinf available */
#define HAVE_ISINF 1

/* Define to 1 if you have the `kill' function. */
#define HAVE_KILL 1

/* kqueue */
/* #undef HAVE_KQUEUE */

/* Have __thread keyword */
#define HAVE_KW_THREAD 1

/* Have large file support */
#define HAVE_LARGE_FILE_SUPPORT 1

/* Define to 1 if you have the `iconv' library (-liconv). */
/* #undef HAVE_LIBICONV */

/* Define to 1 if you have the <libproc.h> header file. */
/* #undef HAVE_LIBPROC_H */

/* Define to 1 if you have the <link.h> header file. */
#define HAVE_LINK_H 1

/* linux/in.h */
// #define HAVE_LINUX_IN_H 1

/* Define to 1 if you have the <linux/magic.h> header file. */
// #define HAVE_LINUX_MAGIC_H 1

/* Define to 1 if you have the <linux/netlink.h> header file. */
// #define HAVE_LINUX_NETLINK_H 1

/* Define to 1 if you have the <linux/rtc.h> header file. */
// #define HAVE_LINUX_RTC_H 1

/* Define to 1 if you have the <linux/rtnetlink.h> header file. */
// #define HAVE_LINUX_RTNETLINK_H 1

/* Define to 1 if you have the <linux/serial.h> header file. */
// #define HAVE_LINUX_SERIAL_H 1

/* Define to 1 if you have the <localcharset.h> header file. */
/* #undef HAVE_LOCALCHARSET_H */

/* Define to 1 if you have the `localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if you have the `lockf' function. */
#define HAVE_LOCKF 1

/* lseek64 */
#define HAVE_LSEEK64 1

/* Define to 1 if you have the `lutimes' function. */
#define HAVE_LUTIMES 1

/* Define to 1 if you have the <machine/endian.h> header file. */
/* #undef HAVE_MACHINE_ENDIAN_H */

/* mach_absolute_time */
/* #undef HAVE_MACH_ABSOLUTE_TIME */

/* mach_timebase_info */
/* #undef HAVE_MACH_TIMEBASE_INFO */

/* Define to 1 if you have the `madvise' function. */
#define HAVE_MADVISE 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Have mincore */
#define HAVE_MINCORE 1

/* Define to 1 if you have the `mkdtemp' function. */
#define HAVE_MKDTEMP 1

/* Define to 1 if you have the `mknodat' function. */
#define HAVE_MKNODAT 1

/* Define to 1 if you have the `mkstemp' function. */
#define HAVE_MKSTEMP 1

/* mkstemps */
/* #undef HAVE_MKSTEMPS */

/* Define to 1 if you have the `mmap' function. */
#define HAVE_MMAP 1

/* mmap64 */
#define HAVE_MMAP64 1

/* Moving collector */
#define HAVE_MOVING_COLLECTOR 1

/* Define to 1 if you have the `mremap' function. */
#define HAVE_MREMAP 1

/* Have MSG_NOSIGNAL */
#define HAVE_MSG_NOSIGNAL 1

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <netinet/tcp.h> header file. */
#define HAVE_NETINET_TCP_H 1

/* Define to 1 if you have the <net/if.h> header file. */
#define HAVE_NET_IF_H 1

/* _NSGetEnviron */
/* #undef HAVE_NSGETENVIRON */

/* No GC support. */
/* #undef HAVE_NULL_GC */

/* Define to 1 if you have the <pathconf.h> header file. */
/* #undef HAVE_PATHCONF_H */

/* pipe2 */
#define HAVE_PIPE2 1

/* Define to 1 if you have the `poll' function. */
#define HAVE_POLL 1

/* Define to 1 if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* posix_fadvise */
#define HAVE_POSIX_FADVISE 1

/* posix_fadvise64 */
#define HAVE_POSIX_FADVISE64 1

/* Define to 1 if you have the `posix_fallocate' function. */
#define HAVE_POSIX_FALLOCATE 1

/* Define to 1 if you have the `posix_madvise' function. */
#define HAVE_POSIX_MADVISE 1

/* Define to 1 if you have the `prctl' function. */
#define HAVE_PRCTL 1

/* Define to 1 if you have the `preadv' function. */
#define HAVE_PREADV 1

/* struct fd_set.__fds_bits */
/* #undef HAVE_PRIVATE_FDS_BITS */

/* Define to 1 if you have the `psignal' function. */
#define HAVE_PSIGNAL 1

/* Define to 1 if you have the `pthread_attr_getstack' function. */
#define HAVE_PTHREAD_ATTR_GETSTACK 1

/* Define to 1 if you have the `pthread_attr_getstacksize' function. */
#define HAVE_PTHREAD_ATTR_GETSTACKSIZE 1

/* Define to 1 if you have the `pthread_attr_get_np' function. */
/* #undef HAVE_PTHREAD_ATTR_GET_NP */

/* Define to 1 if you have the `pthread_attr_setstacksize' function. */
#define HAVE_PTHREAD_ATTR_SETSTACKSIZE 1

/* Define to 1 if you have the `pthread_cond_timedwait_relative_np' function.
   */
/* #undef HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP */

/* Define to 1 if you have the `pthread_getattr_np' function. */
#define HAVE_PTHREAD_GETATTR_NP 1

/* Define to 1 if you have the `pthread_getname_np' function. */
#define HAVE_PTHREAD_GETNAME_NP 1

/* Define to 1 if you have the `pthread_get_stackaddr_np' function. */
/* #undef HAVE_PTHREAD_GET_STACKADDR_NP */

/* Define to 1 if you have the `pthread_get_stacksize_np' function. */
/* #undef HAVE_PTHREAD_GET_STACKSIZE_NP */

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define to 1 if you have the `pthread_kill' function. */
#define HAVE_PTHREAD_KILL 1

/* Define to 1 if you have the `pthread_mutex_timedlock' function. */
#define HAVE_PTHREAD_MUTEX_TIMEDLOCK 1

/* Define to 1 if you have the <pthread_np.h> header file. */
/* #undef HAVE_PTHREAD_NP_H */

/* Define to 1 if you have the `pthread_setname_np' function. */
#define HAVE_PTHREAD_SETNAME_NP 1

/* Define to 1 if you have the <pwd.h> header file. */
#define HAVE_PWD_H 1

/* Define to 1 if you have the `pwritev' function. */
#define HAVE_PWRITEV 1

/* readdir_r */
#define HAVE_READDIR_R 1

/* Define to 1 if you have the `readlinkat' function. */
#define HAVE_READLINKAT 1

/* Define to 1 if you have the `readv' function. */
#define HAVE_READV 1

/* read_real_time */
/* #undef HAVE_READ_REAL_TIME */

/* Define to 1 if you have the `remap_file_pages' function. */
#define HAVE_REMAP_FILE_PAGES 1

/* Define to 1 if you have the `rewinddir' function. */
#define HAVE_REWINDDIR 1

/* struct rt_msghdr */
#define HAVE_RT_MSGHDR 1

/* sched_getaffinity */
// #define HAVE_SCHED_GETAFFINITY 1

/* Define to 1 if you have the `sched_getcpu' function. */
#define HAVE_SCHED_GETCPU 1

/* sched_setaffinity */
#define HAVE_SCHED_SETAFFINITY 1

/* Define to 1 if you have the `seekdir' function. */
#define HAVE_SEEKDIR 1

/* Define to 1 if you have the <semaphore.h> header file. */
#define HAVE_SEMAPHORE_H 1

/* Define to 1 if you have the `sendfile' function. */
#define HAVE_SENDFILE 1

/* sendfile with 4 arguments */
#define HAVE_SENDFILE_4 1

/* sendfile with 6 arguments */
#define HAVE_SENDFILE_6 1

/* Define to 1 if you have the `setdomainname' function. */
#define HAVE_SETDOMAINNAME 1

/* Define to 1 if you have the `setgrent' function. */
#define HAVE_SETGRENT 1

/* Define to 1 if you have the `setgroups' function. */
#define HAVE_SETGROUPS 1

/* Define to 1 if you have the `sethostid' function. */
#define HAVE_SETHOSTID 1

/* Define to 1 if you have the `sethostname' function. */
#define HAVE_SETHOSTNAME 1

/* Define to 1 if you have the `setpgid' function. */
#define HAVE_SETPGID 1

/* Define to 1 if you have the `setpriority' function. */
#define HAVE_SETPRIORITY 1

/* Define to 1 if you have the `setpwent' function. */
#define HAVE_SETPWENT 1

/* Define to 1 if you have the `setresuid' function. */
#define HAVE_SETRESUID 1

/* Define to 1 if you have the `setusershell' function. */
#define HAVE_SETUSERSHELL 1

/* Define to 1 if you have the `shm_open' function. */
#define HAVE_SHM_OPEN 1

/* shm_open that works well enough with mmap */
#define HAVE_SHM_OPEN_THAT_WORKS_WELL_ENOUGH_WITH_MMAP 1

/* Define to 1 if you have the `sigaction' function. */
#define HAVE_SIGACTION 1

/* Define to 1 if you have the `signal' function. */
#define HAVE_SIGNAL 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Have signbit */
#define HAVE_SIGNBIT 1

/* Can get interface list */
#define HAVE_SIOCGIFCONF 1

/* sockaddr_in6 has sin6_len */
/* #undef HAVE_SOCKADDR_IN6_SIN_LEN */

/* sockaddr_in has sin_len */
/* #undef HAVE_SOCKADDR_IN_SIN_LEN */

/* Have socklen_t */
#define HAVE_SOCKLEN_T 1

/* Have SOL_IP */
#define HAVE_SOL_IP 1

/* Have SOL_IPV6 */
#define HAVE_SOL_IPV6 1

/* Have SOL_TCP */
/* #undef HAVE_SOL_TCP */

/* stat64 */
#define HAVE_STAT64 1

/* struct statfs */
#define HAVE_STATFS 1

/* struct statfs.f_fstypename */
/* #undef HAVE_STATFS_FSTYPENAME */

/* Define to 1 if you have the `statvfs' function. */
#define HAVE_STATVFS 1

/* struct statvfs.f_fstypename */
/* #undef HAVE_STATVFS_FSTYPENAME */

/* struct stat.st_birthtime */
/* #undef HAVE_STAT_BIRTHTIME */

/* struct stat.st_atimensec */
/* #undef HAVE_STAT_NSEC */

/* struct stat.st_atim */
#define HAVE_STAT_TIM 1

/* struct stat.st_atimespec */
/* #undef HAVE_STAT_TIMESPEC */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `stime' function. */
#define HAVE_STIME 1

/* Define to 1 if you have the `stpcpy' function. */
#define HAVE_STPCPY 1

/* strcpy_s */
/* #undef HAVE_STRCPY_S */

/* Define to 1 if you have the `strerror_r' function. */
#define HAVE_STRERROR_R 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcpy' function. */
/* #undef HAVE_STRLCPY */

/* Define to 1 if you have the `strndup' function. */
/* #undef HAVE_STRNDUP */

/* Define to 1 if you have the `strtok_r' function. */
#define HAVE_STRTOK_R 1

/* Define to 1 if the system has the type `struct cmsghdr'. */
#define HAVE_STRUCT_CMSGHDR 1

/* Define to 1 if `d_off' is a member of `struct dirent'. */
#define HAVE_STRUCT_DIRENT_D_OFF 1

/* Define to 1 if `d_reclen' is a member of `struct dirent'. */
#define HAVE_STRUCT_DIRENT_D_RECLEN 1

/* Define to 1 if `d_type' is a member of `struct dirent'. */
#define HAVE_STRUCT_DIRENT_D_TYPE 1

/* Define to 1 if the system has the type `struct flock'. */
#define HAVE_STRUCT_FLOCK 1

/* Define to 1 if the system has the type `struct iovec'. */
#define HAVE_STRUCT_IOVEC 1

/* Have struct ip_mreq */
/* #undef HAVE_STRUCT_IP_MREQ */

/* Have struct ip_mreqn */
#define HAVE_STRUCT_IP_MREQN 1

/* Define to 1 if `kp_proc' is a member of `struct kinfo_proc'. */
/* #undef HAVE_STRUCT_KINFO_PROC_KP_PROC */

/* Define to 1 if the system has the type `struct linger'. */
#define HAVE_STRUCT_LINGER 1

/* Define to 1 if `pw_gecos' is a member of `struct passwd'. */
#define HAVE_STRUCT_PASSWD_PW_GECOS 1

/* Define to 1 if the system has the type `struct pollfd'. */
#define HAVE_STRUCT_POLLFD 1

/* Define to 1 if the system has the type `struct sockaddr'. */
#define HAVE_STRUCT_SOCKADDR 1

/* Define to 1 if the system has the type `struct sockaddr_in'. */
#define HAVE_STRUCT_SOCKADDR_IN 1

/* Define to 1 if the system has the type `struct sockaddr_in6'. */
#define HAVE_STRUCT_SOCKADDR_IN6 1

/* Define to 1 if the system has the type `struct sockaddr_storage'. */
#define HAVE_STRUCT_SOCKADDR_STORAGE 1

/* Define to 1 if the system has the type `struct sockaddr_un'. */
#define HAVE_STRUCT_SOCKADDR_UN 1

/* Define to 1 if the system has the type `struct stat'. */
#define HAVE_STRUCT_STAT 1

/* Define to 1 if `f_flags' is a member of `struct statfs'. */
#define HAVE_STRUCT_STATFS_F_FLAGS 1

/* Define to 1 if `st_atim' is a member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_ATIM 1

/* Define to 1 if `st_atimespec' is a member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_ATIMESPEC */

/* Define to 1 if `st_ctim' is a member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_CTIM 1

/* Define to 1 if `st_mtim' is a member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_MTIM 1

/* Define to 1 if the system has the type `struct timespec'. */
#define HAVE_STRUCT_TIMESPEC 1

/* Define to 1 if the system has the type `struct timeval'. */
#define HAVE_STRUCT_TIMEVAL 1

/* Define to 1 if the system has the type `struct timezone'. */
#define HAVE_STRUCT_TIMEZONE 1

/* Define to 1 if the system has the type `struct utimbuf'. */
#define HAVE_STRUCT_UTIMBUF 1

/* HAVE_SUPPORT_FOR_DUAL_MODE_IPV4_PACKET_INFO */
#define HAVE_SUPPORT_FOR_DUAL_MODE_IPV4_PACKET_INFO 1

/* Define to 1 if the system has the type `suseconds_t'. */
#define HAVE_SUSECONDS_T 1

/* Define to 1 if you have the `swab' function. */
#define HAVE_SWAB 1

/* Define to 1 if you have the `sysconf' function. */
#define HAVE_SYSCONF 1

/* Define to 1 if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1

/* Define to 1 if you have the `system' function. */
#define HAVE_SYSTEM 1

/* Define to 1 if you have the <sys/auxv.h> header file. */
#define HAVE_SYS_AUXV_H 1

/* Define to 1 if you have the <sys/cdefs.h> header file. */
#define HAVE_SYS_CDEFS_H 1

/* Define to 1 if you have the <sys/endian.h> header file. */
/* #undef HAVE_SYS_ENDIAN_H */

/* Define to 1 if you have the <sys/epoll.h> header file. */
#define HAVE_SYS_EPOLL_H 1

/* Define to 1 if you have the <sys/errno.h> header file. */
#define HAVE_SYS_ERRNO_H 1

/* Define to 1 if you have the <sys/event.h> header file. */
/* #undef HAVE_SYS_EVENT_H */

/* Define to 1 if you have the <sys/extattr.h> header file. */
/* #undef HAVE_SYS_EXTATTR_H */

/* Define to 1 if you have the <sys/filio.h> header file. */
/* #undef HAVE_SYS_FILIO_H */

/* Define to 1 if you have the <sys/inotify.h> header file. */
#define HAVE_SYS_INOTIFY_H 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/ipc.h> header file. */
#define HAVE_SYS_IPC_H 1

/* Define to 1 if you have the <sys/mkdev.h> header file. */
/* #undef HAVE_SYS_MKDEV_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/mount.h> header file. */
#define HAVE_SYS_MOUNT_H 1

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/poll.h> header file. */
#define HAVE_SYS_POLL_H 1

/* Define to 1 if you have the <sys/prctl.h> header file. */
#define HAVE_SYS_PRCTL_H 1

/* Define to 1 if you have the <sys/random.h> header file. */
/* #undef HAVE_SYS_RANDOM_H */

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have the <sys/sdt.h> header file. */
/* #undef HAVE_SYS_SDT_H */

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/sendfile.h> header file. */
#define HAVE_SYS_SENDFILE_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/sockio.h> header file. */
/* #undef HAVE_SYS_SOCKIO_H */

/* Define to 1 if you have the <sys/statfs.h> header file. */
#define HAVE_SYS_STATFS_H 1

/* Define to 1 if you have the <sys/statvfs.h> header file. */
#define HAVE_SYS_STATVFS_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/syscall.h> header file. */
#define HAVE_SYS_SYSCALL_H 1

/* Define to 1 if you have the <sys/sysctl.h> header file. */
#define HAVE_SYS_SYSCTL_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/uio.h> header file. */
#define HAVE_SYS_UIO_H 1

/* Define to 1 if you have the <sys/un.h> header file. */
#define HAVE_SYS_UN_H 1

/* Define to 1 if you have the <sys/user.h> header file. */
#define HAVE_SYS_USER_H 1

/* Define to 1 if you have the <sys/utime.h> header file. */
/* #undef HAVE_SYS_UTIME_H */

/* Define to 1 if you have the <sys/utsname.h> header file. */
#define HAVE_SYS_UTSNAME_H 1

/* Define to 1 if you have the <sys/vfstab.h> header file. */
/* #undef HAVE_SYS_VFSTAB_H */

/* Define to 1 if you have the <sys/wait.h> header file. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the <sys/xattr.h> header file. */
#define HAVE_SYS_XATTR_H 1

/* Have system zlib */
#define HAVE_SYS_ZLIB 1

/* tcgetattr */
#define HAVE_TCGETATTR 1

/* HAVE_TCP_FSM_H */
/* #undef HAVE_TCP_FSM_H */

/* TCPSTATE enum in netinet/tcp.h */
#define HAVE_TCP_H_TCPSTATE_ENUM 1

/* tcp/var.h */
/* #undef HAVE_TCP_VAR_H */

/* TCSANOW */
#define HAVE_TCSANOW 1

/* tcsetattr */
#define HAVE_TCSETATTR 1

/* Define to 1 if you have the `telldir' function. */
#define HAVE_TELLDIR 1

/* Define to 1 if you have the <termios.h> header file. */
#define HAVE_TERMIOS_H 1

/* Define to 1 if you have the <term.h> header file. */
/* #undef HAVE_TERM_H */

/* Have timezone variable */
/* #undef HAVE_TIMEZONE */

/* TIOCGWINSZ */
#define HAVE_TIOCGWINSZ 1

/* tls_model available */
#define HAVE_TLS_MODEL_ATTR 1

/* Have tm_gmtoff */
#define HAVE_TM_GMTOFF 1

/* Define to 1 if you have the `trunc' function. */
#define HAVE_TRUNC 1

/* Define to 1 if you have the `ttyname_r' function. */
#define HAVE_TTYNAME_R 1

/* Define to 1 if you have the <ucontext.h> header file. */
#define HAVE_UCONTEXT_H 1

/* uname */
#define HAVE_UNAME 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <unwind.h> header file. */
#define HAVE_UNWIND_H 1

/* Define to 1 if you have /usr/include/malloc.h. */
#define HAVE_USR_INCLUDE_MALLOC_H 1

/* Define to 1 if you have the `utimensat' function. */
#define HAVE_UTIMENSAT 1

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Don't use UWP Windows API support */
#define HAVE_UWP_WINAPI_SUPPORT 0

/* Define to 1 if you have the `vasprintf' function. */
#define HAVE_VASPRINTF 1

/* Support for the visibility ("hidden") attribute */
#define HAVE_VISIBILITY_HIDDEN 1

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define to 1 if you have the `vsyslog' function. */
#define HAVE_VSYSLOG 1

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the <winternl.h> header file. */
/* #undef HAVE_WINTERNL_H */

/* Have a working sigaltstack */
#define HAVE_WORKING_SIGALTSTACK 1

/* Define to 1 if you have the `writev' function. */
#define HAVE_WRITEV 1

/* Have system zlib */
#define HAVE_ZLIB 1

/* Define to 1 if you have the `_finite' function. */
/* #undef HAVE__FINITE */

/* ... */
// #define HOST_AMD64 1

/* Targeting the Android platform */
/* #undef HOST_ANDROID */

/* ... */
/* #undef HOST_ARM */

/* ... */
/* #undef HOST_ARM64 */

/* Host Platform is Darwin */
/* #undef HOST_DARWIN */

/* ... */
/* #undef HOST_MIPS */

/* This platform does not support symlinks */
/* #undef HOST_NO_SYMLINKS */

/* ... */
/* #undef HOST_POWERPC */

/* ... */
/* #undef HOST_POWERPC64 */

/* ... */
/* #undef HOST_S390X */

/* ... */
/* #undef HOST_SPARC */

/* ... */
/* #undef HOST_SPARC64 */

/* Host wasm */
/* #undef HOST_WASM */
#define HOST_WASM 1

/* Host Platform is Win32 */
/* #undef HOST_WIN32 */

/* ... */
/* #undef HOST_X86 */
// #define HOST_X86

/* inotify_rm_watch with unsigned wd */
/* #undef INOTIFY_RM_WATCH_WD_UNSIGNED */

/* struct ipv6_mreq with unsigned ipv6mr_interface */
/* #undef IPV6MR_INTERFACE_UNSIGNED */

/* kevent with void *data */
/* #undef KEVENT_HAS_VOID_UDATA */

/* kevent with int parameters */
/* #undef KEVENT_REQUIRES_INT_PARAMS */

/* Enable lazy gc thread creation by the embedding host. */
/* #undef LAZY_GC_THREAD_CREATION */

/* Full version of LLVM libraries */
/* #undef LLVM_VERSION */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Define to 1 if `major', `minor', and `makedev' are declared in <mkdev.h>.
   */
/* #undef MAJOR_IN_MKDEV */

/* Define to 1 if `major', `minor', and `makedev' are declared in
   <sysmacros.h>. */
/* #undef MAJOR_IN_SYSMACROS */

/* The architecture this is running on */
#define MONO_ARCHITECTURE "wasm"

/* Enable the allocation and indexing of arrays greater than Int32.MaxValue */
/* #undef MONO_BIG_ARRAYS */

/* Version of the corlib-runtime interface */
#define MONO_CORLIB_VERSION 1051600014

/* The runtime is compiled for cross-compiling mode */
/* #undef MONO_CROSS_COMPILE */

/* Disable banned functions from being used by the runtime */
#define MONO_INSIDE_RUNTIME 1

/* Make jemalloc assert for mono */
/* #undef MONO_JEMALLOC_ASSERT */

/* Make jemalloc default for mono */
/* #undef MONO_JEMALLOC_DEFAULT */

/* Enable jemalloc usage for mono */
/* #undef MONO_JEMALLOC_ENABLED */

/* The LLVM back end is dynamically loaded */
/* #undef MONO_LLVM_LOADED */

/* AOT cross offsets file */
/* #undef MONO_OFFSETS_FILE */

/* Do not include names of unmanaged functions in the crash dump */
#define MONO_PRIVATE_CRASHES 1

/* Reduce runtime requirements (and capabilities) */
/* #undef MONO_SMALL_CONFIG */

/* Xen-specific behaviour */
#define MONO_XEN_OPT 1

/* Length of zero length arrays */
#define MONO_ZERO_LEN_ARRAY 0

/* Name of /dev/random */
#define NAME_DEV_RANDOM "/dev/random"

/* Define if Unix sockets cannot be created in an anonymous namespace */
/* #undef NEED_LINK_UNLINK */

/* Name of package */
#define PACKAGE "mono"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "https://github.com/mono/mono/issues/new"

/* Define to the full name of this package. */
#define PACKAGE_NAME "mono"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "mono 5.16.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "mono"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "5.16.0"

/* getpriority with int who */
#define PRIORITY_REQUIRES_INT_WHO 1

/* pthread is a pointer */
/* #undef PTHREAD_POINTER_ID */

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* size of machine integer registers */
#define SIZEOF_REGISTER SIZEOF_VOID_P

/* The size of `size_t', as computed by sizeof. */
#define SIZEOF_SIZE_T 4

/* The size of `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 4

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if strerror_r returns char *. */
#define STRERROR_R_CHAR_P 1

/* ... */
// #define TARGET_AMD64 1

/* ... */
/* #undef TARGET_ANDROID */

/* ... */
/* #undef TARGET_ARM */

/* ... */
/* #undef TARGET_ARM64 */

/* byte order of target */
#define TARGET_BYTE_ORDER G_BYTE_ORDER

/* The JIT/AOT targets iOS */
/* #undef TARGET_IOS */

/* The JIT/AOT targets Apple platforms */
/* #undef TARGET_MACH */

/* ... */
/* #undef TARGET_MIPS */

/* The JIT/AOT targets OSX */
/* #undef TARGET_OSX */

/* ... */
/* #undef TARGET_POWERPC */

/* ... */
/* #undef TARGET_POWERPC64 */

/* ... */
/* #undef TARGET_PS3 */

/* ... */
/* #undef TARGET_PS4 */

/* ... */
/* #undef TARGET_S390X */

/* ... */
/* #undef TARGET_SPARC */

/* ... */
/* #undef TARGET_SPARC64 */

/* Target wasm */
/* #undef TARGET_WASM */
#define TARGET_WASM 1

/* The JIT/AOT targets WatchOS */
/* #undef TARGET_WATCHOS */

/* Target Platform is Win32 */
/* #undef TARGET_WIN32 */

/* ... */
/* #undef TARGET_X86 */
// #define TARGET_X86

/* ... */
/* #undef TARGET_XBOX360 */

/* ... */
#define USE_GCC_ATOMIC_OPS 1

/* Use kqueue for the threadpool */
/* #undef USE_KQUEUE_FOR_THREADPOOL */

/* ... */
/* #undef USE_MACH_SEMA */

/* Use malloc for each single mempool allocation */
/* #undef USE_MALLOC_FOR_MEMPOOLS */

/* Version number of package */
#define VERSION "5.16.0"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* 64 bit mode with 4 byte longs and pointers */
/* #undef __mono_ilp32__ */

/* ... */
/* #undef __mono_ppc64__ */
