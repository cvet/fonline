cmake_minimum_required(VERSION 3.22)

# Force some variables for internal debugging purposes
if(NOT ${FO_FORCE_ENABLE_3D} STREQUAL "")
    set(FO_ENABLE_3D ${FO_FORCE_ENABLE_3D})
endif()

# Configuration checks
if(FO_CODE_COVERAGE AND(FO_BUILD_CLIENT OR FO_BUILD_SERVER OR FO_BUILD_MAPPER OR FO_BUILD_EDITOR OR FO_BUILD_ASCOMPILER OR FO_BUILD_BAKER OR FO_UNIT_TESTS))
    AbortMessage("Code coverge build can not be mixed with other builds")
endif()

if(FO_BUILD_ASCOMPILER AND NOT FO_ANGELSCRIPT_SCRIPTING)
    AbortMessage("AngelScript compiler build can not be without AngelScript scripting enabled")
endif()

# Third-party libs
StatusMessage("Third-party libs:")

# Rpmalloc
if(NOT FO_DISABLE_RPMALLOC AND(FO_WINDOWS OR FO_LINUX OR FO_MAC OR FO_IOS OR FO_ANDROID))
    StatusMessage("+ Rpmalloc")
    set(FO_RPMALLOC_DIR "${FO_ENGINE_ROOT}/ThirdParty/rpmalloc")
    set(FO_RPMALLOC_SOURCE
        "${FO_RPMALLOC_DIR}/rpmalloc/rpmalloc.c"
        "${FO_RPMALLOC_DIR}/rpmalloc/rpmalloc.h"
        "${FO_RPMALLOC_DIR}/rpmalloc/rpnew.h")
    include_directories("${FO_RPMALLOC_DIR}/rpmalloc")
    add_library(rpmalloc STATIC EXCLUDE_FROM_ALL ${FO_RPMALLOC_SOURCE})
    add_compile_definitions(FO_HAVE_RPMALLOC=${expr_RpmallocEnabled})
    add_compile_definitions(ENABLE_PRELOAD=${expr_StandaloneRpmallocEnabled})
    target_compile_definitions(rpmalloc PRIVATE "$<$<PLATFORM_ID:Linux>:_GNU_SOURCE>")
    list(APPEND FO_COMMON_LIBS rpmalloc)
    DisableLibWarnings(rpmalloc)
else()
    add_compile_definitions(FO_HAVE_RPMALLOC=0)
endif()

# SDL
StatusMessage("+ SDL")
set(FO_SDL_DIR "${FO_ENGINE_ROOT}/ThirdParty/SDL")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Forced by FOnline" FORCE)
add_subdirectory("${FO_SDL_DIR}" EXCLUDE_FROM_ALL)
include_directories("${FO_SDL_DIR}/include")
list(APPEND FO_RENDER_LIBS SDL3-static SDL_uclibc)
DisableLibWarnings(SDL3-static SDL_uclibc)

# Tracy profiler
StatusMessage("+ Tracy")
set(FO_TRACY_DIR "${FO_ENGINE_ROOT}/ThirdParty/tracy")
add_compile_definitions($<${expr_TracyEnabled}:TRACY_ENABLE>)
add_compile_definitions($<${expr_TracyOnDemand}:TRACY_ON_DEMAND>)
add_compile_definitions(FO_TRACY=${expr_TracyEnabled})
set(TRACY_STATIC ON CACHE BOOL "Forced by FOnline" FORCE)
add_subdirectory("${FO_TRACY_DIR}" EXCLUDE_FROM_ALL)
include_directories("${FO_TRACY_DIR}/public")
list(APPEND FO_COMMON_LIBS TracyClient)
DisableLibWarnings(TracyClient)

# Zlib
StatusMessage("+ Zlib")
set(FO_ZLIB_DIR "${FO_ENGINE_ROOT}/ThirdParty/zlib")
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "Forced by FOnline" FORCE)
add_subdirectory("${FO_ZLIB_DIR}" EXCLUDE_FROM_ALL)
include_directories("${FO_ZLIB_DIR}" "${FO_ZLIB_DIR}/contrib" "${CMAKE_CURRENT_BINARY_DIR}/${FO_ZLIB_DIR}")
set(FO_ZLIB_CONTRIB_SOURCE
    "${FO_ZLIB_DIR}/contrib/minizip/unzip.c"
    "${FO_ZLIB_DIR}/contrib/minizip/unzip.h"
    "${FO_ZLIB_DIR}/contrib/minizip/ioapi.c"
    "${FO_ZLIB_DIR}/contrib/minizip/ioapi.h")
add_library(zlibcontrib STATIC EXCLUDE_FROM_ALL ${FO_ZLIB_CONTRIB_SOURCE})
list(APPEND FO_COMMON_LIBS zlibstatic zlibcontrib)
DisableLibWarnings(zlibstatic zlibcontrib)
set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ZLIB_DIR}" CACHE STRING "Forced by FOnline" FORCE)
set(ZLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ZLIB_DIR}" CACHE STRING "Forced by FOnline" FORCE)
set(ZLIB_LIBRARY "zlibstatic" CACHE STRING "Forced by FOnline" FORCE)
set(ZLIB_USE_STATIC_LIBS ON CACHE BOOL "Forced by FOnline" FORCE)
add_library(ZLIB::ZLIB ALIAS zlibstatic)

