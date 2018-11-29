cmake_minimum_required( VERSION 2.8 )

set( EGLIB_COMMON_SOURCE
	"repo/mono/eglib/eglib-remap.h"
	"repo/mono/eglib/sort.frag.h"
	"repo/mono/eglib/glib.h"
	"repo/mono/eglib/garray.c"
	"repo/mono/eglib/gbytearray.c"
	"repo/mono/eglib/gerror.c"
	"repo/mono/eglib/ghashtable.c"
	"repo/mono/eglib/giconv.c"
	"repo/mono/eglib/gmem.c"
	"repo/mono/eglib/gmodule.h"
	"repo/mono/eglib/goutput.c"
	"repo/mono/eglib/gqsort.c"
	"repo/mono/eglib/gstr.c"
	"repo/mono/eglib/gslist.c"
	"repo/mono/eglib/gstring.c"
	"repo/mono/eglib/gptrarray.c"
	"repo/mono/eglib/glist.c"
	"repo/mono/eglib/gqueue.c"
	"repo/mono/eglib/gpath.c"
	"repo/mono/eglib/gshell.c"
	"repo/mono/eglib/gspawn.c"
	"repo/mono/eglib/gfile.c"
	"repo/mono/eglib/gfile-posix.c"
	"repo/mono/eglib/gpattern.c"
	"repo/mono/eglib/gmarkup.c"
	"repo/mono/eglib/gutf8.c"
	"repo/mono/eglib/gunicode.c"
	"repo/mono/eglib/unicode-data.h"
)

set( EGLIB_POSIX_SOURCE
	"repo/mono/eglib/gdate-unix.c"
	"repo/mono/eglib/gdir-unix.c"
	"repo/mono/eglib/gfile-unix.c"
	"repo/mono/eglib/gmisc-unix.c"
	"repo/mono/eglib/gmodule-unix.c"
	"repo/mono/eglib/gtimer-unix.c"
)

set( EGLIB_WIN32_SOURCE
	"repo/mono/eglib/gdate-win32.c"
	"repo/mono/eglib/gdir-win32.c"
	"repo/mono/eglib/gfile-win32.c"
	"repo/mono/eglib/gmisc-win32.c"
	"repo/mono/eglib/gmodule-win32.c"
	"repo/mono/eglib/gmodule-win32-internals.h"
	"repo/mono/eglib/gtimer-win32.c"
	"repo/mono/eglib/gunicode-win32.c"
)

set( LIBGCMONOSGEN_SOURCE
	"repo/mono/sgen/gc-internal-agnostic.h"
	"repo/mono/sgen/sgen-alloc.c"
	"repo/mono/sgen/sgen-archdep.h"
	"repo/mono/sgen/sgen-cardtable.c"
	"repo/mono/sgen/sgen-cardtable.h"
	"repo/mono/sgen/sgen-client.h"
	"repo/mono/sgen/sgen-conf.h"
	"repo/mono/sgen/sgen-copy-object.h"
	"repo/mono/sgen/sgen-debug.c"
	"repo/mono/sgen/sgen-descriptor.c"
	"repo/mono/sgen/sgen-descriptor.h"
	"repo/mono/sgen/sgen-fin-weak-hash.c"
	"repo/mono/sgen/sgen-gc.c"
	"repo/mono/sgen/sgen-gc.h"
	"repo/mono/sgen/sgen-gchandles.c"
	"repo/mono/sgen/sgen-gray.c"
	"repo/mono/sgen/sgen-gray.h"
	"repo/mono/sgen/sgen-hash-table.c"
	"repo/mono/sgen/sgen-hash-table.h"
	"repo/mono/sgen/sgen-internal.c"
	"repo/mono/sgen/sgen-layout-stats.c"
	"repo/mono/sgen/sgen-layout-stats.h"
	"repo/mono/sgen/sgen-los.c"
	"repo/mono/sgen/sgen-major-copy-object.h"
	"repo/mono/sgen/sgen-marksweep-drain-gray-stack.h"
	"repo/mono/sgen/sgen-marksweep.c"
	"repo/mono/sgen/sgen-memory-governor.c"
	"repo/mono/sgen/sgen-memory-governor.h"
	"repo/mono/sgen/sgen-minor-copy-object.h"
	"repo/mono/sgen/sgen-minor-scan-object.h"
	"repo/mono/sgen/sgen-nursery-allocator.c"
	"repo/mono/sgen/sgen-pinning-stats.c"
	"repo/mono/sgen/sgen-pinning.c"
	"repo/mono/sgen/sgen-pinning.h"
	"repo/mono/sgen/sgen-pointer-queue.c"
	"repo/mono/sgen/sgen-pointer-queue.h"
	"repo/mono/sgen/sgen-array-list.h"
	"repo/mono/sgen/sgen-array-list.c"
	"repo/mono/sgen/sgen-protocol-def.h"
	"repo/mono/sgen/sgen-protocol.c"
	"repo/mono/sgen/sgen-protocol.h"
	"repo/mono/sgen/sgen-qsort.c"
	"repo/mono/sgen/sgen-qsort.h"
	"repo/mono/sgen/sgen-scan-object.h"
	"repo/mono/sgen/sgen-simple-nursery.c"
	"repo/mono/sgen/sgen-split-nursery.c"
	"repo/mono/sgen/sgen-tagged-pointer.h"
	"repo/mono/sgen/sgen-thread-pool.c"
	"repo/mono/sgen/sgen-thread-pool.h"
	"repo/mono/sgen/sgen-workers.c"
	"repo/mono/sgen/sgen-workers.h"
)
if( EMSCRIPTEN )
	set( LIBMINI_ARCH_SOURCE
		"repo/mono/mini/mini-wasm.c"
		"repo/mono/mini/mini-wasm.h"
		"repo/mono/mini/exceptions-wasm.c"
		"repo/mono/mini/tramp-wasm.c" )
