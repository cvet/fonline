cmake_minimum_required( VERSION 3.10.2 )
project( FOnlineProject )

# Options
set( FONLINE_VERBOSE_BUILD OFF CACHE BOOL "Verbose build mode" )
set( FONLINE_OUTPUT_BINARIES_PATH "${CMAKE_BINARY_DIR}" CACHE PATH "Binaries output path" )
set( FONLINE_WEB_DEBUG OFF CACHE BOOL "Web debug build" )
set( FONLINE_BUILD_ONLY_EDITOR OFF CACHE BOOL "Build only Editor to speed up compilation time" )

# Quiet all non-error messages instead ourself
function( message mode )
	if( ${mode} STREQUAL "FATAL_ERROR" )
		_message( FATAL_ERROR ${ARGN} )
	elseif( ${mode} STREQUAL "SEND_ERROR" )
		_message( SEND_ERROR ${ARGN} )
	elseif( FONLINE_VERBOSE_BUILD )
		_message( ${mode} ${ARGN} )
	endif()
endfunction()
function( StatusMessage )
	_message( STATUS ${ARGN} )
endfunction()
function( AbortMessage )
	_message( FATAL_ERROR ${ARGN} )
endfunction()
if( FONLINE_VERBOSE_BUILD )
	StatusMessage( "Verbose build mode" )
	set( CMAKE_VERBOSE_MAKEFILE ON CACHE STRING "" FORCE )
else()
	set( CMAKE_VERBOSE_MAKEFILE OFF CACHE STRING "" FORCE )
endif()

# Build options
StatusMessage( "Start project generation" )

set( BUILD_CONFIGURATION "Release" )
set( BUILD_ONLY_CLIENT NO )
set( BUILD_CLIENT_LIBRARY NO )
set( BUILD_TEST_CASES NO )
set( BUILD_CODE_COVERAGE NO )
set( BUILD_OPTIONS "" )
set( BUILD_DEFINES "UNICODE" "_UNICODE" )

# Architecture configuration
if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
	StatusMessage( "CPU architecture: 64-bit" )
	set( ARCH_STR "x64" )
else()
	StatusMessage( "CPU architecture: 32-bit" )
	set( ARCH_STR "x86" )
endif()

if( WIN32 )
	set( OS "Windows" )
	set( BUILD_CONFIGURATION "RelWithDebInfo" )
	if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		set( CLIENT_POSTFIX "64" )
	else()
		set( NON_CLIENT_POSTFIX "32" )
	endif()
	set( USE_GLEW YES )
	set( BUILD_TEST_CASES YES )
	link_directories( "ThirdParty/fbxsdk/fbxsdk-libs/${OS}/Release/${ARCH_STR}" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};/MT;/MP" )
	# Debug: /MTd /MP /RTCc /RTCsu
	list( APPEND BUILD_DEFINES "_CRT_SECURE_NO_WARNINGS" "_CRT_SECURE_NO_DEPRECATE" "_WINSOCK_DEPRECATED_NO_WARNINGS" )
elseif( ${CMAKE_SYSTEM_NAME} MATCHES "Linux" )
	set( OS "Linux" )
	set( BUILD_CONFIGURATION "RelWithDebInfo" )
	if( CMAKE_SIZEOF_VOID_P EQUAL 4 )
		set( CLIENT_POSTFIX "32" )
		set( NON_CLIENT_POSTFIX "32" )
	endif()
	find_package( X11 REQUIRED )
	find_package( OpenGL REQUIRED )
	set( RENDER_LIBS "GL" )
	set( USE_GLEW YES )
	set( BUILD_TEST_CASES YES )
	set( BUILD_CODE_COVERAGE YES )
	link_directories( "ThirdParty/fbxsdk/fbxsdk-libs/${OS}/Release/${ARCH_STR}" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-no-pie;-O2;-g;-rdynamic" )
elseif( APPLE AND NOT IOS_PLATFORM )
	set( OS "Mac" )
	set( BUILD_CONFIGURATION "RelWithDebInfo" )
	set( BUILD_ONLY_CLIENT YES )
	find_package( OpenGL REQUIRED )
	set( RENDER_LIBS "${OPENGL_LIBRARIES}" )
	set( USE_GLEW YES )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-no-pie;-O2;-g;-rdynamic" )
elseif( APPLE AND IOS_PLATFORM )
	set( OS "iOS" )
	set( BUILD_ONLY_CLIENT YES )
	find_library( OPENGLES OpenGLES )
	find_library( METAL Metal )
	find_library( COREGRAPGHICS CoreGraphics )
	find_library( QUARTZCORE QuartzCore )
	find_library( UIKIT UIKit )
	find_library( AVFOUNDATION AVFoundation )
	find_library( GAMECONTROLLER GameController )
	find_library( COREMOTION CoreMotion )
	set( CLIENT_LIBS ${OPENGLES} ${METAL} ${COREGRAPGHICS} ${QUARTZCORE} ${UIKIT} ${AVFOUNDATION} ${GAMECONTROLLER} ${COREMOTION} )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-no-pie;-O3" )
elseif( ANDROID )
	set( OS "Android" )
	set( BUILD_ONLY_CLIENT YES )
	set( BUILD_CLIENT_LIBRARY YES )
	set( RENDER_LIBS "GLESv1_CM" "GLESv2" )
	set( CLIENT_LIBS "android" "log" "atomic" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-pie;-O3" )
elseif( EMSCRIPTEN )
	set( OS "Web" )
	set( BUILD_ONLY_CLIENT YES )
	set( CMAKE_EXECUTABLE_SUFFIX ".js" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s TOTAL_MEMORY=268435456" ) # 256 Mb
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s ERROR_ON_UNDEFINED_SYMBOLS=1" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s ALLOW_MEMORY_GROWTH=1" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s USE_WEBGL2=1" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s FORCE_FILESYSTEM=1" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s ENVIRONMENT='web'" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s DISABLE_EXCEPTION_CATCHING=1" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s NO_DYNAMIC_EXECUTION=1" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s USE_SDL_IMAGE=0" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s USE_SDL_TTF=0" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s USE_SDL_NET=0" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s NO_EXIT_RUNTIME=1" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s EXPORTED_RUNTIME_METHODS=\"['FS_createPath', 'FS_createDataFile']\"" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};-s EXTRA_EXPORTED_RUNTIME_METHODS=\"['Pointer_stringify', 'intArrayFromString', 'UTF8ToString', 'addRunDependency', 'removeRunDependency', 'stackTrace', 'getMemory']\"" )
	set( BUILD_OPTIONS "${BUILD_OPTIONS};--js-library \"${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/mono/repo/sdks/wasm/library_mono.js\"" )
	if( FONLINE_WEB_DEBUG )
		set( BUILD_CONFIGURATION "Debug" )
		set( CLIENT_POSTFIX "${CLIENT_POSTFIX}_Debug" )
		set( NON_CLIENT_POSTFIX "${NON_CLIENT_POSTFIX}_Debug" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-O0" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-g3" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};--memory-init-file 1" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s WASM=0" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s SAFE_HEAP=1" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s ASSERTIONS=1" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s STACK_OVERFLOW_CHECK=1" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s GL_DEBUG=1" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s GL_ASSERTIONS=1" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s GL_TESTING=1" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s WARN_UNALIGNED=1" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s ERROR_ON_MISSING_LIBRARIES=1" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s DEMANGLE_SUPPORT=1" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s EXCEPTION_DEBUG=1" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s BINARYEN_TRAP_MODE='allow'" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s PRECISE_F32=1" )
	else()
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-Oz" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-fno-rtti" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-fno-exceptions" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};--no-heap-copy" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s DISABLE_EXCEPTION_CATCHING=1" )
		set( BUILD_OPTIONS "${BUILD_OPTIONS};-s BINARYEN_TRAP_MODE='clamp'" )
		# Uncomment these optimizations later, they may give side effects
		# set( BUILD_OPTIONS "${BUILD_OPTIONS};--closure 1" ) - conflicts with NO_DYNAMIC_EXECUTION
		# set( BUILD_OPTIONS "${BUILD_OPTIONS};--llvm-lto 1" )
		# set( BUILD_OPTIONS "${BUILD_OPTIONS};-s INLINING_LIMIT=1" )
	endif()
	list( APPEND BUILD_DEFINES "AS_MAX_PORTABILITY" "WIP_16BYTE_ALIGN" )
