cmake_minimum_required(VERSION 3.16.3)

# Force some variables for internal debugging purposes
if(NOT ${FO_FORCE_SINGLEPLAYER} STREQUAL "")
    set(FO_SINGLEPLAYER ${FO_FORCE_SINGLEPLAYER})
endif()

if(NOT ${FO_FORCE_ENABLE_3D} STREQUAL "")
    set(FO_ENABLE_3D ${FO_FORCE_ENABLE_3D})
endif()

# Configuration checks
if(FO_CODE_COVERAGE AND(FO_BUILD_CLIENT OR FO_BUILD_SERVER OR FO_BUILD_EDITOR OR FO_BUILD_MAPPER OR FO_BUILD_SINGLE OR FO_BUILD_ASCOMPILER OR FO_BUILD_BAKER OR FO_UNIT_TESTS))
    AbortMessage("Code coverge build can not be mixed with other builds")
endif()

if(FO_BUILD_SINGLE AND(FO_BUILD_SERVER OR FO_BUILD_CLIENT))
    AbortMessage("Singleplayer/Multiplayer configuration mismatch")
endif()

if(FO_SINGLEPLAYER AND(FO_BUILD_SERVER OR FO_BUILD_CLIENT))
    AbortMessage("No client or server builds in singleplayer mode")
endif()

if(NOT FO_SINGLEPLAYER AND FO_BUILD_SINGLE)
    AbortMessage("No single build in multiplayer mode")
endif()

if(FO_BUILD_ASCOMPILER AND NOT FO_ANGELSCRIPT_SCRIPTING)
    AbortMessage("AngelScript compiler build can not be without AngelScript scripting enabled")
endif()

# Global defines
add_compile_definitions(FO_SINGLEPLAYER=$<BOOL:${FO_SINGLEPLAYER}>)
add_compile_definitions(FO_ENABLE_3D=$<BOOL:${FO_ENABLE_3D}>)
add_compile_definitions(FO_NATIVE_SCRIPTING=$<BOOL:${FO_NATIVE_SCRIPTING}>)
add_compile_definitions(FO_ANGELSCRIPT_SCRIPTING=$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}>)
add_compile_definitions(FO_MONO_SCRIPTING=$<BOOL:${FO_MONO_SCRIPTING}>)
add_compile_definitions(FO_GEOMETRY=$<IF:$<STREQUAL:${FO_GEOMETRY},HEXAGONAL>,1,$<IF:$<STREQUAL:${FO_GEOMETRY},SQUARE>,2,0>>)
add_compile_definitions(FO_NO_MANUAL_STACK_TRACE=$<CONFIG:Release_Ext>)
add_compile_definitions(FO_NO_EXTRA_ASSERTS=0) # Todo: FO_NO_EXTRA_ASSERTS=$<CONFIG:Release_Ext> for first need separate asserts from valid error
add_compile_definitions(FO_NO_TEXTURE_LOOKUP=$<CONFIG:Release_Ext>)
add_compile_definitions(FO_DIRECT_SPRITES_DRAW=$<CONFIG:Release_Ext>)

# Compiler options
set(CMAKE_CXX_STANDARD 17)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/std:c++17>)
    add_compile_options(/permissive-)
else()
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-std=c++17>)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(--param=max-vartrack-size=1000000)
endif()

if(FO_CODE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-O0 -fprofile -instr-generate -fcoverage-mapping)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        add_compile_options(-O0 --coverage)
        add_link_options(--coverage)
    endif()
endif()

# Basic includes
include_directories("${FO_ENGINE_ROOT}/Source/Common")
include_directories("${FO_ENGINE_ROOT}/Source/Server")
include_directories("${FO_ENGINE_ROOT}/Source/Client")
include_directories("${FO_ENGINE_ROOT}/Source/Tools")
include_directories("${FO_ENGINE_ROOT}/Source/Scripting")
include_directories("${FO_ENGINE_ROOT}/Source/Frontend")
include_directories("${FO_ENGINE_ROOT}/Source/Singleplayer")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource")

# Third-party libs
StatusMessage("Third-party libs:")

# Rpmalloc
if(WIN32 OR LINUX OR APPLE OR ANDROID)
    StatusMessage("+ Rpmalloc")
    set(FO_RPMALLOC_DIR "${FO_ENGINE_ROOT}/ThirdParty/rpmalloc")
    set(FO_RPMALLOC_SOURCE
        "${FO_RPMALLOC_DIR}/rpmalloc/rpmalloc.c"
        "${FO_RPMALLOC_DIR}/rpmalloc/rpmalloc.h"
        "${FO_RPMALLOC_DIR}/rpmalloc/rpnew.h")
    include_directories("${FO_RPMALLOC_DIR}/rpmalloc")
    add_library(rpmalloc ${FO_RPMALLOC_SOURCE})
    add_compile_definitions(FO_HAVE_RPMALLOC=1)
    add_compile_definitions(ENABLE_PRELOAD=${expr_StandaloneRpmallocEnabled})
    target_compile_definitions(rpmalloc PRIVATE "$<$<PLATFORM_ID:Linux>:_GNU_SOURCE>")
    list(APPEND FO_COMMON_LIBS "rpmalloc")
    DisableLibWarnings(rpmalloc)
else()
    add_compile_definitions(FO_HAVE_RPMALLOC=0)
endif()

# SDL2
StatusMessage("+ SDL2")
set(FO_SDL_DIR "${FO_ENGINE_ROOT}/ThirdParty/SDL2")
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "Forced by FOnline" FORCE)
set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "Forced by FOnline" FORCE)
set(SDL_SHARED OFF CACHE BOOL "Forced by FOnline" FORCE)
set(SDL_STATIC ON CACHE BOOL "Forced by FOnline" FORCE)
set(SDL_TEST OFF CACHE BOOL "Forced by FOnline" FORCE)

if(WIN32 AND WINRT)
    include("${FO_ENGINE_ROOT}/BuildTools/sdl-winrt.cmake")
    add_library(SDL2-static STATIC ${FO_SDL_WINRT_SOURCE} ${FO_SDL_WINRT_CX_SOURCE})
    add_library(SDL2main STATIC "${FO_SDL_DIR}/src/main/winrt/SDL_winrt_main_NonXAML.cpp")
    target_compile_options(SDL2-static PRIVATE "/std:c++14")
    set_property(TARGET SDL2-static PROPERTY CXX_STANDARD 14)
    target_compile_definitions(SDL2-static PRIVATE "_CRT_SECURE_NO_WARNINGS")
else()
    add_subdirectory("${FO_SDL_DIR}")
endif()

include_directories("${FO_SDL_DIR}/include")
add_compile_definitions(GL_GLEXT_PROTOTYPES)
target_compile_definitions(SDL2main PRIVATE "GL_GLEXT_PROTOTYPES")
target_compile_definitions(SDL2-static PRIVATE "GL_GLEXT_PROTOTYPES")
list(APPEND FO_RENDER_LIBS "SDL2main" "SDL2-static")
list(APPEND FO_DUMMY_TRAGETS "sdl_headers_copy")
DisableLibWarnings(SDL2main SDL2-static)

# Tracy profiler
StatusMessage("+ Tracy")
set(FO_TRACY_DIR "${FO_ENGINE_ROOT}/ThirdParty/tracy")
add_compile_definitions($<${expr_TracyEnabled}:TRACY_ENABLE>)
add_compile_definitions($<${expr_TracyOnDemand}:TRACY_ON_DEMAND>)
add_compile_definitions(FO_TRACY=${expr_TracyEnabled})
set(TRACY_STATIC ON CACHE BOOL "Forced by FOnline" FORCE)
add_subdirectory("${FO_TRACY_DIR}")
include_directories("${FO_TRACY_DIR}/public")
list(APPEND FO_COMMON_LIBS "TracyClient")
DisableLibWarnings(TracyClient)