elseif( CMAKE_SIZEOF_VOID_P EQUAL 8 )
	if( ANDROID OR ( APPLE AND IOS_PLATFORM ) )
		set( LIBMINI_ARCH_SOURCE
			"repo/mono/mini/mini-arm64.c"
			"repo/mono/mini/mini-arm64.h"
			"repo/mono/mini/exceptions-arm64.c"
			"repo/mono/mini/tramp-arm64.c"
			"repo/mono/mini/mini-arm64-gsharedvt.h"
			"repo/mono/mini/mini-arm64-gsharedvt.c"
			"repo/mono/mini/tramp-arm64-gsharedvt.c" )
	else()
		set( LIBMINI_ARCH_SOURCE
			"repo/mono/mini/mini-amd64.c"
			"repo/mono/mini/mini-amd64.h"
			"repo/mono/mini/exceptions-amd64.c"
			"repo/mono/mini/tramp-amd64.c"
			"repo/mono/mini/mini-amd64-gsharedvt.h"
			"repo/mono/mini/mini-amd64-gsharedvt.c"
			"repo/mono/mini/tramp-amd64-gsharedvt.c" )
	endif()
else()
	if( ANDROID OR ( APPLE AND IOS_PLATFORM ) )
		set( LIBMINI_ARCH_SOURCE
			"repo/mono/mini/mini-arm.c"
			"repo/mono/mini/mini-arm.h"
			"repo/mono/mini/exceptions-arm.c"
			"repo/mono/mini/tramp-arm.c"
			"repo/mono/mini/mini-arm-gsharedvt.c"
			"repo/mono/mini/tramp-arm-gsharedvt.c" )
	else()
		set( LIBMINI_ARCH_SOURCE
			"repo/mono/mini/mini-x86.c"
			"repo/mono/mini/mini-x86.h"
			"repo/mono/mini/exceptions-x86.c"
			"repo/mono/mini/tramp-x86.c"
			"repo/mono/mini/mini-x86-gsharedvt.c"
			"repo/mono/mini/tramp-x86-gsharedvt.c" )
	endif()
endif()
#set( LIBMINI_ARCH_SOURCE
	#"repo/mono/mini/mini-ppc.c"
	#"repo/mono/mini/mini-ppc.h"
	#"repo/mono/mini/exceptions-ppc.c"
	#"repo/mono/mini/tramp-ppc.c"
	#"repo/mono/mini/mini-mips.c"
	#"repo/mono/mini/mini-mips.h"
	#"repo/mono/mini/exceptions-mips.c"
	#"repo/mono/mini/tramp-mips.c"
	#"repo/mono/mini/mini-sparc.c"
	#"repo/mono/mini/mini-sparc.h"
	#"repo/mono/mini/exceptions-sparc.c"
	#"repo/mono/mini/tramp-sparc.c"
	#"repo/mono/mini/mini-s390x.c"
	#"repo/mono/mini/mini-s390x.h"
	#"repo/mono/mini/support-s390x.h"
	#"repo/mono/mini/exceptions-s390x.c"
	#"repo/mono/mini/tramp-s390x.c"
#)

set( LIBMINI_COMMON_SOURCE
	"repo/mono/mini/mini-runtime.c"
	"repo/mono/mini/seq-points.c"
	"repo/mono/mini/seq-points.h"
	"repo/mono/mini/ir-emit.h"
	"repo/mono/mini/method-to-ir.c"
	"repo/mono/mini/cfgdump.h"
	"repo/mono/mini/cfgdump.c"
	"repo/mono/mini/decompose.c"
	"repo/mono/mini/mini.h"
	"version.h"
	"repo/mono/mini/optflags-def.h"
	"repo/mono/mini/jit-icalls.h"
	"repo/mono/mini/jit-icalls.c"
	"repo/mono/mini/trace.c"
	"repo/mono/mini/trace.h"
	"repo/mono/mini/patch-info.h"
	"repo/mono/mini/mini-ops.h"
	"repo/mono/mini/mini-arch.h"
	"repo/mono/mini/dominators.c"
	"repo/mono/mini/cfold.c"
	"repo/mono/mini/regalloc.h"
	"repo/mono/mini/helpers.c"
	"repo/mono/mini/liveness.c"
	"repo/mono/mini/ssa.c"
	"repo/mono/mini/abcremoval.c"
	"repo/mono/mini/abcremoval.h"
	"repo/mono/mini/local-propagation.c"
	"repo/mono/mini/driver.c"
	"repo/mono/mini/debug-mini.c"
	"repo/mono/mini/linear-scan.c"
	"repo/mono/mini/aot-compiler.h"
	"repo/mono/mini/aot-compiler.c"
	"repo/mono/mini/aot-runtime.c"
	"repo/mono/mini/aot-runtime-wasm.c"
	"repo/mono/mini/graph.c"
	#"repo/mono/mini/mini-codegen.c"
	"repo/mono/mini/mini-exceptions.c"
	"repo/mono/mini/mini-trampolines.c"
	"repo/mono/mini/branch-opts.c"
	"repo/mono/mini/mini-generic-sharing.c"
	"repo/mono/mini/simd-methods.h"
	"repo/mono/mini/tasklets.c"
	"repo/mono/mini/tasklets.h"
	"repo/mono/mini/mini-native-types.c"
	"repo/mono/mini/simd-intrinsics.c"
	"repo/mono/mini/mini-unwind.h"
	"repo/mono/mini/unwind.c"
	"repo/mono/mini/image-writer.h"
	"repo/mono/mini/image-writer.c"
	"repo/mono/mini/dwarfwriter.h"
	"repo/mono/mini/dwarfwriter.c"
	"repo/mono/mini/mini-gc.h"
	"repo/mono/mini/mini-gc.c"
	"repo/mono/mini/debugger-agent.h"
	"repo/mono/mini/debugger-agent.c"
	"repo/mono/mini/debugger-engine.h"
	"repo/mono/mini/debugger-engine.c"
	"repo/mono/mini/debugger-agent-stubs.c"
	"repo/mono/mini/xdebug.c"
	"repo/mono/mini/mini-llvm.h"
	"repo/mono/mini/mini-llvm-cpp.h"
	"repo/mono/mini/llvm-jit.h"
	"repo/mono/mini/alias-analysis.c"
	"repo/mono/mini/mini-cross-helpers.c"
	"repo/mono/mini/arch-stubs.c"
	"repo/mono/mini/llvm-runtime.h"
	"repo/mono/mini/type-checking.c"
	"repo/mono/mini/lldb.h"
	"repo/mono/mini/lldb.c"
	"repo/mono/mini/memory-access.c"
	"repo/mono/mini/intrinsics.c"
	"repo/mono/mini/mini-profiler.c"
	"repo/mono/mini/interp-stubs.c"
)
if( NOT EMSCRIPTEN )
	set( LIBMINI_COMMON_SOURCE ${LIBMINI_COMMON_SOURCE}
		"repo/mono/mini/mini.c"
		"repo/mono/mini/mini-codegen.c" )
