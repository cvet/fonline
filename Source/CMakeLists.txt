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
	add_definitions( "-DGLEW_STATIC" )
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

set( IMGUI_DIR "imgui" )
file( GLOB IMGUI_SOURCE "${IMGUI_DIR}/*.cpp" "${IMGUI_DIR}/*.h" )
#list( REMOVE_ITEM IMGUI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${IMGUI_DIR}/imgui_demo.cpp" )
#list( REMOVE_ITEM IMGUI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${IMGUI_DIR}/imgui_demo.h" )
source_group( "ThirdParty\\imgui" FILES ${IMGUI_SOURCE} )
include_directories( "imgui" )
add_definitions( "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS" )

set( COMMON_SOURCE
	${IMGUI_SOURCE}
	${fmt_SOURCE}

	Common.cpp Common.h # Clean up from different things
	Entity.cpp Entity.h
	ProtoMap.cpp # Remove
	Exception.cpp Exception.h
	Debugger.cpp Debugger.h # Delete?
	Log.cpp Log.h
	IniParser.cpp IniParser.h # Rename (IniFile)
	MsgFiles.cpp MsgFiles.h

	FileSystem.cpp FileSystem.h
	FileManager.cpp FileManager.h # Rename (File)
	# FilesCollection -> ProjectFiles?
	DataFile.cpp DataFile.h

	GraphicApi.cpp GraphicApi.h
	GraphicStructures.cpp GraphicStructures.h

	Properties.cpp Properties.h
	ProtoManager.cpp ProtoManager.h

	ScriptBind.h
	ScriptFunctions.h
	Script.cpp Script.h
	ScriptExtensions.cpp
	ScriptInvoker.cpp ScriptInvoker.h
	ScriptPragmas.cpp ScriptPragmas.h
	ScriptProfiler.cpp ScriptProfiler.h
	DummyData.h # Rename (ScriptReference) or include to ScriptBind.h

	NetProtocol.h # Include to NetBuffer?
	BufferManager.cpp BufferManager.h # Rename (NetBuffer)
	UcsTables.h
	Text.cpp Text.h # Rename (GameTexts, StringExt)
	Threading.cpp Threading.h # Use C++ threads lib?
	Timer.cpp Timer.h # Rename (Timing)
	LineTracer.h LineTracer.cpp

	# Refactor
	${GLEW_SOURCE} # Remove, use dynamic binding
	Crypt.cpp Crypt.h # Split to DataStorage/LocalStorage
	Access.h # To NetProtocol.h?
	MsgStr.h # To Common.h?
	3dAnimation.h # To Graphic...?
	PlatformSpecific.h # To Common.h
	Types.h # To Common.h
	Randomizer.h # To Common.h
	DataMask.h # To Common.h?
	Defines.h # To Common.h
)
set( SERVER_SOURCE
	${COMMON_SOURCE}
	${Json_SOURCE}
	${fbxsdk_SOURCE}

	# Location
	Map.cpp Map.h
	Critter.cpp Critter.h
	Item.cpp Item.h

	# To Server.h?
	EntityManager.cpp EntityManager.h
	# LocationManager
	MapManager.cpp MapManager.h
	CritterManager.cpp CritterManager.h
	ItemManager.cpp ItemManager.h

	AppGui.cpp AppGuiDX.cpp AppGui.h
	ResourceConverter.cpp ResourceConverter.h
	DataBase.cpp DataBase.h
	Dialogs.cpp Dialogs.h
	Networking.cpp Networking.h

	Server.cpp Server.h
	ServerClient.cpp # Refactor
	ServerItem.cpp # Refactor
	ServerNpc.cpp # Refactor
	ServerScript.cpp
)
set( CLIENT_SOURCE
	${COMMON_SOURCE}
	${OGG_SOURCE}
	${VORBIS_SOURCE}
	${THEORA_SOURCE}

	Client.cpp Client.h
	ClientInterface.cpp # Remove
	SoundManager.cpp SoundManager.h

	# Cl to View?
	MapCl.cpp MapCl.h
	CritterCl.cpp CritterCl.h
	ItemCl.cpp ItemCl.h
	ItemHex.cpp ItemHex.h # Merge with ItemCl or ItemHexView

	Keyboard.cpp Keyboard.h
	HexManager.cpp HexManager.h
	ResourceManager.cpp ResourceManager.h
	GraphicLoader.cpp GraphicLoader.h # Refactor, merge with ResourceManager?

	3dStuff.cpp 3dStuff.h
	F2Palette.h
	SpriteManager.cpp SpriteManager.h # Refactor, split loaders
	SpriteManagerFont.cpp
	Sprites.cpp Sprites.h

	GluStuff.cpp GluStuff.h # Remove
	FlexRect.h # Move to Common?
)
set( EDITOR_SOURCE
	${CLIENT_SOURCE}
	${SERVER_SOURCE}
	Mapper.cpp Mapper.h
)
list( REMOVE_DUPLICATES EDITOR_SOURCE )

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
	add_executable( FOnline WIN32 ${CLIENT_SOURCE} "MainClient.cpp" "Client.rc" )
	# Todo: Make bundles for Mac and maybe iOS
	# add_executable( FOnline MACOSX_BUNDLE ${CLIENT_SOURCE} "Client.rc" )
	set_target_properties( FOnline PROPERTIES ${RO0} ${CLIENT_OUTPUT} ${RO1} ${CLIENT_OUTPUT} ${RO2} ${CLIENT_OUTPUT} ${RO3} ${CLIENT_OUTPUT} ${RO4} ${CLIENT_OUTPUT} )