else()
	AbortMessage( "Unknown OS!" )
endif()

StatusMessage( "Operating system: ${OS}" )
string( TOUPPER "${OS}" OS_UPPER )
list( APPEND BUILD_DEFINES "FO_${OS_UPPER}" )

StatusMessage( "Configuration: ${BUILD_CONFIGURATION}" )
set( CMAKE_CONFIGURATION_TYPES "${BUILD_CONFIGURATION}" CACHE STRING "" FORCE )
set( CMAKE_BUILD_TYPE "${BUILD_CONFIGURATION}" CACHE STRING "" FORCE )

StatusMessage( "Binaries output path: ${FONLINE_OUTPUT_BINARIES_PATH}" )
set( OUTPUT_BINARIES_PATH "${FONLINE_OUTPUT_BINARIES_PATH}/Binaries" )
file( MAKE_DIRECTORY "${OUTPUT_BINARIES_PATH}" )
if( ANDROID )
	set( CLIENT_OUTPUT "${OUTPUT_BINARIES_PATH}/Client/${OS}/libs/${ANDROID_ABI}" )
else()
	set( CLIENT_OUTPUT "${OUTPUT_BINARIES_PATH}/Client/${OS}" )
endif()
set( SERVER_OUTPUT "${OUTPUT_BINARIES_PATH}/Server" )
set( EDITOR_OUTPUT "${OUTPUT_BINARIES_PATH}/Editor" )
set( TESTS_OUTPUT "${OUTPUT_BINARIES_PATH}/Tests" )