endif()

set( LIBMINI_INTERP_SOURCE
	"repo/mono/mini/interp/hacks.h"
	"repo/mono/mini/interp/interp.h"
	"repo/mono/mini/interp/interp-internals.h"
	"repo/mono/mini/interp/interp.c"
	"repo/mono/mini/interp/mintops.h"
	"repo/mono/mini/interp/mintops.c"
	"repo/mono/mini/interp/transform.c"
)

set( LIBMINI_LLVM_SOURCE
	"repo/mono/mini/mini-llvm-loaded.c"
	"repo/mono/mini/mini-llvm.c"
	"repo/mono/mini/llvm-runtime.cpp"
	"repo/mono/mini/mini-llvm-cpp.cpp"
	"repo/mono/mini/llvm-jit.cpp"
)

set( LIBMINI_POSIX_SOURCE
	"repo/mono/mini/mini-posix.c"
)
if( APPLE )
	set( LIBMINI_POSIX_SOURCE ${LIBMINI_POSIX_SOURCE}
		"repo/mono/mini/mini-darwin.c" )
endif()

set( LIBMINI_WIN32_SOURCE
	"repo/mono/mini/mini-windows.c"
	"repo/mono/mini/mini-windows.h"
)

set( LIBMONORUNTIME_COMMON_SOURCE
	"repo/mono/metadata/appdomain.c"
	"repo/mono/metadata/domain.c"
	"repo/mono/metadata/appdomain-icalls.h"
	"repo/mono/metadata/assembly.c"
	"repo/mono/metadata/assembly-internals.h"
	"repo/mono/metadata/attach.h"
	"repo/mono/metadata/attach.c"
	"repo/mono/metadata/cil-coff.h"
	"repo/mono/metadata/class.c"
	"repo/mono/metadata/class-getters.h"
	"repo/mono/metadata/class-internals.h"
	"repo/mono/metadata/class-inlines.h"
	"repo/mono/metadata/class-accessors.c"
	"repo/mono/metadata/class-init.c"
	"repo/mono/metadata/class-init.h"
	"repo/mono/metadata/class-private-definition.h"
	"repo/mono/metadata/cominterop.c"
	"repo/mono/metadata/cominterop.h"
	"repo/mono/metadata/console-io.h"
	"repo/mono/metadata/coree.c"
	"repo/mono/metadata/coree.h"
	"repo/mono/metadata/coree-internals.h"
	"repo/mono/metadata/culture-info.h"
	"repo/mono/metadata/culture-info-tables.h"
	"repo/mono/metadata/debug-helpers.c"
	"repo/mono/metadata/debug-mono-symfile.c"
	"repo/mono/metadata/debug-mono-ppdb.h"
	"repo/mono/metadata/debug-mono-ppdb.c"
	"repo/mono/metadata/decimal-ms.c"
	"repo/mono/metadata/decimal-ms.h"
	"repo/mono/metadata/domain-internals.h"
	"repo/mono/metadata/environment.c"
	"repo/mono/metadata/exception.c"
	"repo/mono/metadata/exception-internals.h"
	"repo/mono/metadata/w32file.c"
	"repo/mono/metadata/w32file.h"
	"repo/mono/metadata/w32file-internals.h"
	"repo/mono/metadata/filewatcher.c"
	"repo/mono/metadata/filewatcher.h"
	"repo/mono/metadata/gc-internals.h"
	"repo/mono/metadata/icall.c"
	"repo/mono/metadata/icall-table.c"
	"repo/mono/metadata/icall-table.h"
	"repo/mono/metadata/icall-internals.h"
	"repo/mono/metadata/icall-def.h"
	"repo/mono/metadata/image.c"
	"repo/mono/metadata/image-internals.h"
	"repo/mono/metadata/jit-info.c"
	"repo/mono/metadata/loader.c"
	"repo/mono/metadata/locales.c"
	"repo/mono/metadata/locales.h"
	"repo/mono/metadata/lock-tracer.c"
	"repo/mono/metadata/lock-tracer.h"
	"repo/mono/metadata/marshal.c"
	"repo/mono/metadata/marshal.h"
	"repo/mono/metadata/marshal-ilgen.c"
	"repo/mono/metadata/marshal-ilgen.h"
	"repo/mono/metadata/marshal-internals.h"
	"repo/mono/metadata/mempool.c"
	"repo/mono/metadata/mempool.h"
	"repo/mono/metadata/mempool-internals.h"
	"repo/mono/metadata/metadata.c"
	"repo/mono/metadata/metadata-verify.c"
	"repo/mono/metadata/metadata-internals.h"
	"repo/mono/metadata/method-builder.h"
	"repo/mono/metadata/method-builder-internals.h"
	"repo/mono/metadata/method-builder.c"
	"repo/mono/metadata/method-builder-ilgen.h"
	"repo/mono/metadata/method-builder-ilgen-internals.h"
	"repo/mono/metadata/method-builder-ilgen.c"
	"repo/mono/metadata/mono-basic-block.c"
	"repo/mono/metadata/mono-basic-block.h"
	"repo/mono/metadata/mono-config.c"
	"repo/mono/metadata/mono-debug.c"
	"repo/mono/metadata/debug-internals.h"
	"repo/mono/metadata/mono-endian.c"
	"repo/mono/metadata/mono-endian.h"
	"repo/mono/metadata/mono-hash.c"
	"repo/mono/metadata/mono-hash.h"
	"repo/mono/metadata/mono-conc-hash.c"
	"repo/mono/metadata/mono-conc-hash.h"
	"repo/mono/metadata/mono-mlist.c"
	"repo/mono/metadata/mono-mlist.h"
	"repo/mono/metadata/mono-perfcounters.c"
	"repo/mono/metadata/mono-perfcounters.h"
	"repo/mono/metadata/mono-perfcounters-def.h"
	"repo/mono/metadata/mono-ptr-array.h"
	#"repo/mono/metadata/mono-route.c"
	"repo/mono/metadata/mono-route.h"
	"repo/mono/metadata/monitor.h"
	"repo/mono/metadata/normalization-tables.h"
	"repo/mono/metadata/number-formatter.h"
	"repo/mono/metadata/number-ms.c"
	"repo/mono/metadata/number-ms.h"
	"repo/mono/metadata/object.c"
	"repo/mono/metadata/object-internals.h"
	"repo/mono/metadata/opcodes.c"
	"repo/mono/metadata/property-bag.h"
	"repo/mono/metadata/property-bag.c"
	"repo/mono/metadata/w32socket.c"
	"repo/mono/metadata/w32socket.h"
	"repo/mono/metadata/w32socket-internals.h"
	"repo/mono/metadata/w32process.c"
	"repo/mono/metadata/w32process.h"
	"repo/mono/metadata/w32process-internals.h"
	"repo/mono/metadata/profiler.c"
	"repo/mono/metadata/profiler-private.h"
	"repo/mono/metadata/rand.h"
	"repo/mono/metadata/rand.c"
	"repo/mono/metadata/remoting.h"
	"repo/mono/metadata/remoting.c"
	"repo/mono/metadata/runtime.c"
	"repo/mono/metadata/mono-security.c"
	"repo/mono/metadata/security.h"
	"repo/mono/metadata/security-core-clr.c"
	"repo/mono/metadata/security-core-clr.h"
	"repo/mono/metadata/security-manager.c"
	"repo/mono/metadata/security-manager.h"
	"repo/mono/metadata/string-icalls.c"
	"repo/mono/metadata/string-icalls.h"
	"repo/mono/metadata/sysmath.h"
	"repo/mono/metadata/sysmath.c"
	"repo/mono/metadata/tabledefs.h"
	"repo/mono/metadata/threads.c"
	"repo/mono/metadata/threads-types.h"
	"repo/mono/metadata/threadpool.c"
	"repo/mono/metadata/threadpool.h"
	"repo/mono/metadata/threadpool-worker-default.c"
	"repo/mono/metadata/threadpool-worker.h"
	"repo/mono/metadata/threadpool-io.c"
	"repo/mono/metadata/threadpool-io.h"
	"repo/mono/metadata/verify.c"
	"repo/mono/metadata/verify-internals.h"
	"repo/mono/metadata/wrapper-types.h"
	"repo/mono/metadata/dynamic-image-internals.h"
	"repo/mono/metadata/dynamic-stream.c"
	"repo/mono/metadata/dynamic-stream-internals.h"
	"repo/mono/metadata/reflection-cache.h"
	"repo/mono/metadata/custom-attrs-internals.h"
	"repo/mono/metadata/sre-internals.h"
	"repo/mono/metadata/reflection-internals.h"
	#"repo/mono/metadata/file-mmap-posix.c"
	#"repo/mono/metadata/file-mmap-windows.c"
	"repo/mono/metadata/file-mmap.h"
	"repo/mono/metadata/object-offsets.h"
	"repo/mono/metadata/abi-details.h"
	"repo/mono/metadata/class-abi-details.h"
	"repo/mono/metadata/metadata-cross-helpers.c"
	"repo/mono/metadata/seq-points-data.h"
	"repo/mono/metadata/seq-points-data.c"
	"repo/mono/metadata/handle.c"
	"repo/mono/metadata/handle.h"
	"repo/mono/metadata/w32mutex.h"
	"repo/mono/metadata/w32semaphore.h"
	"repo/mono/metadata/w32event.h"
	"repo/mono/metadata/w32handle-namespace.h"
	#"repo/mono/metadata/w32handle-namespace.c"
	"repo/mono/metadata/w32handle.h"
	"repo/mono/metadata/w32handle.c"
	"repo/mono/metadata/w32error.h"
	"repo/mono/metadata/reflection.c"
	"repo/mono/metadata/dynamic-image.c"
	"repo/mono/metadata/sre.c"
	"repo/mono/metadata/sre-encode.c"
	"repo/mono/metadata/sre-save.c"
	"repo/mono/metadata/custom-attrs.c"
	"repo/mono/metadata/fdhandle.h"
	"repo/mono/metadata/fdhandle.c"
	"repo/mono/metadata/callspec.h"
	"repo/mono/metadata/callspec.c"
	"repo/mono/metadata/runtime.h"
	"repo/mono/metadata/mono-config-dirs.h"
	"repo/mono/metadata/mono-config-dirs.c"
	#"repo/mono/metadata/console-null.c"
	"repo/mono/metadata/null-gc.c"
	"repo/mono/metadata/null-gc-handles.h"
	"repo/mono/metadata/null-gc-handles.c"
	"repo/mono/metadata/gc-stats.c"
	"repo/mono/metadata/gc.c"
	"repo/mono/metadata/monitor.c"
	"repo/mono/metadata/boehm-gc.c"
	"repo/mono/metadata/sgen-bridge.c"
	"repo/mono/metadata/sgen-bridge-internals.h"
	"repo/mono/metadata/sgen-old-bridge.c"
	"repo/mono/metadata/sgen-new-bridge.c"
	"repo/mono/metadata/sgen-tarjan-bridge.c"
	"repo/mono/metadata/sgen-toggleref.c"
	"repo/mono/metadata/sgen-toggleref.h"
	"repo/mono/metadata/sgen-stw.c"
	"repo/mono/metadata/sgen-mono.c"
	"repo/mono/metadata/sgen-mono.h"
	"repo/mono/metadata/sgen-mono-ilgen.c"
	"repo/mono/metadata/sgen-mono-ilgen.h"
	"repo/mono/metadata/sgen-client-mono.h"
	"repo/mono/metadata/assembly.h"
	"repo/mono/metadata/attrdefs.h"
	"repo/mono/metadata/appdomain.h"
	"repo/mono/metadata/blob.h"
	"repo/mono/metadata/class.h"
	"repo/mono/metadata/debug-helpers.h"
	"repo/mono/metadata/debug-mono-symfile.h"
	"repo/mono/metadata/threads.h"
	"repo/mono/metadata/environment.h"
	"repo/mono/metadata/exception.h"
	"repo/mono/metadata/image.h"
	"repo/mono/metadata/loader.h"
	"repo/mono/metadata/metadata.h"
	"repo/mono/metadata/mono-config.h"
	"repo/mono/metadata/mono-debug.h"
	"repo/mono/metadata/mono-gc.h"
	"repo/mono/metadata/sgen-bridge.h"
	"repo/mono/metadata/object.h"
	"repo/mono/metadata/opcodes.h"
	"repo/mono/metadata/profiler.h"
	"repo/mono/metadata/profiler-events.h"
	"repo/mono/metadata/reflection.h"
	"repo/mono/metadata/row-indexes.h"
	"repo/mono/metadata/tokentype.h"
	"repo/mono/metadata/verify.h"
)
if( WIN32 )
	set( LIBMONORUNTIME_COMMON_SOURCE ${LIBMONORUNTIME_COMMON_SOURCE}
		"repo/mono/metadata/file-mmap-windows.c" )