else()
	add_library( FOnline SHARED ${CLIENT_SOURCE} "MainClient.cpp" )
	set_target_properties( FOnline PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CLIENT_OUTPUT} )
endif()
set_target_properties( FOnline PROPERTIES OUTPUT_NAME "FOnline${CLIENT_POSTFIX}" COMPILE_FLAGS "${CLIENT_DEFINES}" )
target_link_libraries( FOnline "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SDL2main" "SDL2-static" "Acm" "SHA" "${NCODEHOOK}" "${UNQLITE}" "${RENDER_LIBS}" "${CMAKE_DL_LIBS}" "${CLIENT_LIBS}" )

if( NOT BUILD_ONLY_CLIENT )
	add_executable( FOnlineServer WIN32 ${SERVER_SOURCE} "MainServer.cpp" "Server.rc" )
	set_target_properties( FOnlineServer PROPERTIES ${RO0} ${SERVER_OUTPUT} ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
	set_target_properties( FOnlineServer PROPERTIES OUTPUT_NAME "FOnlineServer${NON_CLIENT_POSTFIX}" COMPILE_FLAGS "${SERVER_DEFINES}" )
	target_link_libraries( FOnlineServer "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SDL2main" "SDL2-static" "SHA" "libcurl" "${NCODEHOOK}" "${FBXSDK}" "assimp" "${RENDER_LIBS}" "mongoc_static" "bson_static" "${UNQLITE}" "ssl" "crypto" "${CMAKE_DL_LIBS}" )

	if( NOT WIN32 )
		set( SERVER_SOURCE_NO_GUI ${SERVER_SOURCE} )
		list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "${CMAKE_CURRENT_SOURCE_DIR}/GraphicApi.h" )
		list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "${CMAKE_CURRENT_SOURCE_DIR}/GraphicApi.cpp" )
		list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "${CMAKE_CURRENT_SOURCE_DIR}/AppGui.h" )
		list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "${CMAKE_CURRENT_SOURCE_DIR}/AppGui.cpp" )
		list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "${CMAKE_CURRENT_SOURCE_DIR}/AppGuiDX.cpp" )
		list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "${GLEW_SOURCE}" )
		add_executable( FOnlineServerDaemon ${SERVER_SOURCE_NO_GUI} "MainServer.cpp" "Server.rc" )
		set_target_properties( FOnlineServerDaemon PROPERTIES ${RO0} ${SERVER_OUTPUT} ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
		set_target_properties( FOnlineServerDaemon PROPERTIES OUTPUT_NAME "FOnlineServerDaemon${NON_CLIENT_POSTFIX}" COMPILE_FLAGS "${SERVER_DEFINES} -DFO_SERVER_DAEMON" )
		target_link_libraries( FOnlineServerDaemon "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SHA" "libcurl" "${NCODEHOOK}" "${FBXSDK}" "assimp" "mongoc_static" "bson_static" "${UNQLITE}" "ssl" "crypto" "${CMAKE_DL_LIBS}" )
	endif()

	add_executable( FOnlineEditor WIN32 ${EDITOR_SOURCE} "MainEditor.cpp" "Editor.rc" )
	set_target_properties( FOnlineEditor PROPERTIES ${RO0} ${EDITOR_OUTPUT} ${RO1} ${EDITOR_OUTPUT} ${RO2} ${EDITOR_OUTPUT} ${RO3} ${EDITOR_OUTPUT} ${RO4} ${EDITOR_OUTPUT} )
	set_target_properties( FOnlineEditor PROPERTIES OUTPUT_NAME "FOnlineEditor${NON_CLIENT_POSTFIX}" COMPILE_FLAGS "${EDITOR_DEFINES}" )
	target_link_libraries( FOnlineEditor "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SDL2main" "SDL2-static" "Acm" "SHA" "${NCODEHOOK}" "${UNQLITE}" "${FBXSDK}" "assimp" "${RENDER_LIBS}" "ssl" "crypto" "mongoc_static" "bson_static" "libcurl" "${CMAKE_DL_LIBS}" )

	set_property( GLOBAL PROPERTY USE_FOLDERS ON )
	set_property( TARGET FOnlineServer FOnline FOnlineEditor PROPERTY FOLDER "Binaries" )
	set_property( TARGET Acm Angelscript AngelscriptExt ${NCODEHOOK} png16_static SDL2main SDL2-static SHA zlibstatic libcurl assimp IrrXML mongoc_static bson_static ${UNQLITE} mbedcrypto mbedtls mbedx509 ssl crypto PROPERTY FOLDER "Libraries" )
endif()