# Compiler options
set( BUILD_OPTIONS "${BUILD_OPTIONS}" )
string( REPLACE ";" " " BUILD_OPTIONS "${BUILD_OPTIONS}" )
string( STRIP "${BUILD_OPTIONS}" BUILD_OPTIONS )
if( MSVC )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17 ${BUILD_OPTIONS}" )
else()
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 ${BUILD_OPTIONS}" )
endif()
string( STRIP "${CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS )
StatusMessage( "Compiler flags: ${CMAKE_CXX_FLAGS}" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${BUILD_OPTIONS}" )
if( MSVC )
	set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${BUILD_OPTIONS}" )
	set( CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} ${BUILD_OPTIONS}" )
	set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${BUILD_OPTIONS}" )
	set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${BUILD_OPTIONS}" )
	set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${BUILD_OPTIONS}" )
	set( CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} ${BUILD_OPTIONS}" )
	set( CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${BUILD_OPTIONS}" )
	set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${BUILD_OPTIONS}" )
	set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL:NO" )
	set( CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO" )
	set( CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} /INCREMENTAL:NO" )
	set( CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /INCREMENTAL:NO" )
	set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /INCREMENTAL:NO" )
else()
	set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${BUILD_OPTIONS}" )
endif()
set( CMAKE_CXX_STANDARD 17 )

# Embedded resources generation
function( CreateResources dir output )
	# Generate file content
	set( outputString "" )
	file( GLOB bins ${dir}/*.zip )
	foreach( bin ${bins} )
		string( REGEX MATCH "([^/]+)$" filename ${bin} )
		string( REGEX REPLACE "\\.| " "_" filename ${filename} )
		file( READ ${bin} filedata HEX )
		string( REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " filedata ${filedata} )
		string( APPEND outputString "const unsigned char Resource_${filename}[] = { ${filedata}};" )
	endforeach()

	# Write if content different
	set( outputHash "" )
	if( EXISTS ${output} )
		file( MD5 ${output} outputHash )
	endif()
	string( MD5 outputStringHash "${outputString}" )
	if( NOT "${outputStringHash}" STREQUAL "${outputHash}" )
		StatusMessage( "Generate embedded resources" )
		file( WRITE ${output} "${outputString}" )
	endif()
endfunction()
CreateResources( "Resources/Embedded" "${CMAKE_BINARY_DIR}/EmbeddedResources/EmbeddedResources.h" )
include_directories( "${CMAKE_BINARY_DIR}/EmbeddedResources" )

# Third-party libs
StatusMessage( "Third-party libs:" )

function( DisableLibWarnings lib )
	target_compile_options( ${lib} PRIVATE
		$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-w>
		$<$<CXX_COMPILER_ID:MSVC>:/W0> )
endfunction()

# SDL2
StatusMessage( "+ SDL2" )
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
	set( HIDAPI OFF CACHE STRING "" FORCE )
endif()
if( APPLE AND IOS_PLATFORM )
	set( SDL_JOYSTICK ON CACHE STRING "" FORCE )
	set( SDL_HAPTIC ON CACHE STRING "" FORCE )
endif()
if( EMSCRIPTEN )
	set( SDL_JOYSTICK ON CACHE STRING "" FORCE )
endif()
add_subdirectory( "${SDL_DIR}" )
include_directories( "${SDL_DIR}/include" )
list( APPEND BUILD_DEFINES "GL_GLEXT_PROTOTYPES" )
target_compile_definitions( SDL2main PRIVATE "GL_GLEXT_PROTOTYPES" )
target_compile_definitions( SDL2-static PRIVATE "GL_GLEXT_PROTOTYPES" )
DisableLibWarnings( SDL2main )
DisableLibWarnings( SDL2-static )

# Zlib
StatusMessage( "+ Zlib" )
set( ZLIB_DIR "ThirdParty/zlib" )
add_subdirectory( "${ZLIB_DIR}" )
include_directories( "${ZLIB_DIR}" "${ZLIB_DIR}/contrib" "${CMAKE_BINARY_DIR}/${ZLIB_DIR}" )
DisableLibWarnings( zlibstatic )

# PNG
StatusMessage( "+ PNG" )
if( NOT ( ANDROID OR ( APPLE AND IOS_PLATFORM ) OR EMSCRIPTEN ) )
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
StatusMessage( "+ Ogg" )
set( OGG_DIR "ThirdParty/ogg" )
file( GLOB OGG_SOURCE "${OGG_DIR}/src/*.c" )
include_directories( "${OGG_DIR}/include" )
add_library( Ogg ${OGG_SOURCE} )
DisableLibWarnings( Ogg )

# Vorbis
StatusMessage( "+ Vorbis" )
set( VORBIS_DIR "ThirdParty/Vorbis" )
file( GLOB VORBIS_SOURCE "${VORBIS_DIR}/lib/*.c" )
list( REMOVE_ITEM VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${VORBIS_DIR}/lib/barkmel.c" )
list( REMOVE_ITEM VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${VORBIS_DIR}/lib/psytune.c" )
list( REMOVE_ITEM VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${VORBIS_DIR}/lib/tone.c" )
include_directories( "${VORBIS_DIR}/include" )
include_directories( "${VORBIS_DIR}/lib" )
add_library( Vorbis ${VORBIS_SOURCE} )
target_link_libraries( Vorbis Ogg )
DisableLibWarnings( Vorbis )

# Theora
StatusMessage( "+ Theora" )
set( THEORA_DIR "ThirdParty/Theora" )
file( GLOB THEORA_SOURCE "${THEORA_DIR}/lib/*.c" )
list( REMOVE_ITEM THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${THEORA_DIR}/lib/analyze.c" )
list( REMOVE_ITEM THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${THEORA_DIR}/lib/encode.c" )
list( REMOVE_ITEM THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${THEORA_DIR}/lib/encapiwrapper.c" )
include_directories( "${THEORA_DIR}/include" )
add_library( Theora ${THEORA_SOURCE} )
DisableLibWarnings( Theora )

# AngelScript
StatusMessage( "+ AngelScript" )
set( ANGEL_SCRIPT_DIR "ThirdParty/AngelScript" )
add_subdirectory( "${ANGEL_SCRIPT_DIR}/sdk/angelscript/projects/cmake" )
include_directories( "${ANGEL_SCRIPT_DIR}/sdk/angelscript/include" "${ANGEL_SCRIPT_DIR}/sdk/angelscript/source" "${ANGEL_SCRIPT_DIR}/sdk/add_on" )
list( APPEND BUILD_DEFINES "AS_NO_EXCEPTIONS" "AS_NO_THREADS" )
target_compile_definitions( Angelscript PRIVATE "AS_NO_EXCEPTIONS;AS_NO_THREADS" )
DisableLibWarnings( Angelscript )

# AngelScriptExt
StatusMessage( "+ AngelScriptExt" )
set( ANGEL_SCRIPT_EXT_DIR "Source/Common/AngelScriptExt" )
set( ANGEL_SCRIPT_SDK_DIR "ThirdParty/AngelScript/sdk" )
include_directories( "${ANGEL_SCRIPT_EXT_DIR}" )
include_directories( "ThirdParty/AngelScript/preprocessor" )
include_directories( "${ANGEL_SCRIPT_SDK_DIR}/add_on/scriptstdstring" )
include_directories( "${ANGEL_SCRIPT_SDK_DIR}/add_on/scriptarray" )
include_directories( "${ANGEL_SCRIPT_SDK_DIR}/add_on/scriptdictionary" )
include_directories( "${ANGEL_SCRIPT_SDK_DIR}/add_on/scriptfile" )
include_directories( "${ANGEL_SCRIPT_SDK_DIR}/add_on/scriptany" )
include_directories( "${ANGEL_SCRIPT_SDK_DIR}/add_on/datetime" )
include_directories( "${ANGEL_SCRIPT_SDK_DIR}/add_on/scriptmath" )
include_directories( "${ANGEL_SCRIPT_SDK_DIR}/add_on/weakref" )
include_directories( "${ANGEL_SCRIPT_SDK_DIR}/add_on/scripthelper" )
file( GLOB AS_SOURCE "${ANGEL_SCRIPT_EXT_DIR}/*.*" )
file( GLOB AS_PREPROCESSOR_SOURCE "ThirdParty/AngelScript/preprocessor/*.*" )
file( GLOB AS_STRING_SOURCE "${ANGEL_SCRIPT_SDK_DIR}/add_on/scriptstdstring/*.*" )
file( GLOB AS_ARRAY_SOURCE "${ANGEL_SCRIPT_SDK_DIR}/add_on/scriptarray/*.*" )
file( GLOB AS_DICTIONARY_SOURCE "${ANGEL_SCRIPT_SDK_DIR}/add_on/scriptdictionary/*.*" )
file( GLOB AS_FILE_SOURCE "${ANGEL_SCRIPT_SDK_DIR}/add_on/scriptfile/*.*" )
file( GLOB AS_ANY_SOURCE "${ANGEL_SCRIPT_SDK_DIR}/add_on/scriptany/*.*" )
file( GLOB AS_DATE_SOURCE "${ANGEL_SCRIPT_SDK_DIR}/add_on/datetime/*.*" )
file( GLOB AS_MATH_SOURCE "${ANGEL_SCRIPT_SDK_DIR}/add_on/scriptmath/*.*" )
file( GLOB AS_WEAKREF_SOURCE "${ANGEL_SCRIPT_SDK_DIR}/add_on/weakref/*.*" )
file( GLOB AS_HELPER_SOURCE "${ANGEL_SCRIPT_SDK_DIR}/add_on/scripthelper/*.*" )
add_library( AngelscriptExt ${AS_SOURCE} ${AS_PREPROCESSOR_SOURCE} ${AS_MATH_SOURCE}
	${AS_WEAKREF_SOURCE} ${AS_HELPER_SOURCE} ${AS_STRING_SOURCE} ${AS_ARRAY_SOURCE}
	${AS_DICTIONARY_SOURCE} ${AS_FILE_SOURCE} ${AS_ANY_SOURCE} ${AS_DATE_SOURCE} )
DisableLibWarnings( AngelscriptExt )

# Acm
StatusMessage( "+ Acm" )
set( ACM_DIR "ThirdParty/Acm" )
add_subdirectory( "${ACM_DIR}" )
include_directories( "${ACM_DIR}" )
DisableLibWarnings( Acm )

# SHA
StatusMessage( "+ SHA" )
set( SHA_DIR "ThirdParty/SHA" )
add_subdirectory( "${SHA_DIR}" )
include_directories( "${SHA_DIR}" )
DisableLibWarnings( SHA )

# GLEW
if( USE_GLEW )
	StatusMessage( "+ GLEW" )
	set( GLEW_DIR "ThirdParty/GLEW" )
	file( GLOB GLEW_SOURCE "${GLEW_DIR}/GL/*.*" )
	include_directories( "${GLEW_DIR}" )
	add_library( GLEW ${GLEW_SOURCE} )
	list( APPEND BUILD_DEFINES "GLEW_STATIC" )
	target_compile_definitions( GLEW PRIVATE "GLEW_STATIC" )
	set( GLEW_LIB "GLEW" )
	DisableLibWarnings( GLEW )
endif()

# NCodeHook
if( WIN32 )
	StatusMessage( "+ NCodeHook" )
	set( NCODE_HOOK_DIR "ThirdParty/NCodeHook" )
	add_subdirectory( "${NCODE_HOOK_DIR}" )
	include_directories( "${NCODE_HOOK_DIR}" )
	set( NCODEHOOK_LIB "NCodeHook" )
	DisableLibWarnings( NCodeHook )
endif()

# Assimp
set( ASSIMP_DIR "ThirdParty/Assimp" )
if( NOT BUILD_ONLY_CLIENT )
	StatusMessage( "+ Assimp" )
	set( BUILD_SHARED_LIBS OFF CACHE STRING "" FORCE )
	set( ASSIMP_NO_EXPORT ON CACHE STRING "" FORCE )
	set( ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE STRING "" FORCE )
	set( ASSIMP_BUILD_ASSIMP_VIEW OFF CACHE STRING "" FORCE )
	set( ASSIMP_BUILD_SAMPLES OFF CACHE STRING "" FORCE )
	set( ASSIMP_BUILD_TESTS OFF CACHE STRING "" FORCE )
	set( ASSIMP_OPT_BUILD_PACKAGES OFF CACHE STRING "" FORCE )
	set( ASSIMP_INSTALL_PDB OFF CACHE STRING "" FORCE )
	set( IGNORE_GIT_HASH ON CACHE STRING "" FORCE )
	add_subdirectory( "${ASSIMP_DIR}" )
	include_directories( "${CMAKE_BINARY_DIR}/${ASSIMP_DIR}/include" )
	DisableLibWarnings( assimp )
	DisableLibWarnings( IrrXML )
else()
	StatusMessage( "+ Assimp headers" )
endif()
include_directories( "${ASSIMP_DIR}/include" )

# Fbx SDK
StatusMessage( "+ Fbx SDK" )
set( FBXSDK_DIR "ThirdParty/fbxsdk" )
include_directories( "${FBXSDK_DIR}" )
if( WIN32 )
	set( FBXSDK_LIB "libfbxsdk-mt" )
else()
	set( FBXSDK_LIB "libfbxsdk.a")
endif()

# cURL & mbedTLS
if( NOT BUILD_ONLY_CLIENT )
	StatusMessage( "+ cURL" )
	set( MBED_TLS_DIR "ThirdParty/mbedTLS" )
	set( USE_STATIC_MBEDTLS_LIBRARY ON CACHE STRING "" FORCE )
	set( ENABLE_PROGRAMS OFF CACHE STRING "" FORCE )
	set( ENABLE_TESTING OFF CACHE STRING "" FORCE )
	add_subdirectory( "${MBED_TLS_DIR}" )
	DisableLibWarnings( mbedcrypto )
	DisableLibWarnings( mbedx509 )
	DisableLibWarnings( mbedtls )

	StatusMessage( "+ mbedTLS" )
	set( CURL_DIR "ThirdParty/cURL" )
	set( HAVE_POLL_FINE_EXITCODE 0 CACHE STRING "" FORCE )
	set( CURL_STATICLIB ON CACHE STRING "" FORCE )
	set( CURL_CA_PATH "none" CACHE STRING "" FORCE )
	set( BUILD_CURL_EXE OFF CACHE STRING "" FORCE )
	set( BUILD_TESTING OFF CACHE STRING "" FORCE )
	set( CMAKE_USE_MBEDTLS ON CACHE STRING "" FORCE )
	set( MBEDTLS_LIBRARIES "mbedcrypto;mbedtls;mbedx509" CACHE STRING "" FORCE )
	set( MBEDTLS_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/${MBED_TLS_DIR}/include" CACHE STRING "" FORCE )
	add_subdirectory( "${CURL_DIR}" )
	include_directories( "${CURL_DIR}/include" )
	list( APPEND BUILD_DEFINES "CURL_STATICLIB" )
	target_compile_definitions( libcurl PRIVATE "CURL_STATICLIB" )
	DisableLibWarnings( libcurl )
endif()

# Nlohmann Json
StatusMessage( "+ Nlohmann Json" )
set( JSON_DIR "ThirdParty/Json" )
include_directories( "${JSON_DIR}" )

# Fmt
StatusMessage( "+ Fmt" )
set( FMT_DIR "ThirdParty/fmt" )
add_subdirectory( "${FMT_DIR}" )
include_directories( "${FMT_DIR}/include" )
DisableLibWarnings( fmt )

# Asio & Websockets
if( NOT BUILD_ONLY_CLIENT )
	StatusMessage( "+ Asio" )
	set( ASIO_DIR "ThirdParty/Asio" )
	include_directories( "${ASIO_DIR}/include" )

	StatusMessage( "+ Websockets" )
	set( WEBSOCKETS_DIR "ThirdParty/websocketpp" )
	include_directories( "${WEBSOCKETS_DIR}" )
endif()

# MongoDB & Bson
if( NOT BUILD_ONLY_CLIENT )
	StatusMessage( "+ Bson" )
	set( BSON_DIR "ThirdParty/libbson" )
	set( ENABLE_STATIC ON CACHE STRING "" FORCE )
	set( BSON_ENABLE_STATIC ON CACHE STRING "" FORCE )
	add_subdirectory( "${BSON_DIR}" )
	set( BSON_INCLUDE_DIR "${CMAKE_BINARY_DIR}/${BSON_DIR}/src/bson"
		"${CMAKE_CURRENT_SOURCE_DIR}/${BSON_DIR}/src/bson" CACHE STRING "" FORCE )
	set( BSON_LIBRARIES "bson_static" CACHE STRING "" FORCE )
	set( BSON_STATIC_LIBRARIES "bson_static" CACHE STRING "" FORCE )
	include_directories( "${BSON_INCLUDE_DIR}" )
	list( APPEND BUILD_DEFINES "BSON_COMPILATION" "BSON_STATIC" "JSONSL_PARSE_NAN" )
	target_compile_definitions( bson_static PRIVATE "BSON_COMPILATION;BSON_STATIC;JSONSL_PARSE_NAN" )
	DisableLibWarnings( bson_static )

	StatusMessage( "+ MongoDB" )
	set( MONGODB_DIR "ThirdParty/mongo-c-driver" )
	set( ENABLE_SASL "OFF" CACHE STRING "" FORCE )
	set( MONGOC_ENABLE_STATIC ON CACHE STRING "" FORCE )
	add_subdirectory( "${MONGODB_DIR}" )
	set( MONGOC_INCLUDE_DIR "${CMAKE_BINARY_DIR}/${MONGODB_DIR}/src/mongoc"
		"${CMAKE_CURRENT_SOURCE_DIR}/${MONGODB_DIR}/src/mongoc" CACHE STRING "" FORCE )
	include_directories( "${MONGOC_INCLUDE_DIR}" )
	target_compile_definitions( mongoc_static PRIVATE "BSON_COMPILATION;BSON_STATIC;JSONSL_PARSE_NAN" )
	DisableLibWarnings( mongoc_static )
endif()

# Unqlite
if( NOT EMSCRIPTEN )
	StatusMessage( "+ Unqlite" )
	set( UNQLITE_DIR "ThirdParty/unqlite" )
	add_subdirectory( "${UNQLITE_DIR}" )
	include_directories( "${UNQLITE_DIR}" )
	set( UNQLITE_LIB "unqlite" )
	DisableLibWarnings( unqlite )
endif()

# Variant
if( NOT BUILD_ONLY_CLIENT )
	StatusMessage( "+ Variant" )
	set( VARIANT_DIR "ThirdParty/variant" )
	include_directories( "${VARIANT_DIR}/include" )
endif()

# OpenSSL
if( NOT BUILD_ONLY_CLIENT )
	StatusMessage( "+ OpenSSL" )
	set( OPENSSL_DIR "ThirdParty/openssl" )
	set( BUILD_OBJECT_LIBRARY_ONLY ON CACHE BOOL "" FORCE )
	add_subdirectory( "${OPENSSL_DIR}" )
	include_directories( "${OPENSSL_DIR}" )
	include_directories( "${CMAKE_BINARY_DIR}/${OPENSSL_DIR}/crypto" )
	include_directories( "${CMAKE_BINARY_DIR}/${OPENSSL_DIR}/ssl" )
	DisableLibWarnings( crypto )
	DisableLibWarnings( ssl )
endif()

# Dear ImGui
StatusMessage( "+ Dear ImGui" )
set( DEAR_IMGUI_DIR "ThirdParty/imgui" )
file( GLOB IMGUI_SOURCE "${DEAR_IMGUI_DIR}/*.cpp" "${DEAR_IMGUI_DIR}/*.h" )
#list( REMOVE_ITEM IMGUI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${DEAR_IMGUI_DIR}/imgui_demo.cpp" )
#list( REMOVE_ITEM IMGUI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${DEAR_IMGUI_DIR}/imgui_demo.h" )
include_directories( "${DEAR_IMGUI_DIR}" )
add_library( ImGui ${IMGUI_SOURCE} )
list( APPEND BUILD_DEFINES "IMGUI_DISABLE_OBSOLETE_FUNCTIONS" )
target_compile_definitions( ImGui PRIVATE "IMGUI_DISABLE_OBSOLETE_FUNCTIONS" )
DisableLibWarnings( ImGui )

# Catch2
StatusMessage( "+ Catch2" )
set( CATCH2_DIR "ThirdParty/Catch2" )
include_directories( "${CATCH2_DIR}/single_include/catch2" )

# Backward-cpp
if( NOT WIN32 )
	StatusMessage( "+ Backward-cpp" )
	set( BACKWARDCPP_DIR "ThirdParty/backward-cpp" )
	include_directories( "${BACKWARDCPP_DIR}" )
	if( "${OS}" STREQUAL "Linux" )
		set( BACKWARDCPP_LIBS "bfd" )
	endif()
endif()

# Mono
StatusMessage( "+ Mono" )
set( MONO_DIR "ThirdParty/mono" )
add_subdirectory( "${MONO_DIR}" )
include_directories( "${MONO_DIR}" )
include_directories( "${MONO_DIR}/repo" )
include_directories( "${MONO_DIR}/repo/mono" )
include_directories( "${MONO_DIR}/repo/mono/eglib" )
DisableLibWarnings( libmono )

# Spark
StatusMessage( "+ Spark" )
set( SPARK_DIR "ThirdParty/spark" )
set( SPARK_STATIC_BUILD ON CACHE BOOL "" FORCE )
add_subdirectory( "${SPARK_DIR}/projects/engine/core" )
include_directories( "${CMAKE_BINARY_DIR}/${SPARK_DIR}/include" )
DisableLibWarnings( spark )

# glslang & SPIRV-Cross
if( NOT BUILD_ONLY_CLIENT )
	StatusMessage( "+ glslang" )
	set( GLSLANG_DIR "ThirdParty/glslang" )
	set( SKIP_GLSLANG_INSTALL "ON" CACHE STRING "" FORCE )
	set( ENABLE_HLSL "OFF" CACHE STRING "" FORCE )
	set( ENABLE_GLSLANG_BINARIES "OFF" CACHE STRING "" FORCE )
	set( ENABLE_SPVREMAPPER "OFF" CACHE STRING "" FORCE )
	set( ENABLE_AMD_EXTENSIONS "OFF" CACHE STRING "" FORCE )
	set( ENABLE_NV_EXTENSIONS "OFF" CACHE STRING "" FORCE )
	if( EMSCRIPTEN )
		set( ENABLE_GLSLANG_WEB "ON" CACHE STRING "" FORCE )
		set( ENABLE_EMSCRIPTEN_SINGLE_FILE "ON" CACHE STRING "" FORCE )
	endif()
	add_subdirectory( "${GLSLANG_DIR}" )
	include_directories( "${GLSLANG_DIR}/glslang/Include" )
	set( GLSLANG_LIBS "glslang" "OGLCompiler" "OSDependent" "SPIRV" )
	DisableLibWarnings( glslang )
	DisableLibWarnings( OGLCompiler )
	DisableLibWarnings( OSDependent )
	DisableLibWarnings( SPIRV )

	StatusMessage( "+ SPIRV-Cross" )
	set( SPIRV_CROSS_DIR "ThirdParty/SPIRV-Cross" )
	set( SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS "ON" CACHE STRING "" FORCE )
	set( SPIRV_CROSS_STATIC "ON" CACHE STRING "" FORCE )
	set( SPIRV_CROSS_SHARED "OFF" CACHE STRING "" FORCE )
	set( SPIRV_CROSS_CLI "OFF" CACHE STRING "" FORCE )
	set( SPIRV_CROSS_ENABLE_TESTS "OFF" CACHE STRING "" FORCE )
	set( SPIRV_CROSS_ENABLE_GLSL "ON" CACHE STRING "" FORCE )
	set( SPIRV_CROSS_ENABLE_HLSL "ON" CACHE STRING "" FORCE )
	set( SPIRV_CROSS_ENABLE_MSL "OFF" CACHE STRING "" FORCE )
	set( SPIRV_CROSS_ENABLE_CPP "OFF" CACHE STRING "" FORCE )
	set( SPIRV_CROSS_ENABLE_REFLECT "OFF" CACHE STRING "" FORCE )
	set( SPIRV_CROSS_ENABLE_C_API "OFF" CACHE STRING "" FORCE )
	set( SPIRV_CROSS_ENABLE_UTIL "OFF" CACHE STRING "" FORCE )
	add_subdirectory( "${SPIRV_CROSS_DIR}" )
	include_directories( "${SPIRV_CROSS_DIR}/include" )
	set( SPIRV_CROSS_LIBS "spirv-cross-core" "spirv-cross-glsl" "spirv-cross-hlsl" )
	DisableLibWarnings( spirv-cross-core )
	DisableLibWarnings( spirv-cross-glsl )
	DisableLibWarnings( spirv-cross-hlsl )
endif()

# Rapid YAML
StatusMessage( "+ Rapid YAML" )
set( RAPID_YAML_DIR "ThirdParty/rapidyaml" )
add_subdirectory( "${RAPID_YAML_DIR}" )
include_directories( "${RAPID_YAML_DIR}/src" )
DisableLibWarnings( ryml )

# Common source
set( COMMON_SOURCE
	Source/Common/Common.cpp Source/Common/Common.h
	Source/Common/Entity.cpp Source/Common/Entity.h
	Source/Common/ProtoMap.cpp
	Source/Common/Testing.cpp Source/Common/Testing.h
	Source/Common/Debugger.cpp Source/Common/Debugger.h
	Source/Common/Log.cpp Source/Common/Log.h
	Source/Common/IniFile.cpp Source/Common/IniFile.h
	Source/Common/MsgFiles.cpp Source/Common/MsgFiles.h
	Source/Common/MsgStr_Include.h
	Source/Common/FileSystem.cpp Source/Common/FileSystem.h
	Source/Common/FileUtils.cpp Source/Common/FileUtils.h
	Source/Common/DataFile.cpp Source/Common/DataFile.h
	Source/Common/GraphicApi.cpp Source/Common/GraphicApi.h
	Source/Common/GraphicStructures.cpp Source/Common/GraphicStructures.h
	Source/Common/3dAnimation.cpp Source/Common/3dAnimation.h
	Source/Common/Properties.cpp Source/Common/Properties.h
	Source/Common/ProtoManager.cpp Source/Common/ProtoManager.h
	Source/Common/Script.cpp Source/Common/Script.h
	Source/Common/ScriptExtensions.cpp
	Source/Common/ScriptInvoker.cpp Source/Common/ScriptInvoker.h
	Source/Common/ScriptPragmas.cpp Source/Common/ScriptPragmas.h
	Source/Common/ScriptProfiler.cpp Source/Common/ScriptProfiler.h
	Source/Common/ScriptBind_Include.h
	Source/Common/ScriptFunctions_Include.h
	Source/Common/ScriptReference_Include.h
	Source/Common/NetBuffer.cpp Source/Common/NetBuffer.h
	Source/Common/NetProtocol_Include.h
	Source/Common/StringUtils.cpp Source/Common/StringUtils.h
	Source/Common/UcsTables_Include.h
	Source/Common/Threading.cpp Source/Common/Threading.h
	Source/Common/Timer.cpp Source/Common/Timer.h
	Source/Common/LineTracer.h Source/Common/LineTracer.cpp
	Source/Common/Crypt.cpp Source/Common/Crypt.h
	Source/Common/Settings.h Source/Common/Settings.cpp
	Source/Common/Randomizer.h Source/Common/Randomizer.cpp
	Source/Common/System.h Source/Common/System.cpp
	Source/Common/YamlFile.h Source/Common/YamlFile.cpp
)

# Server source
set( SERVER_SOURCE
	${COMMON_SOURCE}
	Source/Server/AdminPanel.cpp Source/Server/AdminPanel.h
	Source/Server/Map.cpp Source/Server/Map.h
	Source/Server/Critter.cpp Source/Server/Critter.h
	Source/Server/Item.cpp Source/Server/Item.h
	Source/Server/EntityManager.cpp Source/Server/EntityManager.h
	Source/Server/MapManager.cpp Source/Server/MapManager.h
	Source/Server/CritterManager.cpp Source/Server/CritterManager.h
	Source/Server/ItemManager.cpp Source/Server/ItemManager.h
	Source/Server/AppGui.cpp Source/Server/AppGuiDX.cpp Source/Server/AppGui.h
	Source/Server/DataBase.cpp Source/Server/DataBase.h
	Source/Server/Dialogs.cpp Source/Server/Dialogs.h
	Source/Server/Networking.cpp Source/Server/Networking.h
	Source/Server/Server.cpp Source/Server/Server.h
	Source/Server/ServerClient.cpp
	Source/Server/ServerItem.cpp
	Source/Server/ServerNpc.cpp
	Source/Server/ServerScript.cpp
)

# Server source without gui
set( SERVER_SOURCE_NO_GUI ${SERVER_SOURCE} )
list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "Source/Common/GraphicApi.h" )
list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "Source/Common/GraphicApi.cpp" )
list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "Source/Server/AppGui.h" )
list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "Source/Server/AppGui.cpp" )
list( REMOVE_ITEM SERVER_SOURCE_NO_GUI "Source/Server/AppGuiDX.cpp" )

# Client source
set( CLIENT_SOURCE
	${COMMON_SOURCE}
	Source/Client/Client.cpp Source/Client/Client.h
	Source/Client/ClientInterface.cpp
	Source/Client/SoundManager.cpp Source/Client/SoundManager.h
	Source/Client/MapView.cpp Source/Client/MapView.h
	Source/Client/CritterView.cpp Source/Client/CritterView.h
	Source/Client/ItemView.cpp Source/Client/ItemView.h
	Source/Client/ItemHexView.cpp Source/Client/ItemHexView.h
	Source/Client/Keyboard.cpp Source/Client/Keyboard.h
	Source/Client/HexManager.cpp Source/Client/HexManager.h
	Source/Client/ResourceManager.cpp Source/Client/ResourceManager.h
	Source/Client/GraphicLoader.cpp Source/Client/GraphicLoader.h
	Source/Client/3dStuff.cpp Source/Client/3dStuff.h
	Source/Client/SpriteManager.cpp Source/Client/SpriteManager.h
	Source/Client/SpriteManagerFont.cpp
	Source/Client/F2Palette_Include.h
	Source/Client/Sprites.cpp Source/Client/Sprites.h
	Source/Client/GluStuff.cpp Source/Client/GluStuff.h # Remove
)

# Editor source
set( EDITOR_SOURCE
	${CLIENT_SOURCE}
	${SERVER_SOURCE}
	Source/Editor/Mapper.cpp Source/Editor/Mapper.h
	Source/Editor/ResourceConverter.cpp Source/Editor/ResourceConverter.h
	Source/Editor/BuildSystem.cpp Source/Editor/BuildSystem.h
)
list( REMOVE_DUPLICATES EDITOR_SOURCE )

# Basic includes
include_directories( "Source/Common" )
include_directories( "Source/Server" )
include_directories( "Source/Client" )
include_directories( "Source/Editor" )

# Applications
StatusMessage( "Applications:" )

set( ALL_TARGETS "" )
set( RO0 RUNTIME_OUTPUT_DIRECTORY )
set( RO1 RUNTIME_OUTPUT_DIRECTORY_DEBUG )
set( RO2 RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL )
set( RO3 RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO )
set( RO4 RUNTIME_OUTPUT_DIRECTORY_RELEASE )

if( NOT FONLINE_BUILD_ONLY_EDITOR )
	if( NOT BUILD_CLIENT_LIBRARY )
		StatusMessage( "+ FOnline" )
		list( APPEND ALL_TARGETS "FOnline" )
		add_executable( FOnline WIN32 ${CLIENT_SOURCE} "Source/Applications/ClientApp.cpp" "Resources/Client.rc" )
		# Todo: Make bundles for Mac and maybe iOS
		# add_executable( FOnline MACOSX_BUNDLE ${CLIENT_SOURCE} "Client.rc" )
		set_target_properties( FOnline PROPERTIES ${RO0} ${CLIENT_OUTPUT} ${RO1} ${CLIENT_OUTPUT} ${RO2} ${CLIENT_OUTPUT} ${RO3} ${CLIENT_OUTPUT} ${RO4} ${CLIENT_OUTPUT} )
	else()
		StatusMessage( "+ FOnline (shared library)" )
		list( APPEND ALL_TARGETS "FOnline" )
		add_library( FOnline SHARED ${CLIENT_SOURCE} "Source/Applications/ClientApp.cpp" )
		set_target_properties( FOnline PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CLIENT_OUTPUT} )
	endif()
	set_target_properties( FOnline PROPERTIES OUTPUT_NAME "FOnline${CLIENT_POSTFIX}" )
	set_target_properties( FOnline PROPERTIES COMPILE_DEFINITIONS "${BUILD_DEFINES};FONLINE_CLIENT" )
	target_link_libraries( FOnline "ryml" "spark" "libmono" "ImGui" "fmt" "Angelscript" "AngelscriptExt" "${GLEW_LIB}" "zlibstatic" "SDL2main" "SDL2-static" "SHA" "${NCODEHOOK_LIB}" "${UNQLITE_LIB}" "${RENDER_LIBS}" "${CMAKE_DL_LIBS}" "${BACKWARDCPP_LIBS}" "${CLIENT_LIBS}" )
	target_link_libraries( FOnline "Acm" "Ogg" "Vorbis" "Theora" "${PNG16}" )
endif()

if( NOT BUILD_ONLY_CLIENT )
	if( NOT FONLINE_BUILD_ONLY_EDITOR )
		StatusMessage( "+ FOnlineServer" )
		list( APPEND ALL_TARGETS "FOnlineServer" )
		add_executable( FOnlineServer WIN32 ${SERVER_SOURCE} "Source/Applications/ServerApp.cpp" "Resources/Server.rc" )
		set_target_properties( FOnlineServer PROPERTIES ${RO0} ${SERVER_OUTPUT} ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
		set_target_properties( FOnlineServer PROPERTIES OUTPUT_NAME "FOnlineServer${NON_CLIENT_POSTFIX}" )
		set_target_properties( FOnlineServer PROPERTIES COMPILE_DEFINITIONS "${BUILD_DEFINES};FONLINE_SERVER" )
		target_link_libraries( FOnlineServer "ryml" "libmono" "ImGui" "fmt" "Angelscript" "AngelscriptExt" "${GLEW_LIB}" "zlibstatic" "${PNG16}" "SDL2main" "SDL2-static" "SHA" "libcurl" "${NCODEHOOK_LIB}" "${FBXSDK_LIB}" "assimp" "${RENDER_LIBS}" "mongoc_static" "bson_static" "${UNQLITE_LIB}" "ssl" "crypto" "${BACKWARDCPP_LIBS}" "${CMAKE_DL_LIBS}" )

		if( WIN32 )
			StatusMessage( "+ FOnlineServerService" )
			list( APPEND ALL_TARGETS "FOnlineServerService" )
			add_executable( FOnlineServerService ${SERVER_SOURCE_NO_GUI} "Source/Applications/ServerServiceApp.cpp" )
			set_target_properties( FOnlineServerService PROPERTIES ${RO0} ${SERVER_OUTPUT} ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
			set_target_properties( FOnlineServerService PROPERTIES OUTPUT_NAME "FOnlineServerService${NON_CLIENT_POSTFIX}" )
			set_target_properties( FOnlineServerService PROPERTIES COMPILE_DEFINITIONS "${BUILD_DEFINES};FONLINE_SERVER;FO_NO_GRAPHIC" )
			target_link_libraries( FOnlineServerService "ryml" "libmono" "ImGui" "fmt" "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SHA" "libcurl" "${NCODEHOOK_LIB}" "${FBXSDK_LIB}" "assimp" "mongoc_static" "bson_static" "${UNQLITE_LIB}" "ssl" "crypto" "${BACKWARDCPP_LIBS}" "${CMAKE_DL_LIBS}" )		else()
			StatusMessage( "+ FOnlineServerDaemon" )
			list( APPEND ALL_TARGETS "FOnlineServerDaemon" )
			add_executable( FOnlineServerDaemon ${SERVER_SOURCE_NO_GUI} "Source/Applications/ServerDaemonApp.cpp" )
			set_target_properties( FOnlineServerDaemon PROPERTIES ${RO0} ${SERVER_OUTPUT} ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
			set_target_properties( FOnlineServerDaemon PROPERTIES OUTPUT_NAME "FOnlineServerDaemon${NON_CLIENT_POSTFIX}" )
			set_target_properties( FOnlineServerDaemon PROPERTIES COMPILE_DEFINITIONS "${BUILD_DEFINES};FONLINE_SERVER;FO_NO_GRAPHIC" )
			target_link_libraries( FOnlineServerDaemon "ryml" "libmono" "ImGui" "fmt" "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SHA" "libcurl" "${NCODEHOOK_LIB}" "${FBXSDK_LIB}" "assimp" "mongoc_static" "bson_static" "${UNQLITE_LIB}" "ssl" "crypto" "${BACKWARDCPP_LIBS}" "${CMAKE_DL_LIBS}" )
		endif()

		if( BUILD_TEST_CASES )
			StatusMessage( "+ FOnlineUnitTests" )
			list( APPEND ALL_TARGETS "FOnlineUnitTests" )
			add_executable( FOnlineUnitTests ${EDITOR_SOURCE} "Source/Applications/ServerApp.cpp" "Source/Applications/ServerServiceApp.cpp" "Source/Applications/ServerDaemonApp.cpp" "Source/Applications/ClientApp.cpp" "Source/Applications/EditorApp.cpp" )
			set_target_properties( FOnlineUnitTests PROPERTIES ${RO0} ${TESTS_OUTPUT} ${RO1} ${TESTS_OUTPUT} ${RO2} ${TESTS_OUTPUT} ${RO3} ${TESTS_OUTPUT} ${RO4} ${EDITOR_OUTPUT} )
			set_target_properties( FOnlineUnitTests PROPERTIES OUTPUT_NAME "FOnlineUnitTests${NON_CLIENT_POSTFIX}" )
			set_target_properties( FOnlineUnitTests PROPERTIES COMPILE_DEFINITIONS "${BUILD_DEFINES};FONLINE_EDITOR;FO_TESTING" )
			target_link_libraries( FOnlineUnitTests "ryml" "spark" "libmono" "ImGui" "fmt" "Angelscript" "AngelscriptExt" "${GLEW_LIB}" "Ogg" "Vorbis" "Theora" "zlibstatic" "${PNG16}" "SDL2main" "SDL2-static" "Acm" "SHA" "${NCODEHOOK_LIB}" "${UNQLITE_LIB}" "${FBXSDK_LIB}" "assimp" "${RENDER_LIBS}" "ssl" "crypto" "mongoc_static" "bson_static" "libcurl" "${BACKWARDCPP_LIBS}" "${GLSLANG_LIBS}" "${SPIRV_CROSS_LIBS}" "${CMAKE_DL_LIBS}" )
		endif()
		if( BUILD_CODE_COVERAGE )
			StatusMessage( "+ FOnlineCodeCoverage" )
			list( APPEND ALL_TARGETS "FOnlineCodeCoverage" )
			add_executable( FOnlineCodeCoverage ${EDITOR_SOURCE} "Source/Applications/ServerApp.cpp" "Source/Applications/ServerServiceApp.cpp" "Source/Applications/ServerDaemonApp.cpp" "Source/Applications/ClientApp.cpp" "Source/Applications/EditorApp.cpp" )
			set_target_properties( FOnlineCodeCoverage PROPERTIES ${RO0} ${TESTS_OUTPUT} ${RO1} ${TESTS_OUTPUT} ${RO2} ${TESTS_OUTPUT} ${RO3} ${TESTS_OUTPUT} ${RO4} ${EDITOR_OUTPUT} )
			set_target_properties( FOnlineCodeCoverage PROPERTIES OUTPUT_NAME "FOnlineCodeCoverage${NON_CLIENT_POSTFIX}" )
			set_target_properties( FOnlineCodeCoverage PROPERTIES COMPILE_DEFINITIONS "${BUILD_DEFINES};FONLINE_EDITOR;FO_TESTING" )
			target_compile_options( FOnlineCodeCoverage PRIVATE "-O0;-fprofile-arcs;-ftest-coverage" )
			target_link_libraries( FOnlineCodeCoverage "gcov" "ryml" "spark" "libmono" "ImGui" "fmt" "Angelscript" "AngelscriptExt" "${GLEW_LIB}" "Ogg" "Vorbis" "Theora" "zlibstatic" "${PNG16}" "SDL2main" "SDL2-static" "Acm" "SHA" "${NCODEHOOK_LIB}" "${UNQLITE_LIB}" "${FBXSDK_LIB}" "assimp" "${RENDER_LIBS}" "ssl" "crypto" "mongoc_static" "bson_static" "libcurl" "${BACKWARDCPP_LIBS}" "${GLSLANG_LIBS}" "${SPIRV_CROSS_LIBS}" "${CMAKE_DL_LIBS}" )
		endif()
	endif()

	StatusMessage( "+ FOnlineEditor" )
	list( APPEND ALL_TARGETS "FOnlineEditor" )
	add_executable( FOnlineEditor WIN32 ${EDITOR_SOURCE} "Source/Applications/EditorApp.cpp" "Resources/Editor.rc" )
	set_target_properties( FOnlineEditor PROPERTIES ${RO0} ${EDITOR_OUTPUT} ${RO1} ${EDITOR_OUTPUT} ${RO2} ${EDITOR_OUTPUT} ${RO3} ${EDITOR_OUTPUT} ${RO4} ${EDITOR_OUTPUT} )
	set_target_properties( FOnlineEditor PROPERTIES OUTPUT_NAME "FOnlineEditor${NON_CLIENT_POSTFIX}" )
	set_target_properties( FOnlineEditor PROPERTIES COMPILE_DEFINITIONS "${BUILD_DEFINES};FONLINE_EDITOR" )
	target_link_libraries( FOnlineEditor "ryml" "spark" "libmono" "ImGui" "fmt" "Angelscript" "AngelscriptExt" "${GLEW_LIB}" "Ogg" "Vorbis" "Theora" "zlibstatic" "${PNG16}" "SDL2main" "SDL2-static" "Acm" "SHA" "${NCODEHOOK_LIB}" "${UNQLITE_LIB}" "${FBXSDK_LIB}" "assimp" "${RENDER_LIBS}" "ssl" "crypto" "mongoc_static" "bson_static" "libcurl" "${BACKWARDCPP_LIBS}" "${GLSLANG_LIBS}" "${SPIRV_CROSS_LIBS}" "${CMAKE_DL_LIBS}" )

	set_property( GLOBAL PROPERTY USE_FOLDERS ON )
	set_property( TARGET ${ALL_TARGETS} PROPERTY FOLDER "Binaries" )
	set_property( TARGET spark libmono ImGui fmt Acm Angelscript AngelscriptExt ${GLEW_LIB} Ogg Vorbis Theora ${NCODEHOOK_LIB} png16_static SDL2main SDL2-static SHA zlibstatic libcurl assimp IrrXML mongoc_static bson_static ${UNQLITE_LIB} mbedcrypto mbedtls mbedx509 ssl crypto ${GLSLANG_LIBS} ${SPIRV_CROSS_LIBS} PROPERTY FOLDER "Libraries" )
endif()