else()
	set( LIBMONORUNTIME_COMMON_SOURCE ${LIBMONORUNTIME_COMMON_SOURCE}
		"repo/mono/metadata/mono-route.c"
		"repo/mono/metadata/w32handle-namespace.c"
		"repo/mono/metadata/file-mmap-posix.c" )
endif()

set( LIBMONORUNTIME_POSIX_SOURCE
	"repo/mono/metadata/console-unix.c"
	"repo/mono/metadata/w32mutex-unix.c"
	"repo/mono/metadata/w32semaphore-unix.c"
	"repo/mono/metadata/w32event-unix.c"
	"repo/mono/metadata/w32process-unix.c"
	"repo/mono/metadata/w32process-unix-internals.h"
	"repo/mono/metadata/w32process-unix-osx.c"
	"repo/mono/metadata/w32process-unix-bsd.c"
	"repo/mono/metadata/w32process-unix-haiku.c"
	"repo/mono/metadata/w32process-unix-default.c"
	"repo/mono/metadata/w32socket-unix.c"
	"repo/mono/metadata/w32file-unix.c"
	"repo/mono/metadata/w32file-unix-glob.c"
	"repo/mono/metadata/w32file-unix-glob.h"
	"repo/mono/metadata/w32error-unix.c"
)

set( LIBMONORUNTIME_WIN32_SOURCE
	"repo/mono/metadata/console-win32.c"
	"repo/mono/metadata/console-win32-internals.h"
	"repo/mono/metadata/cominterop-win32-internals.h"
	"repo/mono/metadata/w32file-win32.c"
	"repo/mono/metadata/w32file-win32-internals.h"
	"repo/mono/metadata/icall-windows.c"
	"repo/mono/metadata/icall-windows-internals.h"
	"repo/mono/metadata/marshal-windows.c"
	"repo/mono/metadata/marshal-windows-internals.h"
	"repo/mono/metadata/mono-security-windows.c"
	"repo/mono/metadata/mono-security-windows-internals.h"
	"repo/mono/metadata/w32mutex-win32.c"
	"repo/mono/metadata/w32semaphore-win32.c"
	"repo/mono/metadata/w32event-win32.c"
	"repo/mono/metadata/w32process-win32.c"
	"repo/mono/metadata/w32process-win32-internals.h"
	"repo/mono/metadata/w32socket-win32.c"
	"repo/mono/metadata/w32error-win32.c"
)

