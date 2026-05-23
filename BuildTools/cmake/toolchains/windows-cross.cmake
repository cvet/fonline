# Cross-compile from a POSIX host (typically Linux) to Windows MSVC ABI targets,
# using clang-cl + lld-link with the Windows SDK / CRT staged by xwin.
#
# Required env:
#   FO_XWIN_ROOT - absolute path to the xwin "splat" output (must contain crt/ and sdk/).
#
# Optional env (override only when the auto-detected tool isn't right):
#   FO_CLANG_CL  - clang-cl binary used as both C and CXX compiler.
#   FO_LLVM_ML   - MASM-compatible assembler used for ASM_MASM sources.
#
# Defaults to win64 (x86_64). Override CMAKE_SYSTEM_PROCESSOR via the cache
# (e.g. -DCMAKE_SYSTEM_PROCESSOR=x86) before including this file to target win32.

set(CMAKE_SYSTEM_NAME Windows)
if(NOT DEFINED CMAKE_SYSTEM_PROCESSOR OR CMAKE_SYSTEM_PROCESSOR STREQUAL "")
    set(CMAKE_SYSTEM_PROCESSOR x86_64)
endif()
set(CMAKE_CROSSCOMPILING TRUE)

set(_fo_xwin_root "$ENV{FO_XWIN_ROOT}")
if(_fo_xwin_root STREQUAL "")
    message(FATAL_ERROR "FO_XWIN_ROOT is not set. Splat the Windows SDK via xwin and export FO_XWIN_ROOT to that directory.")
endif()
if(NOT EXISTS "${_fo_xwin_root}/crt" OR NOT EXISTS "${_fo_xwin_root}/sdk")
    message(FATAL_ERROR "FO_XWIN_ROOT=${_fo_xwin_root} does not look like an xwin splat output (missing crt/ or sdk/).")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    set(_fo_xwin_arch_dir "x86_64")
    set(_fo_xwin_target_triple "x86_64-pc-windows-msvc")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86|i[3-6]86)$")
    set(_fo_xwin_arch_dir "x86")
    set(_fo_xwin_target_triple "i686-pc-windows-msvc")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(_fo_xwin_arch_dir "aarch64")
    set(_fo_xwin_target_triple "aarch64-pc-windows-msvc")
else()
    message(FATAL_ERROR "Unsupported CMAKE_SYSTEM_PROCESSOR for windows-cross: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

function(_fo_find_versioned_llvm_tool out_var env_var)
    set(_resolved "$ENV{${env_var}}")
    if(NOT _resolved STREQUAL "")
        set(${out_var} "${_resolved}" PARENT_SCOPE)
        return()
    endif()
    # Use a per-invocation cache slot so find_program does not reuse a hit
    # from a previous tool lookup in the same configure.
    set(_cache_var "_fo_llvm_tool_${out_var}")
    find_program(${_cache_var} NAMES ${ARGN} REQUIRED)
    set(${out_var} "${${_cache_var}}" PARENT_SCOPE)
endfunction()

_fo_find_versioned_llvm_tool(_fo_clang_cl FO_CLANG_CL clang-cl-20 clang-cl-19 clang-cl)
_fo_find_versioned_llvm_tool(_fo_llvm_ml  FO_LLVM_ML  llvm-ml-20  llvm-ml-19  llvm-ml)
_fo_find_versioned_llvm_tool(_fo_lld_link FO_LLD_LINK lld-link-20 lld-link-19 lld-link)
_fo_find_versioned_llvm_tool(_fo_llvm_rc  FO_LLVM_RC  llvm-rc-20  llvm-rc-19  llvm-rc)
_fo_find_versioned_llvm_tool(_fo_llvm_mt  FO_LLVM_MT  llvm-mt-20  llvm-mt-19  llvm-mt)
_fo_find_versioned_llvm_tool(_fo_llvm_lib FO_LLVM_LIB llvm-lib-20 llvm-lib-19 llvm-lib)

set(CMAKE_C_COMPILER "${_fo_clang_cl}")
set(CMAKE_CXX_COMPILER "${_fo_clang_cl}")
set(CMAKE_RC_COMPILER "${_fo_llvm_rc}")
set(CMAKE_MT "${_fo_llvm_mt}")
set(CMAKE_AR "${_fo_llvm_lib}")
set(CMAKE_LINKER "${_fo_lld_link}")
set(CMAKE_ASM_MASM_COMPILER "${_fo_llvm_ml}")
set(CMAKE_C_COMPILER_TARGET "${_fo_xwin_target_triple}" CACHE STRING "clang-cl Windows target triple" FORCE)
set(CMAKE_CXX_COMPILER_TARGET "${_fo_xwin_target_triple}" CACHE STRING "clang-cl Windows target triple" FORCE)

# xwin ships only Release CRTs; MSVC Debug CRTs are not redistributable, so all
# configurations link the Release dynamic CRT.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
cmake_policy(SET CMP0091 NEW)

set(_fo_xwin_includes
    "/imsvc${_fo_xwin_root}/crt/include"
    "/imsvc${_fo_xwin_root}/sdk/include/ucrt"
    "/imsvc${_fo_xwin_root}/sdk/include/um"
    "/imsvc${_fo_xwin_root}/sdk/include/shared"
    "/imsvc${_fo_xwin_root}/sdk/include/winrt")
list(JOIN _fo_xwin_includes " " _fo_xwin_includes_flag)

# clang-cl on a Linux host can otherwise keep its x86_64 Windows default even
# when CMake's processor/linker settings select the x86 SDK and /machine:X86.
set(_fo_xwin_target_flag "--target=${_fo_xwin_target_triple}")
set(CMAKE_C_FLAGS_INIT "${_fo_xwin_target_flag} ${_fo_xwin_includes_flag} /D_CRT_SECURE_NO_WARNINGS")
set(CMAKE_CXX_FLAGS_INIT "${_fo_xwin_target_flag} ${_fo_xwin_includes_flag} /D_CRT_SECURE_NO_WARNINGS /EHsc")

set(_fo_xwin_libpaths
    "/libpath:${_fo_xwin_root}/crt/lib/${_fo_xwin_arch_dir}"
    "/libpath:${_fo_xwin_root}/sdk/lib/ucrt/${_fo_xwin_arch_dir}"
    "/libpath:${_fo_xwin_root}/sdk/lib/um/${_fo_xwin_arch_dir}")
list(JOIN _fo_xwin_libpaths " " _fo_xwin_libpaths_flag)
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_fo_xwin_libpaths_flag}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_fo_xwin_libpaths_flag}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_fo_xwin_libpaths_flag}")

set(CMAKE_RC_FLAGS_INIT "-I${_fo_xwin_root}/sdk/include/um -I${_fo_xwin_root}/sdk/include/shared")

# llvm-ml defaults to 32-bit when invoked under its bare name. Pin to the
# matching bitness for x86_64 / aarch64 so 64-bit registers and SEH directives
# parse correctly.
if(_fo_xwin_arch_dir STREQUAL "x86_64")
    set(CMAKE_ASM_MASM_FLAGS_INIT "-m64")
elseif(_fo_xwin_arch_dir STREQUAL "aarch64")
    set(CMAKE_ASM_MASM_FLAGS_INIT "-marm64")
endif()
