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
	link_directories( "${CMAKE_CURRENT_SOURCE_DIR}/Lib/${OS}/${CONFIGURATION_NAME}/${ARCH_STR}" )
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
	link_directories( "${CMAKE_CURRENT_SOURCE_DIR}/Lib/${OS}/${CONFIGURATION_NAME}/${ARCH_STR}" )
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
	if( CMAKE_COMPILER_IS_GNUCXX )
		set_source_files_properties( "${CMAKE_CURRENT_SOURCE_DIR}/Debugger.cpp" PROPERTIES COMPILE_FLAGS "-Wdeprecated-declarations" )
	endif()
endif()

string( TOUPPER "${OS}" OS_UPPER )
set( SERVER_DEFINES "-DFONLINE_SERVER -DFO_${OS_UPPER}" )
set( CLIENT_DEFINES "-DFONLINE_CLIENT -DFO_${OS_UPPER}" )
set( EDITOR_DEFINES "-DFONLINE_EDITOR -DFO_${OS_UPPER}" )

#
# Resources generation
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
CreateResources( "Resources" "Resources/Resources.h" )

#
# Sources
#

set( SKIP_INSTALL_ALL ON )

set( SDL_DIR "SDL2" ) # SDL2-2.0.3
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

set( ZLIB_DIR "zlib" ) # zlib-1.2.8
add_subdirectory( "${ZLIB_DIR}" )
include_directories( "${ZLIB_DIR}" "${ZLIB_DIR}/contrib" "${CMAKE_BINARY_DIR}/${ZLIB_DIR}" )

if( NOT BUILD_ONLY_CLIENT )
	set( PNG16 "png16_static" )
	set( PNG_DIR "PNG" ) # lpng1616
	set( ZLIB_LIBRARY "zlibstatic" CACHE STRING "" FORCE )
	set( ZLIB_INCLUDE_DIR "../${ZLIB_DIR}" "${CMAKE_BINARY_DIR}/${ZLIB_DIR}" CACHE STRING "" FORCE )
	set( PNG_SHARED OFF CACHE STRING "" FORCE )
	set( PNG_STATIC ON CACHE STRING "" FORCE )
	add_subdirectory( "${PNG_DIR}" )
	include_directories( "${PNG_DIR}" "${CMAKE_BINARY_DIR}/${PNG_DIR}" )
endif()

set( OGG_DIR "ogg" ) # libogg-1.3.1
file( GLOB OGG_SOURCE "${OGG_DIR}/src/*.c" )
source_group( "ThirdParty\\ogg" FILES ${OGG_SOURCE} )
include_directories( "${OGG_DIR}/include" )