# Zlib
StatusMessage("+ Zlib")
set(FO_ZLIB_DIR "${FO_ENGINE_ROOT}/ThirdParty/zlib")
add_subdirectory("${FO_ZLIB_DIR}")
include_directories("${FO_ZLIB_DIR}" "${FO_ZLIB_DIR}/contrib" "${CMAKE_CURRENT_BINARY_DIR}/${FO_ZLIB_DIR}")
list(APPEND FO_COMMON_LIBS "zlibstatic")
DisableLibWarnings(zlibstatic)

# PNG
if(FO_BUILD_BAKER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE)
    StatusMessage("+ PNG")
    set(FO_PNG_DIR "${FO_ENGINE_ROOT}/ThirdParty/PNG")
    set(SKIP_INSTALL_ALL ON CACHE BOOL "Forced by FOnline" FORCE)
    set(ZLIB_LIBRARY "zlibstatic" CACHE STRING "Forced by FOnline" FORCE)
    set(ZLIB_INCLUDE_DIR "../${FO_ZLIB_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/${FO_ZLIB_DIR}" CACHE STRING "Forced by FOnline" FORCE)
    set(PNG_SHARED OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(PNG_STATIC ON CACHE BOOL "Forced by FOnline" FORCE)
    add_subdirectory("${FO_PNG_DIR}")
    include_directories("${FO_PNG_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/${FO_PNG_DIR}")
    list(APPEND FO_BAKER_LIBS "png16_static")
    DisableLibWarnings(png16_static)
endif()

# Ogg
StatusMessage("+ Ogg")
set(FO_OGG_DIR "${FO_ENGINE_ROOT}/ThirdParty/ogg")
set(FO_OGG_SOURCE
    "${FO_OGG_DIR}/src/bitwise.c"
    "${FO_OGG_DIR}/src/framing.c")
include_directories("${FO_OGG_DIR}/include")
add_library(Ogg ${FO_OGG_SOURCE})
list(APPEND FO_CLIENT_LIBS "Ogg")
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
add_library(Vorbis ${FO_VORBIS_SOURCE})
target_link_libraries(Vorbis Ogg)
list(APPEND FO_CLIENT_LIBS "Vorbis")
target_compile_definitions(Vorbis PRIVATE "_CRT_SECURE_NO_WARNINGS")
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
add_library(Theora ${FO_THEORA_SOURCE})
list(APPEND FO_CLIENT_LIBS "Theora")
DisableLibWarnings(Theora)

# Acm
StatusMessage("+ Acm")
set(FO_ACM_DIR "${FO_ENGINE_ROOT}/ThirdParty/Acm")
add_subdirectory("${FO_ACM_DIR}")
include_directories("${FO_ACM_DIR}")
list(APPEND FO_CLIENT_LIBS "Acm")
DisableLibWarnings(Acm)

# SHA
StatusMessage("+ SHA")
set(FO_SHA_DIR "${FO_ENGINE_ROOT}/ThirdParty/SHA")
add_subdirectory("${FO_SHA_DIR}")
include_directories("${FO_SHA_DIR}")
list(APPEND FO_COMMON_LIBS "SHA")
DisableLibWarnings(SHA)

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
    add_library(GLEW ${FO_GLEW_SOURCE})
    add_compile_definitions(GLEW_STATIC)
    target_compile_definitions(GLEW PRIVATE "GLEW_STATIC")
    list(APPEND FO_RENDER_LIBS "GLEW")
    DisableLibWarnings(GLEW)
endif()

# Assimp
StatusMessage("+ Assimp (math headers)")
include_directories("${FO_ENGINE_ROOT}/ThirdParty/AssimpMath")

# Fbx SDK
if(FO_ENABLE_3D)
    if((FO_BUILD_BAKER OR FO_BUILD_EDITOR) AND NOT((WIN32 OR LINUX) AND CMAKE_SIZEOF_VOID_P EQUAL 8))
        AbortMessage("Using of FBX SDK for non 64 bit Linux & Windows builds is not supported")
    endif()

    if((FO_BUILD_BAKER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE) AND(WIN32 OR LINUX) AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        StatusMessage("+ Fbx SDK")

        set(FO_FBXSDK_DIR "${FO_ENGINE_ROOT}/ThirdParty/fbxsdk")
        include_directories("${FO_FBXSDK_DIR}")

        if(WIN32)
            list(APPEND FO_BAKER_SYSTEM_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/ThirdParty/fbxsdk/libfbxsdk.lib")
        else()
            list(APPEND FO_BAKER_SYSTEM_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/ThirdParty/fbxsdk/libfbxsdk.so")
        endif()

        add_compile_definitions(FO_HAVE_FBXSDK=1)

    else()
        add_compile_definitions(FO_HAVE_FBXSDK=0)
    endif()
endif()

macro(CopyFbxSdkLib target)
    if(FO_ENABLE_3D)
        if(WIN32)
            get_target_property(dir ${target} RUNTIME_OUTPUT_DIRECTORY)
            add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/ThirdParty/fbxsdk/libfbxsdk.dll" ${dir})
        endif()
    endif()
endmacro()

# Nlohmann Json
StatusMessage("+ Nlohmann Json")
set(FO_JSON_DIR "${FO_ENGINE_ROOT}/ThirdParty/Json")
include_directories("${FO_JSON_DIR}")
add_compile_definitions(FO_HAVE_JSON=1)

# Fmt
StatusMessage("+ Fmt")
set(FO_FMT_DIR "${FO_ENGINE_ROOT}/ThirdParty/fmt")
set(FMT_INSTALL OFF CACHE BOOL "Forced by FOnline" FORCE)
add_subdirectory("${FO_FMT_DIR}")
include_directories("${FO_FMT_DIR}/include")
list(APPEND FO_COMMON_LIBS "fmt")
DisableLibWarnings(fmt)

# LibreSSL
if((FO_BUILD_SERVER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE) AND NOT FO_SINGLEPLAYER)
    StatusMessage("+ LibreSSL")
    set(FO_LIBRESSL_DIR "${FO_ENGINE_ROOT}/ThirdParty/LibreSSL")
    set(LIBRESSL_SKIP_INSTALL ON CACHE BOOL "Forced by FOnline" FORCE)
    set(LIBRESSL_APPS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(LIBRESSL_TESTS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_ASM ON CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_EXTRATESTS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_NC OFF CACHE BOOL "Forced by FOnline" FORCE)
    add_subdirectory("${FO_LIBRESSL_DIR}")
    include_directories("${FO_LIBRESSL_DIR}")
    include_directories("${FO_LIBRESSL_DIR}/include")
    include_directories("${CMAKE_CURRENT_BINARY_DIR}/${FO_LIBRESSL_DIR}/crypto")
    include_directories("${CMAKE_CURRENT_BINARY_DIR}/${FO_LIBRESSL_DIR}/ssl")
    set(LIBRESSL_FOUND YES CACHE BOOL "Forced by FOnline" FORCE)
    set(LIBRESSL_LIBRARIES "ssl;crypto;tls" CACHE STRING "Forced by FOnline" FORCE)
    set(LIBRESSL_INCLUDE_DIRS "" CACHE STRING "Forced by FOnline" FORCE)
    set(LIBRESSL_LIBRARY_DIRS "" CACHE STRING "Forced by FOnline" FORCE)
    list(APPEND FO_SERVER_LIBS "ssl" "crypto" "tls")
    DisableLibWarnings(ssl crypto tls)
endif()

# Asio & Websockets
if((FO_BUILD_SERVER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE) AND NOT ANDROID AND NOT FO_SINGLEPLAYER)
    StatusMessage("+ Asio")
    set(FO_ASIO_DIR "${FO_ENGINE_ROOT}/ThirdParty/Asio")
    include_directories("${FO_ASIO_DIR}/include")

    StatusMessage("+ Websockets")
    set(FO_WEBSOCKETS_DIR "${FO_ENGINE_ROOT}/ThirdParty/websocketpp")
    include_directories("${FO_WEBSOCKETS_DIR}")

    add_compile_definitions(FO_HAVE_ASIO=1)
else()
    add_compile_definitions(FO_HAVE_ASIO=0)
endif()

# MongoDB & Bson
if(FO_BUILD_SERVER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE OR FO_BUILD_SINGLE)
    StatusMessage("+ Bson")
    set(FO_MONGODB_DIR "${FO_ENGINE_ROOT}/ThirdParty/mongo-c-driver")

    set(ENABLE_BSON ON CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_STATIC BUILD_ONLY CACHE STRING "Forced by FOnline" FORCE)
    set(ENABLE_SRV OFF CACHE STRING "Forced by FOnline" FORCE)
    set(ENABLE_UNINSTALL OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_TESTS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_EXAMPLES OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_SSL LIBRESSL CACHE STRING "Forced by FOnline" FORCE)
    set(ENABLE_SASL OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_ZLIB OFF CACHE STRING "Forced by FOnline" FORCE)
    set(ENABLE_AUTOMATIC_INIT_AND_CLEANUP OFF CACHE STRING "Forced by FOnline" FORCE)
    set(MONGO_USE_CCACHE OFF CACHE STRING "Forced by FOnline" FORCE)

    if((FO_BUILD_SERVER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE) AND NOT FO_SINGLEPLAYER)
        StatusMessage("+ MongoDB")
        set(ENABLE_MONGOC ON CACHE STRING "Forced by FOnline" FORCE)
        add_compile_definitions(FO_HAVE_MONGO=1)
    else()
        set(ENABLE_MONGOC OFF CACHE STRING "Forced by FOnline" FORCE)
        add_compile_definitions(FO_HAVE_MONGO=0)
    endif()

    add_subdirectory("${FO_MONGODB_DIR}")

    include_directories("${CMAKE_CURRENT_BINARY_DIR}/${FO_MONGODB_DIR}/src/libbson/src/bson")
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/${FO_MONGODB_DIR}/src/libbson/src")
    target_compile_definitions(bson_static PRIVATE "BSON_COMPILATION;BSON_STATIC;JSONSL_PARSE_NAN")
    add_compile_definitions(BSON_COMPILATION BSON_STATIC JSONSL_PARSE_NAN)
    list(APPEND FO_SERVER_LIBS "bson_static")
    DisableLibWarnings(bson_static)

    if(ENABLE_MONGOC)
        include_directories("${CMAKE_CURRENT_BINARY_DIR}/${FO_MONGODB_DIR}/src/libmongoc/src/mongoc")
        include_directories("${CMAKE_CURRENT_SOURCE_DIR}/${FO_MONGODB_DIR}/src/libmongoc/src")
        target_compile_definitions(mongoc_static PRIVATE "BSON_COMPILATION;BSON_STATIC;JSONSL_PARSE_NAN")
        list(APPEND FO_SERVER_LIBS "mongoc_static")
        list(APPEND FO_DUMMY_TRAGETS "mongoc-cxx-check" "dist" "distcheck")
        DisableLibWarnings(mongoc_static mongoc-cxx-check)
    endif()
endif()

# Unqlite
if(NOT EMSCRIPTEN)
    StatusMessage("+ Unqlite")
    set(FO_UNQLITE_DIR "${FO_ENGINE_ROOT}/ThirdParty/unqlite")
    add_subdirectory("${FO_UNQLITE_DIR}")
    include_directories("${FO_UNQLITE_DIR}")
    list(APPEND FO_COMMON_LIBS "unqlite")
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
    "${FO_DEAR_IMGUI_DIR}/imstb_truetype.h")
include_directories("${FO_DEAR_IMGUI_DIR}")
add_library(ImGui ${FO_IMGUI_SOURCE})
target_compile_definitions(ImGui PRIVATE "IMGUI_DISABLE_OBSOLETE_FUNCTIONS" "IMGUI_DISABLE_DEMO_WINDOWS" "IMGUI_DISABLE_DEBUG_TOOLS")
add_compile_definitions(IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DISABLE_DEBUG_TOOLS)
list(APPEND FO_COMMON_LIBS "ImGui")

if(WIN32 AND WINRT)
    target_compile_definitions(ImGui PRIVATE "IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS")
    add_compile_definitions(IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS)
endif()

DisableLibWarnings(ImGui)

# Catch2
StatusMessage("+ Catch2")
set(FO_CATCH2_DIR "${FO_ENGINE_ROOT}/ThirdParty/Catch2")
include_directories("${FO_CATCH2_DIR}/single_include/catch2")

# Backward-cpp
if(WIN32 OR LINUX OR(APPLE AND NOT PLATFORM))
    set(FO_BACKWARDCPP_DIR "${FO_ENGINE_ROOT}/ThirdParty/backward-cpp")
    include_directories("${FO_BACKWARDCPP_DIR}")

    if(NOT WIN32)
        check_include_file("libunwind.h" haveLibUnwind)
        check_include_file("bfd.h" haveBFD)

        if(haveLibUnwind)
            StatusMessage("+ Backward-cpp (with libunwind)")
        elseif(haveBFD)
            StatusMessage("+ Backward-cpp (with bfd)")
            list(APPEND FO_COMMON_SYSTEM_LIBS "bfd")
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
add_subdirectory("${FO_SPARK_DIR}/projects/engine/core")
add_subdirectory("${FO_SPARK_DIR}/projects/external/pugi")
include_directories("${FO_SPARK_DIR}/spark/include")
include_directories("${FO_SPARK_DIR}/thirdparty/PugiXML")
list(APPEND FO_CLIENT_LIBS "SPARK_Core" "PugiXML")
DisableLibWarnings(SPARK_Core PugiXML)

# glslang & SPIRV-Cross
if(FO_BUILD_BAKER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE)
    StatusMessage("+ glslang")
    set(FO_GLSLANG_DIR "${FO_ENGINE_ROOT}/ThirdParty/glslang")
    set(BUILD_EXTERNAL OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(OVERRIDE_MSVCCRT OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_HLSL OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_SPVREMAPPER OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_AMD_EXTENSIONS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_NV_EXTENSIONS OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_CTEST OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_RTTI ON CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_EXCEPTIONS ON CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_OPT OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_PCH OFF CACHE BOOL "Forced by FOnline" FORCE)
    set(ENABLE_GLSLANG_INSTALL OFF CACHE BOOL "Forced by FOnline" FORCE)

    if(EMSCRIPTEN)
        set(ENABLE_GLSLANG_WEB ON CACHE BOOL "Forced by FOnline" FORCE)
        set(ENABLE_GLSLANG_WEB_DEVEL ON CACHE BOOL "Forced by FOnline" FORCE)
        set(ENABLE_EMSCRIPTEN_SINGLE_FILE ON CACHE BOOL "Forced by FOnline" FORCE)
        set(ENABLE_EMSCRIPTEN_ENVIRONMENT_NODE OFF CACHE BOOL "Forced by FOnline" FORCE)
    endif()

    add_subdirectory("${FO_GLSLANG_DIR}")
    include_directories("${FO_GLSLANG_DIR}/glslang/Public")
    include_directories("${FO_GLSLANG_DIR}/SPIRV")
    list(APPEND FO_BAKER_LIBS "glslang" "glslang-default-resource-limits" "OGLCompiler" "OSDependent" "SPIRV" "GenericCodeGen" "MachineIndependent")
    DisableLibWarnings(glslang glslang-default-resource-limits OGLCompiler OSDependent SPIRV GenericCodeGen MachineIndependent)

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
    add_subdirectory("${FO_SPIRV_CROSS_DIR}")
    include_directories("${FO_SPIRV_CROSS_DIR}")
    include_directories("${FO_SPIRV_CROSS_DIR}/include")
    list(APPEND FO_BAKER_LIBS "spirv-cross-core" "spirv-cross-glsl" "spirv-cross-hlsl" "spirv-cross-msl")
    DisableLibWarnings(spirv-cross-core spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl)
endif()

# span
include_directories("${FO_ENGINE_ROOT}/ThirdParty/span/include/tcb")

# AngelScript scripting
if(FO_ANGELSCRIPT_SCRIPTING)
    # AngelScript
    StatusMessage("+ AngelScript")
    set(FO_ANGELSCRIPT_DIR "${FO_ENGINE_ROOT}/ThirdParty/AngelScript")

    # add_compile_definitions( $<$<CONFIG:Debug>:AS_DEBUG=1> )
    # set( CMAKE_ASM_FLAGS "/gg5" CACHE STRING "Forced by FOnline" FORCE )
    # set( CMAKE_ASM_FLAGS_DEBUG "/gg6" CACHE STRING "Forced by FOnline" FORCE )
    # set( CMAKE_ASM_MASM_FLAGS "/gg1" CACHE STRING "Forced by FOnline" FORCE )
    # set( CMAKE_ASM_MASM_FLAGS_DEBUG "/gg2" CACHE STRING "Forced by FOnline" FORCE )
    # set( CMAKE_ASM_MASMFLAGS "/gg3" CACHE STRING "Forced by FOnline" FORCE )
    # set( CMAKE_ASM_MASMFLAGS_DEBUG "/gg4" CACHE STRING "Forced by FOnline" FORCE )
    add_subdirectory("${FO_ANGELSCRIPT_DIR}/sdk/angelscript/projects/cmake")
    include_directories("${FO_ANGELSCRIPT_DIR}/sdk/angelscript/include" "${FO_ANGELSCRIPT_DIR}/sdk/angelscript/source" "${FO_ANGELSCRIPT_DIR}/sdk/add_on")
    list(APPEND FO_COMMON_LIBS "Angelscript")
    DisableLibWarnings(Angelscript)

    # target_compile_options( Angelscript PRIVATE "/wdA4018" )
    # target_compile_options( Angelscript PRIVATE  )

    # AngelScriptExt
    StatusMessage("+ AngelScriptExt")
    set(FO_ANGELSCRIPT_EXT_DIR "${FO_ENGINE_ROOT}/Source/Common/AngelScriptExt")
    set(FO_ANGELSCRIPT_PREPROCESSOR_DIR "${FO_ENGINE_ROOT}/ThirdParty/AngelScript/preprocessor")
    set(FO_ANGELSCRIPT_SDK_DIR "${FO_ENGINE_ROOT}/ThirdParty/AngelScript/sdk")
    include_directories("${FO_ANGELSCRIPT_EXT_DIR}")
    include_directories("${FO_ANGELSCRIPT_PREPROCESSOR_DIR}")
    include_directories("${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptstdstring")
    include_directories("${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptarray")
    include_directories("${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptdictionary")
    include_directories("${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptfile")
    include_directories("${FO_ANGELSCRIPT_SDK_DIR}/add_on/datetime")
    include_directories("${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptmath")
    include_directories("${FO_ANGELSCRIPT_SDK_DIR}/add_on/weakref")
    include_directories("${FO_ANGELSCRIPT_SDK_DIR}/add_on/scripthelper")
    set(FO_ANGELSCRIPT_EXT_SOURCE
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptExtensions.cpp"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptExtensions.h"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptReflection.cpp"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptReflection.h"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptScriptDict.cpp"
        "${FO_ANGELSCRIPT_EXT_DIR}/AngelScriptScriptDict.h"
        "${FO_ANGELSCRIPT_PREPROCESSOR_DIR}/preprocessor.cpp"
        "${FO_ANGELSCRIPT_PREPROCESSOR_DIR}/preprocessor.h"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptstdstring/scriptstdstring.cpp"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptstdstring/scriptstdstring.h"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptstdstring/scriptstdstring_utils.cpp"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptarray/scriptarray.cpp"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptarray/scriptarray.h"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptdictionary/scriptdictionary.cpp"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptdictionary/scriptdictionary.h"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptfile/scriptfile.cpp"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptfile/scriptfile.h"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptfile/scriptfilesystem.cpp"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptfile/scriptfilesystem.h"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/datetime/datetime.cpp"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/datetime/datetime.h"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptmath/scriptmath.cpp"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptmath/scriptmath.h"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/weakref/weakref.cpp"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/weakref/weakref.h"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scripthelper/scripthelper.cpp"
        "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scripthelper/scripthelper.h")
    add_library(AngelscriptExt ${FO_ANGELSCRIPT_EXT_SOURCE})
    target_link_libraries(AngelscriptExt Angelscript)
    list(APPEND FO_COMMON_LIBS "AngelscriptExt")
    target_compile_definitions(AngelscriptExt PRIVATE "_CRT_SECURE_NO_WARNINGS" "FO_${FO_OS_UPPER}")
    DisableLibWarnings(AngelscriptExt)

    if(NOT FO_BUILD_BAKER)
        target_compile_definitions(Angelscript PRIVATE "AS_NO_COMPILER")
        target_compile_definitions(AngelscriptExt PRIVATE "AS_NO_COMPILER")
        add_compile_definitions(AS_NO_COMPILER)
    endif()

    if(EMSCRIPTEN OR(APPLE AND PLATFORM) OR(ANDROID AND CMAKE_SIZEOF_VOID_P EQUAL 8))
        target_compile_definitions(Angelscript PRIVATE "AS_MAX_PORTABILITY")
        target_compile_definitions(AngelscriptExt PRIVATE "AS_MAX_PORTABILITY")
        add_compile_definitions(AS_MAX_PORTABILITY)
    endif()

    if(EMSCRIPTEN)
        target_compile_definitions(Angelscript PRIVATE "WIP_16BYTE_ALIGN")
        target_compile_definitions(AngelscriptExt PRIVATE "WIP_16BYTE_ALIGN")
        add_compile_definitions(WIP_16BYTE_ALIGN)
    endif()
endif()

# Mono scripting
if(FO_MONO_SCRIPTING)
    StatusMessage("+ Mono")
    set(FO_MONO_DIR "${FO_ENGINE_ROOT}/ThirdParty/mono")
    add_subdirectory("${FO_MONO_DIR}")
    include_directories("${FO_MONO_DIR}")
    include_directories("${FO_MONO_DIR}/repo")
    include_directories("${FO_MONO_DIR}/repo/mono")
    include_directories("${FO_MONO_DIR}/repo/mono/eglib")
    list(APPEND FO_COMMON_LIBS "libmono")
    add_compile_definitions(HAVE_EXTERN_DEFINED_WINAPI_SUPPORT)
    DisableLibWarnings(libmono)
endif()

# App icon
set(FO_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${FO_DEV_NAME}.rc")
get_filename_component(FO_APP_ICON ${FO_APP_ICON} REALPATH)
set(FO_GEN_FILE_CONTENT "101 ICON \"${FO_APP_ICON}\"")
configure_file("${FO_ENGINE_ROOT}/BuildTools/blank.cmake" ${FO_RC_FILE} FILE_PERMISSIONS OWNER_WRITE OWNER_READ)

# Engine sources
list(APPEND FO_COMMON_SOURCE
    "${FO_ENGINE_ROOT}/Source/Common/AnyData.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/AnyData.h"
    "${FO_ENGINE_ROOT}/Source/Common/CacheStorage.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/CacheStorage.h"
    "${FO_ENGINE_ROOT}/Source/Common/Common.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Common.h"
    "${FO_ENGINE_ROOT}/Source/Common/ConfigFile.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/ConfigFile.h"
    "${FO_ENGINE_ROOT}/Source/Common/DataSource.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/DataSource.h"
    "${FO_ENGINE_ROOT}/Source/Common/DeferredCalls.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/DeferredCalls.h"
    "${FO_ENGINE_ROOT}/Source/Common/Dialogs.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Dialogs.h"
    "${FO_ENGINE_ROOT}/Source/Common/DiskFileSystem.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/DiskFileSystem.h"
    "${FO_ENGINE_ROOT}/Source/Common/EngineBase.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/EngineBase.h"
    "${FO_ENGINE_ROOT}/Source/Common/Entity.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Entity.h"
    "${FO_ENGINE_ROOT}/Source/Common/EntityProtos.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/EntityProtos.h"
    "${FO_ENGINE_ROOT}/Source/Common/EntityProperties.h"
    "${FO_ENGINE_ROOT}/Source/Common/FileSystem.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/FileSystem.h"
    "${FO_ENGINE_ROOT}/Source/Common/GenericUtils.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/GenericUtils.h"
    "${FO_ENGINE_ROOT}/Source/Common/GeometryHelper.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/GeometryHelper.h"
    "${FO_ENGINE_ROOT}/Source/Common/LineTracer.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/LineTracer.h"
    "${FO_ENGINE_ROOT}/Source/Common/Log.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Log.h"
    "${FO_ENGINE_ROOT}/Source/Common/MapLoader.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/MapLoader.h"
    "${FO_ENGINE_ROOT}/Source/Common/NetBuffer.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/NetBuffer.h"
    "${FO_ENGINE_ROOT}/Source/Common/NetCommand.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/NetCommand.h"
    "${FO_ENGINE_ROOT}/Source/Common/NetProtocol-Include.h"
    "${FO_ENGINE_ROOT}/Source/Common/Platform.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Platform.h"
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
    "${FO_ENGINE_ROOT}/Source/Common/StringUtils.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/StringUtils.h"
    "${FO_ENGINE_ROOT}/Source/Common/TextPack.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/TextPack.h"
    "${FO_ENGINE_ROOT}/Source/Common/Timer.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Timer.h"
    "${FO_ENGINE_ROOT}/Source/Common/TwoBitMask.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/TwoBitMask.h"
    "${FO_ENGINE_ROOT}/Source/Common/TwoDimensionalGrid.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/TwoDimensionalGrid.h"
    "${FO_ENGINE_ROOT}/Source/Common/UcsTables-Include.h"
    "${FO_ENGINE_ROOT}/Source/Common/WinApi-Include.h"
    "${FO_ENGINE_ROOT}/Source/Common/WinApiUndef-Include.h"
    "${FO_ENGINE_ROOT}/Source/Scripting/CommonGlobalScriptMethods.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/Version-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DebugSettings-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/EmbeddedResources-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/GenericCode-Common.cpp")

list(APPEND FO_SERVER_BASE_SOURCE
    "${FO_ENGINE_ROOT}/Source/Server/AdminPanel.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/AdminPanel.h"
    "${FO_ENGINE_ROOT}/Source/Server/ClientConnection.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/ClientConnection.h"
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
    "${FO_ENGINE_ROOT}/Source/Server/Networking.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/Networking.h"
    "${FO_ENGINE_ROOT}/Source/Server/Player.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/Player.h"
    "${FO_ENGINE_ROOT}/Source/Server/Server.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/Server.h"
    "${FO_ENGINE_ROOT}/Source/Server/ServerDeferredCalls.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/ServerDeferredCalls.h"
    "${FO_ENGINE_ROOT}/Source/Server/ServerEntity.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/ServerEntity.h"
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
    "${FO_ENGINE_ROOT}/Source/Client/ParticleSprites.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/ParticleSprites.h"
    "${FO_ENGINE_ROOT}/Source/Client/PlayerView.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/PlayerView.h"
    "${FO_ENGINE_ROOT}/Source/Client/RenderTarget.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/RenderTarget.h"
    "${FO_ENGINE_ROOT}/Source/Client/ResourceManager.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/ResourceManager.h"
    "${FO_ENGINE_ROOT}/Source/Client/ServerConnection.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/ServerConnection.h"
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
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientGlobalScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientPlayerScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientItemScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientCritterScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientMapScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientLocationScriptMethods.cpp")

if(NOT FO_SINGLEPLAYER)
    list(APPEND FO_SERVER_SOURCE
        ${FO_SERVER_BASE_SOURCE}
        "${FO_ENGINE_ROOT}/Source/Scripting/ServerScripting.h"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Server.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Server.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Server.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Server.cpp")

    list(APPEND FO_CLIENT_SOURCE
        ${FO_CLIENT_BASE_SOURCE}
        "${FO_ENGINE_ROOT}/Source/Scripting/ClientScripting.h"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Client.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Client.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Client.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Client.cpp")

else()
    list(APPEND FO_SINGLE_SOURCE
        ${FO_SERVER_BASE_SOURCE}
        ${FO_CLIENT_BASE_SOURCE}
        "${FO_ENGINE_ROOT}/Source/Singleplayer/Single.cpp"
        "${FO_ENGINE_ROOT}/Source/Singleplayer/Single.h"
        "${FO_ENGINE_ROOT}/Source/Scripting/SingleScripting.h"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Single.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Single.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Single.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Single.cpp")
endif()

list(APPEND FO_EDITOR_SOURCE
    "${FO_ENGINE_ROOT}/Source/Tools/Editor.h"
    "${FO_ENGINE_ROOT}/Source/Tools/Editor.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/AssetExplorer.h"
    "${FO_ENGINE_ROOT}/Source/Tools/AssetExplorer.cpp"

    # "${FO_ENGINE_ROOT}/Source/Tools/DialogEditor.h"
    # "${FO_ENGINE_ROOT}/Source/Tools/DialogEditor.cpp"
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
    "${FO_ENGINE_ROOT}/Source/Scripting/MapperScripting.h"
    "${FO_ENGINE_ROOT}/Source/Scripting/MapperGlobalScriptMethods.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Mapper.cpp")

list(APPEND FO_BAKER_SOURCE
    "${FO_ENGINE_ROOT}/Source/Tools/Baker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/Baker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/EffectBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/EffectBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/F2Palette-Include.h"
    "${FO_ENGINE_ROOT}/Source/Tools/ImageBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/ImageBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/ModelBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/ModelBaker.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Baker.cpp")

if(FO_ANGELSCRIPT_SCRIPTING)
    list(APPEND FO_ASCOMPILER_SOURCE
        "${FO_ENGINE_ROOT}/Source/Scripting/MapperScripting.h"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-MapperCompiler.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-MapperCompiler.cpp")

    if(NOT FO_SINGLEPLAYER)
        list(APPEND FO_ASCOMPILER_SOURCE
            "${FO_ENGINE_ROOT}/Source/Scripting/ServerScripting.h"
            "${FO_ENGINE_ROOT}/Source/Scripting/ClientScripting.h"
            "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ServerCompiler.cpp"
            "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ServerCompilerValidation.cpp"
            "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ClientCompiler.cpp"
            "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-ServerCompiler.cpp"
            "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-ClientCompiler.cpp")
    else()
        list(APPEND FO_ASCOMPILER_SOURCE
            "${FO_ENGINE_ROOT}/Source/Scripting/SingleScripting.h"
            "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-SingleCompiler.cpp"
            "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-SingleCompilerValidation.cpp"
            "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-SingleCompiler.cpp")
    endif()
endif()

list(APPEND FO_SOURCE_META_FILES
    "${FO_ENGINE_ROOT}/Source/Singleplayer/Single.h"
    "${FO_ENGINE_ROOT}/Source/Frontend/Application.h"
    "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.h"
    "${FO_ENGINE_ROOT}/Source/Common/Common.h"
    "${FO_ENGINE_ROOT}/Source/Common/Entity.h"
    "${FO_ENGINE_ROOT}/Source/Common/EntityProperties.h"
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
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerGlobalScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerPlayerScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerItemScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerCritterScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerMapScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerLocationScriptMethods.cpp"
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
    "${FO_ENGINE_ROOT}/Source/Tests/Test_GenericUtils.cpp")

# Code generation
include(FindPython3)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

list(APPEND FO_CODEGEN_COMMAND_ARGS -buildhash "${FO_BUILD_HASH}")
list(APPEND FO_CODEGEN_COMMAND_ARGS -genoutput "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource")
list(APPEND FO_CODEGEN_COMMAND_ARGS -devname "${FO_DEV_NAME}")
list(APPEND FO_CODEGEN_COMMAND_ARGS -gamename "${FO_NICE_NAME} ${FO_GAME_VERSION}")
list(APPEND FO_CODEGEN_COMMAND_ARGS -gameversion "${FO_GAME_VERSION}")

if(FO_INFO_MARKDOWN_OUTPUT)
    list(APPEND FO_CODEGEN_COMMAND_ARGS -markdown)
    list(APPEND FO_CODEGEN_COMMAND_ARGS -mdpath "${FO_INFO_MARKDOWN_OUTPUT}")
endif()

if(FO_NATIVE_SCRIPTING)
    list(APPEND FO_CODEGEN_COMMAND_ARGS -native)
endif()

if(FO_ANGELSCRIPT_SCRIPTING)
    list(APPEND FO_CODEGEN_COMMAND_ARGS -angelscript)

    if(FO_GENERATE_ANGELSCRIPT_CONTENT)
        list(APPEND FO_CODEGEN_COMMAND_ARGS -ascontentoutput "${FO_GENERATE_ANGELSCRIPT_CONTENT}")
    endif()
endif()

if(FO_MONO_SCRIPTING)
    list(APPEND FO_CODEGEN_COMMAND_ARGS -csharp)
endif()

if(NOT FO_SINGLEPLAYER)
    list(APPEND FO_CODEGEN_COMMAND_ARGS -multiplayer)
else()
    list(APPEND FO_CODEGEN_COMMAND_ARGS -singleplayer)
endif()

foreach(entry ${FO_MONO_ASSEMBLIES})
    list(APPEND FO_CODEGEN_COMMAND_ARGS -monoassembly ${entry})

    foreach(ref ${MonoAssembly_${entry}_CommonRefs})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoserverref "${entry},${ref}")
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoclientref "${entry},${ref}")
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monosingleref "${entry},${ref}")
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monomapperref "${entry},${ref}")
    endforeach()

    foreach(ref ${MonoAssembly_${entry}_ServerRefs})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoserverref "${entry},${ref}")
    endforeach()

    foreach(ref ${MonoAssembly_${entry}_ClientRefs})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoclientref "${entry},${ref}")
    endforeach()

    foreach(ref ${MonoAssembly_${entry}_SingleRefs})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monosingleref "${entry},${ref}")
    endforeach()

    foreach(ref ${MonoAssembly_${entry}_MapperRefs})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monomapperref "${entry},${ref}")
    endforeach()

    foreach(src ${MonoAssembly_${entry}_CommonSource})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoserversource "${entry},${src}")
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monoclientsource "${entry},${src}")
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monosinglesource "${entry},${src}")
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

    foreach(src ${MonoAssembly_${entry}_SingleSource})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monosinglesource "${entry},${src}")
        list(APPEND FO_MONO_SOURCE ${src})
    endforeach()

    foreach(src ${MonoAssembly_${entry}_MapperSource})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -monomappersource "${entry},${src}")
        list(APPEND FO_MONO_SOURCE ${src})
    endforeach()