set( LIBMONOUTILS_ARCH_SOURCE
	""
	#"repo/mono/utils/mach-support-x86.c"
	#"repo/mono/utils/mono-hwcap-x86.c"
	#"repo/mono/utils/mach-support-amd64.c"
	#"repo/mono/utils/mach-support-arm.c"
	#"repo/mono/utils/mono-hwcap-arm.c"
	#"repo/mono/utils/mach-support-arm64.c"
	#"repo/mono/utils/mono-hwcap-arm64.c"
	#"repo/mono/utils/mono-hwcap-mips.c"
	#"repo/mono/utils/mono-hwcap-ppc.c"
	#"repo/mono/utils/mono-hwcap-sparc.c"
	#"repo/mono/utils/mono-hwcap-os390x.c"
	#"repo/mono/utils/mono-hwcap-wasm.c"
	#"repo/mono/utils/mach-support-unknown.c"
	#"repo/mono/utils/mono-hwcap-cross.c"
)
if( APPLE AND IOS_PLATFORM )
	set( LIBMONOUTILS_ARCH_SOURCE ${LIBMONOUTILS_ARCH_SOURCE}
		"repo/mono/utils/mono-hwcap-arm64.c"
		"repo/mono/utils/mach-support-arm64.c" )
elseif( ANDROID )
	set( LIBMONOUTILS_ARCH_SOURCE ${LIBMONOUTILS_ARCH_SOURCE}
		"repo/mono/utils/mono-hwcap-arm.c"
		"repo/mono/utils/mach-support-arm.c" )
elseif( EMSCRIPTEN )
	set( LIBMONOUTILS_ARCH_SOURCE ${LIBMONOUTILS_ARCH_SOURCE}
		"repo/mono/utils/mono-hwcap-wasm.c"
		"repo/mono/utils/mach-support-unknown.c" )