set( VORBIS_DIR "Vorbis" ) # libvorbis-1.3.3
file( GLOB VORBIS_SOURCE "${VORBIS_DIR}/lib/*.c" )
list( REMOVE_ITEM VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${VORBIS_DIR}/lib/barkmel.c" )
list( REMOVE_ITEM VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${VORBIS_DIR}/lib/psytune.c" )
list( REMOVE_ITEM VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${VORBIS_DIR}/lib/tone.c" )
source_group( "ThirdParty\\vorbis" FILES ${VORBIS_SOURCE} )
include_directories( "${VORBIS_DIR}/include" )
include_directories( "${VORBIS_DIR}/lib" )

set( THEORA_DIR "Theora" ) # libtheora-1.1.1
file( GLOB THEORA_SOURCE "${THEORA_DIR}/lib/*.c" )
list( REMOVE_ITEM THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${THEORA_DIR}/lib/analyze.c" )
list( REMOVE_ITEM THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${THEORA_DIR}/lib/encode.c" )
list( REMOVE_ITEM THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${THEORA_DIR}/lib/encapiwrapper.c" )
source_group( "ThirdParty\\theora" FILES ${THEORA_SOURCE} )
include_directories( "${THEORA_DIR}/include" )

add_definitions( "-DAS_NO_EXCEPTIONS -DAS_NO_THREADS" )
set( ANGEL_SCRIPT_DIR "AngelScript" )
add_subdirectory( "${ANGEL_SCRIPT_DIR}/sdk/angelscript/projects/cmake" )
add_subdirectory( "${ANGEL_SCRIPT_DIR}" )
include_directories( "${ANGEL_SCRIPT_DIR}/sdk/angelscript/include" "${ANGEL_SCRIPT_DIR}" )

add_subdirectory( "Acm" )
add_subdirectory( "SHA" )

if( USE_GLEW )
	file( GLOB GLEW_SOURCE "GL/*.*" )
	source_group( "ThirdParty\\GLEW" FILES ${GLEW_SOURCE} )
endif()

if( WIN32 )
	add_subdirectory( "NCodeHook" )
	set( NCODEHOOK "NCodeHook" )
endif()

if( NOT BUILD_ONLY_CLIENT )
	set( BUILD_SHARED_LIBS OFF CACHE STRING "" FORCE )
	set( ASSIMP_NO_EXPORT ON CACHE STRING "" FORCE )
	set( ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE STRING "" FORCE )
	set( ASSIMP_BUILD_ASSIMP_VIEW OFF CACHE STRING "" FORCE )
	set( ASSIMP_BUILD_SAMPLES OFF CACHE STRING "" FORCE )
	set( ASSIMP_BUILD_TESTS OFF CACHE STRING "" FORCE )
	set( ASSIMP_OPT_BUILD_PACKAGES OFF CACHE STRING "" FORCE )
	set( ASSIMP_INSTALL_PDB OFF CACHE STRING "" FORCE )
	add_subdirectory( "Assimp" )
	include_directories( "${CMAKE_BINARY_DIR}/Assimp/include" )
endif()
include_directories( "Assimp/include" )

file( GLOB fbxsdk_SOURCE "fbxsdk/*.*" )
source_group( "ThirdParty\\fbxsdk" FILES ${fbxsdk_SOURCE} )
if( WIN32 )
	set( FBXSDK "libfbxsdk-mt" )
else()
	set( FBXSDK "libfbxsdk.a")
endif()

if( NOT BUILD_ONLY_CLIENT )
	# mbed TLS 2.13.0
	set( USE_STATIC_MBEDTLS_LIBRARY ON CACHE STRING "" FORCE )
	set( ENABLE_PROGRAMS OFF CACHE STRING "" FORCE )
	set( ENABLE_TESTING OFF CACHE STRING "" FORCE )
	add_subdirectory( "mbedTLS" )

	set( CURL_DIR "cURL" ) # curl-7.57.0
	set( CURL_STATICLIB ON CACHE STRING "" FORCE )
	set( CURL_CA_PATH "none" CACHE STRING "" FORCE )
	set( BUILD_CURL_EXE OFF CACHE STRING "" FORCE )
	set( BUILD_TESTING OFF CACHE STRING "" FORCE )
	set( CMAKE_USE_MBEDTLS ON CACHE STRING "" FORCE )
	set( MBEDTLS_LIBRARIES "mbedcrypto;mbedtls;mbedx509" CACHE STRING "" FORCE )
	set( MBEDTLS_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/mbedTLS/include" CACHE STRING "" FORCE )
	add_definitions( "-DCURL_STATICLIB" )
	add_subdirectory( "cURL" )
	include_directories( "${CURL_DIR}/include" )
endif()

file( GLOB Json_SOURCE "Json/*.*" )
source_group( "ThirdParty\\Json" FILES ${Json_SOURCE} )

file( GLOB fmt_SOURCE "fmt/fmt/*.*" )
source_group( "ThirdParty\\fmt" FILES ${fmt_SOURCE} )

if( NOT BUILD_ONLY_CLIENT )
	include_directories( "Asio/include" )
	include_directories( "websocketpp" )
endif()

# MongoDB C driver
if( NOT BUILD_ONLY_CLIENT )
	set( ENABLE_STATIC ON CACHE STRING "" FORCE )
	set( BSON_ENABLE_STATIC ON CACHE STRING "" FORCE )
	add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/libbson" )
	set( BSON_INCLUDE_DIR "${CMAKE_BINARY_DIR}/libbson/src/bson"
		"${CMAKE_CURRENT_SOURCE_DIR}/libbson/src/bson" CACHE STRING "" FORCE )
	set( BSON_LIBRARIES "bson_static" CACHE STRING "" FORCE )
	set( BSON_STATIC_LIBRARIES "bson_static" CACHE STRING "" FORCE )
	include_directories( "${BSON_INCLUDE_DIR}" )
	target_compile_definitions( bson_static PUBLIC BSON_COMPILATION BSON_STATIC JSONSL_PARSE_NAN )

	set( ENABLE_SASL "OFF" CACHE STRING "" FORCE )
	set( MONGOC_ENABLE_STATIC ON CACHE STRING "" FORCE )
	add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/mongo-c-driver" )
	set( MONGOC_INCLUDE_DIR "${CMAKE_BINARY_DIR}/mongo-c-driver/src/mongoc"
		"${CMAKE_CURRENT_SOURCE_DIR}/mongo-c-driver/src/mongoc" CACHE STRING "" FORCE )
	include_directories( "${MONGOC_INCLUDE_DIR}" )
endif()

if( NOT EMSCRIPTEN )
	add_subdirectory( "unqlite" )
	include_directories( "unqlite" )
	set( UNQLITE "unqlite" )
endif()

if( NOT BUILD_ONLY_CLIENT )
	include_directories( "variant/include" )
endif()

if( NOT BUILD_ONLY_CLIENT )
	set( BUILD_OBJECT_LIBRARY_ONLY ON CACHE BOOL "" FORCE )
	add_subdirectory( "openssl" )
	include_directories( "${CMAKE_BINARY_DIR}/openssl/crypto" )
	include_directories( "${CMAKE_BINARY_DIR}/openssl/ssl" )
endif()

if( NOT BUILD_ONLY_CLIENT )
	set( wxBUILD_SHARED OFF CACHE BOOL "" FORCE )
	set( wxBUILD_TESTS OFF CACHE STRING "" FORCE )
	set( wxBUILD_SAMPLES OFF CACHE STRING "" FORCE )
	set( wxBUILD_DEMOS OFF CACHE BOOL "" FORCE )
	set( wxUSE_GUI ON CACHE BOOL "" FORCE )
	set( wxBUILD_COMPATIBILITY "3.0" CACHE STRING "" FORCE )
	set( wxBUILD_PRECOMP OFF CACHE BOOL "" FORCE )
	set( wxBUILD_MONOLITHIC OFF CACHE BOOL "" FORCE )
	set( wxBUILD_USE_STATIC_RUNTIME ON CACHE BOOL "" FORCE )
	set( wxBUILD_INSTALL OFF CACHE BOOL "" FORCE )
	set( wxUSE_FS_INET OFF CACHE BOOL "" FORCE )
	set( wxUSE_IPV6 OFF CACHE BOOL "" FORCE )
	set( wxUSE_PROTOCOL OFF CACHE BOOL "" FORCE )
	set( wxUSE_PROTOCOL_FILE OFF CACHE BOOL "" FORCE )
	set( wxUSE_PROTOCOL_FTP OFF CACHE BOOL "" FORCE )
	set( wxUSE_PROTOCOL_HTTP OFF CACHE BOOL "" FORCE )
	set( wxUSE_SOCKETS OFF CACHE BOOL "" FORCE )
	set( wxUSE_URL OFF CACHE BOOL "" FORCE )
	add_subdirectory( "wxWidgets" )
	include_directories( "wxWidgets/include" )
	if( MSVC_IDE )
		include_directories( "wxWidgets/include/msvc" )
		include_directories( "${CMAKE_BINARY_DIR}/wxWidgets/libs/base" )
		link_directories( "${CMAKE_BINARY_DIR}/lib/vc_x64_lib" )
	endif()
	set( WXWIDGETS "gl" "core" "base" "adv" "aui" "html" "media" "propgrid" "qa" "ribbon" "richtext" "stc"
		"webview" "xml" "xrc" "wxexpat" "wxjpeg" "wxpng" "wxregex" "wxscintilla" "wxtiff" "wxzlib" )
endif()

set( COMMON_SOURCE
	${Json_SOURCE}
	${fmt_SOURCE}
	Common.cpp Common.h
	Crypt.cpp Crypt.h
	DataFile.cpp DataFile.h
	Debugger.cpp Debugger.h
	Defines.h
	Entity.cpp Entity.h
	Exception.cpp Exception.h
	FileManager.cpp FileManager.h
	FileSystem.cpp FileSystem.h
	IniParser.cpp IniParser.h
	Log.cpp Log.h
	Methods.cpp Methods.h
	PlatformSpecific.h
	Properties.cpp Properties.h
	Randomizer.h
	Script.cpp Script.h
	ScriptExtensions.cpp
	ScriptBind.h
	ScriptFunctions.h
	ScriptInvoker.cpp ScriptInvoker.h
	ScriptPragmas.cpp ScriptPragmas.h
	ScriptProfiler.cpp ScriptProfiler.h
	Text.cpp Text.h
	Threading.cpp Threading.h
	Timer.cpp Timer.h
	Types.h
)
set( SERVER_CLIENT_EDITOR_SOURCE
	3dAnimation.h
	CritterData.h
	GraphicStructures.cpp GraphicStructures.h
	Item.cpp Item.h
	LineTracer.h
	MsgFiles.cpp MsgFiles.h
	MsgStr.h
	NetProtocol.h
	ProtoManager.cpp ProtoManager.h
	ProtoMap.cpp ProtoMap.h
)
set( SERVER_CLIENT_SOURCE
	Access.h
	BufferManager.cpp BufferManager.h
	DataMask.h
)
set( CLIENT_EDITOR_SOURCE
	${GLEW_SOURCE}
	3dStuff.cpp 3dStuff.h
	CritterCl.cpp CritterCl.h
	F2Palette.h
	FlexRect.h
	GluStuff.cpp GluStuff.h
	GraphicLoader.cpp GraphicLoader.h
	HexManager.cpp HexManager.h
	ItemHex.cpp ItemHex.h
	Keyboard.cpp Keyboard.h
	MapCl.cpp MapCl.h
	ResourceManager.cpp ResourceManager.h
	SpriteManager.cpp SpriteManager.h
	SpriteManagerFont.cpp
	Sprites.cpp Sprites.h
)
set( SERVER_EDITOR_SOURCE
	ResourceConverter.h ResourceConverter.cpp
)

set( SERVER_SOURCE
	${COMMON_SOURCE}
	${SERVER_CLIENT_EDITOR_SOURCE}
	${SERVER_EDITOR_SOURCE}
	${SERVER_CLIENT_SOURCE}
	${fbxsdk_SOURCE}
	Critter.cpp Critter.h
	CritterData.h
	CritterManager.h CritterManager.cpp
	DataBase.cpp DataBase.h
	Dialogs.cpp Dialogs.h
	EntityManager.cpp EntityManager.h
	ItemManager.cpp ItemManager.h
	MainServer.cpp
	Map.cpp Map.h
	MapManager.cpp MapManager.h
	Networking.cpp Networking.h
	Server.cpp Server.h
	ServerClient.cpp
	ServerItem.cpp
	ServerNpc.cpp
	ServerScript.cpp
)
set( CLIENT_SOURCE
	${COMMON_SOURCE}
	${SERVER_CLIENT_EDITOR_SOURCE}
	${SERVER_CLIENT_SOURCE}
	${CLIENT_EDITOR_SOURCE}
	${OGG_SOURCE}
	${VORBIS_SOURCE}
	${THEORA_SOURCE}
	Client.cpp Client.h
	ClientInterface.cpp
	MainClient.cpp
	SoundManager.cpp SoundManager.h
)
set( EDITOR_SOURCE
	${COMMON_SOURCE}
	${SERVER_CLIENT_EDITOR_SOURCE}
	${SERVER_EDITOR_SOURCE}
	${CLIENT_EDITOR_SOURCE}
	${fbxsdk_SOURCE}
	MainEditor.cpp
	Mapper.cpp Mapper.h
)

#
# Builds
#

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}" )

set( RO0 RUNTIME_OUTPUT_DIRECTORY )
set( RO1 RUNTIME_OUTPUT_DIRECTORY_DEBUG )
set( RO2 RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL )
set( RO3 RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO )
set( RO4 RUNTIME_OUTPUT_DIRECTORY_RELEASE )

if( NOT BUILD_CLIENT_LIBRARY )
	add_executable( FOnline WIN32 ${CLIENT_SOURCE} "Client.rc" )
	# Todo: Make bundles for Mac and maybe iOS
	# add_executable( FOnline MACOSX_BUNDLE ${CLIENT_SOURCE} "Client.rc" )
	set_target_properties( FOnline PROPERTIES ${RO0} ${CLIENT_OUTPUT} ${RO1} ${CLIENT_OUTPUT} ${RO2} ${CLIENT_OUTPUT} ${RO3} ${CLIENT_OUTPUT} ${RO4} ${CLIENT_OUTPUT} )
else()
	add_library( FOnline SHARED ${CLIENT_SOURCE} )
	set_target_properties( FOnline PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CLIENT_OUTPUT} )
endif()
set_target_properties( FOnline PROPERTIES OUTPUT_NAME "FOnline${CLIENT_POSTFIX}" COMPILE_FLAGS "${CLIENT_DEFINES}" )
target_link_libraries( FOnline "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SDL2main" "SDL2-static" "Acm" "SHA" "${NCODEHOOK}" "${UNQLITE}" "${RENDER_LIBS}" "${CMAKE_DL_LIBS}" "${CLIENT_LIBS}" )

if( NOT BUILD_ONLY_CLIENT )
	add_executable( FOnlineServer WIN32 ${SERVER_SOURCE} "Server.rc" )
	set_target_properties( FOnlineServer PROPERTIES ${RO0} ${SERVER_OUTPUT} ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
	set_target_properties( FOnlineServer PROPERTIES OUTPUT_NAME "FOnlineServer${NON_CLIENT_POSTFIX}" COMPILE_FLAGS "${SERVER_DEFINES}" )
	target_link_libraries( FOnlineServer "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SHA" "libcurl" "${NCODEHOOK}" "${FBXSDK}" "assimp" "mongoc_static" "bson_static" "${UNQLITE}" "ssl" "crypto" ${WXWIDGETS} "${CMAKE_DL_LIBS}" )

	if( NOT WIN32 )
		add_executable( FOnlineServerDaemon ${SERVER_SOURCE} "Server.rc" )
		set_target_properties( FOnlineServerDaemon PROPERTIES ${RO0} ${SERVER_OUTPUT} ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
		set_target_properties( FOnlineServerDaemon PROPERTIES OUTPUT_NAME "FOnlineServerDaemon${NON_CLIENT_POSTFIX}" COMPILE_FLAGS "${SERVER_DEFINES} -DSERVER_DAEMON" )
		target_link_libraries( FOnlineServerDaemon "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SHA" "libcurl" "${NCODEHOOK}" "${FBXSDK}" "assimp" "mongoc_static" "bson_static" "${UNQLITE}" "ssl" "crypto" "${CMAKE_DL_LIBS}" )
	endif()

	add_executable( FOnlineEditor WIN32 ${EDITOR_SOURCE} "Editor.rc" )
	set_target_properties( FOnlineEditor PROPERTIES ${RO0} ${EDITOR_OUTPUT} ${RO1} ${EDITOR_OUTPUT} ${RO2} ${EDITOR_OUTPUT} ${RO3} ${EDITOR_OUTPUT} ${RO4} ${EDITOR_OUTPUT} )
	set_target_properties( FOnlineEditor PROPERTIES OUTPUT_NAME "FOnlineEditor${NON_CLIENT_POSTFIX}" COMPILE_FLAGS "${EDITOR_DEFINES}" )
	target_link_libraries( FOnlineEditor "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SDL2-static" "SHA" "${NCODEHOOK}" "${UNQLITE}" "${FBXSDK}" "assimp" ${WXWIDGETS} "${RENDER_LIBS}" "${CMAKE_DL_LIBS}" )

	set_property( GLOBAL PROPERTY USE_FOLDERS ON )
	set_property( TARGET FOnlineServer FOnline FOnlineEditor PROPERTY FOLDER "Binaries")
	set_property( TARGET Acm Angelscript AngelscriptExt ${NCODEHOOK} png16_static SDL2main SDL2-static SHA zlibstatic libcurl assimp IrrXML mongoc_static bson_static ${UNQLITE} mbedcrypto mbedtls mbedx509 ssl crypto PROPERTY FOLDER "Libraries")
	if( WXWIDGETS )
		set_property( TARGET ${WXWIDGETS} "wxrc" PROPERTY FOLDER "Libraries/wxWidgets")
	endif()
endif()