endforeach()

foreach(entry ${FO_CONTENT})
    list(APPEND FO_CODEGEN_COMMAND_ARGS -content "${entry}")
endforeach()

foreach(entry ${FO_RESOURCES})
    list(APPEND FO_CODEGEN_COMMAND_ARGS -resource "${entry}")
endforeach()

if(FO_DEBUGGING_CONFIG)
    list(APPEND FO_CODEGEN_COMMAND_ARGS -config "ExternalConfig,${FO_BACKED_RESOURCES_OUTPUT}/Configs/${FO_DEBUGGING_CONFIG}.focfg")
    list(APPEND FO_CODEGEN_COMMAND_ARGS -config "ResourcesDir,${FO_BACKED_RESOURCES_OUTPUT}")
    list(APPEND FO_CODEGEN_COMMAND_ARGS -config "EmbeddedResources,${FO_BACKED_RESOURCES_OUTPUT}/Embedded")
    list(APPEND FO_CODEGEN_COMMAND_ARGS -config "DataSynchronization,False")

    list(APPEND FO_CODEGEN_COMMAND_ARGS -config "BakeOutput,${FO_BACKED_RESOURCES_OUTPUT}")

    foreach(entry ${FO_CONTENT})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -config "BakeContentEntries,+${entry}")
    endforeach()

    foreach(entry ${FO_RESOURCES})
        list(APPEND FO_CODEGEN_COMMAND_ARGS -config "BakeResourceEntries,+${entry}")
    endforeach()
