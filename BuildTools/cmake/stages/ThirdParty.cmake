cmake_minimum_required(VERSION 3.22)

# === Stage: ThirdParty ===
# Adds bundled engine libraries via AddSubdirectory().
# Add or override behaviour via AddStageHook(ThirdParty Pre|Post <macro-name>).

# ---------------------------------------------------------------------------
# Install the find_package() interceptor before any AddSubdirectory() call so
# third-party CMakeLists cannot reach into the host system unannounced.
# Project-side handlers (e.g. OpenSSL → bundled LibreSSL) must already be
# registered by now via RegisterFindPackageHandler() — typically right
# before AddThirdPartyLibraries() in the consuming project's CMakeLists.txt.
#
# Active on every platform. Adding a new third-party tree may require
# registering further handlers if it brings in unexpected probes.
# ---------------------------------------------------------------------------

# Baseline build-tool / system-meta packages we always accept from the host:
# pthreads/Win32-thread metadata, the Python interpreter used by the codegen
# step, optional Perl probes from third-party trees, pkg-config probes that
# legitimately fall through on non-pkg systems, Git for version detection.
RegisterFindPackageHandler(Threads   PassThroughFindPackage)
RegisterFindPackageHandler(Python3   PassThroughFindPackage)
RegisterFindPackageHandler(Perl      PassThroughFindPackage)
RegisterFindPackageHandler(PkgConfig PassThroughFindPackage)
RegisterFindPackageHandler(Git       PassThroughFindPackage)
# Bundled-by-the-library Find modules (e.g. mongo-c-driver ships its own
# FindUtf8Proc.cmake to drive its in-tree utf8proc copy). Let CMake's
# standard find_package() pick those up via CMAKE_MODULE_PATH.
RegisterFindPackageHandler(Utf8Proc  PassThroughFindPackage)

# Host runtime libs we deliberately use through SDL3 and engine code.
if(FO_LINUX)
	# X11 for display, OpenGL for rendering, ALSA for audio backend.
	# CMake's FindX11 also probes Freetype and Fontconfig for Xft support.
	RegisterFindPackageHandler(X11        PassThroughFindPackage)
	RegisterFindPackageHandler(Freetype   PassThroughFindPackage)
	RegisterFindPackageHandler(Fontconfig PassThroughFindPackage)
	RegisterFindPackageHandler(OpenGL     PassThroughFindPackage)
	RegisterFindPackageHandler(ALSA       PassThroughFindPackage)
endif()
if(FO_MAC)
	# OpenGL framework on macOS (Init.cmake also probes it via RequirePackage,
	# but SDL3 in ThirdParty re-probes from inside the interceptor scope).
	RegisterFindPackageHandler(OpenGL PassThroughFindPackage)
endif()

# Optional probes from engine-bundled third-party trees that we do not bundle
# and do not want to satisfy from the host. They must be reported as not-found
# (the consumers handle the absence gracefully).
# SDL3 probes:
RegisterFindPackageHandler(LibUSB              NotFoundFindPackage)
RegisterFindPackageHandler(SdlAndroidPlatform  NotFoundFindPackage)
RegisterFindPackageHandler(Java                NotFoundFindPackage)
# glslang probe:
RegisterFindPackageHandler(SPIRV-Tools-opt     NotFoundFindPackage)
# tracy probe:
RegisterFindPackageHandler(rocprofiler-sdk     NotFoundFindPackage)

macro(find_package _fo_pkg_name)
	if(DEFINED _FO_FIND_PKG_HANDLER_${_fo_pkg_name})
		cmake_language(CALL "${_FO_FIND_PKG_HANDLER_${_fo_pkg_name}}" "${_fo_pkg_name}" ${ARGN})
	else()
		AbortMessage("find_package(${_fo_pkg_name}) blocked: no handler registered. Every find_package() must route through the project registry — call RegisterFindPackageHandler(${_fo_pkg_name} <macro>) before any third-party AddSubdirectory().")
	endif()
endmacro()

# Third-party libs
StatusMessage("Third-party libs:")

