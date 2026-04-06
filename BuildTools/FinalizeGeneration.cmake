cmake_minimum_required(VERSION 3.22)

# Force some variables for internal debugging purposes
if(NOT ${FO_FORCE_ENABLE_3D} STREQUAL "")
    SetValue(FO_ENABLE_3D ${FO_FORCE_ENABLE_3D})
endif()

# Configuration checks
if(FO_CODE_COVERAGE AND (
    FO_BUILD_CLIENT OR
    FO_BUILD_SERVER OR
    FO_BUILD_MAPPER OR
    FO_BUILD_EDITOR OR
    FO_BUILD_ASCOMPILER OR
    FO_BUILD_BAKER OR
    FO_UNIT_TESTS))
    AbortMessage("Code coverage build can not be mixed with other builds")
endif()

if(FO_BUILD_ASCOMPILER AND NOT FO_ANGELSCRIPT_SCRIPTING)
    AbortMessage("AngelScript compiler build can not be without AngelScript scripting enabled")
endif()

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
    AddCompileDefinitionsList(
        FO_HAVE_RPMALLOC=${expr_RpmallocEnabled}
        ENABLE_PRELOAD=${expr_StandaloneRpmallocEnabled})
    TargetCompileDefinitions(rpmalloc PRIVATE "$<$<PLATFORM_ID:Linux>:_GNU_SOURCE>")
else()
    AddCompileDefinitionsList(FO_HAVE_RPMALLOC=0)
endif()

# SDL
StatusMessage("+ SDL")
SetValue(FO_SDL_DIR "${FO_ENGINE_ROOT}/ThirdParty/SDL")
SetBoolCacheValues(
    SDL_TEST_LIBRARY OFF
    SDL_UNIX_CONSOLE_BUILD ${FO_HEADLESS_ONLY})
AddSubdirectory("${FO_SDL_DIR}" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
AddIncludeDirectories("${FO_SDL_DIR}/include")
AppendList(FO_RENDER_LIBS SDL3-static SDL_uclibc)
DisableLibWarnings(SDL3-static SDL_uclibc)

# Tracy profiler
StatusMessage("+ Tracy")
SetValue(FO_TRACY_DIR "${FO_ENGINE_ROOT}/ThirdParty/tracy")
AddCompileDefinitionsList(
    $<${expr_TracyEnabled}:TRACY_ENABLE>
    $<${expr_TracyOnDemand}:TRACY_ON_DEMAND>
    FO_TRACY=${expr_TracyEnabled})
SetBoolCacheValues(TRACY_STATIC ON)
AddSubdirectory("${FO_TRACY_DIR}" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
AddIncludeDirectories("${FO_TRACY_DIR}/public")
AppendList(FO_ESSENTIALS_LIBS TracyClient)
DisableLibWarnings(TracyClient)

# Zlib
StatusMessage("+ Zlib")
SetValue(FO_ZLIB_DIR "${FO_ENGINE_ROOT}/ThirdParty/zlib")
SetBoolCacheValues(ZLIB_BUILD_EXAMPLES OFF)
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
SetStringCacheValues(
    ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ZLIB_DIR}"
    ZLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ZLIB_DIR}"
    ZLIB_LIBRARY "zlibstatic")
SetBoolCacheValues(ZLIB_USE_STATIC_LIBS ON)
AddLibrary(ZLIB::ZLIB ALIAS zlibstatic)