endif()

list(APPEND FO_CODEGEN_META_SOURCE
    ${FO_SOURCE_META_FILES}
    ${FO_CONTENT_META_FILES}
    ${FO_MONO_SOURCE})

foreach(entry ${FO_CODEGEN_META_SOURCE})
    list(APPEND FO_CODEGEN_COMMAND_ARGS -meta ${entry})
endforeach()

list(APPEND FO_CODEGEN_OUTPUT
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/CodeGenTouch"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/Version-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DebugSettings-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/EmbeddedResources-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Server.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Client.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Single.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-Baker.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-ServerCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-ClientCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-SingleCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/DataRegistration-MapperCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/GenericCode-Common.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Server.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Client.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Single.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ServerCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ServerCompilerValidation.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ClientCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-MapperCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-SingleCompiler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/AngelScriptScripting-SingleCompilerValidation.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Server.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Client.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MonoScripting-Single.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Server.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Client.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeScripting-Single.cpp")

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
list(APPEND FO_COMMANDS_GROUP "CodeGeneration")

add_custom_target(ForceCodeGeneration
    COMMAND ${FO_CODEGEN_COMMAND}
    COMMAND ${CMAKE_COMMAND} -E touch ${FO_CODEGEN_OUTPUT}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND FO_COMMANDS_GROUP "ForceCodeGeneration")