# Rpmalloc
if(NOT FO_DISABLE_RPMALLOC AND (FO_WINDOWS OR FO_LINUX OR FO_MAC OR FO_IOS OR FO_ANDROID))
    SetValue(FO_RPMALLOC_DIR "${FO_ENGINE_ROOT}/ThirdParty/rpmalloc")
    SetValue(FO_RPMALLOC_SOURCE
        "${FO_RPMALLOC_DIR}/rpmalloc/rpmalloc.c"
        "${FO_RPMALLOC_DIR}/rpmalloc/rpmalloc.h"
        "${FO_RPMALLOC_DIR}/rpmalloc/rpnew.h")
    AddStaticThirdPartyLibrary(rpmalloc
        SOURCE_LIST FO_RPMALLOC_SOURCE
        APPEND_TO FO_ESSENTIALS_LIBS
        INCLUDE_DIRS "${FO_RPMALLOC_DIR}/rpmalloc")
    AddCompileDefinitionsList(FO_HAVE_RPMALLOC=${expr_RpmallocEnabled})
    # The engine routes all allocation through its own global operator new/delete
    # overrides in MemorySystem.cpp; rpmalloc must not emit its malloc/operator
    # replacements (ENABLE_OVERRIDE defaults to 1 since rpmalloc 2.0).
    TargetCompileDefinitions(rpmalloc PRIVATE ENABLE_OVERRIDE=0)
    TargetCompileDefinitions(rpmalloc PRIVATE "$<$<PLATFORM_ID:Linux>:_GNU_SOURCE>")
    TargetCompileDefinitions(rpmalloc PRIVATE $<$<OR:${expr_DebugBuild},${expr_TracyEnabled}>:ENABLE_STATISTICS=1>)
    SetTargetProperty(rpmalloc C_STANDARD 11)
    TargetCompileOptions(rpmalloc PRIVATE "$<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:MSVC>>:/experimental:c11atomics>")
else()
    AddCompileDefinitionsList(FO_HAVE_RPMALLOC=0)
endif()

# SDL
StatusMessage("+ SDL")
SetValue(FO_SDL_DIR "${FO_ENGINE_ROOT}/ThirdParty/SDL")
SetCacheValues(
    SDL_TEST_LIBRARY OFF
    SDL_UNIX_CONSOLE_BUILD ${FO_HEADLESS_ONLY})