else()
	if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		set( LIBMONOUTILS_ARCH_SOURCE ${LIBMONOUTILS_ARCH_SOURCE}
			"repo/mono/utils/mono-hwcap-x86.c"
			"repo/mono/utils/mach-support-amd64.c" )
	else()
		set( LIBMONOUTILS_ARCH_SOURCE ${LIBMONOUTILS_ARCH_SOURCE}
			"repo/mono/utils/mono-hwcap-x86.c"
			"repo/mono/utils/mach-support-x86.c" )
	endif()
endif()

set( LIBMONOUTILS_COMMON_SOURCE
	"repo/mono/utils/mono-md5.c"
	"repo/mono/utils/mono-sha1.c"
	"repo/mono/utils/mono-logger.c"
	"repo/mono/utils/mono-logger-internals.h"
	"repo/mono/utils/mono-codeman.c"
	"repo/mono/utils/dlmalloc.c"
	"repo/mono/utils/dlmalloc.h"
	"repo/mono/utils/mono-counters.c"
	"repo/mono/utils/mono-compiler.h"
	"repo/mono/utils/mono-complex.h"
	"repo/mono/utils/mono-dl.c"
	#"repo/mono/utils/mono-dl-windows.c"
	#"repo/mono/utils/mono-dl-darwin.c"
	#"repo/mono/utils/mono-dl-posix.c"
	#"repo/mono/utils/mono-dl-wasm.c"
	"repo/mono/utils/mono-dl.h"
	"repo/mono/utils/mono-dl-windows-internals.h"
	#"repo/mono/utils/mono-log-windows.c"
	"repo/mono/utils/mono-log-common.c"
	#"repo/mono/utils/mono-log-posix.c"
	#"repo/mono/utils/mono-log-android.c"
	#"repo/mono/utils/mono-log-darwin.c"
	"repo/mono/utils/mono-internal-hash.c"
	"repo/mono/utils/mono-internal-hash.h"
	"repo/mono/utils/mono-io-portability.c"
	"repo/mono/utils/mono-io-portability.h"
	"repo/mono/utils/monobitset.c"
	"repo/mono/utils/mono-filemap.c"
	"repo/mono/utils/mono-math.c"
	"repo/mono/utils/mono-mmap.c"
	"repo/mono/utils/mono-mmap-windows.c"
	"repo/mono/utils/mono-mmap.h"
	"repo/mono/utils/mono-mmap-internals.h"
	"repo/mono/utils/mono-mmap-windows-internals.h"
	"repo/mono/utils/mono-os-mutex.h"
	#"repo/mono/utils/mono-os-mutex.c"
	"repo/mono/utils/mono-os-wait.h"
	"repo/mono/utils/mono-coop-mutex.h"
	"repo/mono/utils/mono-once.h"
	"repo/mono/utils/mono-lazy-init.h"
	"repo/mono/utils/mono-networkinterfaces.c"
	"repo/mono/utils/mono-networkinterfaces.h"
	"repo/mono/utils/mono-proclib.c"
	"repo/mono/utils/mono-proclib-windows.c"
	"repo/mono/utils/mono-proclib.h"
	"repo/mono/utils/mono-proclib-windows-internals.h"
	"repo/mono/utils/mono-publib.c"
	"repo/mono/utils/mono-string.h"
	"repo/mono/utils/mono-time.c"
	"repo/mono/utils/mono-time.h"
	"repo/mono/utils/strenc.h"
	"repo/mono/utils/strenc.c"
	"repo/mono/utils/mono-uri.c"
	"repo/mono/utils/mono-utils-debug.c"
	"repo/mono/utils/mono-utils-debug.h"
	"repo/mono/utils/mono-poll.c"
	"repo/mono/utils/mono-path.c"
	"repo/mono/utils/mono-os-semaphore.h"
	"repo/mono/utils/mono-coop-semaphore.h"
	"repo/mono/utils/mono-sigcontext.h"
	"repo/mono/utils/mono-stdlib.c"
	"repo/mono/utils/mono-property-hash.h"
	"repo/mono/utils/mono-property-hash.c"
	"repo/mono/utils/mono-value-hash.h"
	"repo/mono/utils/mono-value-hash.c"
	"repo/mono/utils/freebsd-elf_common.h"
	"repo/mono/utils/freebsd-elf32.h"
	"repo/mono/utils/freebsd-elf64.h"
	"repo/mono/utils/freebsd-dwarf.h"
	"repo/mono/utils/dtrace.h"
	"repo/mono/utils/gc_wrapper.h"
	"repo/mono/utils/mono-error.c"
	"repo/mono/utils/mono-error-internals.h"
	"repo/mono/utils/monobitset.h"
	"repo/mono/utils/mono-codeman.h"
	"repo/mono/utils/mono-digest.h"
	"repo/mono/utils/mono-machine.h"
	"repo/mono/utils/mono-math.h"
	"repo/mono/utils/mono-membar.h"
	"repo/mono/utils/mono-path.h"
	"repo/mono/utils/mono-poll.h"
	"repo/mono/utils/mono-uri.h"
	"repo/mono/utils/mono-stdlib.h"
	"repo/mono/utils/valgrind.h"
	"repo/mono/utils/mach-support.h"
	"repo/mono/utils/memcheck.h"
	"repo/mono/utils/mono-context.c"
	"repo/mono/utils/mono-context.h"
	"repo/mono/utils/mono-stack-unwinding.h"
	"repo/mono/utils/hazard-pointer.c"
	"repo/mono/utils/hazard-pointer.h"
	"repo/mono/utils/lock-free-queue.c"
	"repo/mono/utils/lock-free-queue.h"
	"repo/mono/utils/lock-free-alloc.c"
	"repo/mono/utils/lock-free-alloc.h"
	"repo/mono/utils/lock-free-array-queue.c"
	"repo/mono/utils/lock-free-array-queue.h"
	"repo/mono/utils/mono-linked-list-set.c"
	"repo/mono/utils/mono-linked-list-set.h"
	"repo/mono/utils/mono-threads.c"
	"repo/mono/utils/mono-threads-state-machine.c"
	#"repo/mono/utils/mono-threads-posix.c"
	#"repo/mono/utils/mono-threads-posix-signals.c"
	#"repo/mono/utils/mono-threads-mach.c"
	#"repo/mono/utils/mono-threads-mach-helper.c"
	#"repo/mono/utils/mono-threads-windows.c"
	#"repo/mono/utils/mono-threads-linux.c"
	#"repo/mono/utils/mono-threads-freebsd.c"
	#"repo/mono/utils/mono-threads-netbsd.c"
	#"repo/mono/utils/mono-threads-openbsd.c"
	#"repo/mono/utils/mono-threads-android.c"
	#"repo/mono/utils/mono-threads-haiku.c"
	#"repo/mono/utils/mono-threads-wasm.c"
	"repo/mono/utils/mono-threads.h"
	"repo/mono/utils/mono-threads-debug.h"
	"repo/mono/utils/mono-threads-api.h"
	"repo/mono/utils/mono-threads-coop.c"
	"repo/mono/utils/mono-threads-coop.h"
	"repo/mono/utils/mono-tls.h"
	"repo/mono/utils/mono-tls.c"
	"repo/mono/utils/linux_magic.h"
	"repo/mono/utils/mono-memory-model.h"
	"repo/mono/utils/atomic.c"
	"repo/mono/utils/atomic.h"
	"repo/mono/utils/mono-hwcap.c"
	"repo/mono/utils/mono-hwcap.h"
	"repo/mono/utils/mono-hwcap-vars.h"
	"repo/mono/utils/bsearch.h"
	"repo/mono/utils/bsearch.c"
	"repo/mono/utils/mono-signal-handler.h"
	"repo/mono/utils/mono-conc-hashtable.h"
	"repo/mono/utils/mono-conc-hashtable.c"
	"repo/mono/utils/json.h"
	"repo/mono/utils/json.c"
	"repo/mono/utils/networking.c"
	"repo/mono/utils/networking-posix.c"
	"repo/mono/utils/networking-fallback.c"
	"repo/mono/utils/networking-missing.c"
	"repo/mono/utils/networking-windows.c"
	"repo/mono/utils/networking.h"
	"repo/mono/utils/mono-rand.c"
	"repo/mono/utils/mono-rand-windows.c"
	"repo/mono/utils/mono-rand.h"
	"repo/mono/utils/mono-rand-windows-internals.h"
	"repo/mono/utils/memfuncs.c"
	"repo/mono/utils/memfuncs.h"
	"repo/mono/utils/parse.c"
	"repo/mono/utils/parse.h"
	"repo/mono/utils/checked-build.c"
	"repo/mono/utils/checked-build.h"
	"repo/mono/utils/os-event.h"
	"repo/mono/utils/refcount.h"
	"repo/mono/utils/w32api.h"
	"repo/mono/utils/unlocked.h"
	"repo/mono/utils/mono-logger.h"
	"repo/mono/utils/mono-error.h"
	"repo/mono/utils/mono-publib.h"
	"repo/mono/utils/mono-dl-fallback.h"
	"repo/mono/utils/mono-counters.h"
)
if( APPLE )
	set( LIBMONOUTILS_COMMON_SOURCE ${LIBMONOUTILS_COMMON_SOURCE}
		"repo/mono/utils/mono-merp.c"
		"repo/mono/utils/mono-state.c"
		"repo/mono/utils/mono-os-mutex.c"
		"repo/mono/utils/mono-dl-posix.c"
		"repo/mono/utils/mono-dl-darwin.c"
		"repo/mono/utils/mono-log-posix.c"
		"repo/mono/utils/mono-log-darwin.c"
		"repo/mono/utils/mono-threads-mach.c"
		"repo/mono/utils/mono-threads-mach-helper.c"
		"repo/mono/utils/mono-threads-posix.c"
		"repo/mono/utils/mono-threads-posix-signals.c" )