# LibPNG
if(FO_BUILD_BAKER_LIB)
    StatusMessage("+ LibPNG")
    SetValue(FO_PNG_DIR "${FO_ENGINE_ROOT}/ThirdParty/libpng")
    SetBoolCacheValues(
        PNG_SHARED OFF
        PNG_STATIC ON
        PNG_FRAMEWORK OFF
        PNG_TESTS OFF
        PNG_TOOLS OFF
        PNG_DEBUG OFF
        PNG_HARDWARE_OPTIMIZATIONS ON
        PNG_BUILD_ZLIB OFF
        ld-version-script OFF)
    SetStringCacheValues(AWK IGNORE)
    AddSubdirectory("${FO_PNG_DIR}" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
    SetTargetsFolder("ThirdParty/Dummy" png_genfiles)
    AddIncludeDirectories("${FO_PNG_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/${FO_PNG_DIR}")
    AppendList(FO_BAKER_LIBS png_static)
    AppendList(FO_DUMMY_TARGETS png_genfiles)
    DisableLibWarnings(png_static)
endif()

# Ogg
SetValue(FO_OGG_DIR "${FO_ENGINE_ROOT}/ThirdParty/ogg")
SetValue(FO_OGG_SOURCE
    "${FO_OGG_DIR}/src/bitwise.c"
    "${FO_OGG_DIR}/src/framing.c")
AddStaticThirdPartyLibrary(Ogg
    SOURCE_LIST FO_OGG_SOURCE
    APPEND_TO FO_CLIENT_LIBS
    INCLUDE_DIRS "${FO_OGG_DIR}/include")

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

# GLEW
if(FO_USE_GLEW)
    SetValue(FO_GLEW_DIR "${FO_ENGINE_ROOT}/ThirdParty/GLEW")
    SetValue(FO_GLEW_SOURCE
        "${FO_GLEW_DIR}/GL/glew.c"
        "${FO_GLEW_DIR}/GL/glew.h"
        "${FO_GLEW_DIR}/GL/glxew.h"
        "${FO_GLEW_DIR}/GL/wglew.h")
    AddStaticThirdPartyLibrary(GLEW
        SOURCE_LIST FO_GLEW_SOURCE
        APPEND_TO FO_RENDER_LIBS
        INCLUDE_DIRS "${FO_GLEW_DIR}")
    AddCompileDefinitionsList(GLEW_STATIC)
    TargetCompileDefinitions(GLEW PRIVATE "GLEW_STATIC")
endif()

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
if(FO_BUILD_SERVER_LIB)
    StatusMessage("+ LibreSSL")
    SetValue(FO_LIBRESSL_DIR "${FO_ENGINE_ROOT}/ThirdParty/LibreSSL")
    SetBoolCacheValues(
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
    SetBoolCacheValues(LIBRESSL_FOUND ON)
    SetStringCacheValues(
        LIBRESSL_LIBRARIES "ssl;crypto;tls"
        LIBRESSL_INCLUDE_DIRS ""
        LIBRESSL_LIBRARY_DIRS "")
    AppendList(FO_SERVER_LIBS ssl crypto tls)
    AppendList(FO_DUMMY_TARGETS compat_obj crypto_obj ssl_obj tls_compat_obj tls_obj)
    DisableLibWarnings(ssl crypto tls compat_obj crypto_obj ssl_obj tls_compat_obj tls_obj)
endif()

# Asio & Websockets
if(NOT FO_DISABLE_ASIO AND NOT FO_ANDROID AND FO_BUILD_SERVER_LIB)
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

    SetStringCacheValues(
        ENABLE_STATIC BUILD_ONLY
        ENABLE_TESTS OFF
        ENABLE_SSL OFF
        ENABLE_SASL OFF
        ENABLE_ZLIB SYSTEM
        ENABLE_CLIENT_SIDE_ENCRYPTION OFF)
    SetBoolCacheValues(
        ENABLE_SHARED OFF
        ENABLE_SRV OFF
        ENABLE_UNINSTALL OFF
        ENABLE_EXAMPLES OFF
        USE_BUNDLED_UTF8PROC ON)

    if(NOT FO_DISABLE_MONGO)
        StatusMessage("+ MongoDB")
        SetStringCacheValues(ENABLE_MONGOC ON)
        AddCompileDefinitionsList(FO_HAVE_MONGO=1)
    else()
        SetStringCacheValues(ENABLE_MONGOC OFF)
        AddCompileDefinitionsList(FO_HAVE_MONGO=0)
    endif()

    AddSubdirectory("${FO_MONGODB_DIR}" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
    if(TARGET utf8proc_obj)
        SetTargetsFolder("ThirdParty/Dummy" utf8proc_obj)
    endif()

    AddIncludeDirectories(
        "${CMAKE_CURRENT_BINARY_DIR}/${FO_MONGODB_DIR}/src/libbson/src/bson"
        "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MONGODB_DIR}/src/libbson/src")
    TargetCompileDefinitions(bson_static PRIVATE "BSON_COMPILATION;BSON_STATIC;JSONSL_PARSE_NAN")
    AddCompileDefinitionsList(BSON_COMPILATION BSON_STATIC JSONSL_PARSE_NAN)
    AppendList(FO_SERVER_LIBS bson_static)
    DisableLibWarnings(bson_static)

    if(ENABLE_MONGOC)
        AddIncludeDirectories(
            "${CMAKE_CURRENT_BINARY_DIR}/${FO_MONGODB_DIR}/src/libmongoc/src/mongoc"
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
    "${FO_DEAR_IMGUI_DIR}/imgui_demo.cpp"
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
SetBoolCacheValues(SPARK_STATIC_BUILD ON)
AddSubdirectory("${FO_SPARK_DIR}/projects/engine/core" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
AddSubdirectory("${FO_SPARK_DIR}/projects/external/pugi" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
AddIncludeDirectories("${FO_SPARK_DIR}/spark/include" "${FO_SPARK_DIR}/thirdparty/PugiXML")
AppendList(FO_CLIENT_LIBS SPARK_Core PugiXML)
DisableLibWarnings(SPARK_Core PugiXML)

# glslang & SPIRV-Cross
if(FO_BUILD_BAKER_LIB)
    StatusMessage("+ glslang")
    SetValue(FO_GLSLANG_DIR "${FO_ENGINE_ROOT}/ThirdParty/glslang")
    SetBoolCacheValues(
        GLSLANG_TESTS OFF
        GLSLANG_ENABLE_INSTALL OFF
        BUILD_EXTERNAL OFF
        BUILD_WERROR OFF
        SKIP_GLSLANG_INSTALL ON
        ENABLE_SPIRV ON
        ENABLE_HLSL OFF
        ENABLE_GLSLANG_BINARIES OFF
        ENABLE_SPVREMAPPER OFF
        ENABLE_AMD_EXTENSIONS OFF
        ENABLE_NV_EXTENSIONS OFF
        ENABLE_RTTI ON
        ENABLE_EXCEPTIONS ON
        ENABLE_OPT OFF
        ENABLE_PCH OFF
        ALLOW_EXTERNAL_SPIRV_TOOLS OFF)

    if(FO_WEB)
        SetBoolCacheValues(
            ENABLE_GLSLANG_WEB ON
            ENABLE_GLSLANG_WEB_DEVEL ON
            ENABLE_EMSCRIPTEN_SINGLE_FILE ON
            ENABLE_EMSCRIPTEN_ENVIRONMENT_NODE OFF)
    endif()

    AddSubdirectory("${FO_GLSLANG_DIR}" EXCLUDE_FROM_ALL)
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
    SetBoolCacheValues(
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
        SPIRV_SKIP_TESTS ON)
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
    SetStringCacheValues(ANGELSCRIPT_LIBRARY_NAME "AngelScriptCore")
    SetBoolCacheValues(AS_DISABLE_INSTALL ON)
    AddSubdirectory("${FO_ANGELSCRIPT_SDK_DIR}/angelscript/projects/cmake" FOLDER "ThirdParty" EXCLUDE_FROM_ALL)
    TargetCompileDefinitions(AngelScriptCore PUBLIC AS_USE_NAMESPACE)
    TargetCompileDefinitions(AngelScriptCore PUBLIC $<${expr_DebugBuild}:AS_DEBUG>)
    TargetCompileDefinitions(
        AngelScriptCore PUBLIC
        $<$<OR:$<BOOL:${FO_WEB}>,$<BOOL:${FO_MAC}>,$<BOOL:${FO_IOS}>,$<BOOL:${FO_ANDROID}>>:AS_MAX_PORTABILITY>)
    TargetIncludeDirectories(AngelScriptCore PUBLIC
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
    TargetIncludeDirectories(AngelScriptCore PUBLIC
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

# App icon
SetValue(FO_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${FO_DEV_NAME}.rc")
GetFilenameComponent(FO_APP_ICON ${FO_APP_ICON} REALPATH)
SetValue(FO_GEN_FILE_CONTENT "101 ICON \"${FO_APP_ICON}\"")
configure_file(
    "${FO_ENGINE_ROOT}/BuildTools/blank.cmake.txt"
    ${FO_RC_FILE}
    FILE_PERMISSIONS OWNER_WRITE OWNER_READ)

# Engine sources
AppendList(FO_ESSENTIALS_SOURCE
    "${FO_ENGINE_ROOT}/Source/Essentials/Essentials.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/Essentials.h"
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
    "${FO_ENGINE_ROOT}/Source/Essentials/ExceptionHandling.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/ExceptionHandling.cpp"
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
    "${FO_ENGINE_ROOT}/Source/Essentials/NetSockets.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/NetSockets.cpp"
    "${FO_ENGINE_ROOT}/Source/Essentials/WinApi-Include.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/WinApiUndef-Include.h"
    "$<$<BOOL:${MSVC}>:${FO_ENGINE_ROOT}/BuildTools/natvis/essentials.natvis>")

AppendList(FO_COMMON_SOURCE
    "${FO_ENGINE_ROOT}/Source/Common/Common.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Common.h"
    "${FO_ENGINE_ROOT}/Source/Common/WebRelated.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/WebRelated.h"
    "${FO_ENGINE_ROOT}/Source/Common/AnyData.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/AnyData.h"
    "${FO_ENGINE_ROOT}/Source/Common/CacheStorage.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/CacheStorage.h"
    "${FO_ENGINE_ROOT}/Source/Common/ConfigFile.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/ConfigFile.h"
    "${FO_ENGINE_ROOT}/Source/Common/MetadataRegistration.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/MetadataRegistration.h"
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
    "${FO_ENGINE_ROOT}/Source/Common/Movement.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/Movement.h"
    "${FO_ENGINE_ROOT}/Source/Common/MapLoader.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/MapLoader.h"
    "${FO_ENGINE_ROOT}/Source/Common/NetBuffer.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/NetBuffer.h"
    "${FO_ENGINE_ROOT}/Source/Common/NetCommand.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/NetCommand.h"
    "${FO_ENGINE_ROOT}/Source/Common/PathFinding.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/PathFinding.h"
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
    "${FO_ENGINE_ROOT}/Source/Common/ImGuiExt/ImGuiStuff.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/ImGuiExt/ImGuiStuff.h"
    "${FO_ENGINE_ROOT}/Source/Scripting/CommonImGuiScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/CommonGlobalScriptMethods.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/Version-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/EmbeddedResources-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/GenericCode-Common.cpp"
    "$<$<BOOL:${MSVC}>:${FO_ENGINE_ROOT}/BuildTools/natvis/fonline.natjmc>")

AppendList(FO_SERVER_BASE_SOURCE
    "${FO_ENGINE_ROOT}/Source/Server/Critter.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/Critter.h"
    "${FO_ENGINE_ROOT}/Source/Server/CritterManager.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/CritterManager.h"
    "${FO_ENGINE_ROOT}/Source/Server/DataBase.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/DataBase-Json.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/DataBase-Memory.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/DataBase-Mongo.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/DataBase-UnQLite.cpp"
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
    "${FO_ENGINE_ROOT}/Source/Server/RemoteCallValidation.cpp"
    "${FO_ENGINE_ROOT}/Source/Server/RemoteCallValidation.h"
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

AppendList(FO_CLIENT_BASE_SOURCE
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
    "${FO_ENGINE_ROOT}/Source/Client/FogOfWar.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/FogOfWar.h"
    "${FO_ENGINE_ROOT}/Source/Client/HexView.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/HexView.h"
    "${FO_ENGINE_ROOT}/Source/Client/ItemHexView.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/ItemHexView.h"
    "${FO_ENGINE_ROOT}/Source/Client/ItemView.cpp"
    "${FO_ENGINE_ROOT}/Source/Client/ItemView.h"
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
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientImGuiScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientPlayerScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientItemScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientCritterScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientMapScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientLocationScriptMethods.cpp")

AppendList(FO_SERVER_SOURCE
    ${FO_SERVER_BASE_SOURCE}
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-Server.cpp")

AppendList(FO_CLIENT_SOURCE
    ${FO_CLIENT_BASE_SOURCE}
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-Client.cpp")

AppendList(FO_EDITOR_SOURCE
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

AppendList(FO_MAPPER_SOURCE
    "${FO_ENGINE_ROOT}/Source/Tools/Mapper.h"
    "${FO_ENGINE_ROOT}/Source/Tools/Mapper.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/MapperGlobalScriptMethods.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-Mapper.cpp")

AppendList(FO_BAKER_SOURCE
    "${FO_ENGINE_ROOT}/Source/Tools/AngelScriptBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/AngelScriptBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/Baker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/Baker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/ConfigBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/ConfigBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/EffectBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/EffectBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/ImageBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/ImageBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/MapBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/MapBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tools/MetadataBaker.h"
    "${FO_ENGINE_ROOT}/Source/Tools/MetadataBaker.cpp"
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
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-ServerStub.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-ClientStub.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-MapperStub.cpp")

AppendList(FO_SOURCE_META_FILES
    "${FO_ENGINE_ROOT}/Source/Essentials/ExtendedTypes.h"
    "${FO_ENGINE_ROOT}/Source/Essentials/TimeRelated.h"
    "${FO_ENGINE_ROOT}/Source/Frontend/Application.h"
    "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.h"
    "${FO_ENGINE_ROOT}/Source/Common/Common.h"
    "${FO_ENGINE_ROOT}/Source/Common/Entity.h"
    "${FO_ENGINE_ROOT}/Source/Common/EntityProperties.h"
    "${FO_ENGINE_ROOT}/Source/Common/Geometry.h"
    "${FO_ENGINE_ROOT}/Source/Common/Movement.h"
    "${FO_ENGINE_ROOT}/Source/Common/Settings-Include.h"
    "${FO_ENGINE_ROOT}/Source/Common/MetadataRegistration-Template.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/GenericCode-Template.cpp"
    "${FO_ENGINE_ROOT}/Source/Common/TextPack.h"
    "${FO_ENGINE_ROOT}/Source/Common/ImGuiExt/ImGuiStuff.h"
    "${FO_ENGINE_ROOT}/Source/Client/Client.h"
    "${FO_ENGINE_ROOT}/Source/Client/MapSprite.h"
    "${FO_ENGINE_ROOT}/Source/Client/MapView.h"
    "${FO_ENGINE_ROOT}/Source/Client/SpriteManager.h"
    "${FO_ENGINE_ROOT}/Source/Server/Critter.h"
    "${FO_ENGINE_ROOT}/Source/Server/Item.h"
    "${FO_ENGINE_ROOT}/Source/Server/Location.h"
    "${FO_ENGINE_ROOT}/Source/Server/Map.h"
    "${FO_ENGINE_ROOT}/Source/Server/Player.h"
    "${FO_ENGINE_ROOT}/Source/Server/Server.h"
    "${FO_ENGINE_ROOT}/Source/Tools/Mapper.h"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerEntityScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerGlobalScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerPlayerScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerItemScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerCritterScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerMapScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ServerLocationScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientEntityScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientGlobalScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientImGuiScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientPlayerScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientItemScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientCritterScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientMapScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/ClientLocationScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/MapperGlobalScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/CommonImGuiScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Scripting/CommonGlobalScriptMethods.cpp")

AppendList(FO_TESTS_SOURCE
    "${FO_ENGINE_ROOT}/Source/Tests/Test_AngelScriptBytecode.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_AnyData.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_AngelScriptBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_BaseLogging.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_BasicCore.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_CommonHelpers.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_BakerSetup.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_CacheStorage.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ClientEngine.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ClientServerIntegration.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ConfigBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_Compressor.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_Common.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ConfigFile.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_Containers.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_DataBase.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_DataSerialization.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_DataSource.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_DiskFileSystem.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_EntityProtos.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_EffectBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_EngineMetadata.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ExceptionHandling.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_FileSystem.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_FogOfWar.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_NetCommand.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_Rendering.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_Settings.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_SmartPointers.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_StackTrace.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ExtendedTypes.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_GenericUtils.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_Geometry.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_GlobalData.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_HashedString.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ImageBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_LineTracer.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_Logging.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_MapLoader.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_MapBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_MemorySystem.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_MetadataBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ModelBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_NetBuffer.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_PathFinding.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_NetworkClient.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_NetworkServer.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_NetSockets.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ProtoBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ProtoManager.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ProtoTextBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_Properties.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_Platform.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_RawCopyBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_RemoteCallValidation.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_SafeArithmetics.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ScriptBuiltins.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ScriptEntityOps.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ServerAdvancedOps.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_EntityLifecycle.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_LocationAndEntityMgmt.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_CommonScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ServerEngine.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ServerItems.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ServerMapOperations.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_ServerScriptMethods.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_StrongType.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_StringUtils.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_TextBaker.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_TextPack.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_TextureAtlas.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_Timer.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_TimeRelated.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_TwoDimensionalGrid.cpp"
    "${FO_ENGINE_ROOT}/Source/Tests/Test_WorkThread.cpp")

# Code generation
IncludeFile(FindPython3)
RequirePackage(Python3 REQUIRED COMPONENTS Interpreter)

AppendList(FO_CODEGEN_COMMAND_ARGS -maincfg "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}")
AppendList(FO_CODEGEN_COMMAND_ARGS -buildhash "${FO_BUILD_HASH}")
AppendList(FO_CODEGEN_COMMAND_ARGS -genoutput "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource")
AppendList(FO_CODEGEN_COMMAND_ARGS -devname "${FO_DEV_NAME}")
AppendList(FO_CODEGEN_COMMAND_ARGS -nicename "${FO_NICE_NAME}")
AppendList(FO_CODEGEN_COMMAND_ARGS -embedded "${FO_EMBEDDED_DATA_CAPACITY}")

AppendList(FO_CODEGEN_META_SOURCE
    ${FO_SOURCE_META_FILES}
    ${FO_MONO_SOURCE})

foreach(entry ${FO_CODEGEN_META_SOURCE})
    AppendList(FO_CODEGEN_COMMAND_ARGS -meta ${entry})
endforeach()

foreach(entry ${FO_ADDED_COMMON_HEADERS})
    AppendList(FO_CODEGEN_COMMAND_ARGS -commonheader ${entry})
endforeach()

AppendList(FO_CODEGEN_OUTPUT
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/CodeGenTouch"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/Version-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/EmbeddedResources-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-Server.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-Client.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-ServerStub.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-ClientStub.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-MapperStub.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/GenericCode-Common.cpp")

FileWrite("${CMAKE_CURRENT_BINARY_DIR}/codegen-args.txt" "")

foreach(entry ${FO_CODEGEN_COMMAND_ARGS})
    FileAppend("${CMAKE_CURRENT_BINARY_DIR}/codegen-args.txt" "${entry}\n")
endforeach()

SetValue(FO_CODEGEN_COMMAND
    ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/codegen.py"
    "@${CMAKE_CURRENT_BINARY_DIR}/codegen-args.txt")
SetValue(codegenTouchCommand
    ${CMAKE_COMMAND}
    -E touch
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/CodeGenTouch")

AddCustomCommand(OUTPUT ${FO_CODEGEN_OUTPUT}
    COMMAND ${FO_CODEGEN_COMMAND}
    COMMAND ${codegenTouchCommand}
    DEPENDS ${FO_CODEGEN_META_SOURCE}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Code generation")

AddCommandTarget(CodeGeneration
    DEPENDS ${FO_CODEGEN_OUTPUT}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
AppendList(FO_GEN_DEPENDENCIES CodeGeneration)

AddCommandTarget(ForceCodeGeneration
    COMMAND_ARGS
    COMMAND ${FO_CODEGEN_COMMAND}
    COMMAND ${codegenTouchCommand}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Core libs
StatusMessage("Core libs:")

AddCoreStaticLibrary(EssentialsLib FO_ESSENTIALS_SOURCE
    APPEND_TO_GROUP FO_CORE_LIBS_GROUP
    LINK_LIBS ${FO_ESSENTIALS_SYSTEM_LIBS} ${FO_ESSENTIALS_LIBS})

if(FO_ANGELSCRIPT_SCRIPTING)
    StatusMessage("+ AngelScriptScripting")
    SetValue(FO_ANGELSCRIPT_SCRIPTING_DIR "${FO_ENGINE_ROOT}/Source/Scripting/AngelScript")
    SetValue(_fo_prev_folder "${CMAKE_FOLDER}")
    SetValue(CMAKE_FOLDER "CoreLibs")
    AddLibrary(AngelScriptScripting STATIC EXCLUDE_FROM_ALL
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptArray.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptArray.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptBackend.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptBackend.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptCall.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptCall.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptContext.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptContext.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptDict.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptDict.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptDebugger.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptDebugger.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptEntity.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptEntity.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptGlobals.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptGlobals.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptHelpers.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptHelpers.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptMath.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptMath.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptReflection.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptReflection.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptRemoteCalls.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptRemoteCalls.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptScripting.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptScripting.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptString.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptString.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptTypes.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptTypes.h")
    SetValue(CMAKE_FOLDER "${_fo_prev_folder}")
    SetTargetProperty(AngelScriptScripting FOLDER "CoreLibs")
    AddDependencies(AngelScriptScripting
        ${FO_GEN_DEPENDENCIES})
    TargetIncludeDirectories(AngelScriptScripting PUBLIC
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}")
    TargetLinkLibraries(AngelScriptScripting
        EssentialsLib
        AngelScriptCore
        AngelScriptPreprocessor)
    AppendList(FO_CORE_LIBS_GROUP AngelScriptScripting)
endif()

if(FO_BUILD_COMMON_LIB)
    SetValue(FO_APP_HEADLESS_SOURCE
        "${FO_ENGINE_ROOT}/Source/Frontend/Application.h"
        "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationInit.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationHeadless.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationStub.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-Null.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.h")
    AddCoreStaticLibrary(AppHeadless FO_APP_HEADLESS_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP)

    if(NOT FO_HEADLESS_ONLY)
        SetValue(FO_APP_FRONTEND_SOURCE
            "${FO_ENGINE_ROOT}/Source/Frontend/Application.h"
            "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationInit.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Application.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationStub.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-Null.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.h"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-Direct3D.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-OpenGL.cpp")
        AddCoreStaticLibrary(AppFrontend FO_APP_FRONTEND_SOURCE
            APPEND_TO_GROUP FO_CORE_LIBS_GROUP
            LINK_LIBS ${FO_RENDER_SYSTEM_LIBS} ${FO_RENDER_LIBS})
    endif()

    AddCoreStaticLibrary(CommonLib FO_COMMON_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS EssentialsLib ${FO_COMMON_SYSTEM_LIBS} ${FO_COMMON_LIBS})

    if(FO_ANGELSCRIPT_SCRIPTING)
        TargetLinkLibraries(CommonLib AngelScriptScripting)
    endif()
endif()

if(FO_BUILD_CLIENT_LIB)
    AddCoreStaticLibrary(ClientLib FO_CLIENT_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS CommonLib ${FO_CLIENT_SYSTEM_LIBS} ${FO_CLIENT_LIBS})
endif()

if(FO_BUILD_SERVER_LIB)
    AddCoreStaticLibrary(ServerLib FO_SERVER_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS CommonLib ${FO_SERVER_SYSTEM_LIBS} ${FO_SERVER_LIBS})
endif()

if(FO_BUILD_MAPPER_LIB)
    AddCoreStaticLibrary(MapperLib FO_MAPPER_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS ClientLib CommonLib)
endif()

if(FO_BUILD_BAKER_LIB)
    AddCoreStaticLibrary(BakerLib FO_BAKER_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS CommonLib ${FO_BAKER_SYSTEM_LIBS} ${FO_BAKER_LIBS})
endif()

if(FO_BUILD_EDITOR_LIB)
    AddCoreStaticLibrary(EditorLib FO_EDITOR_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS BakerLib CommonLib)
endif()

# Applications
StatusMessage("Applications:")

if(FO_BUILD_CLIENT)
    if(NOT FO_BUILD_LIBRARY)
        # Todo: cmake make bundles for Mac and iOS
        # add_executable( ${FO_DEV_NAME}_Client MACOSX_BUNDLE ... ${FO_RC_FILE} )
        AddExecutableApplication(${FO_DEV_NAME}_Client "${FO_ENGINE_ROOT}/Source/Applications/ClientApp.cpp"
            WIN32
            OUTPUT_DIR ${FO_CLIENT_OUTPUT}
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            OUTPUT_NAME ${FO_DEV_NAME}_Client
            TESTING_APP 0
            LINK_LIBS "AppFrontend" "ClientLib"
            EXTRA_SOURCES ${FO_RC_FILE}
            WRITE_BUILD_HASH)
    else()
        AddSharedApplication(${FO_DEV_NAME}_Client "${FO_ENGINE_ROOT}/Source/Applications/ClientApp.cpp"
            OUTPUT_DIR ${FO_CLIENT_OUTPUT}
            OUTPUT_NAME ${FO_DEV_NAME}_Client
            TESTING_APP 0
            LINK_LIBS "AppFrontend" "ClientLib"
            WRITE_BUILD_HASH)
    endif()
endif()

if(FO_BUILD_SERVER)
    AddExecutableApplication(
        ${FO_DEV_NAME}_Server
        "${FO_ENGINE_ROOT}/Source/Applications/ServerApp.cpp"
        WIN32
        OUTPUT_DIR ${FO_SERVER_OUTPUT}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        OUTPUT_NAME ${FO_DEV_NAME}_Server
        TESTING_APP 0
        LINK_LIBS "ServerLib" "ClientLib" "AppFrontend"
        EXTRA_SOURCES ${FO_RC_FILE}
        WRITE_BUILD_HASH)

    AddExecutableApplication(
        ${FO_DEV_NAME}_ServerHeadless
        "${FO_ENGINE_ROOT}/Source/Applications/ServerHeadlessApp.cpp"
        OUTPUT_DIR ${FO_SERVER_OUTPUT}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        OUTPUT_NAME ${FO_DEV_NAME}_ServerHeadless
        TESTING_APP 0
        LINK_LIBS "ServerLib" "ClientLib" "AppHeadless"
        WRITE_BUILD_HASH)

    if(FO_WINDOWS)
        AddExecutableApplication(
            ${FO_DEV_NAME}_ServerService
            "${FO_ENGINE_ROOT}/Source/Applications/ServerServiceApp.cpp"
            OUTPUT_DIR ${FO_SERVER_OUTPUT}
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            OUTPUT_NAME ${FO_DEV_NAME}_ServerService
            TESTING_APP 0
            LINK_LIBS "ServerLib" "ClientLib" "AppHeadless"
            WRITE_BUILD_HASH)
    else()
        AddExecutableApplication(
            ${FO_DEV_NAME}_ServerDaemon
            "${FO_ENGINE_ROOT}/Source/Applications/ServerDaemonApp.cpp"
            OUTPUT_DIR ${FO_SERVER_OUTPUT}
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            OUTPUT_NAME ${FO_DEV_NAME}_ServerDaemon
            TESTING_APP 0
            LINK_LIBS "ServerLib" "ClientLib" "AppHeadless"
            WRITE_BUILD_HASH)
    endif()
endif()

if(FO_BUILD_EDITOR)
    AddExecutableApplication(${FO_DEV_NAME}_Editor "${FO_ENGINE_ROOT}/Source/Applications/EditorApp.cpp"
        WIN32
        OUTPUT_DIR ${FO_EDITOR_OUTPUT}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        OUTPUT_NAME ${FO_DEV_NAME}_Editor
        TESTING_APP 0
        LINK_LIBS "AppFrontend" "EditorLib" "MapperLib" "BakerLib" "ClientLib" "ServerLib"
        EXTRA_SOURCES ${FO_RC_FILE}
        WRITE_BUILD_HASH)
endif()

if(FO_BUILD_MAPPER)
    AddExecutableApplication(${FO_DEV_NAME}_Mapper "${FO_ENGINE_ROOT}/Source/Applications/MapperApp.cpp"
        WIN32
        OUTPUT_DIR ${FO_MAPPER_OUTPUT}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        OUTPUT_NAME ${FO_DEV_NAME}_Mapper
        TESTING_APP 0
        LINK_LIBS "AppFrontend" "MapperLib" "ClientLib" "BakerLib"
        EXTRA_SOURCES ${FO_RC_FILE}
        WRITE_BUILD_HASH)
endif()

if(FO_BUILD_ASCOMPILER)
    AddExecutableApplication(
        ${FO_DEV_NAME}_ASCompiler
        "${FO_ENGINE_ROOT}/Source/Applications/ASCompilerApp.cpp"
        OUTPUT_DIR ${FO_ASCOMPILER_OUTPUT}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        OUTPUT_NAME ${FO_DEV_NAME}_ASCompiler
        TESTING_APP 0
        LINK_LIBS "AppHeadless" "BakerLib"
        DEPENDS ${FO_GEN_DEPENDENCIES}
        WRITE_BUILD_HASH)
endif()

if(FO_BUILD_BAKER)
    AddExecutableApplication(${FO_DEV_NAME}_Baker "${FO_ENGINE_ROOT}/Source/Applications/BakerApp.cpp"
        OUTPUT_DIR ${FO_BAKER_OUTPUT}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        OUTPUT_NAME ${FO_DEV_NAME}_Baker
        TESTING_APP 0
        LINK_LIBS "AppHeadless" "BakerLib"
        WRITE_BUILD_HASH)

    if(NOT FO_WEB)
        AddSharedApplication(${FO_DEV_NAME}_BakerLib "${FO_ENGINE_ROOT}/Source/Applications/BakerLib.cpp"
            OUTPUT_DIR ${FO_BAKER_OUTPUT}
            OUTPUT_NAME ${FO_DEV_NAME}_BakerLib
            TESTING_APP 0
            LINK_LIBS PRIVATE "BakerLib"
            EXTRA_PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_BAKER_OUTPUT}
            NO_PREFIX
            WRITE_BUILD_HASH)
    endif()
endif()

if(FO_UNIT_TESTS OR FO_CODE_COVERAGE)
    SetValue(FO_CODE_COVERAGE_BACKEND "")

    if(FO_CODE_COVERAGE)
        if(MSVC)
            SetValue(FO_CODE_COVERAGE_BACKEND "msvc")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            SetValue(FO_CODE_COVERAGE_BACKEND "llvm")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            SetValue(FO_CODE_COVERAGE_BACKEND "gcc")
        else()
            AbortMessage("Code coverage backend is not configured for the selected compiler")
        endif()
    endif()

    macro(SetupTestBuild name)
        SetValue(target ${FO_DEV_NAME}_${name})
        SetValue(testBuildSources
            ${FO_TESTS_SOURCE}
            "${FO_ENGINE_ROOT}/Source/Applications/EditorApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/MapperApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/BakerApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ServerApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ServerServiceApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ServerDaemonApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ServerHeadlessApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ClientApp.cpp")

        AddExecutableApplication(
            ${target}
            "${FO_ENGINE_ROOT}/Source/Applications/TestingApp.cpp"
            OUTPUT_DIR ${FO_TESTS_OUTPUT}
            WORKING_DIRECTORY ${FO_TESTS_OUTPUT}
            OUTPUT_NAME ${target}
            TESTING_APP 1
            LINK_LIBS
                "BakerLib"
                "EditorLib"
                "MapperLib"
                ${FO_TESTING_LIBS}
                "ClientLib"
                "ServerLib"
                "AppHeadless"
            DEPENDS ${FO_GEN_DEPENDENCIES}
            EXTRA_SOURCES ${testBuildSources})

        if("${name}" STREQUAL "CodeCoverage")
            SetValue(coverageTool
                ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/codecoverage.py")
            SetValue(coverageToolArgs
                --workspace-root "${CMAKE_CURRENT_SOURCE_DIR}"
                --build-dir "${CMAKE_CURRENT_BINARY_DIR}"
                --binary "$<TARGET_FILE:${target}>"
                --backend "${FO_CODE_COVERAGE_BACKEND}"
                --output-dir "${FO_COVERAGE_OUTPUT}")

            AddCommandTarget(CleanCodeCoverageData
                COMMAND_ARGS COMMAND ${coverageTool} clean ${coverageToolArgs}
                DEPENDS ${target}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Clean code coverage data")

            AddCommandTarget(Run${name}
                COMMAND_ARGS COMMAND ${coverageTool} run ${coverageToolArgs}
                DEPENDS ${target}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Run ${name}")

            AddCommandTarget(GenerateCodeCoverageReport
                COMMAND_ARGS COMMAND ${coverageTool} report ${coverageToolArgs}
                DEPENDS Run${name}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Generate code coverage report")

            AddCommandTarget(AnalyzeCodeCoverage
                COMMAND_ARGS COMMAND ${coverageTool} full ${coverageToolArgs}
                DEPENDS ${target}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Run code coverage and generate report")
        else()
            AddCommandTarget(Run${name}
                COMMAND_ARGS COMMAND ${target}
                COMMENT "Run ${name}")
        endif()
    endmacro()

    if(FO_UNIT_TESTS)
        SetupTestBuild(UnitTests)
    endif()

    if(FO_CODE_COVERAGE)
        SetupTestBuild(CodeCoverage)
    endif()
endif()

# Scripts compilation
SetValue(foMainConfigArgs -ApplyConfig "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}" -ApplySubConfig "NONE")
SetValue(compileASScripts "")
SetValue(compileMonoScripts "")

if(FO_NATIVE_SCRIPTING OR FO_ANGELSCRIPT_SCRIPTING OR FO_MONO_SCRIPTING)
    # Compile AngelScript scripts
    if(FO_ANGELSCRIPT_SCRIPTING)
        SetValue(compileASScripts ${FO_DEV_NAME}_ASCompiler ${foMainConfigArgs})

        AddCommandTarget(CompileAngelScript
            COMMAND_ARGS COMMAND ${compileASScripts}
            DEPENDS ForceCodeGeneration
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            COMMENT "Compile AngelScript scripts")
    endif()

    # Compile Mono scripts
    if(FO_MONO_SCRIPTING)
        SetValue(monoCompileCommands "")

        foreach(entry ${FO_MONO_ASSEMBLIES})
            AppendList(monoCompileCommands -assembly ${entry})
        endforeach()

        SetValue(compileMonoScripts
            ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/compile-mono-scripts.py"
            ${monoCompileCommands})

        AddCommandTarget(CompileMonoScripts
            COMMAND_ARGS COMMAND ${compileMonoScripts}
            SOURCES ${FO_MONO_SOURCE}
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            COMMENT "Compile Mono scripts")
    endif()
endif()

# Baking
SetValue(bakeResources "${FO_DEV_NAME}_Baker" ${foMainConfigArgs})
SetValue(resourceBuildHashCommand
    ${CMAKE_COMMAND}
    -DHASH_FILE="${FO_OUTPUT_PATH}/Baking/Resources.build-hash"
    -DGIT_ROOT="${FO_GIT_ROOT}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/cmake/WriteBuildHash.cmake")

AddCommandTarget(BakeResources
    COMMAND_ARGS
    COMMAND ${bakeResources} -ForceBaking False
    COMMAND ${resourceBuildHashCommand}
    DEPENDS ForceCodeGeneration
    WORKING_DIRECTORY ${FO_OUTPUT_PATH}
    COMMENT "Bake resources")

AddCommandTarget(ForceBakeResources
    COMMAND_ARGS
    COMMAND ${bakeResources} -ForceBaking True
    COMMAND ${resourceBuildHashCommand}
    DEPENDS ForceCodeGeneration
    WORKING_DIRECTORY ${FO_OUTPUT_PATH}
    COMMENT "Bake resources")

# Packaging
StatusMessage("Packages:")

foreach(package ${FO_PACKAGES})
    StatusMessage("+ Package ${package}")

    SetValue(packageBaseCommands
        ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/package.py"
        -maincfg "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}"
        -buildhash "${FO_BUILD_HASH}"
        -devname "${FO_DEV_NAME}"
        -nicename "${FO_NICE_NAME}")

    AddCommandTarget(MakePackage-${package}
        COMMAND_ARGS COMMAND ${CMAKE_COMMAND} -E rm -rf "${FO_OUTPUT_PATH}/${FO_DEV_NAME}-${package}"
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        COMMENT "Make package ${package}")

    foreach(entry ${Package_${package}_Parts})
        SetValue(packageCommands ${packageBaseCommands})

        StringReplace("," ";" entry ${entry})
        ListGet(entry 0 target)
        ListGet(entry 1 platform)
        ListGet(entry 2 arch)
        ListGet(entry 3 pack)
        ListGet(entry 4 customConfig)

        AppendList(packageCommands -target "${target}")
        AppendList(packageCommands -platform "${platform}")
        AppendList(packageCommands -arch "${arch}")
        AppendList(packageCommands -pack "${pack}")

        if(customConfig)
            SetValue(config ${customConfig})
        else()
            SetValue(config ${Package_${package}_Config})
        endif()

        AppendList(packageCommands
            -config "${config}"
            -input ${FO_OUTPUT_PATH}
            -output "${FO_OUTPUT_PATH}/${FO_DEV_NAME}-${package}")

        StatusMessage("  ${target} for ${platform}-${arch} in ${pack} with ${config} config")
        AddCustomCommand(TARGET MakePackage-${package} POST_BUILD
            COMMAND ${packageCommands})
    endforeach()
endforeach()

# Copy ReSharper config
if(MSVC)
    if(FO_RESHARPER_SETTINGS)
        FileCreateLink(
            "${CMAKE_CURRENT_SOURCE_DIR}/${FO_RESHARPER_SETTINGS}"
            "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.sln.DotSettings"
            COPY_ON_ERROR)
    else()
        FileCreateLink(
            "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/ReSharper.sln.DotSettings"
            "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.sln.DotSettings"
            COPY_ON_ERROR)
    endif()
endif()

# Setup targets grouping
SetGlobalProperty(USE_FOLDERS ON)
SetTargetsFolder("Applications" ${FO_APPLICATIONS_GROUP})
SetTargetsFolder("CoreLibs" ${FO_CORE_LIBS_GROUP})
SetTargetsFolder("Commands" ${FO_COMMANDS_GROUP})
SetTargetsFolder(
    "ThirdParty"
    ${FO_ESSENTIALS_LIBS}
    ${FO_COMMON_LIBS}
    ${FO_BAKER_LIBS}
    ${FO_SERVER_LIBS}
    ${FO_CLIENT_LIBS}
    ${FO_RENDER_LIBS}
    ${FO_TESTING_LIBS})
SetTargetsFolder("ThirdParty/Dummy" ${FO_DUMMY_TARGETS})

# Print cached variables
if(FO_VERBOSE_BUILD)
    GetCMakeProperty(FO_CACHE_VARIABLES CACHE_VARIABLES)
    ListSort(FO_CACHE_VARIABLES)

    StatusMessage("Forced variables:")

    foreach(varName ${FO_CACHE_VARIABLES})
        GetCacheProperty(str ${varName} HELPSTRING)
        GetCacheProperty(type ${varName} TYPE)
        StringFind("${str}" "Forced by FOnline" forced)

        if(NOT "${forced}" STREQUAL "-1")
            StatusMessage("- ${varName}: '${${varName}}' type: '${type}'")
        endif()
    endforeach()

    StatusMessage("Default variables:")

    foreach(varName ${FO_CACHE_VARIABLES})
        GetCacheProperty(str ${varName} HELPSTRING)
        GetCacheProperty(type ${varName} TYPE)
        StringFind("${str}" "Forced by FOnline" forced)

        if("${forced}" STREQUAL "-1" AND
            NOT "${type}" STREQUAL "INTERNAL" AND
            NOT "${type}" STREQUAL "STATIC" AND
            NOT "${type}" STREQUAL "UNINITIALIZED")
            StatusMessage("- ${varName}: '${${varName}}' docstring: '${str}' type: '${type}'")
        endif()
    endforeach()
endif()
