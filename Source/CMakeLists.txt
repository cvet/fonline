cmake_minimum_required( VERSION 3.5 )
project( FOnlineProject )

#
# Preprocessor definitions
#

# set( FO_DEBUG YES )
set( BUILD_ONLY_CLIENT NO )
set( BUILD_CLIENT_LIBRARY NO )
set( CMAKE_VERBOSE_MAKEFILE ON CACHE STRING "" FORCE )

if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
	set( ARCH_STR "x64" )
else()
	set( ARCH_STR "x86" )
endif()
if( NOT FO_DEBUG )
	set( CONFIGURATION_NAME "Release" )
	set( CONFIGURATION_NAME_DESKTOP "RelWithDebInfo" )
else()
	set( CONFIGURATION_NAME "Debug" )
	set( CONFIGURATION_NAME_DESKTOP "Debug" )
endif()
set( CMAKE_CONFIGURATION_TYPES "${CONFIGURATION_NAME}" CACHE STRING "" FORCE )
set( CMAKE_BUILD_TYPE "${CONFIGURATION_NAME}" CACHE STRING "" FORCE )

if( WIN32 )
	set( OS "Windows" )
	set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/../Binaries/Client/${OS}" )
	if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		set( CLIENT_POSTFIX "64" )
	else()
		set( NON_CLIENT_POSTFIX "32" )
	endif()
	set( CMAKE_CONFIGURATION_TYPES "${CONFIGURATION_NAME_DESKTOP}" CACHE STRING "" FORCE )
	set( CMAKE_BUILD_TYPE "${CONFIGURATION_NAME_DESKTOP}" CACHE STRING "" FORCE )
	link_directories( "ThirdParty/fbxsdk-libs/${OS}/${CONFIGURATION_NAME}/${ARCH_STR}" )
	set( USE_GLEW YES )
elseif( ${CMAKE_SYSTEM_NAME} MATCHES "Linux" )
	set( OS "Linux" )
	set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/../Binaries/Client/${OS}" )
	if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		set( CLIENT_POSTFIX "64" )
		set( NON_CLIENT_POSTFIX "64" )
	else()
		set( CLIENT_POSTFIX "32" )
		set( NON_CLIENT_POSTFIX "32" )
	endif()
	set( CMAKE_CONFIGURATION_TYPES "${CONFIGURATION_NAME_DESKTOP}" CACHE STRING "" FORCE )
	set( CMAKE_BUILD_TYPE "${CONFIGURATION_NAME_DESKTOP}" CACHE STRING "" FORCE )
	find_package( X11 REQUIRED )
	find_package( OpenGL REQUIRED )
	set( RENDER_LIBS "GL" )
	set( USE_GLEW YES )
	link_directories( "ThirdParty/fbxsdk-libs/${OS}/${CONFIGURATION_NAME}/${ARCH_STR}" )
elseif( APPLE AND NOT IOS_PLATFORM )
	set( OS "Mac" )
	set( BUILD_ONLY_CLIENT YES )
	set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/Binaries/Client/${OS}" )
	set( CMAKE_CONFIGURATION_TYPES "${CONFIGURATION_NAME_DESKTOP}" CACHE STRING "" FORCE )
	set( CMAKE_BUILD_TYPE "${CONFIGURATION_NAME_DESKTOP}" CACHE STRING "" FORCE )
	find_package( OpenGL REQUIRED )
	set( RENDER_LIBS "${OPENGL_LIBRARIES}" )
	set( USE_GLEW YES )
elseif( APPLE AND IOS_PLATFORM )
	set( OS "iOS" )
	set( BUILD_ONLY_CLIENT YES )
	set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/Binaries/Client/${OS}" )
	find_library( OPENGLES OpenGLES )
	find_library( METAL Metal )
	find_library( COREGRAPGHICS CoreGraphics )
	find_library( QUARTZCORE QuartzCore )
	find_library( UIKIT UIKit )
	find_library( AVFOUNDATION AVFoundation )
	find_library( GAMECONTROLLER GameController )
	find_library( COREMOTION CoreMotion )
	set( CLIENT_LIBS ${OPENGLES} ${METAL} ${COREGRAPGHICS} ${QUARTZCORE} ${UIKIT} ${AVFOUNDATION} ${GAMECONTROLLER} ${COREMOTION} )
elseif( ANDROID )
	set( OS "Android" )
	set( BUILD_ONLY_CLIENT YES )
	set( BUILD_CLIENT_LIBRARY YES )
	set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/../Binaries/Client/${OS}/libs/${ANDROID_ABI}" )
	set( RENDER_LIBS "GLESv1_CM" "GLESv2" )
	set( CLIENT_LIBS "android" "log" )