# LibPNG
if(FO_BUILD_BAKER_LIB)
    StatusMessage("+ LibPNG")
    set(FO_PNG_DIR "${FO_ENGINE_ROOT}/ThirdParty/libpng")
    set(PNG_SHARED OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(PNG_STATIC ON CACHE BOOL "Forced by FOnline" FORCE)
    set(PNG_FRAMEWORK OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(PNG_TESTS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(PNG_TOOLS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(PNG_DEBUG OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(PNG_HARDWARE_OPTIMIZATIONS ON CACHE BOOL "Forced by FOnline" FORCE)
    set(PNG_BUILD_ZLIB OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ld-version-script OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(AWK IGNORE CACHE STRING "Forced by FOnline" FORCE)
    add_subdirectory("${FO_PNG_DIR}" EXCLUDE_FROM_ALL)
    include_directories("${FO_PNG_DIR}")
    include_directories("${CMAKE_CURRENT_BINARY_DIR}/${FO_PNG_DIR}")
    list(APPEND FO_BAKER_LIBS png_static)
    list(APPEND FO_DUMMY_TARGETS png_genfiles)
    DisableLibWarnings(png_static)
endif()

# Ogg
StatusMessage("+ Ogg")
set(FO_OGG_DIR "${FO_ENGINE_ROOT}/ThirdParty/ogg")
set(FO_OGG_SOURCE
    "${FO_OGG_DIR}/src/bitwise.c"
    "${FO_OGG_DIR}/src/framing.c")
include_directories("${FO_OGG_DIR}/include")
add_library(Ogg STATIC EXCLUDE_FROM_ALL ${FO_OGG_SOURCE})
list(APPEND FO_CLIENT_LIBS Ogg)
DisableLibWarnings(Ogg)

# Vorbis
StatusMessage("+ Vorbis")
set(FO_VORBIS_DIR "${FO_ENGINE_ROOT}/ThirdParty/Vorbis")
set(FO_VORBIS_SOURCE
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
include_directories("${FO_VORBIS_DIR}/include")
include_directories("${FO_VORBIS_DIR}/lib")
add_library(Vorbis STATIC EXCLUDE_FROM_ALL ${FO_VORBIS_SOURCE})
target_link_libraries(Vorbis Ogg)
list(APPEND FO_CLIENT_LIBS Vorbis)
DisableLibWarnings(Vorbis)

# Theora
StatusMessage("+ Theora")
set(FO_THEORA_DIR "${FO_ENGINE_ROOT}/ThirdParty/Theora")
set(FO_THEORA_SOURCE
    "${FO_THEORA_DIR}/lib/apiwrapper.c"
    "${FO_THEORA_DIR}/lib/bitpack.c"
    "${FO_THEORA_DIR}/lib/cpu.c"
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
include_directories("${FO_THEORA_DIR}/include")
add_library(Theora STATIC EXCLUDE_FROM_ALL ${FO_THEORA_SOURCE})
list(APPEND FO_CLIENT_LIBS Theora)
DisableLibWarnings(Theora)

# Acm
StatusMessage("+ Acm")
set(FO_ACM_DIR "${FO_ENGINE_ROOT}/ThirdParty/Acm")
include_directories("${FO_ACM_DIR}")
set(FO_ACM_SOURCE
    "${FO_ACM_DIR}/acmstrm.cpp"
    "${FO_ACM_DIR}/acmstrm.h")
add_library(AcmDecoder STATIC EXCLUDE_FROM_ALL ${FO_ACM_SOURCE})
list(APPEND FO_CLIENT_LIBS AcmDecoder)
DisableLibWarnings(AcmDecoder)

# GLEW
if(FO_USE_GLEW)
    StatusMessage("+ GLEW")
    set(FO_GLEW_DIR "${FO_ENGINE_ROOT}/ThirdParty/GLEW")
    set(FO_GLEW_SOURCE
        "${FO_GLEW_DIR}/GL/glew.c"
        "${FO_GLEW_DIR}/GL/glew.h"
        "${FO_GLEW_DIR}/GL/glxew.h"
        "${FO_GLEW_DIR}/GL/wglew.h")
    include_directories("${FO_GLEW_DIR}")
    add_library(GLEW STATIC EXCLUDE_FROM_ALL ${FO_GLEW_SOURCE})
    add_compile_definitions(GLEW_STATIC)
    target_compile_definitions(GLEW PRIVATE "GLEW_STATIC")
    list(APPEND FO_RENDER_LIBS GLEW)
    DisableLibWarnings(GLEW)
endif()

# Assimp
StatusMessage("+ Assimp (math headers)")
include_directories("${FO_ENGINE_ROOT}/ThirdParty/AssimpMath")

# ufbx
if(FO_ENABLE_3D AND FO_BUILD_BAKER_LIB)
    StatusMessage("+ ufbx")
    set(FO_UFBX_DIR "${FO_ENGINE_ROOT}/ThirdParty/ufbx")
    include_directories("${FO_UFBX_DIR}")
    set(FO_UFBX_SOURCE
        "${FO_UFBX_DIR}/ufbx.h"
        "${FO_UFBX_DIR}/ufbx.c")
    add_library(ufbx STATIC EXCLUDE_FROM_ALL ${FO_UFBX_SOURCE})
    target_compile_definitions(ufbx PUBLIC "UFBX_NO_STDIO")
    target_compile_definitions(ufbx PUBLIC "UFBX_EXTERNAL_MALLOC")
    list(APPEND FO_BAKER_LIBS ufbx)
    DisableLibWarnings(ufbx)
endif()

# Json
if(NOT FO_DISABLE_JSON)
    StatusMessage("+ Json")
    set(FO_JSON_DIR "${FO_ENGINE_ROOT}/ThirdParty/Json")
    include_directories("${FO_JSON_DIR}")
    add_compile_definitions(FO_HAVE_JSON=1)
else()
    add_compile_definitions(FO_HAVE_JSON=0)
endif()

# LibreSSL
if(FO_BUILD_SERVER_LIB)
    StatusMessage("+ LibreSSL")
    set(FO_LIBRESSL_DIR "${FO_ENGINE_ROOT}/ThirdParty/LibreSSL")
    set(LIBRESSL_SKIP_INSTALL ON CACHE BOOL "Forced by FOnline" FORCE)
    set(LIBRESSL_APPS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(LIBRESSL_TESTS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_ASM ON CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_EXTRATESTS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_NC OFF CACHE BOOL "Forced by FOnline" FORCE)
    add_subdirectory("${FO_LIBRESSL_DIR}" EXCLUDE_FROM_ALL)
    include_directories("${FO_LIBRESSL_DIR}/include")
    include_directories("${CMAKE_CURRENT_BINARY_DIR}/${FO_LIBRESSL_DIR}/crypto")
    include_directories("${CMAKE_CURRENT_BINARY_DIR}/${FO_LIBRESSL_DIR}/ssl")
    set(LIBRESSL_FOUND ON CACHE BOOL "Forced by FOnline" FORCE)
    set(LIBRESSL_LIBRARIES "ssl;crypto;tls" CACHE STRING "Forced by FOnline" FORCE)
    set(LIBRESSL_INCLUDE_DIRS "" CACHE STRING "Forced by FOnline" FORCE)
    set(LIBRESSL_LIBRARY_DIRS "" CACHE STRING "Forced by FOnline" FORCE)
    list(APPEND FO_SERVER_LIBS ssl crypto tls)
    list(APPEND FO_DUMMY_TARGETS compat_obj crypto_obj ssl_obj tls_compat_obj tls_obj)
    DisableLibWarnings(ssl crypto tls compat_obj crypto_obj ssl_obj tls_compat_obj tls_obj)
endif()

# Asio & Websockets
if(NOT FO_DISABLE_ASIO AND NOT FO_ANDROID AND FO_BUILD_SERVER_LIB)
    StatusMessage("+ Asio")
    set(FO_ASIO_DIR "${FO_ENGINE_ROOT}/ThirdParty/Asio")
    include_directories("${FO_ASIO_DIR}/include")

    if(NOT FO_DISABLE_WEB_SOCKETS)
        StatusMessage("+ Websockets")
        set(FO_WEBSOCKETS_DIR "${FO_ENGINE_ROOT}/ThirdParty/websocketpp")
        include_directories("${FO_WEBSOCKETS_DIR}")
        add_compile_definitions(FO_HAVE_WEB_SOCKETS=1)
    endif()

    add_compile_definitions(FO_HAVE_ASIO=1)
else()
    add_compile_definitions(FO_HAVE_ASIO=0)
    add_compile_definitions(FO_HAVE_WEB_SOCKETS=0)
endif()

# MongoDB & Bson
if(FO_BUILD_SERVER_LIB)
    StatusMessage("+ Bson")
    set(FO_MONGODB_DIR "${FO_ENGINE_ROOT}/ThirdParty/mongo-c-driver")

    set(ENABLE_STATIC BUILD_ONLY CACHE STRING "Forced by FOnline" FORCE)
    set(ENABLE_SHARED OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_SRV OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_UNINSTALL OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_TESTS OFF CACHE STRING "Forced by FOnline" FORCE)
    set(ENABLE_EXAMPLES OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_SSL OFF CACHE STRING "Forced by FOnline" FORCE)
    set(ENABLE_SASL OFF CACHE STRING "Forced by FOnline" FORCE)
    set(ENABLE_ZLIB SYSTEM CACHE STRING "Forced by FOnline" FORCE)
    set(ENABLE_CLIENT_SIDE_ENCRYPTION OFF CACHE STRING "Forced by FOnline" FORCE)
    set(USE_BUNDLED_UTF8PROC ON CACHE BOOL "Forced by FOnline" FORCE)

    if(NOT FO_DISABLE_MONGO)
        StatusMessage("+ MongoDB")
        set(ENABLE_MONGOC ON CACHE STRING "Forced by FOnline" FORCE)
        add_compile_definitions(FO_HAVE_MONGO=1)
    else()
        set(ENABLE_MONGOC OFF CACHE STRING "Forced by FOnline" FORCE)
        add_compile_definitions(FO_HAVE_MONGO=0)
    endif()

    add_subdirectory("${FO_MONGODB_DIR}" EXCLUDE_FROM_ALL)

    include_directories("${CMAKE_CURRENT_BINARY_DIR}/${FO_MONGODB_DIR}/src/libbson/src/bson")
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/${FO_MONGODB_DIR}/src/libbson/src")
    target_compile_definitions(bson_static PRIVATE "BSON_COMPILATION;BSON_STATIC;JSONSL_PARSE_NAN")
    add_compile_definitions(BSON_COMPILATION BSON_STATIC JSONSL_PARSE_NAN)
    list(APPEND FO_SERVER_LIBS bson_static)
    DisableLibWarnings(bson_static)

    if(ENABLE_MONGOC)
        include_directories("${CMAKE_CURRENT_BINARY_DIR}/${FO_MONGODB_DIR}/src/libmongoc/src/mongoc")
        include_directories("${CMAKE_CURRENT_SOURCE_DIR}/${FO_MONGODB_DIR}/src/libmongoc/src")
        target_compile_definitions(mongoc_static PRIVATE "BSON_COMPILATION;BSON_STATIC;JSONSL_PARSE_NAN")
        list(APPEND FO_SERVER_LIBS mongoc_static)
        list(APPEND FO_DUMMY_TARGETS utf8proc_obj)
        DisableLibWarnings(mongoc_static utf8proc_obj)
    endif()
endif()

# Unqlite
if(NOT FO_DISABLE_UNQLITE AND NOT FO_WEB)
    StatusMessage("+ Unqlite")
    set(FO_UNQLITE_DIR "${FO_ENGINE_ROOT}/ThirdParty/unqlite")
    add_subdirectory("${FO_UNQLITE_DIR}" EXCLUDE_FROM_ALL)
    include_directories("${FO_UNQLITE_DIR}")
    list(APPEND FO_COMMON_LIBS unqlite)
    DisableLibWarnings(unqlite)
    target_compile_definitions(unqlite PRIVATE "JX9_DISABLE_BUILTIN_FUNC")
    add_compile_definitions(FO_HAVE_UNQLITE=1)
else()
    add_compile_definitions(FO_HAVE_UNQLITE=0)
endif()

# Dear ImGui
StatusMessage("+ Dear ImGui")
set(FO_DEAR_IMGUI_DIR "${FO_ENGINE_ROOT}/ThirdParty/imgui")
set(FO_IMGUI_SOURCE
    "${FO_DEAR_IMGUI_DIR}/imconfig.h"
    "${FO_DEAR_IMGUI_DIR}/imgui.cpp"
    "${FO_DEAR_IMGUI_DIR}/imgui.h"
    "${FO_DEAR_IMGUI_DIR}/imgui_demo.cpp"
    "${FO_DEAR_IMGUI_DIR}/imgui_draw.cpp"
    "${FO_DEAR_IMGUI_DIR}/imgui_internal.h"
    "${FO_DEAR_IMGUI_DIR}/imgui_tables.cpp"
    "${FO_DEAR_IMGUI_DIR}/imgui_widgets.cpp"
    "${FO_DEAR_IMGUI_DIR}/imstb_rectpack.h"
    "${FO_DEAR_IMGUI_DIR}/imstb_textedit.h"
    "${FO_DEAR_IMGUI_DIR}/imstb_truetype.h"
    "${FO_ENGINE_ROOT}/Source/Common/ImGuiExt/ImGuiConfig.h"
    "${FO_ENGINE_ROOT}/Source/Common/ImGuiExt/ImGuiStuff.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/ImGuiExt/ImGuiStuff.h")
include_directories("${FO_DEAR_IMGUI_DIR}")
include_directories("${FO_ENGINE_ROOT}/Source/Common/ImGuiExt")
add_library(imgui STATIC EXCLUDE_FROM_ALL ${FO_IMGUI_SOURCE})
target_compile_definitions(imgui PRIVATE "IMGUI_USER_CONFIG=\"ImGuiConfig.h\"")
add_compile_definitions("IMGUI_USER_CONFIG=\"ImGuiConfig.h\"")
list(APPEND FO_COMMON_LIBS imgui)
DisableLibWarnings(imgui)

# Catch2
StatusMessage("+ Catch2")
set(FO_CATCH2_DIR "${FO_ENGINE_ROOT}/ThirdParty/Catch2")
include_directories("${FO_CATCH2_DIR}")
set(FO_CATCH2_SOURCE
    "${FO_CATCH2_DIR}/catch_amalgamated.cpp"
    "${FO_CATCH2_DIR}/catch_amalgamated.hpp")
add_library(Catch2 STATIC EXCLUDE_FROM_ALL ${FO_CATCH2_SOURCE})
target_compile_definitions(Catch2 PRIVATE "CATCH_AMALGAMATED_CUSTOM_MAIN")
list(APPEND FO_TESTING_LIBS Catch2)
DisableLibWarnings(Catch2)

# Backward-cpp
if(FO_WINDOWS OR FO_LINUX OR FO_MAC)
    set(FO_BACKWARDCPP_DIR "${FO_ENGINE_ROOT}/ThirdParty/backward-cpp")
    include_directories("${FO_BACKWARDCPP_DIR}")

    if(NOT FO_WINDOWS)
        check_include_file("libunwind.h" haveLibUnwind)
        check_include_file("bfd.h" haveBFD)

        if(haveLibUnwind)
            StatusMessage("+ Backward-cpp (with libunwind)")
        elseif(haveBFD)
            StatusMessage("+ Backward-cpp (with bfd)")
            list(APPEND FO_COMMON_SYSTEM_LIBS bfd)
        else()
            StatusMessage("+ Backward-cpp")
        endif()
    else()
        StatusMessage("+ Backward-cpp")
    endif()
endif()

# Spark
StatusMessage("+ Spark")
set(FO_SPARK_DIR "${FO_ENGINE_ROOT}/ThirdParty/spark")
set(SPARK_STATIC_BUILD ON CACHE BOOL "Forced by FOnline" FORCE)
add_subdirectory("${FO_SPARK_DIR}/projects/engine/core" EXCLUDE_FROM_ALL)
add_subdirectory("${FO_SPARK_DIR}/projects/external/pugi" EXCLUDE_FROM_ALL)
include_directories("${FO_SPARK_DIR}/spark/include")
include_directories("${FO_SPARK_DIR}/thirdparty/PugiXML")
list(APPEND FO_CLIENT_LIBS SPARK_Core PugiXML)
DisableLibWarnings(SPARK_Core PugiXML)

# glslang & SPIRV-Cross
if(FO_BUILD_BAKER_LIB)
    StatusMessage("+ glslang")
    set(FO_GLSLANG_DIR "${FO_ENGINE_ROOT}/ThirdParty/glslang")
    set(GLSLANG_TESTS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(BUILD_EXTERNAL OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(BUILD_WERROR OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_SPIRV ON CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_HLSL OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_SPVREMAPPER OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_AMD_EXTENSIONS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_NV_EXTENSIONS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_RTTI ON CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_EXCEPTIONS ON CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_OPT OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_PCH OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ALLOW_EXTERNAL_SPIRV_TOOLS OFF CACHE BOOL "Forced by FOnline" FORCE)

    if(FO_WEB)
        set(ENABLE_GLSLANG_WEB ON CACHE BOOL "Forced by FOnline" FORCE)
        set(ENABLE_GLSLANG_WEB_DEVEL ON CACHE BOOL "Forced by FOnline" FORCE)
        set(ENABLE_EMSCRIPTEN_SINGLE_FILE ON CACHE BOOL "Forced by FOnline" FORCE)
        set(ENABLE_EMSCRIPTEN_ENVIRONMENT_NODE OFF CACHE BOOL "Forced by FOnline" FORCE)
    endif()

    add_subdirectory("${FO_GLSLANG_DIR}" EXCLUDE_FROM_ALL)
    include_directories("${FO_GLSLANG_DIR}/glslang/Public")
    include_directories("${FO_GLSLANG_DIR}/SPIRV")
    list(APPEND FO_BAKER_LIBS glslang glslang-default-resource-limits OSDependent SPIRV GenericCodeGen MachineIndependent)
    DisableLibWarnings(glslang glslang-default-resource-limits OSDependent SPIRV GenericCodeGen MachineIndependent)

    StatusMessage("+ SPIRV-Cross")
    set(FO_SPIRV_CROSS_DIR "${FO_ENGINE_ROOT}/ThirdParty/SPIRV-Cross")
    set(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(SPIRV_CROSS_STATIC ON CACHE BOOL "Forced by FOnline" FORCE)
    set(SPIRV_CROSS_SHARED OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(SPIRV_CROSS_CLI OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "Forced by FOnline" FORCE)
    set(SPIRV_CROSS_ENABLE_HLSL ON CACHE BOOL "Forced by FOnline" FORCE)
    set(SPIRV_CROSS_ENABLE_MSL ON CACHE BOOL "Forced by FOnline" FORCE)
    set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(SPIRV_SKIP_TESTS ON CACHE BOOL "Forced by FOnline" FORCE)
    add_subdirectory("${FO_SPIRV_CROSS_DIR}" EXCLUDE_FROM_ALL)
    include_directories("${FO_SPIRV_CROSS_DIR}")
    include_directories("${FO_SPIRV_CROSS_DIR}/include")
    list(APPEND FO_BAKER_LIBS spirv-cross-core spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl)
    DisableLibWarnings(spirv-cross-core spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl)
endif()

# small_vector
include_directories("${FO_ENGINE_ROOT}/ThirdParty/small_vector/source/include/gch")

# unordered_dense
include_directories("${FO_ENGINE_ROOT}/ThirdParty/unordered_dense/include")

# AngelScript scripting
if(FO_ANGELSCRIPT_SCRIPTING)
    StatusMessage("+ AngelScript")

    # AngelScript core
    set(FO_ANGELSCRIPT_SDK_DIR "${FO_ENGINE_ROOT}/ThirdParty/AngelScript/sdk")
    set(ANGELSCRIPT_LIBRARY_NAME "AngelScriptCore" CACHE STRING "Forced by FOnline" FORCE)
    add_subdirectory("${FO_ANGELSCRIPT_SDK_DIR}/angelscript/projects/cmake" EXCLUDE_FROM_ALL)
    target_compile_definitions(AngelScriptCore PUBLIC AS_USE_NAMESPACE)
    target_compile_definitions(AngelScriptCore PUBLIC WIP_16BYTE_ALIGN)
    target_compile_definitions(AngelScriptCore PUBLIC $<$<OR:$<BOOL:${FO_WEB}>,$<BOOL:${FO_MAC}>,$<BOOL:${FO_IOS}>,$<BOOL:${FO_ANDROID}>>:AS_MAX_PORTABILITY>)
    target_include_directories(AngelScriptCore PUBLIC "${FO_ANGELSCRIPT_SDK_DIR}/angelscript/include")
    target_include_directories(AngelScriptCore PUBLIC "${FO_ANGELSCRIPT_SDK_DIR}/angelscript/source")
    list(APPEND FO_COMMON_LIBS AngelScriptCore)
    DisableLibWarnings(AngelScriptCore)

    # AngelScript preprocessor
    set(FO_ANGELSCRIPT_PREPROCESSOR_DIR "${FO_ENGINE_ROOT}/ThirdParty/AngelScript/preprocessor")
    add_library(AngelScriptPreprocessor STATIC EXCLUDE_FROM_ALL
        "${FO_ANGELSCRIPT_PREPROCESSOR_DIR}/preprocessor.h"
        "${FO_ANGELSCRIPT_PREPROCESSOR_DIR}/preprocessor.cpp")
    target_include_directories(AngelScriptCore PUBLIC "${FO_ANGELSCRIPT_PREPROCESSOR_DIR}")
    list(APPEND FO_COMMON_LIBS AngelScriptPreprocessor)
    DisableLibWarnings(AngelScriptPreprocessor)

    # AngelScript engine specific
    set(FO_ANGELSCRIPT_EXT_DIR "${FO_ENGINE_ROOT}/Source/Scripting/AngelScript")
    add_library(AngelScript STATIC EXCLUDE_FROM_ALL
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptArray.cpp"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptArray.h"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptDict.cpp"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptDict.h"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptMath.cpp"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptMath.h"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptReflection.cpp"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptReflection.h"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptString.cpp"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptString.h" 
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptWrappedCall.h"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptWrappedCall.cpp")
    target_include_directories(AngelScript PUBLIC "${FO_ANGELSCRIPT_EXT_DIR}")
    target_link_libraries(AngelScript AngelScriptCore AngelScriptPreprocessor)
    list(APPEND FO_CORE_LIBS_GROUP AngelScript)
endif()

# Mono scripting
if(FO_MONO_SCRIPTING)
    StatusMessage("+ Mono")

    set(FO_MONO_CONFIGURATION $<IF:${expr_DebugBuild},Debug,Release>)
    set(FO_MONO_TRIPLET ${FO_MONO_OS}.${FO_MONO_ARCH}.${FO_MONO_CONFIGURATION})

    set(FO_DOTNET_DIR ${CMAKE_CURRENT_BINARY_DIR}/dotnet)
    file(MAKE_DIRECTORY ${FO_DOTNET_DIR})

    include_directories(${FO_DOTNET_DIR}/output/mono/${FO_MONO_TRIPLET}/include/mono-2.0)
    link_directories(${FO_DOTNET_DIR}/output/mono/${FO_MONO_TRIPLET}/lib)

    if(FO_WINDOWS)
        set(FO_MONO_SETUP_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/setup-mono.cmd)
    else()
        set(FO_MONO_SETUP_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/setup-mono.sh)
    endif()

    add_custom_command(OUTPUT ${FO_DOTNET_DIR}/READY_${FO_MONO_TRIPLET}
        COMMAND ${FO_MONO_SETUP_SCRIPT} ${FO_MONO_OS} ${FO_MONO_ARCH} ${FO_MONO_CONFIGURATION}
        WORKING_DIRECTORY ${FO_DOTNET_DIR}
        COMMENT "Setup Mono")

    add_custom_target(SetupMono
        DEPENDS ${FO_DOTNET_DIR}/READY_${FO_MONO_TRIPLET}
        WORKING_DIRECTORY ${FO_DOTNET_DIR})
    list(APPEND FO_COMMANDS_GROUP SetupMono)
    list(APPEND FO_GEN_DEPENDENCIES SetupMono)

    list(APPEND FO_COMMON_SYSTEM_LIBS
        monosgen-2.0
        mono-component-debugger-stub-static
        mono-component-diagnostics_tracing-stub-static
        mono-component-hot_reload-stub-static
        mono-component-marshal-ilgen-stub-static)

    if(FO_WINDOWS)
        list(APPEND FO_COMMON_SYSTEM_LIBS bcrypt)
    elseif(FO_WEB)
        list(APPEND FO_COMMON_SYSTEM_LIBS mono-wasm-eh-wasm mono-wasm-nosimd)
    endif()
endif()

# App icon
set(FO_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${FO_DEV_NAME}.rc")
get_filename_component(FO_APP_ICON ${FO_APP_ICON} REALPATH)
set(FO_GEN_FILE_CONTENT "101 ICON \"${FO_APP_ICON}\"")
configure_file("${FO_ENGINE_ROOT}/BuildTools/blank.cmake.txt" ${FO_RC_FILE} FILE_PERMISSIONS OWNER_WRITE OWNER_READ)

# Engine sources
list(APPEND FO_COMMON_SOURCE
    "${FO_ENGINE_ROOT}/Source/Essentials/BasicCore.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/BasicCore.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/GlobalData.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/GlobalData.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/BaseLogging.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/BaseLogging.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/StackTrace.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/StackTrace.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/SmartPointers.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/SmartPointers.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/MemorySystem.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/MemorySystem.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/Containers.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/Containers.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/StringUtils.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/StringUtils.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/UcsTables-Include.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/DiskFileSystem.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/DiskFileSystem.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/Platform.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/Platform.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/ExceptionHadling.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/ExceptionHadling.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/SafeArithmetics.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/SafeArithmetics.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/DataSerialization.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/DataSerialization.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/HashedString.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/HashedString.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/StrongType.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/StrongType.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/TimeRelated.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/TimeRelated.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/ExtendedTypes.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/ExtendedTypes.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/Compressor.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/Compressor.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/WorkThread.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/WorkThread.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/Logging.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/Logging.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/CommonHelpers.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/CommonHelpers.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/WinApi-Include.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/WinApiUndef-Include.h"
    "${FO_ENGINE_ROOT}/Source/Common/Common.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Common.h"
    "${FO_ENGINE_ROOT}/Source/Common/AnyData.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/AnyData.h"
    "${FO_ENGINE_ROOT}/Source/Common/CacheStorage.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/CacheStorage.h"
    "${FO_ENGINE_ROOT}/Source/Common/ConfigFile.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/ConfigFile.h"
    "${FO_ENGINE_ROOT}/Source/Common/DataSource.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/DataSource.h"
    "${FO_ENGINE_ROOT}/Source/Common/EngineBase.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/EngineBase.h"
    "${FO_ENGINE_ROOT}/Source/Common/Entity.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Entity.h"
    "${FO_ENGINE_ROOT}/Source/Common/EntityProtos.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/EntityProtos.h"
    "${FO_ENGINE_ROOT}/Source/Common/EntityProperties.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/EntityProperties.h"
    "${FO_ENGINE_ROOT}/Source/Common/FileSystem.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/FileSystem.h"
    "${FO_ENGINE_ROOT}/Source/Common/Geometry.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Geometry.h"
    "${FO_ENGINE_ROOT}/Source/Common/LineTracer.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/LineTracer.h"
    "${FO_ENGINE_ROOT}/Source/Common/MapLoader.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/MapLoader.h"
    "${FO_ENGINE_ROOT}/Source/Common/NetBuffer.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/NetBuffer.h"
    "${FO_ENGINE_ROOT}/Source/Common/NetCommand.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/NetCommand.h"
    "${FO_ENGINE_ROOT}/Source/Common/Properties.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Properties.h"
    "${FO_ENGINE_ROOT}/Source/Common/PropertiesSerializator.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/PropertiesSerializator.h"
    "${FO_ENGINE_ROOT}/Source/Common/ProtoManager.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/ProtoManager.h"
    "${FO_ENGINE_ROOT}/Source/Common/ScriptSystem.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/ScriptSystem.h"
    "${FO_ENGINE_ROOT}/Source/Common/Settings.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Settings.h"
    "${FO_ENGINE_ROOT}/Source/Common/Settings-Include.h"
    "${FO_ENGINE_ROOT}/Source/Common/TextPack.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/TextPack.h"
    "${FO_ENGINE_ROOT}/Source/Common/TimeEventManager.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/TimeEventManager.h"
    "${FO_ENGINE_ROOT}/Source/Common/Timer.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Timer.h"
    "${FO_ENGINE_ROOT}/Source/Common/TwoDimensionalGrid.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/TwoDimensionalGrid.h"
    "${FO_ENGINE_ROOT}/Source/Scripting/CommonGlobalScriptMethods.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/Version-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/EmbeddedResources-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/GenericCode-Common.cpp")

list(APPEND FO_SERVER_BASE_SOURCE
    "${FO_ENGINE_ROOT}/Source/Server/Critter.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/Critter.h"
    "${FO_ENGINE_ROOT}/Source/Server/CritterManager.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/CritterManager.h"
    "${FO_ENGINE_ROOT}/Source/Server/DataBase.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/DataBase.h"
    "${FO_ENGINE_ROOT}/Source/Server/EntityManager.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/EntityManager.h"
    "${FO_ENGINE_ROOT}/Source/Server/Item.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/Item.h"
    "${FO_ENGINE_ROOT}/Source/Server/ItemManager.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/ItemManager.h"
    "${FO_ENGINE_ROOT}/Source/Server/Location.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/Location.h"
    "${FO_ENGINE_ROOT}/Source/Server/Map.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/Map.h"
    "${FO_ENGINE_ROOT}/Source/Server/MapManager.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/MapManager.h"
    "${FO_ENGINE_ROOT}/Source/Server/NetworkServer.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/NetworkServer-Asio.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/NetworkServer-Interthread.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/NetworkServer-WebSockets.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/NetworkServer.h"
    "${FO_ENGINE_ROOT}/Source/Server/Player.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/Player.h"
    "${FO_ENGINE_ROOT}/Source/Server/Server.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/Server.h"
    "${FO_ENGINE_ROOT}/Source/Server/ServerConnection.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/ServerConnection.h"
    "${FO_ENGINE_ROOT}/Source/Server/ServerEntity.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/ServerEntity.h"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerEntityScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerGlobalScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerPlayerScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerItemScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerCritterScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerMapScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerLocationScriptMethods.cpp")

list(APPEND FO_CLIENT_BASE_SOURCE
    "${FO_ENGINE_ROOT}/Source/Client/3dAnimation.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/3dAnimation.h"
    "${FO_ENGINE_ROOT}/Source/Client/3dStuff.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/3dStuff.h"
    "${FO_ENGINE_ROOT}/Source/Client/Client.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/Client.h"
    "${FO_ENGINE_ROOT}/Source/Client/ClientConnection.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/ClientConnection.h"
    "${FO_ENGINE_ROOT}/Source/Client/ClientEntity.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/ClientEntity.h"
    "${FO_ENGINE_ROOT}/Source/Client/CritterHexView.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/CritterHexView.h"
    "${FO_ENGINE_ROOT}/Source/Client/CritterView.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/CritterView.h"
    "${FO_ENGINE_ROOT}/Source/Client/DefaultSprites.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/DefaultSprites.h"
    "${FO_ENGINE_ROOT}/Source/Client/EffectManager.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/EffectManager.h"
    "${FO_ENGINE_ROOT}/Source/Client/HexView.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/HexView.h"
    "${FO_ENGINE_ROOT}/Source/Client/ItemHexView.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/ItemHexView.h"
    "${FO_ENGINE_ROOT}/Source/Client/ItemView.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/ItemView.h"
    "${FO_ENGINE_ROOT}/Source/Client/Keyboard.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/Keyboard.h"
    "${FO_ENGINE_ROOT}/Source/Client/LocationView.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/LocationView.h"
    "${FO_ENGINE_ROOT}/Source/Client/MapSprite.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/MapSprite.h"
    "${FO_ENGINE_ROOT}/Source/Client/MapView.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/MapView.h"
    "${FO_ENGINE_ROOT}/Source/Client/ModelSprites.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/ModelSprites.h"
    "${FO_ENGINE_ROOT}/Source/Client/NetworkClient.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/NetworkClient-Interthread.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/NetworkClient-Sockets.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/NetworkClient.h"
    "${FO_ENGINE_ROOT}/Source/Client/ParticleSprites.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/ParticleSprites.h"
    "${FO_ENGINE_ROOT}/Source/Client/PlayerView.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/PlayerView.h"
    "${FO_ENGINE_ROOT}/Source/Client/RenderTarget.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/RenderTarget.h"
    "${FO_ENGINE_ROOT}/Source/Client/ResourceManager.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/ResourceManager.h"
    "${FO_ENGINE_ROOT}/Source/Client/SoundManager.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/SoundManager.h"
    "${FO_ENGINE_ROOT}/Source/Client/SpriteManager.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/SpriteManager.h"
    "${FO_ENGINE_ROOT}/Source/Client/TextureAtlas.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/TextureAtlas.h"
    "${FO_ENGINE_ROOT}/Source/Client/Updater.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/Updater.h"
    "${FO_ENGINE_ROOT}/Source/Client/VideoClip.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/VideoClip.h"
    "${FO_ENGINE_ROOT}/Source/Client/VisualParticles.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/VisualParticles.h"
    "${FO_ENGINE_ROOT}/Source/Client/SparkExtension.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/SparkExtension.h"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientEntityScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientGlobalScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientPlayerScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientItemScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientCritterScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientMapScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientLocationScriptMethods.cpp")

list(APPEND FO_SERVER_SOURCE
    ${FO_SERVER_BASE_SOURCE}
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Server.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Server.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Server.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Server.cpp")

list(APPEND FO_CLIENT_SOURCE
    ${FO_CLIENT_BASE_SOURCE}
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Client.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Client.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Client.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Client.cpp")

list(APPEND FO_EDITOR_SOURCE
    "${FO_ENGINE_ROOT}/Source/Tools/Editor.h"
    "${FO_ENGINE_ROOT}/Source/Tools/Editor.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/AssetExplorer.h"
    "${FO_ENGINE_ROOT}/Source/Tools/AssetExplorer.cpp"

    # "${FO_ENGINE_ROOT}/Source/Tools/InterfaceEditor.h"
    # "${FO_ENGINE_ROOT}/Source/Tools/InterfaceEditor.cpp"
    # "${FO_ENGINE_ROOT}/Source/Tools/ProtoEditor.h"
    # "${FO_ENGINE_ROOT}/Source/Tools/ProtoEditor.cpp"
    # "${FO_ENGINE_ROOT}/Source/Tools/ModelEditor.h"
    # "${FO_ENGINE_ROOT}/Source/Tools/ModelEditor.cpp"
    # "${FO_ENGINE_ROOT}/Source/Tools/EffectEditor.h"
    # "${FO_ENGINE_ROOT}/Source/Tools/EffectEditor.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/ParticleEditor.h"
    "${FO_ENGINE_ROOT}/Source/Tools/ParticleEditor.cpp")

list(APPEND FO_MAPPER_SOURCE
    "${FO_ENGINE_ROOT}/Source/Tools/Mapper.h"
    "${FO_ENGINE_ROOT}/Source/Tools/Mapper.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/MapperGlobalScriptMethods.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Mapper.cpp")

list(APPEND FO_BAKER_SOURCE
    "${FO_ENGINE_ROOT}/Source/Tools/AngelScriptBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/AngelScriptBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/Baker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/Baker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/EffectBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/EffectBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/ImageBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/ImageBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/MapBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/MapBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/ModelBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/ModelBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/ProtoBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/ProtoBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/ProtoTextBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/ProtoTextBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/RawCopyBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/RawCopyBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/TextBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/TextBaker.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Baker.cpp")

if(FO_ANGELSCRIPT_SCRIPTING)
    list(APPEND FO_ASCOMPILER_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ServerCompiler.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-ServerCompiler.cpp")

    list(APPEND FO_ASCOMPILER_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ClientCompiler.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-ClientCompiler.cpp")

    list(APPEND FO_ASCOMPILER_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-MapperCompiler.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-MapperCompiler.cpp")

    list(APPEND FO_BAKER_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ServerCompilerValidation.cpp")
endif()

if(MSVC)
    list(APPEND FO_COMMON_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/natvis/fonline.natjmc"
        "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/natvis/unordered_dense.natvis")
endif()

list(APPEND FO_SOURCE_META_FILES
    "${FO_ENGINE_ROOT}/Source/Essentials/ExtendedTypes.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/TimeRelated.h"
    "${FO_ENGINE_ROOT}/Source/Frontend/Application.h"
    "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.h"
    "${FO_ENGINE_ROOT}/Source/Common/Common.h"
    "${FO_ENGINE_ROOT}/Source/Common/Entity.h"
    "${FO_ENGINE_ROOT}/Source/Common/EntityProperties.h"
    "${FO_ENGINE_ROOT}/Source/Common/Geometry.h"
    "${FO_ENGINE_ROOT}/Source/Common/Settings-Include.h"
    "${FO_ENGINE_ROOT}/Source/Common/DataRegistration-Template.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/GenericCode-Template.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/TextPack.h"
    "${FO_ENGINE_ROOT}/Source/Client/Client.h"
    "${FO_ENGINE_ROOT}/Source/Client/MapSprite.h"
    "${FO_ENGINE_ROOT}/Source/Client/MapView.h"
    "${FO_ENGINE_ROOT}/Source/Server/Critter.h"
    "${FO_ENGINE_ROOT}/Source/Server/Item.h"
    "${FO_ENGINE_ROOT}/Source/Server/Location.h"
    "${FO_ENGINE_ROOT}/Source/Server/Map.h"
    "${FO_ENGINE_ROOT}/Source/Server/Player.h"
    "${FO_ENGINE_ROOT}/Source/Server/Server.h"
    "${FO_ENGINE_ROOT}/Source/Tools/Mapper.h"
    "${FO_ENGINE_ROOT}/Source/Scripting/AngelScriptScripting-Template.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/MonoScripting-Template.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/NativeScripting-Template.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerEntityScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerGlobalScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerPlayerScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerItemScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerCritterScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerMapScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerLocationScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientEntityScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientGlobalScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientPlayerScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientItemScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientCritterScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientMapScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientLocationScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/MapperGlobalScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/CommonGlobalScriptMethods.cpp")

list(APPEND FO_TESTS_SOURCE
    "${FO_ENGINE_ROOT}/Source/Tests/Test_AnyData.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_GenericUtils.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_Geometry.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_StringUtils.cpp")

# Code generation
include(FindPython3)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

list(APPEND FO_CODEGEN_COMMAND_ARGS -maincfg "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}")
list(APPEND FO_CODEGEN_COMMAND_ARGS -buildhash "${FO_BUILD_HASH}")
list(APPEND FO_CODEGEN_COMMAND_ARGS -genoutput "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource")
list(APPEND FO_CODEGEN_COMMAND_ARGS -devname "${FO_DEV_NAME}")
list(APPEND FO_CODEGEN_COMMAND_ARGS -nicename "${FO_NICE_NAME}")

if(FO_NATIVE_SCRIPTING)
    list(APPEND FO_CODEGEN_COMMAND_ARGS -native)
endif()

if(FO_ANGELSCRIPT_SCRIPTING)
    list(APPEND FO_CODEGEN_COMMAND_ARGS -angelscript)
endif()

if(FO_MONO_SCRIPTING)
    list(APPEND FO_CODEGEN_COMMAND_ARGS -csharp)
endif()

foreach(entry ${FO_MONO_ASSEMBLIES})
    list(APPEND FO_CODEGEN_COMMAND_ARGS -monoassembly ${entry})

    foreach(ref ${MonoAssembly_${entry}_CommonRefs})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoserverref "${entry},${ref}")
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoclientref "${entry},${ref}")
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monomapperref "${entry},${ref}")
    endforeach()

    foreach(ref ${MonoAssembly_${entry}_ServerRefs})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoserverref "${entry},${ref}")
    endforeach()

    foreach(ref ${MonoAssembly_${entry}_ClientRefs})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoclientref "${entry},${ref}")
    endforeach()

    foreach(ref ${MonoAssembly_${entry}_MapperRefs})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monomapperref "${entry},${ref}")
    endforeach()

    foreach(src ${MonoAssembly_${entry}_CommonSource})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoserversource "${entry},${src}")
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoclientsource "${entry},${src}")
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monomappersource "${entry},${src}")
        list(APPEND FO_MONO_SOURCE ${src})
    endforeach()

    foreach(src ${MonoAssembly_${entry}_ServerSource})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoserversource "${entry},${src}")
        list(APPEND FO_MONO_SOURCE ${src})
    endforeach()

    foreach(src ${MonoAssembly_${entry}_ClientSource})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoclientsource "${entry},${src}")
        list(APPEND FO_MONO_SOURCE ${src})
    endforeach()

    foreach(src ${MonoAssembly_${entry}_MapperSource})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monomappersource "${entry},${src}")
        list(APPEND FO_MONO_SOURCE ${src})
    endforeach()
endforeach()

list(APPEND FO_CODEGEN_META_SOURCE
    ${FO_SOURCE_META_FILES}
    ${FO_MONO_SOURCE})

foreach(entry ${FO_CODEGEN_META_SOURCE})
    list(APPEND FO_CODEGEN_COMMAND_ARGS -meta ${entry})
endforeach()

foreach(entry ${FO_ADDED_COMMON_HEADERS})
    list(APPEND FO_CODEGEN_COMMAND_ARGS -commonheader ${entry})
endforeach()

list(APPEND FO_CODEGEN_OUTPUT
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/CodeGenTouch"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/Version-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/EmbeddedResources-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Server.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Client.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Baker.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-ServerCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-ClientCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-MapperCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/GenericCode-Common.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Server.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Client.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ServerCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ServerCompilerValidation.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ClientCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-MapperCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Server.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Client.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Server.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Client.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Mapper.cpp")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/codegen-args.txt" "")

foreach(entry ${FO_CODEGEN_COMMAND_ARGS})
    file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/codegen-args.txt" "${entry}\n")
endforeach()

set(FO_CODEGEN_COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/codegen.py" "@${CMAKE_CURRENT_BINARY_DIR}/codegen-args.txt")

add_custom_command(OUTPUT ${FO_CODEGEN_OUTPUT}
    COMMAND ${FO_CODEGEN_COMMAND}
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/CodeGenTouch"
    DEPENDS ${FO_CODEGEN_META_SOURCE}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Code generation")

add_custom_target(CodeGeneration
    DEPENDS ${FO_CODEGEN_OUTPUT}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND FO_COMMANDS_GROUP CodeGeneration)
list(APPEND FO_GEN_DEPENDENCIES CodeGeneration)

add_custom_target(ForceCodeGeneration
    COMMAND ${FO_CODEGEN_COMMAND}
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/CodeGenTouch"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND FO_COMMANDS_GROUP ForceCodeGeneration)

# Core libs
StatusMessage("Core libs:")

if(FO_BUILD_COMMON_LIB)
    StatusMessage("+ AppHeadless")
    add_library(AppHeadless STATIC EXCLUDE_FROM_ALL
        "${FO_ENGINE_ROOT}/Source/Frontend/Application.h"
        "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationInit.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationHeadless.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.h")
    add_dependencies(AppHeadless ${FO_GEN_DEPENDENCIES})
    list(APPEND FO_CORE_LIBS_GROUP AppHeadless)

    if(NOT FO_HEADLESS_ONLY)
        StatusMessage("+ AppFrontend")
        add_library(AppFrontend STATIC EXCLUDE_FROM_ALL
            "${FO_ENGINE_ROOT}/Source/Frontend/Application.h"
            "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationInit.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Application.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.h"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-Direct3D.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-OpenGL.cpp")
        add_dependencies(AppFrontend ${FO_GEN_DEPENDENCIES})
        target_link_libraries(AppFrontend ${FO_RENDER_SYSTEM_LIBS} ${FO_RENDER_LIBS})
        list(APPEND FO_CORE_LIBS_GROUP AppFrontend)
    endif()

    StatusMessage("+ CommonLib")
    add_library(CommonLib STATIC EXCLUDE_FROM_ALL ${FO_COMMON_SOURCE})
    add_dependencies(CommonLib ${FO_GEN_DEPENDENCIES})
    target_link_libraries(CommonLib ${FO_COMMON_SYSTEM_LIBS} ${FO_COMMON_LIBS} $<$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}>:AngelScript>)
    list(APPEND FO_CORE_LIBS_GROUP CommonLib)
endif()

if(FO_BUILD_CLIENT_LIB)
    StatusMessage("+ ClientLib")
    add_library(ClientLib STATIC EXCLUDE_FROM_ALL ${FO_CLIENT_SOURCE})
    add_dependencies(ClientLib ${FO_GEN_DEPENDENCIES})
    target_link_libraries(ClientLib CommonLib ${FO_CLIENT_SYSTEM_LIBS} ${FO_CLIENT_LIBS})
    list(APPEND FO_CORE_LIBS_GROUP ClientLib)
endif()

if(FO_BUILD_SERVER_LIB)
    StatusMessage("+ ServerLib")
    add_library(ServerLib STATIC EXCLUDE_FROM_ALL ${FO_SERVER_SOURCE})
    add_dependencies(ServerLib ${FO_GEN_DEPENDENCIES})
    target_link_libraries(ServerLib CommonLib ${FO_SERVER_SYSTEM_LIBS} ${FO_SERVER_LIBS})
    list(APPEND FO_CORE_LIBS_GROUP ServerLib)
endif()

if(FO_BUILD_ASCOMPILER_LIB)
    StatusMessage("+ ASCompilerLib")
    add_library(ASCompilerLib STATIC EXCLUDE_FROM_ALL ${FO_ASCOMPILER_SOURCE})
    add_dependencies(ASCompilerLib ${FO_GEN_DEPENDENCIES})
    target_link_libraries(ASCompilerLib CommonLib)
    list(APPEND FO_CORE_LIBS_GROUP ASCompilerLib)
endif()

if(FO_BUILD_MAPPER_LIB)
    StatusMessage("+ MapperLib")
    add_library(MapperLib STATIC EXCLUDE_FROM_ALL ${FO_MAPPER_SOURCE})
    add_dependencies(MapperLib ${FO_GEN_DEPENDENCIES})
    target_link_libraries(MapperLib ClientLib CommonLib)
    list(APPEND FO_CORE_LIBS_GROUP MapperLib)
endif()

if(FO_BUILD_BAKER_LIB)
    StatusMessage("+ BakerLib")
    add_library(BakerLib STATIC EXCLUDE_FROM_ALL ${FO_BAKER_SOURCE})
    add_dependencies(BakerLib ${FO_GEN_DEPENDENCIES})
    target_link_libraries(BakerLib CommonLib ${FO_BAKER_SYSTEM_LIBS} ${FO_BAKER_LIBS})
    target_link_libraries(BakerLib $<$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}>:ASCompilerLib>)
    list(APPEND FO_CORE_LIBS_GROUP BakerLib)
endif()

if(FO_BUILD_EDITOR_LIB)
    StatusMessage("+ EditorLib")
    add_library(EditorLib STATIC EXCLUDE_FROM_ALL ${FO_EDITOR_SOURCE})
    add_dependencies(EditorLib ${FO_GEN_DEPENDENCIES})
    target_link_libraries(EditorLib BakerLib CommonLib)
    list(APPEND FO_CORE_LIBS_GROUP EditorLib)
endif()

# Applications
StatusMessage("Applications:")

if(FO_BUILD_CLIENT)
    if(NOT FO_BUILD_LIBRARY)
        StatusMessage("+ ${FO_DEV_NAME}_Client")
        list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Client")
        add_executable(${FO_DEV_NAME}_Client WIN32 "${FO_ENGINE_ROOT}/Source/Applications/ClientApp.cpp" ${FO_RC_FILE})

        # Todo: cmake make bundles for Mac and iOS
        # add_executable( ${FO_DEV_NAME}_Client MACOSX_BUNDLE ... ${FO_RC_FILE} )
        set_target_properties(${FO_DEV_NAME}_Client PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_CLIENT_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_OUTPUT_PATH})
    else()
        StatusMessage("+ ${FO_DEV_NAME}_Client (shared library)")
        list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Client")
        add_library(${FO_DEV_NAME}_Client SHARED "${FO_ENGINE_ROOT}/Source/Applications/ClientApp.cpp")
        set_target_properties(${FO_DEV_NAME}_Client PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${FO_CLIENT_OUTPUT})
    endif()

    set_target_properties(${FO_DEV_NAME}_Client PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_Client")
    set_target_properties(${FO_DEV_NAME}_Client PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0")
    target_link_libraries(${FO_DEV_NAME}_Client "AppFrontend" "ClientLib")
    WriteBuildHash(${FO_DEV_NAME}_Client)
endif()

if(FO_BUILD_SERVER)
    StatusMessage("+ ${FO_DEV_NAME}_Server")
    list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Server")
    add_executable(${FO_DEV_NAME}_Server WIN32 "${FO_ENGINE_ROOT}/Source/Applications/ServerApp.cpp" ${FO_RC_FILE})
    set_target_properties(${FO_DEV_NAME}_Server PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SERVER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_OUTPUT_PATH})
    set_target_properties(${FO_DEV_NAME}_Server PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_Server")
    set_target_properties(${FO_DEV_NAME}_Server PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0")
    target_link_libraries(${FO_DEV_NAME}_Server "AppFrontend" "ServerLib" "ClientLib")
    WriteBuildHash(${FO_DEV_NAME}_Server)

    StatusMessage("+ ${FO_DEV_NAME}_ServerHeadless")
    list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_ServerHeadless")
    add_executable(${FO_DEV_NAME}_ServerHeadless "${FO_ENGINE_ROOT}/Source/Applications/ServerHeadlessApp.cpp")
    set_target_properties(${FO_DEV_NAME}_ServerHeadless PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SERVER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_OUTPUT_PATH})
    set_target_properties(${FO_DEV_NAME}_ServerHeadless PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_ServerHeadless")
    set_target_properties(${FO_DEV_NAME}_ServerHeadless PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0")
    target_link_libraries(${FO_DEV_NAME}_ServerHeadless "AppHeadless" "ServerLib")
    WriteBuildHash(${FO_DEV_NAME}_ServerHeadless)

    if(FO_WINDOWS)
        StatusMessage("+ ${FO_DEV_NAME}_ServerService")
        list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_ServerService")
        add_executable(${FO_DEV_NAME}_ServerService "${FO_ENGINE_ROOT}/Source/Applications/ServerServiceApp.cpp")
        set_target_properties(${FO_DEV_NAME}_ServerService PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SERVER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_OUTPUT_PATH})
        set_target_properties(${FO_DEV_NAME}_ServerService PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_ServerService")
        set_target_properties(${FO_DEV_NAME}_ServerService PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0")
        target_link_libraries(${FO_DEV_NAME}_ServerService "AppHeadless" "ServerLib")
        WriteBuildHash(${FO_DEV_NAME}_ServerService)
    else()
        StatusMessage("+ ${FO_DEV_NAME}_ServerDaemon")
        list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_ServerDaemon")
        add_executable(${FO_DEV_NAME}_ServerDaemon "${FO_ENGINE_ROOT}/Source/Applications/ServerDaemonApp.cpp")
        set_target_properties(${FO_DEV_NAME}_ServerDaemon PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SERVER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_OUTPUT_PATH})
        set_target_properties(${FO_DEV_NAME}_ServerDaemon PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_ServerDaemon")
        set_target_properties(${FO_DEV_NAME}_ServerDaemon PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0")
        target_link_libraries(${FO_DEV_NAME}_ServerDaemon "AppHeadless" "ServerLib")
        WriteBuildHash(${FO_DEV_NAME}_ServerDaemon)
    endif()
endif()

if(FO_BUILD_EDITOR)
    StatusMessage("+ ${FO_DEV_NAME}_Editor")
    list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Editor")
    add_executable(${FO_DEV_NAME}_Editor WIN32 "${FO_ENGINE_ROOT}/Source/Applications/EditorApp.cpp" ${FO_RC_FILE})
    set_target_properties(${FO_DEV_NAME}_Editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_EDITOR_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_OUTPUT_PATH})
    set_target_properties(${FO_DEV_NAME}_Editor PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_Editor")
    set_target_properties(${FO_DEV_NAME}_Editor PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0")
    target_link_libraries(${FO_DEV_NAME}_Editor "AppFrontend" "EditorLib" "MapperLib" "BakerLib" "ClientLib" "ServerLib")
    WriteBuildHash(${FO_DEV_NAME}_Editor)
endif()

if(FO_BUILD_MAPPER)
    StatusMessage("+ ${FO_DEV_NAME}_Mapper")
    list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Mapper")
    add_executable(${FO_DEV_NAME}_Mapper WIN32 "${FO_ENGINE_ROOT}/Source/Applications/MapperApp.cpp" ${FO_RC_FILE})
    set_target_properties(${FO_DEV_NAME}_Mapper PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_MAPPER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_OUTPUT_PATH})
    set_target_properties(${FO_DEV_NAME}_Mapper PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_Mapper")
    set_target_properties(${FO_DEV_NAME}_Mapper PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0")
    target_link_libraries(${FO_DEV_NAME}_Mapper "AppFrontend" "MapperLib" "ClientLib" "BakerLib")
    WriteBuildHash(${FO_DEV_NAME}_Mapper)
endif()

if(FO_BUILD_ASCOMPILER)
    StatusMessage("+ ${FO_DEV_NAME}_ASCompiler")
    list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_ASCompiler")
    add_executable(${FO_DEV_NAME}_ASCompiler "${FO_ENGINE_ROOT}/Source/Applications/ASCompilerApp.cpp")
    add_dependencies(${FO_DEV_NAME}_ASCompiler ${FO_GEN_DEPENDENCIES})
    set_target_properties(${FO_DEV_NAME}_ASCompiler PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_ASCOMPILER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_OUTPUT_PATH})
    set_target_properties(${FO_DEV_NAME}_ASCompiler PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_ASCompiler")
    set_target_properties(${FO_DEV_NAME}_ASCompiler PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0")
    target_link_libraries(${FO_DEV_NAME}_ASCompiler "AppHeadless" "ASCompilerLib")
    WriteBuildHash(${FO_DEV_NAME}_ASCompiler)
endif()

if(FO_BUILD_BAKER)
    StatusMessage("+ ${FO_DEV_NAME}_Baker")
    list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Baker")
    add_executable(${FO_DEV_NAME}_Baker "${FO_ENGINE_ROOT}/Source/Applications/BakerApp.cpp")
    set_target_properties(${FO_DEV_NAME}_Baker PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_BAKER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_OUTPUT_PATH})
    set_target_properties(${FO_DEV_NAME}_Baker PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_Baker")
    set_target_properties(${FO_DEV_NAME}_Baker PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0")
    target_link_libraries(${FO_DEV_NAME}_Baker "AppHeadless" "BakerLib")
    WriteBuildHash(${FO_DEV_NAME}_Baker)

    if(NOT FO_WEB)
        StatusMessage("+ ${FO_DEV_NAME}_BakerLib")
        list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_BakerLib")
        add_library(${FO_DEV_NAME}_BakerLib SHARED "${FO_ENGINE_ROOT}/Source/Applications/BakerLib.cpp")
        set_target_properties(${FO_DEV_NAME}_BakerLib PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_BAKER_OUTPUT})
        set_target_properties(${FO_DEV_NAME}_BakerLib PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_BakerLib")
        target_link_libraries(${FO_DEV_NAME}_BakerLib PRIVATE "BakerLib")
        WriteBuildHash(${FO_DEV_NAME}_BakerLib)
    endif()
endif()

if(FO_UNIT_TESTS OR FO_CODE_COVERAGE)
    macro(SetupTestBuild name)
        set(target ${FO_DEV_NAME}_${name})
        StatusMessage("+ ${target}")
        list(APPEND FO_APPLICATIONS_GROUP ${target})
        add_executable(${target}
            "${FO_ENGINE_ROOT}/Source/Applications/TestingApp.cpp"
            ${FO_TESTS_SOURCE}
            "${FO_ENGINE_ROOT}/Source/Applications/EditorApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/MapperApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/BakerApp.cpp")
        add_dependencies(${target} ${FO_GEN_DEPENDENCIES})
        set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_TESTS_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_TESTS_OUTPUT})
        set_target_properties(${target} PROPERTIES OUTPUT_NAME ${target})
        set_target_properties(${target} PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=1")
        target_link_libraries(${target} "BakerLib" "EditorLib" "MapperLib" "${FO_TESTING_LIBS}")

        target_sources(${target} PRIVATE
            "${FO_ENGINE_ROOT}/Source/Applications/ServerApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ServerServiceApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ServerDaemonApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ServerHeadlessApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ClientApp.cpp")
        target_link_libraries(${target} "ClientLib" "ServerLib")

        if(FO_ANGELSCRIPT_SCRIPTING)
            target_link_libraries(${target} "ASCompilerLib")
        endif()

        target_link_libraries(${target} "AppHeadless")

        add_custom_target(Run${name}
            COMMAND ${target}
            COMMENT "Run ${name}")
        list(APPEND FO_COMMANDS_GROUP "Run${name}")
    endmacro()

    if(FO_UNIT_TESTS)
        SetupTestBuild(UnitTests)
    endif()

    if(FO_CODE_COVERAGE)
        SetupTestBuild(CodeCoverage)
    endif()
endif()

# Scripts compilation
set(compileASScripts "")
set(compileMonoScripts "")

if(FO_NATIVE_SCRIPTING OR FO_ANGELSCRIPT_SCRIPTING OR FO_MONO_SCRIPTING)
    # Compile AngelScript scripts
    if(FO_ANGELSCRIPT_SCRIPTING)
        set(compileASScripts ${FO_DEV_NAME}_ASCompiler)

        list(APPEND compileASScripts -ApplyConfig "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}" -ApplySubConfig "NONE")

        add_custom_target(CompileAngelScript
            COMMAND ${compileASScripts}
            DEPENDS ForceCodeGeneration
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            COMMENT "Compile AngelScript scripts")
        list(APPEND FO_COMMANDS_GROUP CompileAngelScript)
    endif()

    # Compile Mono scripts
    if(FO_MONO_SCRIPTING)
        set(monoCompileCommands "")

        foreach(entry ${FO_MONO_ASSEMBLIES})
            list(APPEND monoCompileCommands -assembly ${entry})
        endforeach()

        set(compileMonoScripts ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/compile-mono-scripts.py" ${monoCompileCommands})

        add_custom_target(CompileMonoScripts
            COMMAND ${compileMonoScripts}
            SOURCES ${FO_MONO_SOURCE}
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            COMMENT "Compile Mono scripts")
        list(APPEND FO_COMMANDS_GROUP CompileMonoScripts)
    endif()
endif()

# Baking
set(bakeResources "${FO_DEV_NAME}_Baker")
list(APPEND bakeResources -ApplyConfig "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}" -ApplySubConfig "NONE")

add_custom_target(BakeResources
    COMMAND ${bakeResources} -ForceBaking False
    DEPENDS ForceCodeGeneration
    WORKING_DIRECTORY ${FO_OUTPUT_PATH}
    COMMENT "Bake resources")
list(APPEND FO_COMMANDS_GROUP BakeResources)

add_custom_target(ForceBakeResources
    COMMAND ${bakeResources} -ForceBaking True
    DEPENDS ForceCodeGeneration
    WORKING_DIRECTORY ${FO_OUTPUT_PATH}
    COMMENT "Bake resources")
list(APPEND FO_COMMANDS_GROUP ForceBakeResources)

# Packaging
StatusMessage("Packages:")

foreach(package ${FO_PACKAGES})
    StatusMessage("+ Package ${package}")

    add_custom_target(MakePackage-${package}
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${FO_OUTPUT_PATH}/${FO_DEV_NAME}-${package}"
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        COMMENT "Make package ${package}")
    list(APPEND FO_COMMANDS_GROUP "MakePackage-${package}")

    foreach(entry ${Package_${package}_Parts})
        set(packageCommands ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/package.py")

        list(APPEND packageCommands -maincfg "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}")
        list(APPEND packageCommands -buildhash "${FO_BUILD_HASH}")
        list(APPEND packageCommands -devname "${FO_DEV_NAME}")
        list(APPEND packageCommands -nicename "${FO_NICE_NAME}")

        string(REPLACE "," ";" entry ${entry})
        list(GET entry 0 target)
        list(GET entry 1 platform)
        list(GET entry 2 arch)
        list(GET entry 3 pack)
        list(GET entry 4 customConfig)

        list(APPEND packageCommands -target "${target}")
        list(APPEND packageCommands -platform "${platform}")
        list(APPEND packageCommands -arch "${arch}")
        list(APPEND packageCommands -pack "${pack}")

        if(customConfig)
            set(config ${customConfig})
        else()
            set(config ${Package_${package}_Config})
        endif()

        list(APPEND packageCommands -config "${config}")

        if(FO_ANGELSCRIPT_SCRIPTING)
            list(APPEND packageCommands -angelscript)
        endif()

        if(FO_MONO_SCRIPTING)
            list(APPEND packageCommands -mono)
        endif()

        list(APPEND packageCommands -input ${FO_OUTPUT_PATH})
        list(APPEND packageCommands -output "${FO_OUTPUT_PATH}/${FO_DEV_NAME}-${package}")

        StatusMessage("  ${target} for ${platform}-${arch} in ${pack} with ${config} config")
        add_custom_command(TARGET MakePackage-${package} POST_BUILD COMMAND ${packageCommands})
    endforeach()
endforeach()

# Copy ReSharper config
if(MSVC)
    if(FO_RESHARPER_SETTINGS)
        file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/${FO_RESHARPER_SETTINGS}" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.sln.DotSettings" COPY_ON_ERROR)
    else()
        file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/ReSharper.sln.DotSettings" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.sln.DotSettings" COPY_ON_ERROR)
    endif()
endif()

# Setup targets grouping
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(TARGET ${FO_APPLICATIONS_GROUP} PROPERTY FOLDER "Applications")
set_property(TARGET ${FO_CORE_LIBS_GROUP} PROPERTY FOLDER "CoreLibs")
set_property(TARGET ${FO_COMMANDS_GROUP} PROPERTY FOLDER "Commands")
set_property(TARGET ${FO_COMMON_LIBS} ${FO_BAKER_LIBS} ${FO_SERVER_LIBS} ${FO_CLIENT_LIBS} ${FO_RENDER_LIBS} ${FO_TESTING_LIBS} ${FO_DUMMY_TARGETS} PROPERTY FOLDER "ThirdParty")
set_property(TARGET ${FO_DUMMY_TARGETS} PROPERTY FOLDER "ThirdParty/Dummy")

# Print cached variables
if(FO_VERBOSE_BUILD)
    get_cmake_property(FO_CACHE_VARIABLES CACHE_VARIABLES)
    list(SORT FO_CACHE_VARIABLES)

    StatusMessage("Forced variables:")

    foreach(varName ${FO_CACHE_VARIABLES})
        get_property(str CACHE ${varName} PROPERTY HELPSTRING)
        get_property(type CACHE ${varName} PROPERTY TYPE)
        string(FIND "${str}" "Forced by FOnline" forced)

        if(NOT "${forced}" STREQUAL "-1")
            StatusMessage("- ${varName}: '${${varName}}' type: '${type}'")
        endif()
    endforeach()

    StatusMessage("Default variables:")

    foreach(varName ${FO_CACHE_VARIABLES})
        get_property(str CACHE ${varName} PROPERTY HELPSTRING)
        get_property(type CACHE ${varName} PROPERTY TYPE)
        string(FIND "${str}" "Forced by FOnline" forced)

        if("${forced}" STREQUAL "-1" AND NOT "${type}" STREQUAL "INTERNAL" AND NOT "${type}" STREQUAL "STATIC" AND NOT "${type}" STREQUAL "UNINITIALIZED")
            StatusMessage("- ${varName}: '${${varName}}' docstring: '${str}' type: '${type}'")
        endif()
    endforeach()
endif()