elseif( ANDROID )
	set( LIBMONOUTILS_COMMON_SOURCE ${LIBMONOUTILS_COMMON_SOURCE}
		"repo/mono/utils/mono-os-mutex.c"
		"repo/mono/utils/mono-dl-posix.c"
		"repo/mono/utils/mono-log-posix.c"
		"repo/mono/utils/mono-log-android.c"
		"repo/mono/utils/mono-threads-android.c"
		"repo/mono/utils/mono-threads-posix.c"
		"repo/mono/utils/mono-threads-posix-signals.c" )
elseif( EMSCRIPTEN )
	set( LIBMONOUTILS_COMMON_SOURCE ${LIBMONOUTILS_COMMON_SOURCE}
		"repo/mono/utils/mono-os-mutex.c"
		"repo/mono/utils/mono-dl-wasm.c"
		"repo/mono/utils/mono-log-posix.c"
		"repo/mono/utils/mono-threads-wasm.c" )
elseif( ${CMAKE_SYSTEM_NAME} MATCHES "Linux" )
	set( LIBMONOUTILS_COMMON_SOURCE ${LIBMONOUTILS_COMMON_SOURCE}
		"repo/mono/utils/mono-os-mutex.c"
		"repo/mono/utils/mono-dl-posix.c"
		"repo/mono/utils/mono-log-posix.c"
		"repo/mono/utils/mono-threads-linux.c"
		"repo/mono/utils/mono-threads-posix.c"
		"repo/mono/utils/mono-threads-posix-signals.c" )
elseif( WIN32 )
	set( LIBMONOUTILS_COMMON_SOURCE ${LIBMONOUTILS_COMMON_SOURCE}
		"repo/mono/utils/mono-dl-windows.c"
		"repo/mono/utils/mono-log-windows.c"
		"repo/mono/utils/mono-threads-windows.c" )
endif()

set( LIBMONOUTILS_POSIX_SOURCE
	"repo/mono/utils/os-event-unix.c"
)

set( LIBMONOUTILS_WIN32_SOURCE
	"repo/mono/utils/os-event-win32.c"
	"repo/mono/utils/mono-os-wait-win32.c"
)