# Core libs
StatusMessage("Core libs:")

if(FO_BUILD_CLIENT OR FO_BUILD_SERVER OR FO_BUILD_EDITOR OR FO_BUILD_MAPPER OR FO_BUILD_SINGLE OR FO_BUILD_ASCOMPILER OR FO_BUILD_BAKER OR FO_UNIT_TESTS OR FO_CODE_COVERAGE)
    StatusMessage("+ AppHeadless")
    add_library(AppHeadless STATIC
        "${FO_ENGINE_ROOT}/Source/Frontend/Application.h"
        "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationHeadless.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.h")
    add_dependencies(AppHeadless CodeGeneration)
    list(APPEND FO_CORE_LIBS_GROUP "AppHeadless")

    if(NOT FO_HEADLESS_ONLY)
        StatusMessage("+ AppFrontend")
        add_library(AppFrontend STATIC
            "${FO_ENGINE_ROOT}/Source/Frontend/Application.h"
            "${FO_ENGINE_ROOT}/Source/Frontend/Application.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.h"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-Direct3D.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-OpenGL.cpp")
        add_dependencies(AppFrontend CodeGeneration)
        target_link_libraries(AppFrontend ${FO_RENDER_SYSTEM_LIBS} ${FO_RENDER_LIBS})
        list(APPEND FO_CORE_LIBS_GROUP "AppFrontend")
    endif()

    StatusMessage("+ CommonLib")
    add_library(CommonLib STATIC ${FO_COMMON_SOURCE})
    add_dependencies(CommonLib CodeGeneration)
    target_link_libraries(CommonLib ${FO_COMMON_SYSTEM_LIBS} ${FO_COMMON_LIBS})
    list(APPEND FO_CORE_LIBS_GROUP "CommonLib")
endif()

if(NOT FO_SINGLEPLAYER AND(FO_BUILD_CLIENT OR FO_BUILD_EDITOR OR FO_BUILD_MAPPER OR FO_BUILD_SERVER OR FO_UNIT_TESTS OR FO_CODE_COVERAGE))
    StatusMessage("+ ClientLib")
    add_library(ClientLib STATIC ${FO_CLIENT_SOURCE})
    add_dependencies(ClientLib CodeGeneration)
    target_link_libraries(ClientLib CommonLib ${FO_CLIENT_SYSTEM_LIBS} ${FO_CLIENT_LIBS})
    list(APPEND FO_CORE_LIBS_GROUP "ClientLib")
endif()

if(NOT FO_SINGLEPLAYER AND(FO_BUILD_SERVER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE))
    StatusMessage("+ ServerLib")
    add_library(ServerLib STATIC ${FO_SERVER_SOURCE})
    add_dependencies(ServerLib CodeGeneration)
    target_link_libraries(ServerLib CommonLib ${FO_SERVER_SYSTEM_LIBS} ${FO_SERVER_LIBS})
    list(APPEND FO_CORE_LIBS_GROUP "ServerLib")
endif()

if(FO_SINGLEPLAYER AND(FO_BUILD_SINGLE OR FO_BUILD_EDITOR OR FO_BUILD_MAPPER OR FO_UNIT_TESTS OR FO_CODE_COVERAGE))
    StatusMessage("+ SingleLib")
    add_library(SingleLib STATIC ${FO_SINGLE_SOURCE})
    add_dependencies(SingleLib CodeGeneration)
    target_link_libraries(SingleLib CommonLib ${FO_SERVER_SYSTEM_LIBS} ${FO_SERVER_LIBS} ${FO_CLIENT_SYSTEM_LIBS} ${FO_CLIENT_LIBS})
    list(APPEND FO_CORE_LIBS_GROUP "SingleLib")
endif()

if(FO_BUILD_MAPPER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE)
    StatusMessage("+ MapperLib")
    add_library(MapperLib STATIC ${FO_MAPPER_SOURCE})
    add_dependencies(MapperLib CodeGeneration)
    target_link_libraries(MapperLib CommonLib)
    list(APPEND FO_CORE_LIBS_GROUP "MapperLib")
endif()

if(FO_ANGELSCRIPT_SCRIPTING AND(FO_BUILD_ASCOMPILER OR FO_BUILD_BAKER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE))
    StatusMessage("+ ASCompilerLib")
    add_library(ASCompilerLib STATIC ${FO_ASCOMPILER_SOURCE})
    add_dependencies(ASCompilerLib CodeGeneration)
    target_link_libraries(ASCompilerLib CommonLib)
    list(APPEND FO_CORE_LIBS_GROUP "ASCompilerLib")
endif()

if(FO_BUILD_BAKER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE)
    StatusMessage("+ BakerLib")
    add_library(BakerLib STATIC ${FO_BAKER_SOURCE})
    add_dependencies(BakerLib CodeGeneration)
    set_target_properties(BakerLib PROPERTIES COMPILE_DEFINITIONS "FO_ASYNC_BAKE=0")
    target_link_libraries(BakerLib CommonLib ${FO_BAKER_SYSTEM_LIBS} ${FO_BAKER_LIBS})
    list(APPEND FO_CORE_LIBS_GROUP "BakerLib")
endif()

if(FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE)
    StatusMessage("+ EditorLib")
    add_library(EditorLib STATIC ${FO_EDITOR_SOURCE})
    add_dependencies(EditorLib CodeGeneration)
    target_link_libraries(EditorLib CommonLib)
    list(APPEND FO_CORE_LIBS_GROUP "EditorLib")
endif()

# Applications
StatusMessage("Applications:")

if(NOT FO_SINGLEPLAYER AND FO_BUILD_CLIENT)
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

if(NOT FO_SINGLEPLAYER AND FO_BUILD_SERVER)
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

    if(WIN32)
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

if(FO_SINGLEPLAYER AND FO_BUILD_SINGLE)
    if(NOT FO_BUILD_LIBRARY)
        StatusMessage("+ ${FO_DEV_NAME}")
        list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}")
        add_executable(${FO_DEV_NAME} WIN32 "${FO_ENGINE_ROOT}/Source/Applications/SingleApp.cpp" ${FO_RC_FILE})
        set_target_properties(${FO_DEV_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SINGLE_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_OUTPUT_PATH})
    else()
        StatusMessage("+ ${FO_DEV_NAME} (shared library)")
        list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}")
        add_library(${FO_DEV_NAME} SHARED "${FO_ENGINE_ROOT}/Source/Applications/SingleApp.cpp")
        set_target_properties(${FO_DEV_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${FO_SINGLE_OUTPUT})
    endif()

    add_dependencies(${FO_DEV_NAME} CodeGeneration)
    set_target_properties(${FO_DEV_NAME} PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}")
    set_target_properties(${FO_DEV_NAME} PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0")
    target_link_libraries(${FO_DEV_NAME} "AppFrontend" "SingleLib")
    WriteBuildHash(${FO_DEV_NAME})
endif()

if(FO_BUILD_EDITOR)
    StatusMessage("+ ${FO_DEV_NAME}_Editor")
    list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Editor")
    add_executable(${FO_DEV_NAME}_Editor WIN32 "${FO_ENGINE_ROOT}/Source/Applications/EditorApp.cpp" ${FO_RC_FILE})
    set_target_properties(${FO_DEV_NAME}_Editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_EDITOR_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_OUTPUT_PATH})
    set_target_properties(${FO_DEV_NAME}_Editor PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_Editor")
    set_target_properties(${FO_DEV_NAME}_Editor PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0")
    target_link_libraries(${FO_DEV_NAME}_Editor "AppFrontend" "EditorLib" "MapperLib" "BakerLib")

    if(NOT FO_SINGLEPLAYER)
        target_link_libraries(${FO_DEV_NAME}_Editor "ClientLib" "ServerLib")
    else()
        target_link_libraries(${FO_DEV_NAME}_Editor "SingleLib")
    endif()

    if(FO_ANGELSCRIPT_SCRIPTING)
        target_link_libraries(${FO_DEV_NAME}_Editor "ASCompilerLib")
    endif()

    WriteBuildHash(${FO_DEV_NAME}_Editor)
    CopyFbxSdkLib(${FO_DEV_NAME}_Editor)
endif()

if(FO_BUILD_MAPPER)
    StatusMessage("+ ${FO_DEV_NAME}_Mapper")
    list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Mapper")
    add_executable(${FO_DEV_NAME}_Mapper WIN32 "${FO_ENGINE_ROOT}/Source/Applications/MapperApp.cpp" ${FO_RC_FILE})
    set_target_properties(${FO_DEV_NAME}_Mapper PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_MAPPER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_OUTPUT_PATH})
    set_target_properties(${FO_DEV_NAME}_Mapper PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_Mapper")
    set_target_properties(${FO_DEV_NAME}_Mapper PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0")
    target_link_libraries(${FO_DEV_NAME}_Mapper "AppFrontend" "MapperLib")

    if(NOT FO_SINGLEPLAYER)
        target_link_libraries(${FO_DEV_NAME}_Mapper "ClientLib")
    else()
        target_link_libraries(${FO_DEV_NAME}_Mapper "SingleLib")
    endif()

    WriteBuildHash(${FO_DEV_NAME}_Mapper)
endif()

if(FO_BUILD_ASCOMPILER)
    StatusMessage("+ ${FO_DEV_NAME}_ASCompiler")
    list(APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_ASCompiler")
    add_executable(${FO_DEV_NAME}_ASCompiler "${FO_ENGINE_ROOT}/Source/Applications/ASCompilerApp.cpp")
    add_dependencies(${FO_DEV_NAME}_ASCompiler CodeGeneration)
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

    if(FO_ANGELSCRIPT_SCRIPTING)
        target_link_libraries(${FO_DEV_NAME}_Baker "ASCompilerLib")
    endif()

    WriteBuildHash(${FO_DEV_NAME}_Baker)
    CopyFbxSdkLib(${FO_DEV_NAME}_Baker)
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
        add_dependencies(${target} CodeGeneration)
        set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_TESTS_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_TESTS_OUTPUT})
        set_target_properties(${target} PROPERTIES OUTPUT_NAME ${target})
        set_target_properties(${target} PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=1")
        target_link_libraries(${target} "BakerLib" "EditorLib" "MapperLib" "${FO_TESTING_LIBS}")

        if(NOT FO_SINGLEPLAYER)
            target_sources(${target} PRIVATE
                "${FO_ENGINE_ROOT}/Source/Applications/ServerApp.cpp"
                "${FO_ENGINE_ROOT}/Source/Applications/ServerServiceApp.cpp"
                "${FO_ENGINE_ROOT}/Source/Applications/ServerDaemonApp.cpp"
                "${FO_ENGINE_ROOT}/Source/Applications/ServerHeadlessApp.cpp"
                "${FO_ENGINE_ROOT}/Source/Applications/ClientApp.cpp")
            target_link_libraries(${target} "ClientLib" "ServerLib")
        else()
            target_sources(${target} PRIVATE "${FO_ENGINE_ROOT}/Source/Applications/SingleApp.cpp")
            target_link_libraries(${target} "SingleLib")
        endif()

        if(FO_ANGELSCRIPT_SCRIPTING)
            target_link_libraries(${target} "ASCompilerLib")
        endif()

        target_link_libraries(${target} "AppHeadless")

        CopyFbxSdkLib(${target})
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

        list(APPEND compileASScripts -BakeOutput "${FO_BACKED_RESOURCES_OUTPUT}")

        foreach(entry ${FO_CONTENT})
            list(APPEND compileASScripts -BakeContentEntries "+${entry}")
        endforeach()

        foreach(entry ${FO_RESOURCE})
            list(APPEND compileASScripts -BakeResourceEntries "+${entry}")
        endforeach()

        add_custom_target(CompileAngelScript
            COMMAND ${compileASScripts}
            COMMENT "Compile AngelScript scripts")
        list(APPEND FO_COMMANDS_GROUP "CompileAngelScript")
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
            COMMENT "Compile Mono scripts")
        list(APPEND FO_COMMANDS_GROUP "CompileMonoScripts")
    endif()
endif()

# Bakering
set(bakeResources "${FO_DEV_NAME}_Baker")

list(APPEND bakeResources -BakeOutput "${FO_BACKED_RESOURCES_OUTPUT}")

foreach(entry ${FO_CONTENT})
    list(APPEND bakeResources -BakeContentEntries "+${entry}")
endforeach()

foreach(entry ${FO_RESOURCES})
    list(APPEND bakeResources -BakeResourceEntries "+${entry}")
endforeach()

add_custom_target(BakeResources
    COMMAND ${bakeResources} -ForceBakering False
    WORKING_DIRECTORY ${FO_OUTPUT_PATH}
    COMMENT "Bake resources")
list(APPEND FO_COMMANDS_GROUP "BakeResources")

add_custom_target(ForceBakeResources
    COMMAND ${bakeResources} -ForceBakering True
    WORKING_DIRECTORY ${FO_OUTPUT_PATH}
    COMMENT "Bake resources")
list(APPEND FO_COMMANDS_GROUP "ForceBakeResources")

# Packaging
StatusMessage("Packages:")

foreach(package ${FO_PACKAGES})
    StatusMessage("+ Package ${package}")

    add_custom_target(MakePackage-${package}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        COMMENT "Make package ${package}")
    list(APPEND FO_COMMANDS_GROUP "MakePackage-${package}")

    foreach(entry ${Package_${package}_Parts})
        set(packageCommands ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/package.py")

        list(APPEND packageCommands -buildhash "${FO_BUILD_HASH}")
        list(APPEND packageCommands -devname "${FO_DEV_NAME}")
        list(APPEND packageCommands -nicename "${FO_NICE_NAME}")
        list(APPEND packageCommands -authorname "${FO_AUTHOR_NAME}")
        list(APPEND packageCommands -gameversion "${FO_GAME_VERSION}")

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

        foreach(entry ${FO_RESOURCES})
            string(REPLACE "," ";" entry ${entry})
            list(GET entry 0 packName)
            list(APPEND packageCommands -respack ${packName})
        endforeach()

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
        list(APPEND packageCommands -output ${FO_OUTPUT_PATH}/${FO_DEV_NAME}-${package})
        list(APPEND packageCommands -compresslevel $<IF:${expr_FullOptimization},9,1>)

        StatusMessage("  ${target} for ${platform}-${arch} in ${pack} with ${config} config")
        add_custom_command(TARGET MakePackage-${package} POST_BUILD COMMAND ${packageCommands})
    endforeach()
endforeach()

# External commands
if(FO_MAKE_EXTERNAL_COMMANDS)
    if(WIN32)
        set(prolog "@echo off\n\n")
        set(start "")
        set(breakLine "^")
        set(scriptExt "bat")
        set(pause "pause")
    else()
        set(prolog "#!/bin/bash -e\n\n")
        set(start "")
        set(breakLine "\\")
        set(scriptExt "sh")
        set(pause "read -p \"Press enter to continue\"")
    endif()

    cmake_path(RELATIVE_PATH CMAKE_CURRENT_SOURCE_DIR BASE_DIRECTORY ${FO_OUTPUT_PATH} OUTPUT_VARIABLE rootDir)
    cmake_path(RELATIVE_PATH FO_BACKED_RESOURCES_OUTPUT BASE_DIRECTORY ${FO_OUTPUT_PATH} OUTPUT_VARIABLE resourcesDir)

    set(FO_GEN_FILE_CONTENT "${prolog}${start}python \"${rootDir}/${FO_ENGINE_ROOT}/BuildTools/starter.py\"")

    set(FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT} ${breakLine}\n-devname \"${FO_DEV_NAME}\"")
    set(FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT} ${breakLine}\n-buildhash \"${FO_BUILD_HASH}\"")
    set(FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT} ${breakLine}\n-bininput \"Binaries\"")
    set(FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT} ${breakLine}\n-defaultcfg \"${FO_DEFAULT_CONFIG}\"")
    set(FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT} ${breakLine}\n-mappercfg \"${FO_MAPPER_CONFIG}\"")
    set(FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT} ${breakLine}\n-bakering \"${resourcesDir}\"")

    foreach(entry ${FO_CONTENT})
        cmake_path(RELATIVE_PATH entry BASE_DIRECTORY ${FO_OUTPUT_PATH} OUTPUT_VARIABLE entry)
        set(FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT} ${breakLine}\n-content \"${entry}\"")
    endforeach()

    foreach(entry ${FO_RESOURCES})
        string(REPLACE "," ";" entry ${entry})
        list(GET entry 0 packName)
        list(GET entry 1 packEntry)
        cmake_path(RELATIVE_PATH packEntry BASE_DIRECTORY ${FO_OUTPUT_PATH} OUTPUT_VARIABLE packEntry)
        set(FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT} ${breakLine}\n-resource \"${packName},${packEntry}\"")
    endforeach()

    configure_file("${FO_ENGINE_ROOT}/BuildTools/blank.cmake" "${FO_OUTPUT_PATH}/Starter.${scriptExt}" FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)
endif()

# Setup targets grouping
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(TARGET ${FO_APPLICATIONS_GROUP} PROPERTY FOLDER "Applications")
set_property(TARGET ${FO_CORE_LIBS_GROUP} PROPERTY FOLDER "CoreLibs")
set_property(TARGET ${FO_COMMANDS_GROUP} PROPERTY FOLDER "Commands")
set_property(TARGET ${FO_COMMON_LIBS} ${FO_BAKER_LIBS} ${FO_SERVER_LIBS} ${FO_CLIENT_LIBS} ${FO_RENDER_LIBS} ${FO_TESTING_LIBS} ${FO_DUMMY_TRAGETS} PROPERTY FOLDER "ThirdParty")

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