elseif( EMSCRIPTEN )
	set( OS "Web" )
	set( BUILD_ONLY_CLIENT YES )
	set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/../Binaries/Client/${OS}" )
	set( CMAKE_EXECUTABLE_SUFFIX ".js" )
	set( WEB_OPTIONS "-s TOTAL_MEMORY=268435456" ) # 256 Mb
	set( WEB_OPTIONS "${WEB_OPTIONS} -s ALLOW_MEMORY_GROWTH=1" )
	set( WEB_OPTIONS "${WEB_OPTIONS} -s USE_WEBGL2=1" )
	set( WEB_OPTIONS "${WEB_OPTIONS} -s FORCE_FILESYSTEM=1" )
	set( WEB_OPTIONS "${WEB_OPTIONS} -s ENVIRONMENT='web'" )
	set( WEB_OPTIONS "${WEB_OPTIONS} -s DISABLE_EXCEPTION_CATCHING=1" )
	set( WEB_OPTIONS "${WEB_OPTIONS} -s NO_DYNAMIC_EXECUTION=1" )
	set( WEB_OPTIONS "${WEB_OPTIONS} -s USE_SDL_IMAGE=0" )
	set( WEB_OPTIONS "${WEB_OPTIONS} -s USE_SDL_TTF=0" )
	set( WEB_OPTIONS "${WEB_OPTIONS} -s USE_SDL_NET=0" )
	set( WEB_OPTIONS "${WEB_OPTIONS} -s NO_EXIT_RUNTIME=1" )
	set( WEB_OPTIONS "${WEB_OPTIONS} -s EXPORTED_RUNTIME_METHODS=\"['FS_createPath', 'FS_createDataFile']\"" )
	set( WEB_OPTIONS "${WEB_OPTIONS} -s EXTRA_EXPORTED_RUNTIME_METHODS=\"['Pointer_stringify', 'intArrayFromString', 'UTF8ToString', 'addRunDependency', 'removeRunDependency', 'stackTrace', 'getMemory']\"" )
	if( FO_DEBUG )
		set( WEB_OPTIONS "${WEB_OPTIONS} -O0" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -g3" )
		set( WEB_OPTIONS "${WEB_OPTIONS} --memory-init-file 1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s WASM=0" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s SAFE_HEAP=1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s ASSERTIONS=1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s STACK_OVERFLOW_CHECK=1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s GL_DEBUG=1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s GL_ASSERTIONS=1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s GL_TESTING=1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s FS_LOG=1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s WARN_UNALIGNED=1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s ERROR_ON_UNDEFINED_SYMBOLS=1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s ERROR_ON_MISSING_LIBRARIES=1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s DEMANGLE_SUPPORT=1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s EXCEPTION_DEBUG=1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s BINARYEN_TRAP_MODE='allow'" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s PRECISE_F32=1" )
	else()
		set( WEB_OPTIONS "${WEB_OPTIONS} -Oz" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -fno-rtti" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -fno-exceptions" )
		set( WEB_OPTIONS "${WEB_OPTIONS} --no-heap-copy" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s DISABLE_EXCEPTION_CATCHING=1" )
		set( WEB_OPTIONS "${WEB_OPTIONS} -s BINARYEN_TRAP_MODE='clamp'" )
		# Uncomment these optimizations later, they may give side effects
		# set( WEB_OPTIONS "${WEB_OPTIONS} --closure 1" ) - conflicts with NO_DYNAMIC_EXECUTION
		# set( WEB_OPTIONS "${WEB_OPTIONS} --llvm-lto 1" )
		# set( WEB_OPTIONS "${WEB_OPTIONS} -s INLINING_LIMIT=1" )
	endif()
	set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${WEB_OPTIONS}" )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WEB_OPTIONS}" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WEB_OPTIONS}" )
	add_definitions( "-DAS_MAX_PORTABILITY" )
	add_definitions( "-DWIP_16BYTE_ALIGN" )
else()
	message( FATAL_ERROR "Unknown OS!" )
endif()

add_definitions( "-DUNICODE" )
add_definitions( "-D_UNICODE" )

if( FO_DEBUG )
	set( CLIENT_POSTFIX "${CLIENT_POSTFIX}_Debug" )
	set( NON_CLIENT_POSTFIX "${NON_CLIENT_POSTFIX}_Debug" )
endif()

set( SERVER_OUTPUT "${CMAKE_BINARY_DIR}/../Binaries/Server" )
set( EDITOR_OUTPUT "${CMAKE_BINARY_DIR}/../Binaries/Editor" )

set( FO_OUTPUT_BINARIES_PATH "" CACHE PATH "Fixed output path" )
if( NOT "${FO_OUTPUT_BINARIES_PATH}" STREQUAL "" )
	set( CLIENT_OUTPUT "${FO_OUTPUT_BINARIES_PATH}/Client/${OS}" )
	set( SERVER_OUTPUT "${FO_OUTPUT_BINARIES_PATH}/Server" )
	set( EDITOR_OUTPUT "${FO_OUTPUT_BINARIES_PATH}/Editor" )
	message( STATUS "Output binaries: ${FO_OUTPUT_BINARIES_PATH}" )
endif()

if( MSVC_IDE )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++14" )
	set( COMPILER_DEFINES "-D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE -D_WINSOCK_DEPRECATED_NO_WARNINGS" )
	set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${COMPILER_DEFINES} /MTd /MP /RTCc /RTCsu" )
	set( CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} ${COMPILER_DEFINES} /MT /MP" )
	set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${COMPILER_DEFINES} /MT /MP" )
	set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${COMPILER_DEFINES} /MT /MP" )
	set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${COMPILER_DEFINES} /MTd /MP /RTCc /RTCsu" )
	set( CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} ${COMPILER_DEFINES} /MT /MP" )
	set( CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${COMPILER_DEFINES} /MT /MP" )
	set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${COMPILER_DEFINES} /MT /MP" )
	set( CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /INCREMENTAL:NO" )
	set( CMAKE_VS_PLATFORM_TOOLSET "v120_xp" CACHE STRING "Platform toolset" FORCE )
else()
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14" )
endif()

string( TOUPPER "${OS}" OS_UPPER )
set( SERVER_DEFINES "-DFONLINE_SERVER -DFO_${OS_UPPER}" )
set( CLIENT_DEFINES "-DFONLINE_CLIENT -DFO_${OS_UPPER}" )
set( EDITOR_DEFINES "-DFONLINE_EDITOR -DFO_${OS_UPPER}" )

#
# Embedded resources generation
#

function( CreateResources dir output)
	file( WRITE ${output} "" )
	file( GLOB bins ${dir}/*.zip )
	foreach( bin ${bins} )
		string( REGEX MATCH "([^/]+)$" filename ${bin} )
		string( REGEX REPLACE "\\.| " "_" filename ${filename} )
		file( READ ${bin} filedata HEX )
		string( REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " filedata ${filedata} )
		file( APPEND ${output} "const unsigned char Resource_${filename}[] = { ${filedata}};\n" )
	endforeach()
endfunction()
CreateResources( "Applications/EmbeddedResources" "Applications/EmbeddedResources/Resources.h" )

#
# Sources
#

function( DisableLibWarnings lib )
	target_compile_options( ${lib} PRIVATE
		$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-Wall>
		$<$<CXX_COMPILER_ID:MSVC>:/W0> )
endfunction()

# SDL2
set( SDL_DIR "ThirdParty/SDL2" )
set( SDL_SHARED OFF CACHE STRING "" FORCE )
set( SDL_STATIC ON CACHE STRING "" FORCE )
set( RENDER_D3D OFF CACHE STRING "" FORCE )
set( LIBC ON CACHE STRING "" FORCE )
set( SDL_AUDIO ON CACHE STRING "" FORCE )
set( SDL_VIDEO ON CACHE STRING "" FORCE )
set( SDL_RENDER ON CACHE STRING "" FORCE )
set( SDL_EVENTS ON CACHE STRING "" FORCE )
set( SDL_LOADSO ON CACHE STRING "" FORCE )
set( SDL_ATOMIC OFF CACHE STRING "" FORCE )
set( SDL_JOYSTICK OFF CACHE STRING "" FORCE )
set( SDL_HAPTIC OFF CACHE STRING "" FORCE )
set( SDL_POWER ON CACHE STRING "" FORCE )
set( SDL_THREADS ON CACHE STRING "" FORCE )
set( SDL_TIMERS ON CACHE STRING "" FORCE )
set( SDL_FILE ON CACHE STRING "" FORCE )
set( SDL_CPUINFO OFF CACHE STRING "" FORCE )
set( SDL_FILESYSTEM OFF CACHE STRING "" FORCE )
set( SDL_DLOPEN ON CACHE STRING "" FORCE )
if( ANDROID )
	set( SDL_JOYSTICK ON CACHE STRING "" FORCE )
	set( PTHREADS OFF CACHE STRING "" FORCE )
endif()
if( APPLE AND IOS_PLATFORM )
	set( SDL_JOYSTICK ON CACHE STRING "" FORCE )
	set( SDL_HAPTIC ON CACHE STRING "" FORCE )
endif()
add_definitions( "-DGL_GLEXT_PROTOTYPES" )
add_subdirectory( "${SDL_DIR}" )
include_directories( "${SDL_DIR}/include" )
DisableLibWarnings( SDL2main )
DisableLibWarnings( SDL2-static )

# Zlib
set( ZLIB_DIR "ThirdParty/zlib" )
add_subdirectory( "${ZLIB_DIR}" )
include_directories( "${ZLIB_DIR}" "${ZLIB_DIR}/contrib" "${CMAKE_BINARY_DIR}/${ZLIB_DIR}" )
DisableLibWarnings( zlibstatic )

# PNG
if( NOT BUILD_ONLY_CLIENT )
	set( PNG_DIR "ThirdParty/PNG" )
	set( SKIP_INSTALL_ALL ON CACHE STRING "" FORCE )
	set( PNG16 "png16_static" )
	set( ZLIB_LIBRARY "zlibstatic" CACHE STRING "" FORCE )
	set( ZLIB_INCLUDE_DIR "../${ZLIB_DIR}" "${CMAKE_BINARY_DIR}/${ZLIB_DIR}" CACHE STRING "" FORCE )
	set( PNG_SHARED OFF CACHE STRING "" FORCE )
	set( PNG_STATIC ON CACHE STRING "" FORCE )
	add_subdirectory( "${PNG_DIR}" )
	include_directories( "${PNG_DIR}" "${CMAKE_BINARY_DIR}/${PNG_DIR}" )
	DisableLibWarnings( png16_static )
endif()

# Ogg
set( OGG_DIR "ThirdParty/ogg" )
file( GLOB OGG_SOURCE "${OGG_DIR}/src/*.c" )
include_directories( "${OGG_DIR}/include" )
add_library( Ogg ${OGG_SOURCE} )
DisableLibWarnings( Ogg )

# Vorbis
set( VORBIS_DIR "ThirdParty/Vorbis" )
file( GLOB VORBIS_SOURCE "${VORBIS_DIR}/lib/*.c" )
list( REMOVE_ITEM VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${VORBIS_DIR}/lib/barkmel.c" )
list( REMOVE_ITEM VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${VORBIS_DIR}/lib/psytune.c" )
list( REMOVE_ITEM VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${VORBIS_DIR}/lib/tone.c" )
include_directories( "${VORBIS_DIR}/include" )
include_directories( "${VORBIS_DIR}/lib" )
add_library( Vorbis ${VORBIS_SOURCE} )
DisableLibWarnings( Vorbis )

# Theora
set( THEORA_DIR "ThirdParty/Theora" )
file( GLOB THEORA_SOURCE "${THEORA_DIR}/lib/*.c" )
list( REMOVE_ITEM THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${THEORA_DIR}/lib/analyze.c" )
list( REMOVE_ITEM THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${THEORA_DIR}/lib/encode.c" )
list( REMOVE_ITEM THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${THEORA_DIR}/lib/encapiwrapper.c" )
source_group( "ThirdParty\\theora" FILES ${THEORA_SOURCE} )
include_directories( "${THEORA_DIR}/include" )
add_library( Theora ${THEORA_SOURCE} )
DisableLibWarnings( Theora )

# AngelScript
set( ANGEL_SCRIPT_DIR "ThirdParty/AngelScript" )
add_definitions( "-DAS_NO_EXCEPTIONS -DAS_NO_THREADS" )
add_subdirectory( "${ANGEL_SCRIPT_DIR}/sdk/angelscript/projects/cmake" )
include_directories( "${ANGEL_SCRIPT_DIR}/sdk/angelscript/include" "${ANGEL_SCRIPT_DIR}/sdk/angelscript/source" "${ANGEL_SCRIPT_DIR}/sdk/add_on" )
DisableLibWarnings( Angelscript )

# AngelScriptExt
set( ANGEL_SCRIPT_EXT_DIR "Common/AngelScriptExt" )
add_subdirectory( "${ANGEL_SCRIPT_EXT_DIR}" )

# Acm
set( ACM_DIR "ThirdParty/Acm" )
add_subdirectory( "${ACM_DIR}" )
DisableLibWarnings( Acm )

# SHA
set( SHA_DIR "ThirdParty/SHA" )
add_subdirectory( "${SHA_DIR}" )
DisableLibWarnings( SHA )

# GLEW
if( USE_GLEW )
	set( GLEW_DIR "ThirdParty/GL" )
	file( GLOB GLEW_SOURCE "${GLEW_DIR}/*.*" )
	source_group( "ThirdParty\\GLEW" FILES ${GLEW_SOURCE} )
	add_definitions( "-DGLEW_STATIC" )
endif()

# NCodeHook
if( WIN32 )
	set( NCODE_HOOK_DIR "ThirdParty/NCodeHook" )
	add_subdirectory( "${NCODE_HOOK_DIR}" )
	set( NCODEHOOK "NCodeHook" )
	DisableLibWarnings( NCodeHook )
endif()

# Assimp
if( NOT BUILD_ONLY_CLIENT )
	set( ASSIMP_DIR "ThirdParty/Assimp" )
	set( BUILD_SHARED_LIBS OFF CACHE STRING "" FORCE )
	set( ASSIMP_NO_EXPORT ON CACHE STRING "" FORCE )
	set( ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE STRING "" FORCE )
	set( ASSIMP_BUILD_ASSIMP_VIEW OFF CACHE STRING "" FORCE )
	set( ASSIMP_BUILD_SAMPLES OFF CACHE STRING "" FORCE )
	set( ASSIMP_BUILD_TESTS OFF CACHE STRING "" FORCE )
	set( ASSIMP_OPT_BUILD_PACKAGES OFF CACHE STRING "" FORCE )
	set( ASSIMP_INSTALL_PDB OFF CACHE STRING "" FORCE )
	add_subdirectory( "${ASSIMP_DIR}" )
	include_directories( "${CMAKE_BINARY_DIR}/${ASSIMP_DIR}/include" )
	DisableLibWarnings( assimp )
	DisableLibWarnings( IrrXML )
endif()
include_directories( "${ASSIMP_DIR}/include" )

# Fbx SDK
set( FBXSDK_DIR "ThirdParty/fbxsdk" )
file( GLOB fbxsdk_SOURCE "${FBXSDK_DIR}/*.*" )
source_group( "ThirdParty\\fbxsdk" FILES ${fbxsdk_SOURCE} )
if( WIN32 )
	set( FBXSDK "libfbxsdk-mt" )
else()
	set( FBXSDK "libfbxsdk.a")
endif()

# cURL & mbedTLS
if( NOT BUILD_ONLY_CLIENT )
	set( MBED_TLS_DIR "ThirdParty/mbedTLS" )
	set( USE_STATIC_MBEDTLS_LIBRARY ON CACHE STRING "" FORCE )
	set( ENABLE_PROGRAMS OFF CACHE STRING "" FORCE )
	set( ENABLE_TESTING OFF CACHE STRING "" FORCE )
	add_subdirectory( "${MBED_TLS_DIR}" )
	DisableLibWarnings( mbedcrypto )
	DisableLibWarnings( mbedx509 )
	DisableLibWarnings( mbedtls )

	set( CURL_DIR "ThirdParty/cURL" )
	set( CURL_STATICLIB ON CACHE STRING "" FORCE )
	set( CURL_CA_PATH "none" CACHE STRING "" FORCE )
	set( BUILD_CURL_EXE OFF CACHE STRING "" FORCE )
	set( BUILD_TESTING OFF CACHE STRING "" FORCE )
	set( CMAKE_USE_MBEDTLS ON CACHE STRING "" FORCE )
	set( MBEDTLS_LIBRARIES "mbedcrypto;mbedtls;mbedx509" CACHE STRING "" FORCE )
	set( MBEDTLS_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/${MBED_TLS_DIR}/include" CACHE STRING "" FORCE )
	add_definitions( "-DCURL_STATICLIB" )
	add_subdirectory( "${CURL_DIR}" )
	include_directories( "${CURL_DIR}/include" )
	DisableLibWarnings( libcurl )
endif()

# Nlohmann Json
set( JSON_DIR "ThirdParty/Json" )
file( GLOB Json_SOURCE "${JSON_DIR}/*.*" )
source_group( "ThirdParty\\Json" FILES ${Json_SOURCE} )

# Fmt
set( FMT_DIR "ThirdParty/fmt" )
file( GLOB fmt_SOURCE "${FMT_DIR}/fmt/*.*" )
source_group( "ThirdParty\\fmt" FILES ${fmt_SOURCE} )

# Asio & Websockets
if( NOT BUILD_ONLY_CLIENT )
	set( ASIO_DIR "ThirdParty/Asio" )
	include_directories( "${ASIO_DIR}/include" )

	set( WEBSOCKETS_DIR "ThirdParty/websocketpp" )
	include_directories( "${WEBSOCKETS_DIR}" )
endif()

# MongoDB C driver & Bson
if( NOT BUILD_ONLY_CLIENT )
	set( BSON_DIR "ThirdParty/libbson" )
	set( ENABLE_STATIC ON CACHE STRING "" FORCE )
	set( BSON_ENABLE_STATIC ON CACHE STRING "" FORCE )
	add_subdirectory( "${BSON_DIR}" )
	set( BSON_INCLUDE_DIR "${CMAKE_BINARY_DIR}/${BSON_DIR}/src/bson"
		"${CMAKE_CURRENT_SOURCE_DIR}/${BSON_DIR}/src/bson" CACHE STRING "" FORCE )
	set( BSON_LIBRARIES "bson_static" CACHE STRING "" FORCE )
	set( BSON_STATIC_LIBRARIES "bson_static" CACHE STRING "" FORCE )
	include_directories( "${BSON_INCLUDE_DIR}" )
	target_compile_definitions( bson_static PUBLIC BSON_COMPILATION BSON_STATIC JSONSL_PARSE_NAN )
	DisableLibWarnings( bson_static )

	set( MONGODB_DIR "ThirdParty/mongo-c-driver" )
	set( ENABLE_SASL "OFF" CACHE STRING "" FORCE )
	set( MONGOC_ENABLE_STATIC ON CACHE STRING "" FORCE )
	add_subdirectory( "${MONGODB_DIR}" )
	set( MONGOC_INCLUDE_DIR "${CMAKE_BINARY_DIR}/${MONGODB_DIR}/src/mongoc"
		"${CMAKE_CURRENT_SOURCE_DIR}/${MONGODB_DIR}/src/mongoc" CACHE STRING "" FORCE )
	include_directories( "${MONGOC_INCLUDE_DIR}" )
	DisableLibWarnings( mongoc_static )
endif()

# Unqlite
if( NOT EMSCRIPTEN )
	set( UNQLITE_DIR "ThirdParty/unqlite" )
	add_subdirectory( "${UNQLITE_DIR}" )
	include_directories( "${UNQLITE_DIR}" )
	set( UNQLITE "unqlite" )
	DisableLibWarnings( unqlite )
endif()

# Variant
if( NOT BUILD_ONLY_CLIENT )
	set( VARIANT_DIR "ThirdParty/variant" )
	include_directories( "${VARIANT_DIR}/include" )
endif()

# OpenSSL
if( NOT BUILD_ONLY_CLIENT )
	set( OPENSSL_DIR "ThirdParty/openssl" )
	set( BUILD_OBJECT_LIBRARY_ONLY ON CACHE BOOL "" FORCE )
	add_subdirectory( "${OPENSSL_DIR}" )
	include_directories( "${CMAKE_BINARY_DIR}/${OPENSSL_DIR}/crypto" )
	include_directories( "${CMAKE_BINARY_DIR}/${OPENSSL_DIR}/ssl" )
	DisableLibWarnings( crypto )
	DisableLibWarnings( ssl )
endif()

# Dear ImGui
set( DEAR_IMGUI_DIR "ThirdParty/imgui" )
file( GLOB IMGUI_SOURCE "${DEAR_IMGUI_DIR}/*.cpp" "${DEAR_IMGUI_DIR}/*.h" )
#list( REMOVE_ITEM IMGUI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${DEAR_IMGUI_DIR}/imgui_demo.cpp" )
#list( REMOVE_ITEM IMGUI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${DEAR_IMGUI_DIR}/imgui_demo.h" )
source_group( "ThirdParty\\imgui" FILES ${IMGUI_SOURCE} )
include_directories( "${DEAR_IMGUI_DIR}" )
add_definitions( "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS" )

# Common source
set( COMMON_SOURCE
	${IMGUI_SOURCE}
	${fmt_SOURCE}

	Common/Common.cpp Common/Common.h # Clean up from different things
	Common/Entity.cpp Common/Entity.h
	Common/ProtoMap.cpp # Remove
	Common/Exception.cpp Common/Exception.h
	Common/Debugger.cpp Common/Debugger.h # Delete?
	Common/Log.cpp Common/Log.h
	Common/IniParser.cpp Common/IniParser.h # Rename (IniFile)
	Common/MsgFiles.cpp Common/MsgFiles.h

	Common/FileSystem.cpp Common/FileSystem.h
	Common/FileManager.cpp Common/FileManager.h # Rename (File)
	# FilesCollection -> ProjectFiles?
	Common/DataFile.cpp Common/DataFile.h

	Common/GraphicApi.cpp Common/GraphicApi.h
	Common/GraphicStructures.cpp Common/GraphicStructures.h

	Common/Properties.cpp Common/Properties.h
	Common/ProtoManager.cpp Common/ProtoManager.h

	Common/ScriptBind.h
	Common/ScriptFunctions.h
	Common/Script.cpp Common/Script.h
	Common/ScriptExtensions.cpp
	Common/ScriptInvoker.cpp Common/ScriptInvoker.h
	Common/ScriptPragmas.cpp Common/ScriptPragmas.h
	Common/ScriptProfiler.cpp Common/ScriptProfiler.h
	Common/DummyData.h # Rename (ScriptReference) or include to ScriptBind.h

	Common/NetProtocol.h # Include to NetBuffer?
	Common/BufferManager.cpp Common/BufferManager.h # Rename (NetBuffer)
	Common/UcsTables.h
	Common/Text.cpp Common/Text.h # Rename (GameTexts, StringExt)
	Common/Threading.cpp Common/Threading.h # Use C++ threads lib?
	Common/Timer.cpp Common/Timer.h # Rename (Timing)
	Common/LineTracer.h Common/LineTracer.cpp

	# Refactor
	${GLEW_SOURCE} # Remove, use dynamic binding
	Common/Crypt.cpp Common/Crypt.h # Split to DataStorage/LocalStorage
	Common/Access.h # To NetProtocol.h?
	Common/MsgStr.h # To Common.h?
	Common/3dAnimation.h # To Graphic...?
	Common/PlatformSpecific.h # To Common.h
	Common/Types.h # To Common.h
	Common/Randomizer.h # To Common.h
	Common/DataMask.h # To Common.h?
	Common/Defines.h # To Common.h
)

# Server source
set( SERVER_SOURCE
	${COMMON_SOURCE}
	${Json_SOURCE}
	${fbxsdk_SOURCE}

	# Location
	Server/Map.cpp Server/Map.h
	Server/Critter.cpp Server/Critter.h
	Server/Item.cpp Server/Item.h

	# To Server.h?
	Server/EntityManager.cpp Server/EntityManager.h
	# LocationManager
	Server/MapManager.cpp Server/MapManager.h
	Server/CritterManager.cpp Server/CritterManager.h
	Server/ItemManager.cpp Server/ItemManager.h

	Server/AppGui.cpp Server/AppGuiDX.cpp Server/AppGui.h
	Server/ResourceConverter.cpp Server/ResourceConverter.h
	Server/DataBase.cpp Server/DataBase.h
	Server/Dialogs.cpp Server/Dialogs.h
	Server/Networking.cpp Server/Networking.h

	Server/Server.cpp Server/Server.h
	Server/ServerClient.cpp # Refactor
	Server/ServerItem.cpp # Refactor
	Server/ServerNpc.cpp # Refactor
	Server/ServerScript.cpp
)

# Client source
set( CLIENT_SOURCE
	${COMMON_SOURCE}

	Client/Client.cpp Client/Client.h
	Client/ClientInterface.cpp # Remove
	Client/SoundManager.cpp Client/SoundManager.h

	# Cl to View?
	Client/MapCl.cpp Client/MapCl.h
	Client/CritterCl.cpp Client/CritterCl.h
	Client/ItemCl.cpp Client/ItemCl.h
	Client/ItemHex.cpp Client/ItemHex.h # Merge with ItemCl or ItemHexView

	Client/Keyboard.cpp Client/Keyboard.h
	Client/HexManager.cpp Client/HexManager.h
	Client/ResourceManager.cpp Client/ResourceManager.h
	Client/GraphicLoader.cpp Client/GraphicLoader.h # Refactor, merge with ResourceManager?

	Client/3dStuff.cpp Client/3dStuff.h
	Client/F2Palette.h
	Client/SpriteManager.cpp Client/SpriteManager.h # Refactor, split loaders
	Client/SpriteManagerFont.cpp
	Client/Sprites.cpp Client/Sprites.h

	Client/GluStuff.cpp Client/GluStuff.h # Remove
	Client/FlexRect.h # Move to Common?
)

# Editor source
set( EDITOR_SOURCE
	${CLIENT_SOURCE}
	${SERVER_SOURCE}
	Mapper/Mapper.cpp Mapper/Mapper.h
)
list( REMOVE_DUPLICATES EDITOR_SOURCE )

# Basic includes
include_directories( "Common" )
include_directories( "Server" )
include_directories( "Client" )
include_directories( "Mapper" )
include_directories( "ThirdParty" )
include_directories( "Applications" )

#
# Applications
#

set( RO0 RUNTIME_OUTPUT_DIRECTORY )
set( RO1 RUNTIME_OUTPUT_DIRECTORY_DEBUG )
set( RO2 RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL )
set( RO3 RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO )
set( RO4 RUNTIME_OUTPUT_DIRECTORY_RELEASE )

if( NOT BUILD_CLIENT_LIBRARY )
	add_executable( FOnline WIN32 ${CLIENT_SOURCE} "Applications/MainClient.cpp" "Applications/Client.rc" )
	# Todo: Make bundles for Mac and maybe iOS
	# add_executable( FOnline MACOSX_BUNDLE ${CLIENT_SOURCE} "Client.rc" )
	set_target_properties( FOnline PROPERTIES ${RO0} ${CLIENT_OUTPUT} ${RO1} ${CLIENT_OUTPUT} ${RO2} ${CLIENT_OUTPUT} ${RO3} ${CLIENT_OUTPUT} ${RO4} ${CLIENT_OUTPUT} )
else()
	add_library( FOnline SHARED ${CLIENT_SOURCE} "Applications/MainClient.cpp" )
	set_target_properties( FOnline PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CLIENT_OUTPUT} )
endif()
set_target_properties( FOnline PROPERTIES OUTPUT_NAME "FOnline${CLIENT_POSTFIX}" COMPILE_FLAGS "${CLIENT_DEFINES}" )
target_link_libraries( FOnline "Angelscript" "AngelscriptExt" "Ogg" "Vorbis" "Theora" "zlibstatic" "${PNG16}" "SDL2main" "SDL2-static" "Acm" "SHA" "${NCODEHOOK}" "${UNQLITE}" "${RENDER_LIBS}" "${CMAKE_DL_LIBS}" "${CLIENT_LIBS}" )

if( NOT BUILD_ONLY_CLIENT )
	add_executable( FOnlineServer WIN32 ${SERVER_SOURCE} "Applications/MainServer.cpp" "Applications/Server.rc" )
	set_target_properties( FOnlineServer PROPERTIES ${RO0} ${SERVER_OUTPUT} ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
	set_target_properties( FOnlineServer PROPERTIES OUTPUT_NAME "FOnlineServer${NON_CLIENT_POSTFIX}" COMPILE_FLAGS "${SERVER_DEFINES}" )
	target_link_libraries( FOnlineServer "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SDL2main" "SDL2-static" "SHA" "libcurl" "${NCODEHOOK}" "${FBXSDK}" "assimp" "${RENDER_LIBS}" "mongoc_static" "bson_static" "${UNQLITE}" "ssl" "crypto" "${CMAKE_DL_LIBS}" )

	if( NOT WIN32 )
		set( SERVER_SOURCE_NO_GUI ${SERVER_SOURCE} )
		list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "${CMAKE_CURRENT_SOURCE_DIR}/Common/GraphicApi.h" )
		list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "${CMAKE_CURRENT_SOURCE_DIR}/Common/GraphicApi.cpp" )
		list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "${CMAKE_CURRENT_SOURCE_DIR}/Server/AppGui.h" )
		list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "${CMAKE_CURRENT_SOURCE_DIR}/Server/AppGui.cpp" )
		list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "${CMAKE_CURRENT_SOURCE_DIR}/Server/AppGuiDX.cpp" )
		list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "${GLEW_SOURCE}" )
		add_executable( FOnlineServerDaemon ${SERVER_SOURCE_NO_GUI} "Applications/MainServer.cpp" "Applications/Server.rc" )
		set_target_properties( FOnlineServerDaemon PROPERTIES ${RO0} ${SERVER_OUTPUT} ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
		set_target_properties( FOnlineServerDaemon PROPERTIES OUTPUT_NAME "FOnlineServerDaemon${NON_CLIENT_POSTFIX}" COMPILE_FLAGS "${SERVER_DEFINES} -DFO_SERVER_DAEMON" )
		target_link_libraries( FOnlineServerDaemon "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SHA" "libcurl" "${NCODEHOOK}" "${FBXSDK}" "assimp" "mongoc_static" "bson_static" "${UNQLITE}" "ssl" "crypto" "${CMAKE_DL_LIBS}" )
	endif()

	add_executable( FOnlineEditor WIN32 ${EDITOR_SOURCE} "Applications/MainEditor.cpp" "Applications/Editor.rc" )
	set_target_properties( FOnlineEditor PROPERTIES ${RO0} ${EDITOR_OUTPUT} ${RO1} ${EDITOR_OUTPUT} ${RO2} ${EDITOR_OUTPUT} ${RO3} ${EDITOR_OUTPUT} ${RO4} ${EDITOR_OUTPUT} )
	set_target_properties( FOnlineEditor PROPERTIES OUTPUT_NAME "FOnlineEditor${NON_CLIENT_POSTFIX}" COMPILE_FLAGS "${EDITOR_DEFINES}" )
	target_link_libraries( FOnlineEditor "Angelscript" "AngelscriptExt" "Ogg" "Vorbis" "Theora" "zlibstatic" "${PNG16}" "SDL2main" "SDL2-static" "Acm" "SHA" "${NCODEHOOK}" "${UNQLITE}" "${FBXSDK}" "assimp" "${RENDER_LIBS}" "ssl" "crypto" "mongoc_static" "bson_static" "libcurl" "${CMAKE_DL_LIBS}" )

	set_property( GLOBAL PROPERTY USE_FOLDERS ON )
	set_property( TARGET FOnlineServer FOnline FOnlineEditor PROPERTY FOLDER "Binaries" )
	set_property( TARGET Acm Angelscript AngelscriptExt Ogg Vorbis Theora ${NCODEHOOK} png16_static SDL2main SDL2-static SHA zlibstatic libcurl assimp IrrXML mongoc_static bson_static ${UNQLITE} mbedcrypto mbedtls mbedx509 ssl crypto PROPERTY FOLDER "Libraries" )
endif()