if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
	set( LIBMONOUTILS_WIN32_SOURCE ${LIBMONOUTILS_WIN32_SOURCE}
		"repo/mono/utils/win64.asm" )
endif()

set( MONOPOSIXHELPER_SOURCE
	"repo/support/adler32.c"
	"repo/support/compress.c"
	"repo/support/crc32.c"
	"repo/support/deflate.c"
	"repo/support/errno.c"
	"repo/support/infback.c"
	"repo/support/inffast.c"
	"repo/support/inflate.c"
	"repo/support/inftrees.c"
	"repo/support/map.c"
	"repo/support/signal.c"
	"repo/support/stdio.c"
	"repo/support/stdlib.c"
	"repo/support/string.c"
	"repo/support/trees.c"
	"repo/support/uncompr.c"
	"repo/support/zlib-helper.c"
	"repo/support/zutil.c"
	"repo/support/minizip/unzip.c"
	"repo/support/minizip/zip.c"
	"repo/support/minizip/ioapi.c"
	"repo/support/fcntl.c"
	"repo/support/sys-stat.c"
	"repo/support/crc32.h"
	"repo/support/deflate.h"
	"repo/support/inffast.h"
	"repo/support/inffixed.h"
	"repo/support/inflate.h"
	"repo/support/inftrees.h"
	"repo/support/map.h"
	"repo/support/mph.h"
	"repo/support/trees.h"
	"repo/support/zconf.h"
	"repo/support/zutil.h"
	"repo/support/minizip/crypt.h"
	"repo/support/minizip/unzip.h"
	"repo/support/minizip/iowin32.h"
	"repo/support/minizip/zip.h"
	"repo/support/zlib.h"
	"repo/support/minizip/ioapi.h"
)

#add_definitions( "-DNDEBUG" )
add_definitions( "-D_LIB" )
add_definitions( "-D_UNICODE" )
add_definitions( "-DUNICODE" )
add_definitions( "-D__default_codegen__" )
add_definitions( "-DHAVE_CONFIG_H" )
add_definitions( "-DFD_SETSIZE=1024" )
add_definitions( "-DNVALGRIND" )
add_definitions( "-DHAVE_SGEN_GC" )
add_definitions( "-DGC_NOT_DLL" )
add_definitions( "-DHAVE_CONC_GC_AS_DEFAULT" )
add_definitions( "-DHAVE_WRITE_BARRIERS" )
add_definitions( "-DHAVE_MOVING_COLLECTOR" )

include_directories( "." )
include_directories( "repo" )
include_directories( "repo/mono" )
include_directories( "repo/mono/eglib" )

if( WIN32 )
	add_definitions( "-DWIN32_LEAN_AND_MEAN" )
	add_definitions( "-DWIN32_THREADS" )
	add_definitions( "-DWINVER=0x0600" )
	add_definitions( "-D_WIN32_WINNT=0x0600" )
	add_definitions( "-D_WIN32_IE=0x0501" )
	add_definitions( "-D_UNICODE" )
	add_definitions( "-DUNICODE" )
	add_definitions( "-DFD_SETSIZE=1024" )
	add_definitions( "-mms-bitfields" )
	add_definitions( "-mwindows" )
	if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		add_definitions( "-DWIN64" )
	endif()
	
	add_library( libmono STATIC
		${EGLIB_COMMON_SOURCE} ${EGLIB_WIN32_SOURCE}
		${LIBGCMONOSGEN_SOURCE}
		${LIBMINI_ARCH_SOURCE} ${LIBMINI_COMMON_SOURCE} ${LIBMINI_INTERP_SOURCE} ${LIBMINI_WIN32_SOURCE}
		${LIBMONORUNTIME_COMMON_SOURCE} ${LIBMONORUNTIME_WIN32_SOURCE} ${LIBMONOUTILS_ARCH_SOURCE}
		${LIBMONOUTILS_COMMON_SOURCE} ${LIBMONOUTILS_WIN32_SOURCE} )
	
	target_link_libraries( libmono "mswsock" "ws2_32" "ole32" "oleaut32" "psapi" "version" "advapi32" "winmm" "kernel32" "iphlpapi" )
	
else()
	if( APPLE )
		add_definitions( "-Wno-deprecated-declarations" )
		add_definitions( "-D_THREAD_SAFE" )
		add_definitions( "-DGC_MACOSX_THREADS" )
		add_definitions( "-DUSE_MUNMAP" )
	else()
		add_definitions( "-D_GNU_SOURCE" )
		add_definitions( "-D_REENTRANT" )
		add_definitions( "-DGC_LINUX_THREADS" )
	endif()
	if( EMSCRIPTEN )
		add_definitions( "-DNO_UNALIGNED_ACCESS" )
	endif()
	if( ANDROID )
		add_definitions( "-DARM_FPU_NONE=1" )
		add_definitions( "-DPAGE_SIZE=4096" )
		add_definitions( "-DS_IWRITE=S_IWUSR" )
	endif()
	
	add_definitions( "-DUSE_MMAP" )
	
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -g -fno-strict-aliasing -fwrapv" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -g" )
	
	add_library( libmono STATIC
		${EGLIB_COMMON_SOURCE} ${EGLIB_POSIX_SOURCE}
		${LIBGCMONOSGEN_SOURCE}
		${LIBMINI_ARCH_SOURCE} ${LIBMINI_COMMON_SOURCE} ${LIBMINI_INTERP_SOURCE} ${LIBMINI_POSIX_SOURCE}
		${LIBMONORUNTIME_COMMON_SOURCE} ${LIBMONORUNTIME_POSIX_SOURCE} ${LIBMONOUTILS_ARCH_SOURCE}
		${LIBMONOUTILS_COMMON_SOURCE} ${LIBMONOUTILS_POSIX_SOURCE} )
	
	if( APPLE OR ANDROID )
		target_link_libraries( libmono "m" "dl" )
	else()
		target_link_libraries( libmono "m" "rt" "dl" )
	endif()
endif()