AddSubdirectory("${FO_SDL_DIR}" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
AddIncludeDirectories("${FO_SDL_DIR}/include")
AppendList(FO_RENDER_LIBS SDL3-static)
DisableLibWarnings(SDL3-static)

# Tracy profiler
StatusMessage("+ Tracy")
SetValue(FO_TRACY_DIR "${FO_ENGINE_ROOT}/ThirdParty/tracy")
AddCompileDefinitionsList(
    $<${expr_TracyEnabled}:TRACY_ENABLE>
    $<${expr_TracyOnDemand}:TRACY_ON_DEMAND>
    FO_TRACY=${expr_TracyEnabled})
SetCacheValues(TRACY_STATIC ON)
AddSubdirectory("${FO_TRACY_DIR}" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
AddIncludeDirectories("${FO_TRACY_DIR}/public")
AppendList(FO_ESSENTIALS_LIBS TracyClient)
DisableLibWarnings(TracyClient)

# Zlib
StatusMessage("+ Zlib")
SetValue(FO_ZLIB_DIR "${FO_ENGINE_ROOT}/ThirdParty/zlib")
SetCacheValues(ZLIB_BUILD_EXAMPLES OFF)
AddSubdirectory("${FO_ZLIB_DIR}" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
AddIncludeDirectories("${FO_ZLIB_DIR}" "${FO_ZLIB_DIR}/contrib" "${CMAKE_CURRENT_BINARY_DIR}/${FO_ZLIB_DIR}")
SetValue(FO_ZLIB_CONTRIB_SOURCE
    "${FO_ZLIB_DIR}/contrib/minizip/unzip.c"
    "${FO_ZLIB_DIR}/contrib/minizip/unzip.h"
    "${FO_ZLIB_DIR}/contrib/minizip/ioapi.c"
    "${FO_ZLIB_DIR}/contrib/minizip/ioapi.h")
AddStaticThirdPartyLibrary(zlibcontrib
    SOURCE_LIST FO_ZLIB_CONTRIB_SOURCE)
AppendList(FO_ESSENTIALS_LIBS zlibstatic zlibcontrib)
DisableLibWarnings(zlibstatic zlibcontrib)
SetCacheValues(
    ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ZLIB_DIR}"
    ZLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ZLIB_DIR}"
    ZLIB_LIBRARY "zlibstatic")
SetCacheValues(ZLIB_USE_STATIC_LIBS ON)

# Use IMPORTED INTERFACE (rather than ALIAS) so the target is visible to
# try_compile() invocations launched from third-party CMakeLists.txt that
# call find_package(ZLIB).
SetValue(FO_ZLIB_INCLUDE_DIR_ABS "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ZLIB_DIR}")
SetValue(FO_ZLIB_BINARY_DIR_ABS  "${CMAKE_CURRENT_BINARY_DIR}/${FO_ZLIB_DIR}")
add_library(ZLIB::ZLIB INTERFACE IMPORTED GLOBAL)
set_target_properties(ZLIB::ZLIB PROPERTIES
    INTERFACE_LINK_LIBRARIES zlibstatic
    INTERFACE_INCLUDE_DIRECTORIES "${FO_ZLIB_INCLUDE_DIR_ABS};${FO_ZLIB_BINARY_DIR_ABS}")

# Capture absolute paths now — the handler macro is expanded at the
# find_package() call site (e.g. inside libpng or downstream consumers),
# where CMAKE_CURRENT_SOURCE_DIR / FO_ZLIB_DIR would resolve to the
# consumer subdirectory.
macro(_FoEngineHandleZlibFindPackage _fo_zlib_pkg)
    set(ZLIB_FOUND TRUE)
    set(ZLIB_LIBRARY zlibstatic)
    set(ZLIB_LIBRARIES ZLIB::ZLIB)
    set(ZLIB_INCLUDE_DIR "${FO_ZLIB_INCLUDE_DIR_ABS}")
    set(ZLIB_INCLUDE_DIRS "${FO_ZLIB_INCLUDE_DIR_ABS}")
    set(ZLIB_VERSION_STRING "1.3.2")
    set(ZLIB_VERSION "1.3.2")
endmacro()
RegisterFindPackageHandler(ZLIB _FoEngineHandleZlibFindPackage)

# LibPNG
if(FO_BUILD_BAKER_LIB)
    StatusMessage("+ LibPNG")
    SetValue(FO_PNG_DIR "${FO_ENGINE_ROOT}/ThirdParty/libpng")
    SetCacheValues(
        PNG_SHARED OFF
        PNG_STATIC ON
        PNG_FRAMEWORK OFF
        PNG_TESTS OFF
        PNG_TOOLS OFF
        PNG_DEBUG OFF
        PNG_HARDWARE_OPTIMIZATIONS ON
        PNG_BUILD_ZLIB OFF
        ld-version-script OFF)
    SetCacheValues(AWK IGNORE)
    AddSubdirectory("${FO_PNG_DIR}" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
    SetTargetsFolder("ThirdParty/Dummy" png_genfiles)
    AddIncludeDirectories("${FO_PNG_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/${FO_PNG_DIR}")
    AppendList(FO_BAKER_LIBS png_static)
    AppendList(FO_DUMMY_TARGETS png_genfiles)
    DisableLibWarnings(png_static)
endif()

# Ogg
SetValue(FO_OGG_DIR "${FO_ENGINE_ROOT}/ThirdParty/ogg")
SetValue(FO_OGG_CONFIG_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/ThirdParty/ogg/include")
include(CheckIncludeFiles)
check_include_files(inttypes.h INCLUDE_INTTYPES_H)
check_include_files(stdint.h INCLUDE_STDINT_H)
check_include_files(sys/types.h INCLUDE_SYS_TYPES_H)
foreach(oggIncludeVar IN ITEMS INCLUDE_INTTYPES_H INCLUDE_STDINT_H INCLUDE_SYS_TYPES_H)
    if(NOT DEFINED ${oggIncludeVar} OR "${${oggIncludeVar}}" STREQUAL "")
        SetValue(${oggIncludeVar} 0)
    endif()
endforeach()
SetValue(SIZE16 int16_t)
SetValue(USIZE16 uint16_t)
SetValue(SIZE32 int32_t)
SetValue(USIZE32 uint32_t)
SetValue(SIZE64 int64_t)
SetValue(USIZE64 uint64_t)
include("${FO_OGG_DIR}/cmake/CheckSizes.cmake")
# (FOnline Patch) The engine builds Ogg through a custom static target instead
# of upstream CMake, so generate the Unix type header that os_types.h includes.
configure_file("${FO_OGG_DIR}/include/ogg/config_types.h.in" "${FO_OGG_CONFIG_INCLUDE_DIR}/ogg/config_types.h" @ONLY)
SetValue(FO_OGG_SOURCE
    "${FO_OGG_DIR}/src/bitwise.c"
    "${FO_OGG_DIR}/src/framing.c")
AddStaticThirdPartyLibrary(Ogg
    SOURCE_LIST FO_OGG_SOURCE
    APPEND_TO FO_CLIENT_LIBS
    INCLUDE_DIRS "${FO_OGG_DIR}/include" "${FO_OGG_CONFIG_INCLUDE_DIR}")

# Vorbis
SetValue(FO_VORBIS_DIR "${FO_ENGINE_ROOT}/ThirdParty/Vorbis")
SetValue(FO_VORBIS_SOURCE
    "${FO_VORBIS_DIR}/lib/analysis.c"
    "${FO_VORBIS_DIR}/lib/bitrate.c"
    "${FO_VORBIS_DIR}/lib/block.c"
    "${FO_VORBIS_DIR}/lib/codebook.c"
    "${FO_VORBIS_DIR}/lib/envelope.c"
    "${FO_VORBIS_DIR}/lib/floor0.c"
    "${FO_VORBIS_DIR}/lib/floor1.c"
    "${FO_VORBIS_DIR}/lib/info.c"
    "${FO_VORBIS_DIR}/lib/lookup.c"
    "${FO_VORBIS_DIR}/lib/lpc.c"
    "${FO_VORBIS_DIR}/lib/lsp.c"
    "${FO_VORBIS_DIR}/lib/mapping0.c"
    "${FO_VORBIS_DIR}/lib/mdct.c"
    "${FO_VORBIS_DIR}/lib/psy.c"
    "${FO_VORBIS_DIR}/lib/registry.c"
    "${FO_VORBIS_DIR}/lib/res0.c"
    "${FO_VORBIS_DIR}/lib/sharedbook.c"
    "${FO_VORBIS_DIR}/lib/smallft.c"
    "${FO_VORBIS_DIR}/lib/synthesis.c"
    "${FO_VORBIS_DIR}/lib/vorbisenc.c"
    "${FO_VORBIS_DIR}/lib/vorbisfile.c"
    "${FO_VORBIS_DIR}/lib/window.c")
AddStaticThirdPartyLibrary(Vorbis
    SOURCE_LIST FO_VORBIS_SOURCE
    APPEND_TO FO_CLIENT_LIBS
    INCLUDE_DIRS "${FO_VORBIS_DIR}/include" "${FO_VORBIS_DIR}/lib"
    LINK_LIBS Ogg)

# Theora
SetValue(FO_THEORA_DIR "${FO_ENGINE_ROOT}/ThirdParty/Theora")
SetValue(FO_THEORA_SOURCE
    "${FO_THEORA_DIR}/lib/apiwrapper.c"
    "${FO_THEORA_DIR}/lib/bitpack.c"
    "${FO_THEORA_DIR}/lib/decapiwrapper.c"
    "${FO_THEORA_DIR}/lib/decinfo.c"
    "${FO_THEORA_DIR}/lib/decode.c"
    "${FO_THEORA_DIR}/lib/dequant.c"
    "${FO_THEORA_DIR}/lib/encfrag.c"
    "${FO_THEORA_DIR}/lib/encinfo.c"
    "${FO_THEORA_DIR}/lib/encoder_disabled.c"
    "${FO_THEORA_DIR}/lib/enquant.c"
    "${FO_THEORA_DIR}/lib/fdct.c"
    "${FO_THEORA_DIR}/lib/fragment.c"
    "${FO_THEORA_DIR}/lib/huffdec.c"
    "${FO_THEORA_DIR}/lib/huffenc.c"
    "${FO_THEORA_DIR}/lib/idct.c"
    "${FO_THEORA_DIR}/lib/info.c"
    "${FO_THEORA_DIR}/lib/internal.c"
    "${FO_THEORA_DIR}/lib/mathops.c"
    "${FO_THEORA_DIR}/lib/mcenc.c"
    "${FO_THEORA_DIR}/lib/quant.c"
    "${FO_THEORA_DIR}/lib/rate.c"
    "${FO_THEORA_DIR}/lib/state.c"
    "${FO_THEORA_DIR}/lib/tokenize.c")
AddStaticThirdPartyLibrary(Theora
    SOURCE_LIST FO_THEORA_SOURCE
    APPEND_TO FO_CLIENT_LIBS
    INCLUDE_DIRS "${FO_THEORA_DIR}/include")

# Acm
SetValue(FO_ACM_DIR "${FO_ENGINE_ROOT}/ThirdParty/Acm")
SetValue(FO_ACM_SOURCE
    "${FO_ACM_DIR}/acmstrm.cpp"
    "${FO_ACM_DIR}/acmstrm.h")
AddStaticThirdPartyLibrary(AcmDecoder
    SOURCE_LIST FO_ACM_SOURCE
    APPEND_TO FO_CLIENT_LIBS
    INCLUDE_DIRS "${FO_ACM_DIR}")

# GLM
StatusMessage("+ GLM")
AddIncludeDirectories("${FO_ENGINE_ROOT}/ThirdParty/glm")
AppendList(FO_ESSENTIALS_SOURCE "$<$<BOOL:${MSVC}>:${FO_ENGINE_ROOT}/ThirdParty/glm/util/glm.natvis>")

# ufbx
if(FO_ENABLE_3D AND FO_BUILD_BAKER_LIB)
    SetValue(FO_UFBX_DIR "${FO_ENGINE_ROOT}/ThirdParty/ufbx")
    SetValue(FO_UFBX_SOURCE
        "${FO_UFBX_DIR}/ufbx.h"
        "${FO_UFBX_DIR}/ufbx.c")
    AddStaticThirdPartyLibrary(ufbx
        SOURCE_LIST FO_UFBX_SOURCE
        APPEND_TO FO_BAKER_LIBS
        INCLUDE_DIRS "${FO_UFBX_DIR}")
    TargetCompileDefinitions(ufbx PUBLIC "UFBX_NO_STDIO")
    TargetCompileDefinitions(ufbx PUBLIC "UFBX_EXTERNAL_MALLOC")
    AppendList(FO_ESSENTIALS_SOURCE "$<$<BOOL:${MSVC}>:${FO_UFBX_DIR}/misc/ufbx.natvis>")
    AppendList(FO_ESSENTIALS_SOURCE "$<$<BOOL:${MSVC}>:${FO_UFBX_DIR}/misc/ufbxi.natvis>")
endif()

# Json
StatusMessage("+ Json")
SetValue(FO_JSON_DIR "${FO_ENGINE_ROOT}/ThirdParty/Json")
AddIncludeDirectories("${FO_JSON_DIR}")

# LibreSSL
if(FO_BUILD_SERVER_LIB AND NOT FO_DISABLE_ASIO AND NOT FO_DISABLE_WEB_SOCKETS AND NOT FO_ANDROID)
    SetValue(FO_BUILD_OPENSSL_LIB ON)
endif()

if(FO_BUILD_OPENSSL_LIB)
    StatusMessage("+ LibreSSL")
    SetValue(FO_LIBRESSL_DIR "${FO_ENGINE_ROOT}/ThirdParty/LibreSSL")
    SetCacheValues(
        LIBRESSL_SKIP_INSTALL ON
        LIBRESSL_APPS OFF
        LIBRESSL_TESTS OFF
        ENABLE_ASM ON
        ENABLE_EXTRATESTS OFF
        ENABLE_NC OFF)
    AddSubdirectory("${FO_LIBRESSL_DIR}" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
    SetTargetsFolder("ThirdParty/Dummy" compat_obj crypto_obj ssl_obj tls_compat_obj tls_obj)
    AddIncludeDirectories(
        "${FO_LIBRESSL_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/${FO_LIBRESSL_DIR}/crypto"
        "${CMAKE_CURRENT_BINARY_DIR}/${FO_LIBRESSL_DIR}/ssl")
    SetCacheValues(LIBRESSL_FOUND ON)
    SetCacheValues(
        LIBRESSL_LIBRARIES "ssl;crypto;tls"
        LIBRESSL_INCLUDE_DIRS ""
        LIBRESSL_LIBRARY_DIRS "")
    AppendList(FO_SERVER_LIBS ssl crypto tls)
    AppendList(FO_DUMMY_TARGETS compat_obj crypto_obj ssl_obj tls_compat_obj tls_obj)
    DisableLibWarnings(ssl crypto tls compat_obj crypto_obj ssl_obj tls_compat_obj tls_obj)
endif()

# Asio & Websockets
if(NOT FO_DISABLE_ASIO AND NOT FO_ANDROID AND NOT FO_WEB AND FO_BUILD_SERVER_LIB)
    StatusMessage("+ Asio")
    SetValue(FO_ASIO_DIR "${FO_ENGINE_ROOT}/ThirdParty/Asio")
    AddIncludeDirectories("${FO_ASIO_DIR}/include")

    if(NOT FO_DISABLE_WEB_SOCKETS)
        StatusMessage("+ Websockets")
        SetValue(FO_WEBSOCKETS_DIR "${FO_ENGINE_ROOT}/ThirdParty/websocketpp")
        AddIncludeDirectories("${FO_WEBSOCKETS_DIR}")
        AddCompileDefinitionsList(FO_HAVE_WEB_SOCKETS=1)
    endif()

    AddCompileDefinitionsList(FO_HAVE_ASIO=1)
else()
    AddCompileDefinitionsList(
        FO_HAVE_ASIO=0
        FO_HAVE_WEB_SOCKETS=0)
endif()

# MongoDB & Bson
if(FO_BUILD_SERVER_LIB)
    StatusMessage("+ Bson")
    SetValue(FO_MONGODB_DIR "${FO_ENGINE_ROOT}/ThirdParty/mongo-c-driver")

    SetCacheValues(
        ENABLE_STATIC BUILD_ONLY
        ENABLE_TESTS OFF
        ENABLE_SSL OFF
        ENABLE_SASL OFF
        ENABLE_ZLIB SYSTEM
        # We don't ship snappy or zstd, so disable both. Otherwise mongo-c-driver
        # falls back to pkg_check_modules() and silently links against the host
        # libraries on Linux build agents.
        ENABLE_SNAPPY OFF
        ENABLE_ZSTD OFF
        ENABLE_CLIENT_SIDE_ENCRYPTION OFF)
    SetCacheValues(
        ENABLE_SHARED OFF
        ENABLE_SRV OFF
        ENABLE_UNINSTALL OFF
        ENABLE_EXAMPLES OFF
        USE_BUNDLED_UTF8PROC ON)

    if(NOT FO_DISABLE_MONGO)
        StatusMessage("+ MongoDB")
        SetCacheValues(ENABLE_MONGOC ON)
        AddCompileDefinitionsList(FO_HAVE_MONGO=1)
    else()
        SetCacheValues(ENABLE_MONGOC OFF)
        AddCompileDefinitionsList(FO_HAVE_MONGO=0)
    endif()

    AddSubdirectory("${FO_MONGODB_DIR}" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
    if(TARGET utf8proc_obj)
        SetTargetsFolder("ThirdParty/Dummy" utf8proc_obj)
    endif()

    AddIncludeDirectories(
        "${CMAKE_CURRENT_BINARY_DIR}/${FO_MONGODB_DIR}/src/libbson/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MONGODB_DIR}/src/libbson/src")
    TargetCompileDefinitions(bson_static PRIVATE "BSON_COMPILATION;BSON_STATIC;JSONSL_PARSE_NAN")
    AddCompileDefinitionsList(BSON_COMPILATION BSON_STATIC JSONSL_PARSE_NAN)
    AppendList(FO_SERVER_LIBS bson_static)
    DisableLibWarnings(bson_static)

    if(ENABLE_MONGOC)
        AddIncludeDirectories(
            "${CMAKE_CURRENT_BINARY_DIR}/${FO_MONGODB_DIR}/src/libmongoc/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MONGODB_DIR}/src/libmongoc/src")
        TargetCompileDefinitions(mongoc_static PRIVATE "BSON_COMPILATION;BSON_STATIC;JSONSL_PARSE_NAN")
        AppendList(FO_SERVER_LIBS mongoc_static)
        AppendList(FO_DUMMY_TARGETS utf8proc_obj)
        DisableLibWarnings(mongoc_static utf8proc_obj)
    endif()
endif()

# Unqlite
if(NOT FO_DISABLE_UNQLITE AND NOT FO_WEB)
    StatusMessage("+ Unqlite")
    SetValue(FO_UNQLITE_DIR "${FO_ENGINE_ROOT}/ThirdParty/unqlite")
    AddSubdirectory("${FO_UNQLITE_DIR}" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
    AddIncludeDirectories("${FO_UNQLITE_DIR}")
    AppendList(FO_COMMON_LIBS unqlite)
    DisableLibWarnings(unqlite)
    TargetCompileDefinitions(unqlite PRIVATE "JX9_DISABLE_BUILTIN_FUNC")
    AddCompileDefinitionsList(FO_HAVE_UNQLITE=1)
else()
    AddCompileDefinitionsList(FO_HAVE_UNQLITE=0)
endif()

# Dear ImGui
SetValue(FO_DEAR_IMGUI_DIR "${FO_ENGINE_ROOT}/ThirdParty/imgui")
SetValue(FO_IMGUI_SOURCE
    "${FO_DEAR_IMGUI_DIR}/imconfig.h"
    "${FO_DEAR_IMGUI_DIR}/imgui.cpp"
    "${FO_DEAR_IMGUI_DIR}/imgui.h"
    "${FO_DEAR_IMGUI_DIR}/imgui_draw.cpp"
    "${FO_DEAR_IMGUI_DIR}/imgui_internal.h"
    "${FO_DEAR_IMGUI_DIR}/imgui_tables.cpp"
    "${FO_DEAR_IMGUI_DIR}/imgui_widgets.cpp"
    "${FO_DEAR_IMGUI_DIR}/imstb_rectpack.h"
    "${FO_DEAR_IMGUI_DIR}/imstb_textedit.h"
    "${FO_DEAR_IMGUI_DIR}/imstb_truetype.h"
    "${FO_ENGINE_ROOT}/Source/Common/ImGuiExt/ImGuiConfig.h")
AddIncludeDirectories("${FO_ENGINE_ROOT}/Source/Common/ImGuiExt")
AddStaticThirdPartyLibrary(imgui
    SOURCE_LIST FO_IMGUI_SOURCE
    APPEND_TO FO_COMMON_LIBS
    INCLUDE_DIRS "${FO_DEAR_IMGUI_DIR}")
TargetCompileDefinitions(imgui PRIVATE "IMGUI_USER_CONFIG=\"ImGuiConfig.h\"")
AddCompileDefinitionsList("IMGUI_USER_CONFIG=\"ImGuiConfig.h\"")
AppendList(FO_ESSENTIALS_SOURCE "$<$<BOOL:${MSVC}>:${FO_DEAR_IMGUI_DIR}/misc/debuggers/imgui.natvis>")

# Catch2
SetValue(FO_CATCH2_DIR "${FO_ENGINE_ROOT}/ThirdParty/Catch2")
SetValue(FO_CATCH2_SOURCE
    "${FO_CATCH2_DIR}/catch_amalgamated.cpp"
    "${FO_CATCH2_DIR}/catch_amalgamated.hpp")
AddStaticThirdPartyLibrary(Catch2
    SOURCE_LIST FO_CATCH2_SOURCE
    APPEND_TO FO_TESTING_LIBS
    INCLUDE_DIRS "${FO_CATCH2_DIR}")
TargetCompileDefinitions(Catch2 PRIVATE "CATCH_AMALGAMATED_CUSTOM_MAIN")

# Backward-cpp
if(FO_WINDOWS OR FO_LINUX OR FO_MAC)
    SetValue(FO_BACKWARDCPP_DIR "${FO_ENGINE_ROOT}/ThirdParty/backward-cpp")
    AddIncludeDirectories("${FO_BACKWARDCPP_DIR}")

    if(NOT FO_WINDOWS)
        check_include_file("libunwind.h" haveLibUnwind)
        check_include_file("bfd.h" haveBFD)

        if(haveLibUnwind)
            StatusMessage("+ Backward-cpp (with libunwind)")
        elseif(haveBFD)
            StatusMessage("+ Backward-cpp (with bfd)")
            AppendList(FO_ESSENTIALS_SYSTEM_LIBS bfd)
        else()
            StatusMessage("+ Backward-cpp")
        endif()
    else()
        StatusMessage("+ Backward-cpp")
    endif()
endif()

# Spark
StatusMessage("+ Spark")
SetValue(FO_SPARK_DIR "${FO_ENGINE_ROOT}/ThirdParty/spark")
SetCacheValues(SPARK_STATIC_BUILD ON)
AddSubdirectory("${FO_SPARK_DIR}/projects/engine/core" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
AddSubdirectory("${FO_SPARK_DIR}/projects/external/pugi" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
AddIncludeDirectories("${FO_SPARK_DIR}/spark/include" "${FO_SPARK_DIR}/thirdparty/PugiXML")
AppendList(FO_CLIENT_LIBS SPARK_Core PugiXML)
DisableLibWarnings(SPARK_Core PugiXML)

# glslang & SPIRV-Cross
if(FO_BUILD_BAKER_LIB)
    StatusMessage("+ glslang")
    SetValue(FO_GLSLANG_DIR "${FO_ENGINE_ROOT}/ThirdParty/glslang")
    SetCacheValues(
        GLSLANG_TESTS OFF
        GLSLANG_ENABLE_INSTALL OFF
        BUILD_EXTERNAL OFF
        BUILD_WERROR OFF
        ENABLE_SPIRV ON
        ENABLE_HLSL OFF
        ENABLE_GLSLANG_BINARIES OFF
        ENABLE_RTTI ON
        ENABLE_EXCEPTIONS ON
        ENABLE_OPT OFF
        ENABLE_PCH OFF
        ALLOW_EXTERNAL_SPIRV_TOOLS OFF)

    if(FO_WEB)
        SetCacheValues(
            ENABLE_EMSCRIPTEN_SINGLE_FILE ON
            ENABLE_EMSCRIPTEN_ENVIRONMENT_NODE OFF)
    endif()

    AddSubdirectory("${FO_GLSLANG_DIR}" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
    AddIncludeDirectories("${FO_GLSLANG_DIR}/glslang/Public" "${FO_GLSLANG_DIR}/SPIRV")
    AppendList(FO_BAKER_LIBS
        glslang
        glslang-default-resource-limits
        OSDependent
        SPIRV
        GenericCodeGen
        MachineIndependent)
    DisableLibWarnings(
        glslang
        glslang-default-resource-limits
        OSDependent
        SPIRV
        GenericCodeGen
        MachineIndependent)

    StatusMessage("+ SPIRV-Cross")
    SetValue(FO_SPIRV_CROSS_DIR "${FO_ENGINE_ROOT}/ThirdParty/SPIRV-Cross")
    SetCacheValues(
        SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS OFF
        SPIRV_CROSS_STATIC ON
        SPIRV_CROSS_SHARED OFF
        SPIRV_CROSS_CLI OFF
        SPIRV_CROSS_ENABLE_TESTS OFF
        SPIRV_CROSS_ENABLE_GLSL ON
        SPIRV_CROSS_ENABLE_HLSL ON
        SPIRV_CROSS_ENABLE_MSL ON
        SPIRV_CROSS_ENABLE_CPP OFF
        SPIRV_CROSS_ENABLE_REFLECT OFF
        SPIRV_CROSS_ENABLE_C_API OFF
        SPIRV_CROSS_ENABLE_UTIL OFF
        SPIRV_CROSS_SKIP_INSTALL ON)
    AddSubdirectory("${FO_SPIRV_CROSS_DIR}" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
    AddIncludeDirectories("${FO_SPIRV_CROSS_DIR}" "${FO_SPIRV_CROSS_DIR}/include")
    AppendList(FO_BAKER_LIBS spirv-cross-core spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl)
    DisableLibWarnings(spirv-cross-core spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl)
endif()

# small_vector
AddIncludeDirectories("${FO_ENGINE_ROOT}/ThirdParty/small_vector/source/include/gch")
AppendList(FO_ESSENTIALS_SOURCE
    "$<$<BOOL:${MSVC}>:${FO_ENGINE_ROOT}/ThirdParty/small_vector/source/support/visualstudio/small_vector.natvis>")

# unordered_dense
AddIncludeDirectories("${FO_ENGINE_ROOT}/ThirdParty/unordered_dense/include")
AppendList(FO_ESSENTIALS_SOURCE
    "$<$<BOOL:${MSVC}>:${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/natvis/unordered_dense.natvis>")

# AngelScript scripting
if(FO_ANGELSCRIPT_SCRIPTING)
    StatusMessage("+ AngelScript")

    # AngelScript core
    SetValue(FO_ANGELSCRIPT_SDK_DIR "${FO_ENGINE_ROOT}/ThirdParty/AngelScript/sdk")
    SetCacheValues(ANGELSCRIPT_LIBRARY_NAME "AngelScriptCore")
    SetCacheValues(AS_DISABLE_INSTALL ON)
    AddSubdirectory("${FO_ANGELSCRIPT_SDK_DIR}/angelscript/projects/cmake" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
    TargetCompileDefinitions(AngelScriptCore PUBLIC AS_USE_NAMESPACE)
    TargetCompileDefinitions(AngelScriptCore PUBLIC AS_MODERN_THREADS)
    TargetCompileDefinitions(AngelScriptCore PUBLIC $<${expr_DebugBuild}:AS_DEBUG>)
    TargetCompileDefinitions(
        AngelScriptCore PUBLIC
        $<$<OR:$<BOOL:${FO_WEB}>,$<BOOL:${FO_MAC}>,$<BOOL:${FO_IOS}>,$<BOOL:${FO_ANDROID}>>:AS_MAX_PORTABILITY>)
    TargetIncludeDirectories(AngelScriptCore SYSTEM PUBLIC
        "${FO_ANGELSCRIPT_SDK_DIR}/angelscript/include"
        "${FO_ANGELSCRIPT_SDK_DIR}/angelscript/source")
    AppendList(FO_COMMON_LIBS AngelScriptCore)
    DisableLibWarnings(AngelScriptCore)

    # AngelScript preprocessor
    SetValue(FO_ANGELSCRIPT_PREPROCESSOR_DIR "${FO_ENGINE_ROOT}/ThirdParty/AngelScript/preprocessor")
    SetValue(FO_ANGELSCRIPT_PREPROCESSOR_SOURCE
        "${FO_ANGELSCRIPT_PREPROCESSOR_DIR}/preprocessor.h"
        "${FO_ANGELSCRIPT_PREPROCESSOR_DIR}/preprocessor.cpp")
    AddStaticThirdPartyLibrary(AngelScriptPreprocessor
        SOURCE_LIST FO_ANGELSCRIPT_PREPROCESSOR_SOURCE
        APPEND_TO FO_COMMON_LIBS)
    TargetIncludeDirectories(AngelScriptCore SYSTEM PUBLIC
        "${FO_ANGELSCRIPT_PREPROCESSOR_DIR}")
endif()

# Mono scripting
if(FO_MONO_SCRIPTING)
    StatusMessage("+ Mono")

    SetValue(FO_MONO_CONFIGURATION $<IF:${expr_DebugBuild},Debug,Release>)
    SetValue(FO_MONO_TRIPLET ${FO_MONO_OS}.${FO_MONO_ARCH}.${FO_MONO_CONFIGURATION})

    SetValue(FO_DOTNET_DIR ${CMAKE_CURRENT_BINARY_DIR}/dotnet)
    FileMakeDirectory(${FO_DOTNET_DIR})

    AddIncludeDirectories(${FO_DOTNET_DIR}/output/mono/${FO_MONO_TRIPLET}/include/mono-2.0)
    AddLinkDirectories(${FO_DOTNET_DIR}/output/mono/${FO_MONO_TRIPLET}/lib)

    if(FO_WINDOWS)
        SetValue(FO_MONO_SETUP_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/setup-mono.cmd)
    else()
        SetValue(FO_MONO_SETUP_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/setup-mono.sh)
    endif()

    AddCustomCommand(OUTPUT ${FO_DOTNET_DIR}/READY_${FO_MONO_TRIPLET}
        COMMAND ${FO_MONO_SETUP_SCRIPT} ${FO_MONO_OS} ${FO_MONO_ARCH} ${FO_MONO_CONFIGURATION}
        WORKING_DIRECTORY ${FO_DOTNET_DIR}
        COMMENT "Setup Mono")

    AddCommandTarget(SetupMono
        DEPENDS ${FO_DOTNET_DIR}/READY_${FO_MONO_TRIPLET}
        WORKING_DIRECTORY ${FO_DOTNET_DIR})
    AppendList(FO_GEN_DEPENDENCIES SetupMono)

    AppendList(FO_COMMON_SYSTEM_LIBS
        monosgen-2.0
        mono-component-debugger-stub-static
        mono-component-diagnostics_tracing-stub-static
        mono-component-hot_reload-stub-static
        mono-component-marshal-ilgen-stub-static)

    if(FO_WINDOWS)
        AppendList(FO_COMMON_SYSTEM_LIBS bcrypt)
    elseif(FO_WEB)
        AppendList(FO_COMMON_SYSTEM_LIBS mono-wasm-eh-wasm mono-wasm-nosimd)
    endif()
endif()
