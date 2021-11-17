cmake_minimum_required( VERSION 3.16.3 )
project( FOnlineProject )

# Options
set( FONLINE_VERBOSE_BUILD OFF CACHE BOOL "Verbose build mode" )
set( FONLINE_OUTPUT_PATH "${CMAKE_BINARY_DIR}" CACHE PATH "Common output path" )
set( FONLINE_BUILD_CLIENT OFF CACHE BOOL "Build Multiplayer client binaries" )
set( FONLINE_BUILD_SERVER OFF CACHE BOOL "Build Multiplayer server binaries" )
set( FONLINE_BUILD_SINGLE OFF CACHE BOOL "Build Singleplayer binaries" )
set( FONLINE_BUILD_MAPPER OFF CACHE BOOL "Build Mapper binaries" )
set( FONLINE_BUILD_ASCOMPILER OFF CACHE BOOL "Build AngelScript compiler" )
set( FONLINE_BUILD_BAKER OFF CACHE BOOL "Build Baker binaries" )
set( FONLINE_UNIT_TESTS ON CACHE BOOL "Build only binaries for Unit Testing" )
set( FONLINE_CODE_COVERAGE OFF CACHE BOOL "Build only binaries for Code Coverage reports" )
set( FONLINE_CMAKE_CONTRIBUTION "" CACHE FILEPATH "CMake contribution file" )
set( FONLINE_INFO_MARKDOWN_OUTPUT "" CACHE PATH "Path where information markdown files will be stored" )

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
	set( CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "Forced by FOnline" FORCE )
else()
	set( CMAKE_VERBOSE_MAKEFILE OFF CACHE BOOL "Forced by FOnline" FORCE )
endif()

# Global options
StatusMessage( "Start project generation" )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Forced by FOnline" FORCE ) # Generate compile_commands.json
set( BUILD_SHARED_LIBS OFF CACHE BOOL "Forced by FOnline" FORCE )
set( BUILD_TESTING OFF CACHE BOOL "Forced by FOnline" FORCE )

# Temporary variables
set( FO_DEBUG_BUILD YES )
set( FO_HEADLESS_ONLY YES )
set( FO_DEV_NAME "Unknown" )
set( FO_NICE_NAME "Unknown" )
set( FO_COMPANY_NAME "Unknown" )
set( FO_GAME_VERSION "0.0.1" )
set( FO_ENGINE_VERSION "" )
set( FO_MULTIPLAYER_SCRIPTING YES )
set( FO_SINGLEPLAYER_SCRIPTING YES )
set( FO_NATIVE_SCRIPTING YES )
set( FO_ANGELSCRIPT_SCRIPTING YES )
set( FO_MONO_SCRIPTING YES )
set( FO_OS "" )
set( FO_OS_UPPER "" )
set( FO_BUILD_CONFIGURATION "" )
set( FO_BUILD_LIBRARY NO )
set( FO_BUILD_OPTIONS "" )
set( FO_BUILD_LINKER_OPTIONS "" )
set( FO_BUILD_DEFINES "" )
set( FO_CLIENT_OUTPUT "" )
set( FO_SERVER_OUTPUT "" )
set( FO_SINGLE_OUTPUT "" )
set( FO_TOOLS_OUTPUT "" )
set( FO_TESTS_OUTPUT "" )
set( FO_SCRIPTS_OUTPUT "" )
set( FO_RESOURCES_OUTPUT "" )
set( FO_PACKAGES_OUTPUT "" )
set( FO_BUILD_PLATFORM "" )
set( FO_COMMON_SYSTEM_LIBS "" )
set( FO_COMMON_LIBS "" )
set( FO_SERVER_SYSTEM_LIBS "" )
set( FO_SERVER_LIBS "" )
set( FO_CLIENT_SYSTEM_LIBS "" )
set( FO_CLIENT_LIBS "" )
set( FO_RENDER_SYSTEM_LIBS "" )
set( FO_RENDER_LIBS "" )
set( FO_BAKER_SYSTEM_LIBS "" )
set( FO_BAKER_LIBS "" )
set( FO_USE_GLEW NO )
set( FO_GIT_HASH_RESULT "" )
set( FO_GIT_DIFF_RESULT "" )
set( FO_GIT_HASH "" )
set( FO_VERSION_CONTENT "" )
set( FO_SDL_DIR "" )
set( FO_ZLIB_DIR "" )
set( FO_PNG_DIR "" )
set( FO_OGG_DIR "" )
set( FO_VORBIS_DIR "" )
set( FO_THEORA_DIR "" )
set( FO_ACM_DIR "" )
set( FO_SHA_DIR "" )
set( FO_GLEW_DIR "" )
set( FO_NCODE_HOOK_DIR "" )
set( FO_FBXSDK_DIR "" )
set( FO_JSON_DIR "" )
set( FO_FMT_DIR "" )
set( FO_ASIO_DIR "" )
set( FO_WEBSOCKETS_DIR "" )
set( FO_MONGODB_DIR "" )
set( FO_UNQLITE_DIR "" )
set( FO_VARIANT_DIR "" )
set( FO_LIBRESSL_DIR "" )
set( FO_DEAR_IMGUI_DIR "" )
set( FO_CATCH2_DIR "" )
set( FO_BACKWARDCPP_DIR "" )
set( FO_SPARK_DIR "" )
set( FO_GLSLANG_DIR "" )
set( FO_SPIRV_CROSS_DIR "" )
set( FO_ANGELSCRIPT_DIR "" )
set( FO_ANGELSCRIPT_EXT_DIR "" )
set( FO_ANGELSCRIPT_SDK_DIR "" )
set( FO_ANGELSCRIPT_PREPROCESSOR_DIR "" )
set( FO_MONO_DIR "" )
set( FO_OGG_SOURCE "" )
set( FO_VORBIS_SOURCE "" )
set( FO_THEORA_SOURCE "" )
set( FO_GLEW_SOURCE "" )
set( FO_IMGUI_SOURCE "" )
set( FO_COMMON_SOURCE "" )
set( FO_SERVER_SOURCE "" )
set( FO_CLIENT_SOURCE "" )
set( FO_TOOLS_SOURCE "" )
set( FO_SCRIPTING_SOURCE "" )
set( FO_SINGLE_SOURCE "" )
set( FO_TESTS_SOURCE "" )
set( FO_LINT_SOURCE "" )
set( FO_CORE_LIBS_GROUP "" )
set( FO_COMMANDS_GROUP "" )
set( FO_APPLICATIONS_GROUP "" )
set( FO_CACHE_VARIABLES "" )
set( FO_CONTRIBUTION_DIR "" )
set( FO_CONTENT "" )
set( FO_RESOURCES "" )
set( FO_SPRITE_ATLAS "" )
set( FO_SCRIPT_API "" )
set( FO_ANGELSCRIPT_FILES "" )
set( FO_MONO_ASSEMBLIES "" )
set( FO_CONFIGS "" )
set( FO_PACKAGES "" )

# Some info about build
StatusMessage( "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}" )
StatusMessage( "Generator: ${CMAKE_GENERATOR}" )

# Build configuration
set( CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" )
if( CMAKE_BUILD_TYPE STREQUAL "" )
	set( CMAKE_BUILD_TYPE "Debug" CACHE STRING "Forced by FOnline" FORCE )
endif()
if( CMAKE_BUILD_TYPE STREQUAL "Debug" )
	set( FO_BUILD_CONFIGURATION "Debug" )
	set( FO_DEBUG_BUILD YES )
elseif( CMAKE_BUILD_TYPE STREQUAL "Release" )
	set( FO_BUILD_CONFIGURATION "Release" )
	set( FO_DEBUG_BUILD NO )
else()
	message( FATAL_ERROR "Invalid build type ${CMAKE_BUILD_TYPE}, allowed only Debug and Release" )
endif()
StatusMessage( "Configuration: ${FO_BUILD_CONFIGURATION}" )

set( CMAKE_CONFIGURATION_TYPES "${FO_BUILD_CONFIGURATION}" CACHE STRING "Forced by FOnline" FORCE )
set( CMAKE_BUILD_TYPE "${FO_BUILD_CONFIGURATION}" CACHE STRING "Forced by FOnline" FORCE )

# Basic setup
if( FO_DEBUG_BUILD )
	list( APPEND FO_BUILD_DEFINES "DEBUG" "_DEBUG" "FO_DEBUG=1" )
else()
	list( APPEND FO_BUILD_DEFINES "NDEBUG" "FO_DEBUG=0" )
endif()

# Headless configuration (without video/audio/input)
if( FONLINE_BUILD_CLIENT OR FONLINE_BUILD_SERVER OR FONLINE_BUILD_SINGLE OR FONLINE_BUILD_MAPPER )
	set( FO_HEADLESS_ONLY NO )
else()
	set( FO_HEADLESS_ONLY YES )
endif()

if( WIN32 AND NOT WINRT )
	StatusMessage( "Operating system: Windows" )
	set( FO_OS "Windows" )
	list( APPEND FO_BUILD_DEFINES "FO_WINDOWS=1" "FO_UWP=0" "FO_LINUX=0" "FO_MAC=0" "FO_ANDROID=0" "FO_IOS=0" "FO_WEB=0" "FO_PS4=0" )
	list( APPEND FO_BUILD_DEFINES "FO_HAVE_OPENGL=1" "FO_OPENGL_ES=0" "FO_HAVE_DIRECT_3D=1" "FO_HAVE_METAL=0" "FO_HAVE_VULKAN=0" "FO_HAVE_GNM=0" )
	if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		set( FO_BUILD_PLATFORM "Windows-win64" )
	else()
		set( FO_BUILD_PLATFORM "Windows-win32" )
	endif()
	if( FONLINE_BUILD_CLIENT OR FONLINE_BUILD_SINGLE )
		if( FO_DEBUG_BUILD )
			list( APPEND FO_BUILD_OPTIONS "/MTd" )
		else()
			list( APPEND FO_BUILD_OPTIONS "/MT" )
		endif()
	else()
		if( FO_DEBUG_BUILD )
			list( APPEND FO_BUILD_OPTIONS "/MDd" )
		else()
			list( APPEND FO_BUILD_OPTIONS "/MD" )
		endif()
	endif()
	if( FO_DEBUG_BUILD )
		list( APPEND FO_BUILD_OPTIONS "/RTC1" "/GS" "/JMC" )
		# Todo: "/RTCc" "/sdl" _ALLOW_RTCc_IN_STL
	else()
		list( APPEND FO_BUILD_OPTIONS "/sdl-" ) # "/GS-"
	endif()
	list( APPEND FO_BUILD_OPTIONS "/MP" "/Zi" "/EHsc" "/utf-8" "/volatile:iso" "/W3" "/GR" "/bigobj" "/fp:fast" )
	list( APPEND FO_BUILD_LINKER_OPTIONS "/DEBUG:FULL" "/INCREMENTAL:NO" "/OPT:REF" "/OPT:NOICF" )
	list( APPEND FO_BUILD_DEFINES "UNICODE" "_UNICODE" "_CRT_SECURE_NO_WARNINGS" "_CRT_SECURE_NO_DEPRECATE" "_WINSOCK_DEPRECATED_NO_WARNINGS" )
	list( APPEND FO_COMMON_SYSTEM_LIBS "user32" "ws2_32" "version" "winmm" "imm32" "dbghelp" "psapi" "xinput" )
	if( NOT FO_HEADLESS_ONLY )
		set( FO_USE_GLEW YES )
        list( APPEND FO_RENDER_SYSTEM_LIBS "glu32" "d3d9" "d3d11" "gdi32" "opengl32" )
    endif()
elseif( WIN32 AND WINRT )
	StatusMessage( "Operating system: Universal Windows Platform" )
	set( FO_OS "Windows" )
	list( APPEND FO_BUILD_DEFINES "FO_WINDOWS=1" "FO_UWP=1" "FO_LINUX=0" "FO_MAC=0" "FO_ANDROID=0" "FO_IOS=0" "FO_WEB=0" "FO_PS4=0" )
	list( APPEND FO_BUILD_DEFINES "FO_HAVE_OPENGL=0" "FO_OPENGL_ES=0" "FO_HAVE_DIRECT_3D=1" "FO_HAVE_METAL=0" "FO_HAVE_VULKAN=0" "FO_HAVE_GNM=0" )
	if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		set( FO_BUILD_PLATFORM "UWP-win64" )
	else()
		set( FO_BUILD_PLATFORM "UWP-win32" )
	endif()
	if( FO_DEBUG_BUILD )
		list( APPEND FO_BUILD_OPTIONS "/MDd" "/RTC1" "/GS" "/JMC" )
		# Todo: "/RTCc" "/sdl" _ALLOW_RTCc_IN_STL
	else()
		list( APPEND FO_BUILD_OPTIONS "/MD" "/sdl-" ) # "/GS-"
	endif()
	list( APPEND FO_BUILD_OPTIONS "/ZW" "/MP" "/Zi" "/EHsc" "/utf-8" "/volatile:iso" "/W3" "/GR" "/bigobj" "/fp:fast" )
	list( APPEND FO_BUILD_LINKER_OPTIONS "/APPCONTAINER" "/DEBUG:FULL" "/INCREMENTAL:NO" "/OPT:REF" "/OPT:NOICF" )
	list( APPEND FO_BUILD_DEFINES "UNICODE" "_UNICODE" "_CRT_SECURE_NO_WARNINGS" "_CRT_SECURE_NO_DEPRECATE" "_WINSOCK_DEPRECATED_NO_WARNINGS" )
	list( APPEND FO_COMMON_SYSTEM_LIBS "user32" "ws2_32" "version" "winmm" "imm32" "dbghelp" "psapi" "xinput" )
    if( NOT FO_HEADLESS_ONLY )
        list( APPEND FO_RENDER_SYSTEM_LIBS "d3d9" "gdi32" )
    endif()
elseif( CMAKE_SYSTEM_NAME MATCHES "Linux" )
	StatusMessage( "Operating system: Linux" )
	set( FO_OS "Linux" )
	list( APPEND FO_BUILD_DEFINES "FO_WINDOWS=0" "FO_UWP=0" "FO_LINUX=1" "FO_MAC=0" "FO_ANDROID=0" "FO_IOS=0" "FO_WEB=0" "FO_PS4=0" )
	list( APPEND FO_BUILD_DEFINES "FO_HAVE_OPENGL=1" "FO_OPENGL_ES=0" "FO_HAVE_DIRECT_3D=0" "FO_HAVE_METAL=0" "FO_HAVE_VULKAN=0" "FO_HAVE_GNM=0" )
	if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		set( FO_BUILD_PLATFORM "Linux-x64" )
	else()
		set( FO_BUILD_PLATFORM "Linux-x86" )
	endif()
    if( NOT FO_HEADLESS_ONLY )
	    find_package( X11 REQUIRED )
	    find_package( OpenGL REQUIRED )
	    set( FO_USE_GLEW YES )
	    list( APPEND FO_RENDER_SYSTEM_LIBS "GL" )
    endif()
	list( APPEND FO_BUILD_OPTIONS "-O2" "-g" )
	list( APPEND FO_BUILD_LINKER_OPTIONS "-no-pie" "-rdynamic" )
	if( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
		# Todo: rework and use only libc++ (we use libstdc++ because fbxsdk use it)
		if( FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE OR FONLINE_BUILD_BAKER )
			list( APPEND FO_BUILD_OPTIONS "-stdlib=libstdc++" )
		else()
			list( APPEND FO_BUILD_OPTIONS "-stdlib=libc++" )
		endif()
	endif()
elseif( APPLE AND NOT PLATFORM )
	StatusMessage( "Operating system: macOS" )
	if( NOT CMAKE_SIZEOF_VOID_P EQUAL 8 )
		message( FATAL_ERROR "Invalid pointer size for macOS build" )
	endif()
	set( FO_OS "Mac" )
	list( APPEND FO_BUILD_DEFINES "FO_WINDOWS=0" "FO_UWP=0" "FO_LINUX=0" "FO_MAC=1" "FO_ANDROID=0" "FO_IOS=0" "FO_WEB=0" "FO_PS4=0" )
	list( APPEND FO_BUILD_DEFINES "FO_HAVE_OPENGL=1" "FO_OPENGL_ES=0" "FO_HAVE_DIRECT_3D=0" "FO_HAVE_METAL=1" "FO_HAVE_VULKAN=0" "FO_HAVE_GNM=0" )
	set( FO_BUILD_PLATFORM "macOS-x64" )
    if( NOT FO_HEADLESS_ONLY )
	    find_package( OpenGL REQUIRED )
	    set( FO_USE_GLEW YES )
	    list( APPEND FO_RENDER_SYSTEM_LIBS ${OPENGL_LIBRARIES} )
    endif()
	list( APPEND FO_BUILD_OPTIONS "-stdlib=libc++" "-O2" "-g" )
	list( APPEND FO_BUILD_LINKER_OPTIONS "-rdynamic" )
elseif( APPLE AND PLATFORM )
	StatusMessage( "Operating system: iOS" )
	StatusMessage( "Deployment target: ${DEPLOYMENT_TARGET}" )
	if( NOT CMAKE_SIZEOF_VOID_P EQUAL 8 )
		message( FATAL_ERROR "Invalid pointer size for iOS build" )
	endif()
	set( FO_OS "iOS" )
	list( APPEND FO_BUILD_DEFINES "FO_WINDOWS=0" "FO_UWP=0" "FO_LINUX=0" "FO_MAC=0" "FO_ANDROID=0" "FO_IOS=1" "FO_WEB=0" "FO_PS4=0" )
	list( APPEND FO_BUILD_DEFINES "FO_HAVE_OPENGL=1" "FO_OPENGL_ES=1" "FO_HAVE_DIRECT_3D=0" "FO_HAVE_METAL=1" "FO_HAVE_VULKAN=0" "FO_HAVE_GNM=0" )
	if( PLATFORM STREQUAL "OS64" )
		StatusMessage( "Platform: Device" )
		set( FO_BUILD_PLATFORM "iOS-arm64" )
	elseif( PLATFORM STREQUAL "SIMULATOR64" )
		StatusMessage( "Platform: Simulator" )
		set( FO_BUILD_PLATFORM "iOS-simulator" )
	else()
		message( FATAL_ERROR "Invalid iOS target platform ${PLATFORM}" )
	endif()
    if( NOT FO_HEADLESS_ONLY )
	    find_library( OPENGLES OpenGLES )
	    find_library( METAL Metal )
	    find_library( COREGRAPGHICS CoreGraphics )
        find_library( QUARTZCORE QuartzCore )
        find_library( UIKIT UIKit )
        find_library( AVFOUNDATION AVFoundation )
        find_library( GAMECONTROLLER GameController )
        find_library( COREMOTION CoreMotion )
        list( APPEND FO_RENDER_SYSTEM_LIBS ${OPENGLES} ${METAL} ${COREGRAPGHICS} ${QUARTZCORE} ${UIKIT} ${AVFOUNDATION} ${GAMECONTROLLER} ${COREMOTION} )
        unset( OPENGLES )
        unset( METAL )
        unset( COREGRAPGHICS )
        unset( QUARTZCORE )
        unset( UIKIT )
        unset( AVFOUNDATION )
        unset( GAMECONTROLLER )
        unset( COREMOTION )
    endif()
	if( NOT FO_DEBUG_BUILD )
		list( APPEND FO_BUILD_OPTIONS "-O3" )
	else()
		list( APPEND FO_BUILD_OPTIONS "-g" )
	endif()
	list( APPEND FO_BUILD_OPTIONS "-stdlib=libc++" )
	list( APPEND FO_COMMON_SYSTEM_LIBS "iconv" ) # Todo: ios iconv workaround for SDL, remove in future updates
elseif( ANDROID )
	StatusMessage( "Operating system: Android" )
	set( FO_OS "Android" )
	list( APPEND FO_BUILD_DEFINES "FO_WINDOWS=0" "FO_UWP=0" "FO_LINUX=0" "FO_MAC=0" "FO_ANDROID=1" "FO_IOS=0" "FO_WEB=0" "FO_PS4=0" )
	list( APPEND FO_BUILD_DEFINES "FO_HAVE_OPENGL=1" "FO_OPENGL_ES=1" "FO_HAVE_DIRECT_3D=0" "FO_HAVE_METAL=0" "FO_HAVE_VULKAN=0" "FO_HAVE_GNM=0" )
	set( FO_BUILD_PLATFORM "Android-${ANDROID_ABI}" )
	set( FO_BUILD_LIBRARY YES )
    if( NOT FO_HEADLESS_ONLY )
	    list( APPEND FO_RENDER_SYSTEM_LIBS "GLESv1_CM" "GLESv2" )
    endif()
	list( APPEND FO_COMMON_SYSTEM_LIBS "android" "log" "atomic" )
	if( NOT FO_DEBUG_BUILD )
		list( APPEND FO_BUILD_OPTIONS "-O3" )
	else()
		list( APPEND FO_BUILD_OPTIONS "-g" )
	endif()
	list( APPEND FO_BUILD_LINKER_OPTIONS "-pie" )
elseif( EMSCRIPTEN )
	StatusMessage( "Operating system: Web" )
	set( FO_OS "Web" )
	list( APPEND FO_BUILD_DEFINES "FO_WINDOWS=0" "FO_UWP=0" "FO_LINUX=0" "FO_MAC=0" "FO_ANDROID=0" "FO_IOS=0" "FO_WEB=1" "FO_PS4=0" )
	list( APPEND FO_BUILD_DEFINES "FO_HAVE_OPENGL=1" "FO_OPENGL_ES=1" "FO_HAVE_DIRECT_3D=0" "FO_HAVE_METAL=0" "FO_HAVE_VULKAN=0" "FO_HAVE_GNM=0" )
	set( FO_BUILD_PLATFORM "Web-wasm" )
	set( CMAKE_EXECUTABLE_SUFFIX ".js" )
	if( NOT FO_DEBUG_BUILD )
		list( APPEND FO_BUILD_OPTIONS "-O3" )
	else()
		list( APPEND FO_BUILD_OPTIONS "-g1" ) # Todo: fix compilation with -g4
	endif()
	list( APPEND FO_BUILD_OPTIONS "-s INITIAL_MEMORY=268435456" ) # 256 Mb
	list( APPEND FO_BUILD_OPTIONS "-s ABORT_ON_WASM_EXCEPTIONS=1" )
	list( APPEND FO_BUILD_OPTIONS "-s ERROR_ON_UNDEFINED_SYMBOLS=1" )
	list( APPEND FO_BUILD_OPTIONS "-s ALLOW_MEMORY_GROWTH=1" )
	list( APPEND FO_BUILD_OPTIONS "-s USE_WEBGL2=1" )
	list( APPEND FO_BUILD_OPTIONS "-s FORCE_FILESYSTEM=1" )
	list( APPEND FO_BUILD_OPTIONS "-s NO_DYNAMIC_EXECUTION=1" )
	list( APPEND FO_BUILD_OPTIONS "-s USE_SDL_IMAGE=0" )
	list( APPEND FO_BUILD_OPTIONS "-s USE_SDL_TTF=0" )
	list( APPEND FO_BUILD_OPTIONS "-s USE_SDL_NET=0" )
	list( APPEND FO_BUILD_OPTIONS "-s NO_EXIT_RUNTIME=1" )
	list( APPEND FO_BUILD_OPTIONS "-s EXPORTED_RUNTIME_METHODS=\"['FS_createPath', 'FS_createDataFile']\"" )
	list( APPEND FO_BUILD_OPTIONS "-s EXTRA_EXPORTED_RUNTIME_METHODS=\"['intArrayFromString', 'UTF8ToString', 'addRunDependency', 'removeRunDependency', 'stackTrace']\"" )
	list( APPEND FO_BUILD_OPTIONS "--js-library \"${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/mono/repo/sdks/wasm/library_mono.js\"" )
	# list( APPEND FO_BUILD_OPTIONS "-s LLD_REPORT_UNDEFINED=1" )
	list( APPEND FO_BUILD_OPTIONS "--no-heap-copy" )
	# Todo: need?
	# list( APPEND FO_BUILD_OPTIONS "-s WASM=2" )
	# list( APPEND FO_BUILD_OPTIONS "-s DISABLE_EXCEPTION_CATCHING=0" )
	# Todo: uncomment these optimizations later, they may give side effects
	# list( APPEND FO_BUILD_OPTIONS "--closure 1" ) - conflicts with NO_DYNAMIC_EXECUTION
	# list( APPEND FO_BUILD_OPTIONS "-s CLOSURE_WARNINGS=error" )
	# list( APPEND FO_BUILD_OPTIONS "--llvm-lto 1" )
	# list( APPEND FO_BUILD_OPTIONS "-s INLINING_LIMIT=1" )
elseif( PS4 )
	StatusMessage( "Operating system: PS4" )
	set( FO_OS "PS4" )
	list( APPEND FO_BUILD_DEFINES "FO_WINDOWS=0" "FO_UWP=0" "FO_LINUX=0" "FO_MAC=0" "FO_ANDROID=0" "FO_IOS=0" "FO_WEB=0" "FO_PS4=1" )
	list( APPEND FO_BUILD_DEFINES "FO_HAVE_OPENGL=0" "FO_OPENGL_ES=0" "FO_HAVE_DIRECT_3D=0" "FO_HAVE_METAL=0" "FO_HAVE_VULKAN=0" "FO_HAVE_GNM=1" )
	set( FO_BUILD_PLATFORM "PS4-x64" )
	if( NOT FO_DEBUG_BUILD )
		list( APPEND FO_BUILD_OPTIONS "-O2" "-g" )
	else()
		list( APPEND FO_BUILD_OPTIONS "-O0" "-g" )
	endif()
else()
	AbortMessage( "Unknown OS!" )
endif()

string( TOUPPER "${FO_OS}" FO_OS_UPPER )
list( APPEND FO_BUILD_DEFINES "FO_${FO_OS_UPPER}" )

# Information about CPU architecture
if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
	StatusMessage( "CPU architecture: 64-bit" )
elseif( CMAKE_SIZEOF_VOID_P EQUAL 4 )
	StatusMessage( "CPU architecture: 32-bit" )
else()
	message( FATAL_ERROR "Invalid pointer size, nor 8 or 4 bytes" )
endif()

# Output path
StatusMessage( "Output path: ${FONLINE_OUTPUT_PATH}" )
set( FO_CLIENT_OUTPUT "${FONLINE_OUTPUT_PATH}/Client/${FO_BUILD_PLATFORM}-${FO_BUILD_CONFIGURATION}" )
set( FO_SERVER_OUTPUT "${FONLINE_OUTPUT_PATH}/Server/${FO_BUILD_PLATFORM}-${FO_BUILD_CONFIGURATION}" )
set( FO_SINGLE_OUTPUT "${FONLINE_OUTPUT_PATH}/Single/${FO_BUILD_PLATFORM}-${FO_BUILD_CONFIGURATION}" )
set( FO_TOOLS_OUTPUT "${FONLINE_OUTPUT_PATH}/Tools" )
set( FO_TESTS_OUTPUT "${FONLINE_OUTPUT_PATH}/Tests" )
set( FO_SCRIPTS_OUTPUT "${FONLINE_OUTPUT_PATH}/Scripts" )
set( FO_RESOURCES_OUTPUT "${FONLINE_OUTPUT_PATH}/Resources" )
set( FO_PACKAGES_OUTPUT "${FONLINE_OUTPUT_PATH}/Packages" )
file( MAKE_DIRECTORY "${FONLINE_OUTPUT_PATH}" )
file( MAKE_DIRECTORY "${FONLINE_OUTPUT_PATH}/Client" )
file( MAKE_DIRECTORY "${FONLINE_OUTPUT_PATH}/Server" )
file( MAKE_DIRECTORY "${FONLINE_OUTPUT_PATH}/Single" )
file( MAKE_DIRECTORY "${FO_CLIENT_OUTPUT}" )
file( MAKE_DIRECTORY "${FO_SERVER_OUTPUT}" )
file( MAKE_DIRECTORY "${FO_TOOLS_OUTPUT}" )
file( MAKE_DIRECTORY "${FO_TESTS_OUTPUT}" )
file( MAKE_DIRECTORY "${FO_SCRIPTS_OUTPUT}" )
file( MAKE_DIRECTORY "${FO_RESOURCES_OUTPUT}" )
file( MAKE_DIRECTORY "${FO_PACKAGES_OUTPUT}" )

# Contributions
macro( ResolveContributedFiles fileList )
	set( result "" )
	foreach( file ${fileList} )
		file( GLOB globFiles CONFIGURE_DEPENDS LIST_DIRECTORIES FALSE "${FO_CONTRIBUTION_DIR}/${file}" )
		list( APPEND result ${globFiles} )
	endforeach()
endmacro()

macro( SetupGame )
	set( allowedCommands "DEV_NAME" "NICE_NAME" "COMPANY_NAME" "GAME_VERSION" "ENGINE_VERSION"
		"MULTIPLAYER_SCRIPTING" "SINGLEPLAYER_SCRIPTING" "NATIVE_SCRIPTING" "ANGELSCRIPT_SCRIPTING" "MONO_SCRIPTING" )
	unset( opt )
	foreach( arg ${ARGN} )
		if( NOT opt )
			set( opt ${arg} )
			continue()
		endif()
		list( FIND allowedCommands ${opt} optIndex )
		if( NOT optIndex EQUAL -1 )
			set( FO_${opt} ${arg} )
		else()
			message( FATAL_ERROR "Invalid SetupGame option ${opt} = ${arg}" )
		endif()
		unset( opt )
	endforeach()
endmacro()
macro( AddContent )
	foreach( dir ${ARGN} )
		list( APPEND FO_CONTENT "${FO_CONTRIBUTION_DIR}/${dir}" )
	endforeach()
endmacro()
macro( AddResources packName )
	foreach( dir ${ARGN} )
		list( APPEND FO_RESOURCES "${packName},${FO_CONTRIBUTION_DIR}/${dir}" )
	endforeach()
endmacro()
macro( AddRawResources )
	foreach( dir ${ARGN} )
		list( APPEND FO_RESOURCES "Raw,${FO_CONTRIBUTION_DIR}/${dir}" )
	endforeach()
endmacro()
macro( AddScriptApi )
	ResolveContributedFiles( ${ARGN} )
	list( APPEND FO_SCRIPT_API ${result} )
endmacro()
macro( AddNativeIncludeDir target )
	# Todo:
	# list( TRANSFORM ARGN PREPEND ${FO_CONTRIBUTION_DIR} OUTPUT_VARIABLE result )
	# include_directories( ${result} )
endmacro()
macro( AddNativeSource target )
	# Todo:
	# ResolveContributedFiles( ${ARGN} )
endmacro()
macro( AddAngelScriptSource )
	ResolveContributedFiles( ${ARGN} )
	list( APPEND FO_ANGELSCRIPT_FILES ${result} )
endmacro()
macro( AddMonoAssembly assembly )
	list( APPEND FO_MONO_ASSEMBLIES ${assembly} )
	set( MonoAssembly_${assembly}_CommonRefs "" )
	set( MonoAssembly_${assembly}_ServerRefs "" )
	set( MonoAssembly_${assembly}_ClientRefs "" )
	set( MonoAssembly_${assembly}_SingleRefs "" )
	set( MonoAssembly_${assembly}_MapperRefs "" )
	set( MonoAssembly_${assembly}_CommonSource "" )
	set( MonoAssembly_${assembly}_ServerSource "" )
	set( MonoAssembly_${assembly}_ClientSource "" )
	set( MonoAssembly_${assembly}_SingleSource "" )
	set( MonoAssembly_${assembly}_MapperSource "" )
endmacro()
macro( AddMonoReference assembly target )
	list( APPEND MonoAssembly_${assembly}_${target}Refs ${ARGN} )
endmacro()
macro( AddMonoSource assembly target )
	ResolveContributedFiles( ${ARGN} )
	list( APPEND MonoAssembly_${assembly}_${target}Source ${result} )
endmacro()
macro( CreateConfig config inheritenConfig )
	list( APPEND FO_CONFIGS ${config} )
	set( Config_${config}_Inheriten ${inheritenConfig} )
endmacro()
macro( TweakConfig config option value )
	list( FIND Config_${config}_Options ${option} optionIndex )
	if( optionIndex EQUAL -1 )
		list( APPEND Config_${config}_Options ${option} )
	endif()
	set( Config_${config}___${option} ${value} )
endmacro()
macro( CreatePackage package config debug )
	list( APPEND FO_PACKAGES ${package} )
	set( Package_${package}_Config "${config}" )
	set( Package_${package}_Debug "${debug}" )
	set( Package_${package}_Parts "" )
endmacro()
macro( AddToPackage package binary platform arch packType )
	list( APPEND Package_${package}_Parts "${binary},${platform},${arch},${packType},${ARGN}" )
endmacro()

# User contribution
if( NOT FONLINE_CMAKE_CONTRIBUTION STREQUAL "" )
	StatusMessage( "Contributions:" )
	foreach( inc ${FONLINE_CMAKE_CONTRIBUTION} )
		get_filename_component( FO_CONTRIBUTION_DIR ${inc} DIRECTORY )
		StatusMessage( "+ ${inc}" )
		include( ${inc} )
	endforeach()
endif()

# Core contrinution
set( FO_CONTRIBUTION_DIR ${CMAKE_CURRENT_SOURCE_DIR} )
AddNativeIncludeDir( "Common" "Source/Scripting/Native" )
AddNativeSource( "Common" "Source/Scripting/Native/*.cpp" )
AddAngelScriptSource( "Source/Scripting/AngelScript/*.fos" )
AddMonoAssembly( "FOnline" )
AddMonoSource( "FOnline" "Common" "Source/Scripting/Mono/*.cs" )
AddResources( "Core" "Resources/Core" )

# Scripting defines
if( FO_NATIVE_SCRIPTING )
	list( APPEND FO_BUILD_DEFINES "FO_NATIVE_SCRIPTING" )
endif()
if( FO_ANGELSCRIPT_SCRIPTING )
	list( APPEND FO_BUILD_DEFINES "FO_ANGELSCRIPT_SCRIPTING" )
endif()
if( FO_MONO_SCRIPTING )
	list( APPEND FO_BUILD_DEFINES "FO_MONO_SCRIPTING" )
endif()

# Compiler options
if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
	list( APPEND FO_BUILD_OPTIONS "--param=max-vartrack-size=1000000" )
endif()
set( FO_BUILD_OPTIONS "${FO_BUILD_OPTIONS}" )
string( REPLACE ";" " " FO_BUILD_OPTIONS "${FO_BUILD_OPTIONS}" )
string( STRIP "${FO_BUILD_OPTIONS}" FO_BUILD_OPTIONS )
set( FO_BUILD_LINKER_OPTIONS "${FO_BUILD_LINKER_OPTIONS}" )
string( REPLACE ";" " " FO_BUILD_LINKER_OPTIONS "${FO_BUILD_LINKER_OPTIONS}" )
string( STRIP "${FO_BUILD_LINKER_OPTIONS}" FO_BUILD_LINKER_OPTIONS )
if( MSVC )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17 ${FO_BUILD_OPTIONS}" )
else()
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 ${FO_BUILD_OPTIONS}" )
endif()
string( STRIP "${CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS )
set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_FLAGS}" )
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS}" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FO_BUILD_OPTIONS}" )
set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${CMAKE_C_FLAGS}" )
set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${CMAKE_C_FLAGS}" )
if( MSVC )
	set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FO_BUILD_LINKER_OPTIONS}" )
else()
	set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FO_BUILD_OPTIONS} ${FO_BUILD_LINKER_OPTIONS}" )
endif()
set( CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${CMAKE_EXE_LINKER_FLAGS}" )
set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${CMAKE_EXE_LINKER_FLAGS}" )
set( CMAKE_CXX_STANDARD 17 )
StatusMessage( "Compiler flags (C++): ${CMAKE_CXX_FLAGS}" )
StatusMessage( "Compiler flags (C): ${CMAKE_C_FLAGS}" )
StatusMessage( "Linker flags: ${CMAKE_EXE_LINKER_FLAGS}" )

# Basic includes
include_directories( "Source/Common" )
include_directories( "Source/Server" )
include_directories( "Source/Client" )
include_directories( "Source/Tools" )
include_directories( "Source/Scripting" )

# Generated includes
macro( GenerateInclude includeName )
	set( GeneratedInclude_${includeName} "// Automatically generated, do not change manually\n" )
endmacro()
macro( AddGeneratedIncludeLine includeName line )
	set( GeneratedInclude_${includeName} "${GeneratedInclude_${includeName}}${line}\n" )
endmacro()
macro( FinalizeGeneratedInclude includeName )
	set( filePath "${CMAKE_BINARY_DIR}/GeneratedSource/${includeName}.h" )
	set( curContent "" )
	if( EXISTS ${filePath} )
		file( READ ${filePath} curContent )
	endif()
	if( NOT "${curContent}" STREQUAL "${GeneratedInclude_${includeName}}" )
		file( WRITE ${filePath} "${GeneratedInclude_${includeName}}" )
	endif()
endmacro()
include_directories( "${CMAKE_BINARY_DIR}/GeneratedSource" )

# Custom script API
GenerateInclude( "ScriptApi-Custom" )
foreach( headerPath ${FO_SCRIPT_API} )
	AddGeneratedIncludeLine( "ScriptApi-Custom" "#include \"${headerPath}\"" )
endforeach()
FinalizeGeneratedInclude( "ScriptApi-Custom" )

# Embedded resources
GenerateInclude( "EmbeddedResources-Include" )
file( GLOB bins "Resources/Embedded/*.zipped" )
foreach( bin ${bins} )
	string( REGEX MATCH "([^/]+)$" filename ${bin} )
	string( REGEX REPLACE "\\.| " "_" filename ${filename} )
	file( READ ${bin} filedata HEX )
	string( REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " filedata ${filedata} )
	AddGeneratedIncludeLine( "EmbeddedResources-Include" "const unsigned char Resource_${filename}[] = { ${filedata}};" )
endforeach()
FinalizeGeneratedInclude( "EmbeddedResources-Include" )

# Unique hash for build
if( FO_ENGINE_VERSION STREQUAL "" )
	execute_process( COMMAND git rev-parse --short=7 HEAD WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}" RESULT_VARIABLE FO_GIT_HASH_RESULT OUTPUT_VARIABLE FO_GIT_HASH )
	execute_process( COMMAND git diff-index --quiet HEAD WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}" RESULT_VARIABLE FO_GIT_DIFF_RESULT )
	string( STRIP "${FO_GIT_HASH}" FO_GIT_HASH )
	if( NOT FO_GIT_HASH_RESULT STREQUAL "0" OR NOT FO_GIT_DIFF_RESULT STREQUAL "0" )
		string( RANDOM LENGTH 5 ALPHABET "0123456789abcdef" SOME_RANDOM )
		set( FO_ENGINE_VERSION "fff${SOME_RANDOM}" CACHE STRING "Custom version for build (FO_VERSION in code), use hex digits (0-9, a-f), maximum are 8" )
	endif()
endif()

GenerateInclude( "Version-Include" )
if( FO_ENGINE_VERSION STREQUAL "" )
	if( $ENV{GITHUB_SHA} )
		StatusMessage( "Build version: ${FO_GIT_HASH} (commit hash at GitHub)" )
		AddGeneratedIncludeLine( "Version-Include" "static constexpr auto FO_VERSION = 0x${FO_GIT_HASH}; /* Commit hash at GitHub */" )
		AddGeneratedIncludeLine( "Version-Include" "static constexpr auto FO_VERSION_STR = \"${FO_GIT_HASH}\";" )
		AddGeneratedIncludeLine( "Version-Include" "static constexpr auto FO_GITHUB = true;" )
	else()
		StatusMessage( "Build version: ${FO_GIT_HASH} (commit hash)" )
		AddGeneratedIncludeLine( "Version-Include" "static constexpr auto FO_VERSION = 0x${FO_GIT_HASH}; /* Commit hash */" )
		AddGeneratedIncludeLine( "Version-Include" "static constexpr auto FO_VERSION_STR = \"${FO_GIT_HASH}\";" )
		AddGeneratedIncludeLine( "Version-Include" "static constexpr auto FO_GITHUB = false;" )
	endif()
else()
	StatusMessage( "Build version: ${FO_ENGINE_VERSION} (custom version)" )
	AddGeneratedIncludeLine( "Version-Include" "static constexpr auto FO_VERSION = 0x${FO_ENGINE_VERSION}; /* Custom version */" )
	AddGeneratedIncludeLine( "Version-Include" "static constexpr auto FO_VERSION_STR = \"${FO_ENGINE_VERSION}\";" )
	AddGeneratedIncludeLine( "Version-Include" "static constexpr auto FO_GITHUB = false;" )
endif()
FinalizeGeneratedInclude( "Version-Include" )

# Third-party libs
StatusMessage( "Third-party libs:" )

function( DisableLibWarnings )
	foreach( lib ${ARGV} )
		target_compile_options( ${lib} PRIVATE
			$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-w>
			$<$<CXX_COMPILER_ID:MSVC>:/W0> )
	endforeach()
endfunction()

# SDL2
StatusMessage( "+ SDL2" )
set( FO_SDL_DIR "ThirdParty/SDL2" )
set( SDL_SHARED OFF CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_STATIC ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_AUDIO ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_VIDEO ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_RENDER ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_EVENTS ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_LOADSO ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_ATOMIC OFF CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_JOYSTICK ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_HAPTIC ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_POWER ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_THREADS ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_TIMERS ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_FILE ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_CPUINFO OFF CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_FILESYSTEM OFF CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_DLOPEN ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_SENSOR ON CACHE BOOL "Forced by FOnline" FORCE )
set( RENDER_D3D OFF CACHE BOOL "Forced by FOnline" FORCE )
set( LIBC ON CACHE BOOL "Forced by FOnline" FORCE )
if( ANDROID )
	set( PTHREADS OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( HIDAPI OFF CACHE BOOL "Forced by FOnline" FORCE )
endif()
if( WIN32 AND WINRT )
	include( "BuildTools/sdl-winrt.cmake" )
	add_library( SDL2-static STATIC ${FO_SDL_WINRT_SOURCE} ${FO_SDL_WINRT_CX_SOURCE} )
	add_library( SDL2main STATIC "${FO_SDL_DIR}/src/main/winrt/SDL_winrt_main_NonXAML.cpp" )
	target_compile_options( SDL2-static PRIVATE "/std:c++14" )
	set_property( TARGET SDL2-static PROPERTY CXX_STANDARD 14 )
	target_compile_definitions( SDL2-static PRIVATE "_CRT_SECURE_NO_WARNINGS" )
else()
	add_subdirectory( "${FO_SDL_DIR}" )
endif()
include_directories( "${FO_SDL_DIR}/include" )
list( APPEND FO_BUILD_DEFINES "GL_GLEXT_PROTOTYPES" )
target_compile_definitions( SDL2main PRIVATE "GL_GLEXT_PROTOTYPES" )
target_compile_definitions( SDL2-static PRIVATE "GL_GLEXT_PROTOTYPES" )
list( APPEND FO_RENDER_LIBS "SDL2main" "SDL2-static" )
DisableLibWarnings( SDL2main SDL2-static )

# Zlib
StatusMessage( "+ Zlib" )
set( FO_ZLIB_DIR "ThirdParty/zlib" )
add_subdirectory( "${FO_ZLIB_DIR}" )
include_directories( "${FO_ZLIB_DIR}" "${FO_ZLIB_DIR}/contrib" "${CMAKE_BINARY_DIR}/${FO_ZLIB_DIR}" )
list( APPEND FO_COMMON_LIBS "zlibstatic" )
DisableLibWarnings( zlibstatic )

# PNG
if( FONLINE_BUILD_BAKER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
	StatusMessage( "+ PNG" )
	set( FO_PNG_DIR "ThirdParty/PNG" )
	set( SKIP_INSTALL_ALL ON CACHE BOOL "Forced by FOnline" FORCE )
	set( ZLIB_LIBRARY "zlibstatic" CACHE STRING "Forced by FOnline" FORCE )
	set( ZLIB_INCLUDE_DIR "../${FO_ZLIB_DIR}" "${CMAKE_BINARY_DIR}/${FO_ZLIB_DIR}" CACHE STRING "Forced by FOnline" FORCE )
	set( PNG_SHARED OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( PNG_STATIC ON CACHE BOOL "Forced by FOnline" FORCE )
	add_subdirectory( "${FO_PNG_DIR}" )
	include_directories( "${FO_PNG_DIR}" "${CMAKE_BINARY_DIR}/${FO_PNG_DIR}" )
	list( APPEND FO_BAKER_LIBS "png16_static" )
	DisableLibWarnings( png16_static )
endif()

# Ogg & Vorbis & Theora & Acm
# Todo: FONLINE_BUILD_BAKER
if( YES )
	# Ogg
	StatusMessage( "+ Ogg" )
	set( FO_OGG_DIR "ThirdParty/ogg" )
	file( GLOB FO_OGG_SOURCE "${FO_OGG_DIR}/src/*.c" )
	include_directories( "${FO_OGG_DIR}/include" )
	add_library( Ogg ${FO_OGG_SOURCE} )
	list( APPEND FO_CLIENT_LIBS "Ogg" )
	DisableLibWarnings( Ogg )

	# Vorbis
	StatusMessage( "+ Vorbis" )
	set( FO_VORBIS_DIR "ThirdParty/Vorbis" )
	file( GLOB FO_VORBIS_SOURCE "${FO_VORBIS_DIR}/lib/*.c" )
	list( REMOVE_ITEM FO_VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${FO_VORBIS_DIR}/lib/barkmel.c" )
	list( REMOVE_ITEM FO_VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${FO_VORBIS_DIR}/lib/psytune.c" )
	list( REMOVE_ITEM FO_VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${FO_VORBIS_DIR}/lib/tone.c" )
	include_directories( "${FO_VORBIS_DIR}/include" )
	include_directories( "${FO_VORBIS_DIR}/lib" )
	add_library( Vorbis ${FO_VORBIS_SOURCE} )
	target_link_libraries( Vorbis Ogg )
	list( APPEND FO_CLIENT_LIBS "Vorbis" )
	target_compile_definitions( Vorbis PRIVATE "_CRT_SECURE_NO_WARNINGS" )
	DisableLibWarnings( Vorbis )

	# Theora
	# StatusMessage( "+ Theora" )
	# set( FO_THEORA_DIR "ThirdParty/Theora" )
	# file( GLOB FO_THEORA_SOURCE "${FO_THEORA_DIR}/lib/*.c" )
	# list( REMOVE_ITEM FO_THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${FO_THEORA_DIR}/lib/analyze.c" )
	# list( REMOVE_ITEM FO_THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${FO_THEORA_DIR}/lib/encode.c" )
	# list( REMOVE_ITEM FO_THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${FO_THEORA_DIR}/lib/encapiwrapper.c" )
	# include_directories( "${FO_THEORA_DIR}/include" )
	# add_library( Theora ${FO_THEORA_SOURCE} )
	# list( APPEND FO_CLIENT_LIBS "Theora" )
	# DisableLibWarnings( Theora )

	# Acm
	StatusMessage( "+ Acm" )
	set( FO_ACM_DIR "ThirdParty/Acm" )
	add_subdirectory( "${FO_ACM_DIR}" )
	include_directories( "${FO_ACM_DIR}" )
	list( APPEND FO_CLIENT_LIBS "Acm" )
	DisableLibWarnings( Acm )
endif()

# SHA
StatusMessage( "+ SHA" )
set( FO_SHA_DIR "ThirdParty/SHA" )
add_subdirectory( "${FO_SHA_DIR}" )
include_directories( "${FO_SHA_DIR}" )
list( APPEND FO_COMMON_LIBS "SHA" )
DisableLibWarnings( SHA )

# GLEW
if( FO_USE_GLEW )
	StatusMessage( "+ GLEW" )
	set( FO_GLEW_DIR "ThirdParty/GLEW" )
	file( GLOB FO_GLEW_SOURCE "${FO_GLEW_DIR}/GL/*.*" )
	include_directories( "${FO_GLEW_DIR}" )
	add_library( GLEW ${FO_GLEW_SOURCE} )
	list( APPEND FO_BUILD_DEFINES "GLEW_STATIC" )
	target_compile_definitions( GLEW PRIVATE "GLEW_STATIC" )
	list( APPEND FO_RENDER_LIBS "GLEW" )
	DisableLibWarnings( GLEW )
endif()

# NCodeHook
if( WIN32 )
	StatusMessage( "+ NCodeHook" )
	set( FO_NCODE_HOOK_DIR "ThirdParty/NCodeHook" )
	add_subdirectory( "${FO_NCODE_HOOK_DIR}" )
	include_directories( "${FO_NCODE_HOOK_DIR}" )
	list( APPEND FO_COMMON_LIBS "NCodeHook" )
	DisableLibWarnings( NCodeHook )
endif()

StatusMessage( "+ Assimp headers" )
include_directories( "ThirdParty/AssimpMath" )

# Fbx SDK
if( FONLINE_BUILD_BAKER AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8 )
	message( FATAL_ERROR "Using of FBX SDK for non 64 bit builds is not supported" )
endif()
if( ( FONLINE_BUILD_BAKER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE ) AND ( WIN32 OR CMAKE_SYSTEM_NAME MATCHES "Linux" ) AND CMAKE_SIZEOF_VOID_P EQUAL 8 )
	StatusMessage( "+ Fbx SDK" )
	set( FO_FBXSDK_DIR "ThirdParty/fbxsdk" )
	include_directories( "${FO_FBXSDK_DIR}" )
	if( WIN32 )
		file( COPY "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/fbxsdk/libfbxsdk.dll" DESTINATION "${FO_TOOLS_OUTPUT}" )
		file( COPY "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/fbxsdk/libfbxsdk.dll" DESTINATION "${FO_TESTS_OUTPUT}" )
		list( APPEND FO_BAKER_SYSTEM_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/fbxsdk/libfbxsdk.lib" )
	else()
		list( APPEND FO_BAKER_SYSTEM_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/fbxsdk/libfbxsdk.so" )
	endif()
	list( APPEND FO_BUILD_DEFINES "FO_HAVE_FBXSDK=1" )
else()
	list( APPEND FO_BUILD_DEFINES "FO_HAVE_FBXSDK=0" )
endif()

# Nlohmann Json
StatusMessage( "+ Nlohmann Json" )
set( FO_JSON_DIR "ThirdParty/Json" )
include_directories( "${FO_JSON_DIR}" )
list( APPEND FO_BUILD_DEFINES "FO_HAVE_JSON=1" )

# Fmt
StatusMessage( "+ Fmt" )
set( FO_FMT_DIR "ThirdParty/fmt" )
add_subdirectory( "${FO_FMT_DIR}" )
include_directories( "${FO_FMT_DIR}/include" )
list( APPEND FO_COMMON_LIBS "fmt" )
DisableLibWarnings( fmt )

# LibreSSL
if( FONLINE_BUILD_SERVER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
	StatusMessage( "+ LibreSSL" )
	set( FO_LIBRESSL_DIR "ThirdParty/LibreSSL" )
	set( LIBRESSL_SKIP_INSTALL ON CACHE BOOL "Forced by FOnline" FORCE )
	set( LIBRESSL_APPS OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( LIBRESSL_TESTS OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( BUILD_SHARED_LIBS OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_ASM ON CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_EXTRATESTS OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_NC OFF CACHE BOOL "Forced by FOnline" FORCE )
	add_subdirectory( "${FO_LIBRESSL_DIR}" )
	include_directories( "${FO_LIBRESSL_DIR}" )
	include_directories( "${CMAKE_BINARY_DIR}/${FO_LIBRESSL_DIR}/crypto" )
	include_directories( "${CMAKE_BINARY_DIR}/${FO_LIBRESSL_DIR}/ssl" )
	list( APPEND FO_SERVER_LIBS "ssl" "crypto" "tls" "ssl_obj" "crypto_obj" "tls_obj" )
	DisableLibWarnings( ssl crypto tls ssl_obj crypto_obj tls_obj )
endif()

# Asio & Websockets
if( ( FONLINE_BUILD_SERVER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE ) AND NOT ANDROID )
	StatusMessage( "+ Asio" )
	set( FO_ASIO_DIR "ThirdParty/Asio" )
	include_directories( "${FO_ASIO_DIR}/include" )

	StatusMessage( "+ Websockets" )
	set( FO_WEBSOCKETS_DIR "ThirdParty/websocketpp" )
	include_directories( "${FO_WEBSOCKETS_DIR}" )

	list( APPEND FO_BUILD_DEFINES "FO_HAVE_ASIO=1" )
else()
	list( APPEND FO_BUILD_DEFINES "FO_HAVE_ASIO=0" )
endif()

# MongoDB & Bson
if( FONLINE_BUILD_SERVER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE OR FONLINE_BUILD_SINGLE )
	StatusMessage( "+ Bson" )
	set( FO_MONGODB_DIR "ThirdParty/mongo-c-driver" )

	set( ENABLE_BSON ON CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_STATIC BUILD_ONLY CACHE STRING "Forced by FOnline" FORCE )
	set( ENABLE_SRV OFF CACHE STRING "Forced by FOnline" FORCE )
	set( ENABLE_UNINSTALL OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_TESTS OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_EXAMPLES OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_SSL OFF CACHE STRING "Forced by FOnline" FORCE )
	set( ENABLE_SASL OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_ZLIB OFF CACHE STRING "Forced by FOnline" FORCE )

	if( FONLINE_BUILD_SERVER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
		StatusMessage( "+ MongoDB" )
		set( ENABLE_MONGOC ON CACHE STRING "Forced by FOnline" FORCE )
		list( APPEND FO_BUILD_DEFINES "FO_HAVE_MONGO=1" )
	else()
		set( ENABLE_MONGOC OFF CACHE STRING "Forced by FOnline" FORCE )
		list( APPEND FO_BUILD_DEFINES "FO_HAVE_MONGO=0" )
	endif()

	add_subdirectory( "${FO_MONGODB_DIR}" )

	include_directories( "${CMAKE_BINARY_DIR}/${FO_MONGODB_DIR}/src/libbson/src/bson" )
	include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MONGODB_DIR}/src/libbson/src" )
	target_compile_definitions( bson_static PRIVATE "BSON_COMPILATION;BSON_STATIC;JSONSL_PARSE_NAN" )
	list( APPEND FO_BUILD_DEFINES "BSON_COMPILATION" "BSON_STATIC" "JSONSL_PARSE_NAN" )
	list( APPEND FO_SERVER_LIBS "bson_static" )
	DisableLibWarnings( bson_static )

	if( FONLINE_BUILD_SERVER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
		include_directories( "${CMAKE_BINARY_DIR}/${FO_MONGODB_DIR}/src/libmongoc/src/mongoc" )
		include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MONGODB_DIR}/src/libmongoc/src" )
		target_compile_definitions( mongoc_static PRIVATE "BSON_COMPILATION;BSON_STATIC;JSONSL_PARSE_NAN" )
		list( APPEND FO_SERVER_LIBS "mongoc_static" )
		DisableLibWarnings( mongoc_static )
	endif()
endif()

# Unqlite
if( NOT EMSCRIPTEN )
	StatusMessage( "+ Unqlite" )
	set( FO_UNQLITE_DIR "ThirdParty/unqlite" )
	add_subdirectory( "${FO_UNQLITE_DIR}" )
	include_directories( "${FO_UNQLITE_DIR}" )
	list( APPEND FO_COMMON_LIBS "unqlite" )
	DisableLibWarnings( unqlite )
	list( APPEND FO_BUILD_DEFINES "FO_HAVE_UNQLITE=1" )
else()
	list( APPEND FO_BUILD_DEFINES "FO_HAVE_UNQLITE=0" )
endif()

# Dear ImGui
StatusMessage( "+ Dear ImGui" )
set( FO_DEAR_IMGUI_DIR "ThirdParty/imgui" )
file( GLOB FO_IMGUI_SOURCE "${FO_DEAR_IMGUI_DIR}/*.cpp" "${FO_DEAR_IMGUI_DIR}/*.h" )
#list( REMOVE_ITEM FO_IMGUI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${FO_DEAR_IMGUI_DIR}/imgui_demo.cpp" )
#list( REMOVE_ITEM FO_IMGUI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${FO_DEAR_IMGUI_DIR}/imgui_demo.h" )
include_directories( "${FO_DEAR_IMGUI_DIR}" )
add_library( ImGui ${FO_IMGUI_SOURCE} )
target_compile_definitions( ImGui PRIVATE "IMGUI_DISABLE_OBSOLETE_FUNCTIONS" )
list( APPEND FO_BUILD_DEFINES "IMGUI_DISABLE_OBSOLETE_FUNCTIONS" )
list( APPEND FO_COMMON_LIBS "ImGui" )
if( WIN32 AND WINRT )
	target_compile_definitions( ImGui PRIVATE "IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS" )
	list( APPEND FO_BUILD_DEFINES "IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS" )
endif()
DisableLibWarnings( ImGui )

# Catch2
StatusMessage( "+ Catch2" )
set( FO_CATCH2_DIR "ThirdParty/Catch2" )
include_directories( "${FO_CATCH2_DIR}/single_include/catch2" )

# Backward-cpp
if( WIN32 OR CMAKE_SYSTEM_NAME MATCHES "Linux" OR APPLE AND NOT PLATFORM )
	set( FO_BACKWARDCPP_DIR "ThirdParty/backward-cpp" )
	include_directories( "${FO_BACKWARDCPP_DIR}" )
	if( NOT WIN32 )
		check_include_file( "libunwind.h" haveLibUnwind )
		check_include_file( "bfd.h" haveBFD )
		if( haveLibUnwind )
			StatusMessage( "+ Backward-cpp (with libunwind)" )
		elif( haveBFD )
			StatusMessage( "+ Backward-cpp (with bfd)" )
			list( APPEND FO_COMMON_SYSTEM_LIBS "bfd" )
		else()
			StatusMessage( "+ Backward-cpp" )
		endif()
	else()
		StatusMessage( "+ Backward-cpp" )
	endif()
endif()

# Spark
StatusMessage( "+ Spark" )
set( FO_SPARK_DIR "ThirdParty/spark" )
set( SPARK_STATIC_BUILD ON CACHE BOOL "Forced by FOnline" FORCE )
add_subdirectory( "${FO_SPARK_DIR}/projects/engine/core" )
include_directories( "${CMAKE_BINARY_DIR}/${FO_SPARK_DIR}/include" )
list( APPEND FO_CLIENT_LIBS "SPARK_Core" )
DisableLibWarnings( SPARK_Core )

# glslang & SPIRV-Cross
if( FONLINE_BUILD_BAKER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
	StatusMessage( "+ glslang" )
	set( FO_GLSLANG_DIR "ThirdParty/glslang" )
	set( BUILD_EXTERNAL OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( SKIP_GLSLANG_INSTALL ON CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_HLSL "OFF" CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_SPVREMAPPER OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_AMD_EXTENSIONS OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_NV_EXTENSIONS OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_CTEST OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_OPT OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_PCH OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( ENABLE_CTEST OFF CACHE BOOL "Forced by FOnline" FORCE )
	if( EMSCRIPTEN )
		set( ENABLE_GLSLANG_WEB ON CACHE BOOL "Forced by FOnline" FORCE )
		set( ENABLE_GLSLANG_WEB_DEVEL ON CACHE BOOL "Forced by FOnline" FORCE )
		set( ENABLE_EMSCRIPTEN_SINGLE_FILE ON CACHE BOOL "Forced by FOnline" FORCE )
		set( ENABLE_EMSCRIPTEN_ENVIRONMENT_NODE OFF CACHE BOOL "Forced by FOnline" FORCE )
	endif()
	add_subdirectory( "${FO_GLSLANG_DIR}" )
	include_directories( "${FO_GLSLANG_DIR}/glslang/Public" )
	include_directories( "${FO_GLSLANG_DIR}/SPIRV" )
	list( APPEND FO_BAKER_LIBS "glslang" "OGLCompiler" "OSDependent" "SPIRV" )
	DisableLibWarnings( glslang OGLCompiler OSDependent SPIRV )

	StatusMessage( "+ SPIRV-Cross" )
	set( FO_SPIRV_CROSS_DIR "ThirdParty/SPIRV-Cross" )
	set( SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( SPIRV_CROSS_STATIC ON CACHE BOOL "Forced by FOnline" FORCE )
	set( SPIRV_CROSS_SHARED OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( SPIRV_CROSS_CLI OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "Forced by FOnline" FORCE )
	set( SPIRV_CROSS_ENABLE_HLSL ON CACHE BOOL "Forced by FOnline" FORCE )
	set( SPIRV_CROSS_ENABLE_MSL ON CACHE BOOL "Forced by FOnline" FORCE )
	set( SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( SPIRV_SKIP_TESTS ON CACHE BOOL "Forced by FOnline" FORCE )
	add_subdirectory( "${FO_SPIRV_CROSS_DIR}" )
	include_directories( "${FO_SPIRV_CROSS_DIR}" )
	include_directories( "${FO_SPIRV_CROSS_DIR}/include" )
	list( APPEND FO_BAKER_LIBS "spirv-cross-core" "spirv-cross-glsl" "spirv-cross-hlsl" "spirv-cross-msl" )
	DisableLibWarnings( spirv-cross-core spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl )
endif()

# AngelScript scripting
if( FO_ANGELSCRIPT_SCRIPTING )
	# AngelScript
	StatusMessage( "+ AngelScript" )
	set( FO_ANGELSCRIPT_DIR "ThirdParty/AngelScript" )
	add_subdirectory( "${FO_ANGELSCRIPT_DIR}/sdk/angelscript/projects/cmake" )
	include_directories( "${FO_ANGELSCRIPT_DIR}/sdk/angelscript/include" "${FO_ANGELSCRIPT_DIR}/sdk/angelscript/source" "${FO_ANGELSCRIPT_DIR}/sdk/add_on" )
	list( APPEND FO_COMMON_LIBS "Angelscript" )
	DisableLibWarnings( Angelscript )

	# AngelScriptExt
	StatusMessage( "+ AngelScriptExt" )
	set( FO_ANGELSCRIPT_EXT_DIR "Source/Common/AngelScriptExt" )
	set( FO_ANGELSCRIPT_PREPROCESSOR_DIR "ThirdParty/AngelScript/preprocessor" )
	set( FO_ANGELSCRIPT_SDK_DIR "ThirdParty/AngelScript/sdk" )
	include_directories( "${FO_ANGELSCRIPT_EXT_DIR}" )
	include_directories( "${FO_ANGELSCRIPT_PREPROCESSOR_DIR}" )
	include_directories( "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptstdstring" )
	include_directories( "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptarray" )
	include_directories( "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptdictionary" )
	include_directories( "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptfile" )
	include_directories( "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptany" )
	include_directories( "${FO_ANGELSCRIPT_SDK_DIR}/add_on/datetime" )
	include_directories( "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptmath" )
	include_directories( "${FO_ANGELSCRIPT_SDK_DIR}/add_on/weakref" )
	include_directories( "${FO_ANGELSCRIPT_SDK_DIR}/add_on/scripthelper" )
	file( GLOB FO_ANGELSCRIPT_EXT_SOURCE
		"${FO_ANGELSCRIPT_EXT_DIR}/*.*"
		"${FO_ANGELSCRIPT_PREPROCESSOR_DIR}/*.*"
		"${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptstdstring/*.*"
		"${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptarray/*.*"
		"${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptdictionary/*.*"
		"${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptfile/*.*"
		"${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptany/*.*"
		"${FO_ANGELSCRIPT_SDK_DIR}/add_on/datetime/*.*"
		"${FO_ANGELSCRIPT_SDK_DIR}/add_on/scriptmath/*.*"
		"${FO_ANGELSCRIPT_SDK_DIR}/add_on/weakref/*.*"
		"${FO_ANGELSCRIPT_SDK_DIR}/add_on/scripthelper/*.*" )
	add_library( AngelscriptExt ${FO_ANGELSCRIPT_EXT_SOURCE} )
	target_link_libraries( AngelscriptExt Angelscript )
	list( APPEND FO_COMMON_LIBS "AngelscriptExt" )
	target_compile_definitions( AngelscriptExt PRIVATE "_CRT_SECURE_NO_WARNINGS" "FO_${FO_OS_UPPER}" )
	DisableLibWarnings( AngelscriptExt )

	if( NOT FONLINE_BUILD_BAKER )
		target_compile_definitions( Angelscript PRIVATE "AS_NO_COMPILER" )
		target_compile_definitions( AngelscriptExt PRIVATE "AS_NO_COMPILER" )
		list( APPEND FO_BUILD_DEFINES "AS_NO_COMPILER" )
	endif()
	if( EMSCRIPTEN OR ( APPLE AND PLATFORM ) OR ( ANDROID AND CMAKE_SIZEOF_VOID_P EQUAL 8 ) )
		target_compile_definitions( Angelscript PRIVATE "AS_MAX_PORTABILITY" )
		target_compile_definitions( AngelscriptExt PRIVATE "AS_MAX_PORTABILITY" )
		list( APPEND FO_BUILD_DEFINES "AS_MAX_PORTABILITY" )
	endif()
	if( EMSCRIPTEN )
		target_compile_definitions( Angelscript PRIVATE "WIP_16BYTE_ALIGN" )
		target_compile_definitions( AngelscriptExt PRIVATE "WIP_16BYTE_ALIGN" )
		list( APPEND FO_BUILD_DEFINES "WIP_16BYTE_ALIGN" )
	endif()
endif()

# Mono scripting
if( FO_MONO_SCRIPTING )
	StatusMessage( "+ Mono" )
	set( FO_MONO_DIR "ThirdParty/mono" )
	add_subdirectory( "${FO_MONO_DIR}" )
	include_directories( "${FO_MONO_DIR}" )
	include_directories( "${FO_MONO_DIR}/repo" )
	include_directories( "${FO_MONO_DIR}/repo/mono" )
	include_directories( "${FO_MONO_DIR}/repo/mono/eglib" )
	list( APPEND FO_COMMON_LIBS "libmono" )
	list( APPEND FO_BUILD_DEFINES "HAVE_EXTERN_DEFINED_WINAPI_SUPPORT" )
	DisableLibWarnings( libmono )
endif()

# Engine sources
set( FO_COMMON_SOURCE
	"Source/Common/Application.cpp"
	"Source/Common/Application.h"
	"Source/Common/ApplicationHeadless.cpp"
	"Source/Common/CacheStorage.cpp"
	"Source/Common/CacheStorage.h"
	"Source/Common/Common.cpp"
	"Source/Common/Common.h"
	"Source/Common/ConfigFile.cpp"
	"Source/Common/ConfigFile.h"
	"Source/Common/DataSource.cpp"
	"Source/Common/DataSource.h"
	"Source/Common/DiskFileSystem.cpp"
	"Source/Common/DiskFileSystem.h"
	"Source/Common/Entity.cpp"
	"Source/Common/Entity.h"
	"Source/Common/FileSystem.cpp"
	"Source/Common/FileSystem.h"
	"Source/Common/GenericUtils.cpp"
	"Source/Common/GenericUtils.h"
	"Source/Common/GeometryHelper.cpp"
	"Source/Common/GeometryHelper.h"
	"Source/Common/KeyCodes-Include.h"
	"Source/Common/LineTracer.cpp"
	"Source/Common/LineTracer.h"
	"Source/Common/Log.cpp"
	"Source/Common/Log.h"
	"Source/Common/MapLoader.cpp"
	"Source/Common/MapLoader.h"
	"Source/Common/MsgFiles.cpp"
	"Source/Common/MsgFiles.h"
	"Source/Common/MsgStr-Include.h"
	"Source/Common/NetBuffer.cpp"
	"Source/Common/NetBuffer.h"
	"Source/Common/NetCommand.cpp"
	"Source/Common/NetCommand.h"
	"Source/Common/NetProtocol-Include.h"
	"Source/Common/Properties.cpp"
	"Source/Common/Properties.h"
	"Source/Common/ProtoManager.cpp"
	"Source/Common/ProtoManager.h"
	"Source/Common/ScriptSystem.cpp"
	"Source/Common/ScriptSystem.h"
	"Source/Common/Settings.cpp"
	"Source/Common/Settings.h"
	"Source/Common/Settings-Include.h"
	"Source/Common/StringUtils.cpp"
	"Source/Common/StringUtils.h"
	"Source/Common/Testing.cpp"
	"Source/Common/Testing.h"
	"Source/Common/Timer.cpp"
	"Source/Common/Timer.h"
	"Source/Common/TwoBitMask.cpp"
	"Source/Common/TwoBitMask.h"
	"Source/Common/UcsTables-Include.h"
	"Source/Common/WinApi-Include.h" )

list( REMOVE_ITEM FO_COMMON_SOURCE "Source/Common/Application.cpp" )
list( REMOVE_ITEM FO_COMMON_SOURCE "Source/Common/ApplicationHeadless.cpp" )

set( FO_SERVER_SOURCE
	"Source/Server/AdminPanel.cpp"
	"Source/Server/AdminPanel.h"
	"Source/Server/AppGui.cpp"
	"Source/Server/AppGui.h"
	"Source/Server/AppGuiDX.cpp"
	"Source/Server/AppGuiStub.cpp"
	"Source/Server/ClientConnection.cpp"
	"Source/Server/ClientConnection.h"
	"Source/Server/Critter.cpp"
	"Source/Server/Critter.h"
	"Source/Server/CritterManager.cpp"
	"Source/Server/CritterManager.h"
	"Source/Server/DataBase.cpp"
	"Source/Server/DataBase.h"
	"Source/Server/Dialogs.cpp"
	"Source/Server/Dialogs.h"
	"Source/Server/EntityManager.cpp"
	"Source/Server/EntityManager.h"
	"Source/Server/Item.cpp"
	"Source/Server/Item.h"
	"Source/Server/ItemManager.cpp"
	"Source/Server/ItemManager.h"
	"Source/Server/Location.cpp"
	"Source/Server/Location.h"
	"Source/Server/Map.cpp"
	"Source/Server/Map.h"
	"Source/Server/MapManager.cpp"
	"Source/Server/MapManager.h"
	"Source/Server/Networking.cpp"
	"Source/Server/Networking.h"
	"Source/Server/Player.cpp"
	"Source/Server/Player.h"
	"Source/Server/PropertiesSerializator.cpp"
	"Source/Server/PropertiesSerializator.h"
	"Source/Server/Server.cpp"
	"Source/Server/Server.h"
	"Source/Server/ServerScripting.h" )

list( REMOVE_ITEM FO_SERVER_SOURCE "Source/Server/AppGui.cpp" )
list( REMOVE_ITEM FO_SERVER_SOURCE "Source/Server/AppGuiDX.cpp" )
list( REMOVE_ITEM FO_SERVER_SOURCE "Source/Server/AppGuiStub.cpp" )

set( FO_CLIENT_SOURCE
	"Source/Client/3dAnimation.cpp"
	"Source/Client/3dAnimation.h"
	"Source/Client/3dStuff.cpp"
	"Source/Client/3dStuff.h"
	"Source/Client/Client.cpp"
	"Source/Client/Client.h"
	"Source/Client/ClientScripting.h"
	"Source/Client/CritterView.cpp"
	"Source/Client/CritterView.h"
	"Source/Client/EffectManager.cpp"
	"Source/Client/EffectManager.h"
	"Source/Client/HexManager.cpp"
	"Source/Client/HexManager.h"
	"Source/Client/ItemHexView.cpp"
	"Source/Client/ItemHexView.h"
	"Source/Client/ItemView.cpp"
	"Source/Client/ItemView.h"
	"Source/Client/Keyboard.cpp"
	"Source/Client/Keyboard.h"
	"Source/Client/LocationView.cpp"
	"Source/Client/LocationView.h"
	"Source/Client/MapView.cpp"
	"Source/Client/MapView.h"
	"Source/Client/PlayerView.cpp"
	"Source/Client/PlayerView.h"
	"Source/Client/ResourceManager.cpp"
	"Source/Client/ResourceManager.h"
	"Source/Client/SoundManager.cpp"
	"Source/Client/SoundManager.h"
	"Source/Client/SpriteManager.cpp"
	"Source/Client/SpriteManager.h"
	"Source/Client/Sprites.cpp"
	"Source/Client/Sprites.h" )

set( FO_TOOLS_SOURCE
	"Source/Tools/EffectBaker.h"
	"Source/Tools/EffectBaker.cpp"
	"Source/Tools/F2Palette-Include.h"
	"Source/Tools/ImageBaker.h"
	"Source/Tools/ImageBaker.cpp"
	"Source/Tools/Mapper.h"
	"Source/Tools/Mapper.cpp"
	"Source/Tools/MapperScripting.h"
	"Source/Tools/ModelBaker.h"
	"Source/Tools/ModelBaker.cpp" )

set( FO_SCRIPTING_SOURCE
	"Source/Scripting/AngelScriptScripting.cpp"
	"Source/Scripting/MonoScripting.cpp"
	"Source/Scripting/NativeScripting.cpp" )

set( FO_TESTS_SOURCE
	"Source/Tests/UnitTest1.cpp" )

set( FO_LINT_SOURCE
	"Source/Scripting/ScriptingLint.cpp"
	"Source/Scripting/ScriptApi-Client.h"
	"Source/Scripting/ScriptApi-Common.h"
	"Source/Scripting/ScriptApi-Enums.h"
	"Source/Scripting/ScriptApi-Events.h"
	"Source/Scripting/ScriptApi-Mapper.h"
	"Source/Scripting/ScriptApi-Properties.h"
	"Source/Scripting/ScriptApi-Server.h"
	"${CMAKE_BINARY_DIR}/GeneratedSource/ScriptApi-Custom.h" )

# Make symlink of scripting API files for correct linting
function( MirrorSource fileDir fileName fileExt postfix )
	if( NOT EXISTS "${CMAKE_BINARY_DIR}/GeneratedSource/${fileName}-${postfix}.${fileExt}" )
		file( CREATE_LINK "${fileDir}/${fileName}.${fileExt}" "${CMAKE_BINARY_DIR}/GeneratedSource/${fileName}-${postfix}.${fileExt}" COPY_ON_ERROR SYMBOLIC )
	endif()
endfunction()

MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "ScriptApi-Client" "h" "Mirror" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "ScriptApi-Common" "h" "Mirror" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "ScriptApi-Enums" "h" "Mirror" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "ScriptApi-Events" "h" "Mirror" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "ScriptApi-Mapper" "h" "Mirror" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "ScriptApi-Properties" "h" "Mirror" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "ScriptApi-Server" "h" "Mirror" )
MirrorSource( "${CMAKE_BINARY_DIR}/GeneratedSource" "ScriptApi-Custom" "h" "Mirror" )

MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Server" "ServerScripting" "h" "AngelScript" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Server" "ServerScripting" "h" "Mono" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Server" "ServerScripting" "h" "Native" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Client" "ClientScripting" "h" "AngelScript" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Client" "ClientScripting" "h" "Mono" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Client" "ClientScripting" "h" "Native" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Tools" "MapperScripting" "h" "AngelScript" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Tools" "MapperScripting" "h" "Mono" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Tools" "MapperScripting" "h" "Native" )

MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "AngelScriptScripting" "cpp" "Server" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "AngelScriptScripting" "cpp" "Client" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "AngelScriptScripting" "cpp" "Mapper" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "AngelScriptScripting" "cpp" "Single" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "AngelScriptScripting" "cpp" "ServerCompiler" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "AngelScriptScripting" "cpp" "ClientCompiler" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "AngelScriptScripting" "cpp" "MapperCompiler" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "MonoScripting" "cpp" "Server" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "MonoScripting" "cpp" "Client" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "MonoScripting" "cpp" "Mapper" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "MonoScripting" "cpp" "Single" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "NativeScripting" "cpp" "Server" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "NativeScripting" "cpp" "Client" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "NativeScripting" "cpp" "Mapper" )
MirrorSource( "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting" "NativeScripting" "cpp" "Single" )

# Applications
StatusMessage( "Applications:" )

if( FONLINE_BUILD_CLIENT OR FONLINE_BUILD_SERVER OR FONLINE_BUILD_MAPPER OR FONLINE_BUILD_ASCOMPILER OR FONLINE_BUILD_BAKER )
	add_library( CommonLib STATIC ${FO_COMMON_SOURCE} )
	set_target_properties( CommonLib PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=0" )
	target_link_libraries( CommonLib ${FO_COMMON_SYSTEM_LIBS} ${FO_COMMON_LIBS} )
	list( APPEND FO_CORE_LIBS_GROUP "CommonLib" )
endif()
if( FONLINE_BUILD_CLIENT OR FONLINE_BUILD_MAPPER )
	add_library( ClientLib STATIC ${FO_CLIENT_SOURCE} )
	set_target_properties( ClientLib PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=0" )
	target_link_libraries( ClientLib ${FO_CLIENT_SYSTEM_LIBS} ${FO_CLIENT_LIBS} )
	list( APPEND FO_CORE_LIBS_GROUP "ClientLib" )
endif()
if( FONLINE_BUILD_SERVER )
	add_library( ServerLib STATIC ${FO_SERVER_SOURCE} )
	set_target_properties( ServerLib PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=0" )
	target_link_libraries( ServerLib ${FO_SERVER_SYSTEM_LIBS} ${FO_SERVER_LIBS} )
	list( APPEND FO_CORE_LIBS_GROUP "ServerLib" )
endif()

if( FONLINE_BUILD_CLIENT OR FONLINE_BUILD_MAPPER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
	add_library( ClientScriptingLib STATIC "${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Client.cpp" "${CMAKE_BINARY_DIR}/GeneratedSource/MonoScripting-Client.cpp" "${CMAKE_BINARY_DIR}/GeneratedSource/NativeScripting-Client.cpp" "${CMAKE_BINARY_DIR}/GeneratedSource/ClientScripting-AngelScript.h" "${CMAKE_BINARY_DIR}/GeneratedSource/ClientScripting-Mono.h" "${CMAKE_BINARY_DIR}/GeneratedSource/ClientScripting-Native.h" )
	set_target_properties( ClientScriptingLib PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_SERVER_SCRIPTING=0;FO_CLIENT_SCRIPTING=1;FO_MAPPER_SCRIPTING=0;FO_SINGLEPLAYER_SCRIPTING=0;FO_ANGELSCRIPT_COMPILER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=0" )
	list( APPEND FO_CORE_LIBS_GROUP "ClientScriptingLib" )
endif()
if( FONLINE_BUILD_SERVER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
	add_library( ServerScriptingLib STATIC "${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Server.cpp" "${CMAKE_BINARY_DIR}/GeneratedSource/MonoScripting-Server.cpp" "${CMAKE_BINARY_DIR}/GeneratedSource/NativeScripting-Server.cpp" "${CMAKE_BINARY_DIR}/GeneratedSource/ServerScripting-AngelScript.h" "${CMAKE_BINARY_DIR}/GeneratedSource/ServerScripting-Mono.h" "${CMAKE_BINARY_DIR}/GeneratedSource/ServerScripting-Native.h" )
	set_target_properties( ServerScriptingLib PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_SERVER_SCRIPTING=1;FO_CLIENT_SCRIPTING=0;FO_MAPPER_SCRIPTING=0;FO_SINGLEPLAYER_SCRIPTING=0;FO_ANGELSCRIPT_COMPILER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=0" )
	list( APPEND FO_CORE_LIBS_GROUP "ServerScriptingLib" )
endif()
if( FONLINE_BUILD_MAPPER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
	add_library( MapperScriptingLib STATIC "${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Mapper.cpp" "${CMAKE_BINARY_DIR}/GeneratedSource/MonoScripting-Mapper.cpp" "${CMAKE_BINARY_DIR}/GeneratedSource/NativeScripting-Mapper.cpp" "${CMAKE_BINARY_DIR}/GeneratedSource/MapperScripting-AngelScript.h" "${CMAKE_BINARY_DIR}/GeneratedSource/MapperScripting-Mono.h" "${CMAKE_BINARY_DIR}/GeneratedSource/MapperScripting-Native.h" )
	set_target_properties( MapperScriptingLib PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_SERVER_SCRIPTING=0;FO_CLIENT_SCRIPTING=0;FO_MAPPER_SCRIPTING=1;FO_SINGLEPLAYER_SCRIPTING=0;FO_ANGELSCRIPT_COMPILER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=1" )
	list( APPEND FO_CORE_LIBS_GROUP "MapperScriptingLib" )
endif()

if( FONLINE_BUILD_CLIENT )
	if( NOT FO_BUILD_LIBRARY )
		StatusMessage( "+ FOnlineClient" )
		list( APPEND FO_APPLICATIONS_GROUP "FOnlineClient" )
		add_executable( FOnlineClient WIN32 "Source/Applications/ClientApp.cpp" "Source/Common/Application.cpp" "Resources/Default.rc" )
		# Todo: cmake make bundles for Mac and iOS
		# add_executable( FOnlineClient MACOSX_BUNDLE ... "Default.rc" )
		set_target_properties( FOnlineClient PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_CLIENT_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_DEBUG ${FO_CLIENT_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_RELEASE ${FO_CLIENT_OUTPUT} )
	else()
		StatusMessage( "+ FOnlineClient (shared library)" )
		list( APPEND FO_APPLICATIONS_GROUP "FOnlineClient" )
		add_library( FOnlineClient SHARED "Source/Applications/ClientApp.cpp" "Source/Common/Application.cpp" )
		set_target_properties( FOnlineClient PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${FO_CLIENT_OUTPUT} )
	endif()
	set_target_properties( FOnlineClient PROPERTIES OUTPUT_NAME "FOnlineClient" )
	set_target_properties( FOnlineClient PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=0" )
	target_link_libraries( FOnlineClient "ClientLib" "ClientScriptingLib" "CommonLib" "${FO_RENDER_SYSTEM_LIBS}" "${FO_RENDER_LIBS}" "${CMAKE_DL_LIBS}" )
endif()

if( FONLINE_BUILD_SERVER )
	StatusMessage( "+ FOnlineServer" )
	list( APPEND FO_APPLICATIONS_GROUP "FOnlineServer" )
	add_executable( FOnlineServer WIN32 "Source/Applications/ServerApp.cpp" "Source/Common/Application.cpp" "Source/Server/AppGui.cpp" "Source/Server/AppGuiDX.cpp" "Resources/Default.rc" )
	set_target_properties( FOnlineServer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SERVER_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_DEBUG ${FO_SERVER_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_RELEASE ${FO_SERVER_OUTPUT} )
	set_target_properties( FOnlineServer PROPERTIES OUTPUT_NAME "FOnlineServer" )
	set_target_properties( FOnlineServer PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=0" )
	target_link_libraries( FOnlineServer "ServerLib" "ServerScriptingLib" "CommonLib" "${FO_RENDER_SYSTEM_LIBS}" "${FO_RENDER_LIBS}" "${CMAKE_DL_LIBS}" )

	StatusMessage( "+ FOnlineServerHeadless" )
	list( APPEND FO_APPLICATIONS_GROUP "FOnlineServerHeadless" )
	add_executable( FOnlineServerHeadless "Source/Applications/ServerHeadlessApp.cpp" "Source/Common/ApplicationHeadless.cpp" "Source/Server/AppGuiStub.cpp" )
	set_target_properties( FOnlineServerHeadless PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SERVER_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_DEBUG ${FO_SERVER_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_RELEASE ${FO_SERVER_OUTPUT} )
	set_target_properties( FOnlineServerHeadless PROPERTIES OUTPUT_NAME "FOnlineServerHeadless" )
	set_target_properties( FOnlineServerHeadless PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=0" )
	target_link_libraries( FOnlineServerHeadless "ServerLib" "ServerScriptingLib" "CommonLib" "${CMAKE_DL_LIBS}" )

	if( WIN32 )
		StatusMessage( "+ FOnlineServerService" )
		list( APPEND FO_APPLICATIONS_GROUP "FOnlineServerService" )
		add_executable( FOnlineServerService "Source/Applications/ServerServiceApp.cpp" "Source/Common/ApplicationHeadless.cpp" "Source/Server/AppGuiStub.cpp" )
		set_target_properties( FOnlineServerService PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SERVER_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_DEBUG ${FO_SERVER_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_RELEASE ${FO_SERVER_OUTPUT} )
		set_target_properties( FOnlineServerService PROPERTIES OUTPUT_NAME "FOnlineServerService" )
		set_target_properties( FOnlineServerService PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=0" )
		target_link_libraries( FOnlineServerService "ServerLib" "ServerScriptingLib" "CommonLib" "${CMAKE_DL_LIBS}" )
	else()
		StatusMessage( "+ FOnlineServerDaemon" )
		list( APPEND FO_APPLICATIONS_GROUP "FOnlineServerDaemon" )
		add_executable( FOnlineServerDaemon "Source/Applications/ServerDaemonApp.cpp" "Source/Common/ApplicationHeadless.cpp" "Source/Server/AppGuiStub.cpp" )
		set_target_properties( FOnlineServerDaemon PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SERVER_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_DEBUG ${FO_SERVER_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_RELEASE ${FO_SERVER_OUTPUT} )
		set_target_properties( FOnlineServerDaemon PROPERTIES OUTPUT_NAME "FOnlineServerDaemon" )
		set_target_properties( FOnlineServerDaemon PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=0" )
		target_link_libraries( FOnlineServerDaemon "ServerLib" "ServerScriptingLib" "CommonLib" "${CMAKE_DL_LIBS}" )
	endif()
endif()

if( FONLINE_BUILD_SINGLE )
	set( FO_SINGLE_SOURCE ${FO_SERVER_SOURCE} ${FO_CLIENT_SOURCE} ${FO_COMMON_SOURCE} )
	list( APPEND FO_SINGLE_SOURCE "Source/Common/Application.cpp" "Source/Server/AppGuiStub.cpp" )

	add_library( SingleScriptingLib STATIC "${CMAKE_BINARY_DIR}/GeneratedSource/NativeScripting-Single.cpp" "${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Single.cpp" "${CMAKE_BINARY_DIR}/GeneratedSource/MonoScripting-Single.cpp" )
	set_target_properties( SingleScriptingLib PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=1;FO_SERVER_SCRIPTING=0;FO_CLIENT_SCRIPTING=0;FO_MAPPER_SCRIPTING=0;FO_SINGLEPLAYER_SCRIPTING=1;FO_ANGELSCRIPT_COMPILER=0;FO_API_MULTIPLAYER_ONLY=0;FO_API_SINGLEPLAYER_ONLY=1" )
	list( APPEND FO_CORE_LIBS_GROUP "SingleScriptingLib" )

	if( NOT FO_BUILD_LIBRARY )
		StatusMessage( "+ FOnlineSingle" )
		list( APPEND FO_APPLICATIONS_GROUP "FOnlineSingle" )
		add_executable( FOnlineSingle WIN32 "Source/Applications/ClientApp.cpp" ${FO_SINGLE_SOURCE} "Resources/Default.rc" )
		set_target_properties( FOnlineSingle PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SINGLE_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_DEBUG ${FO_SINGLE_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_RELEASE ${FO_SINGLE_OUTPUT} )
	else()
		StatusMessage( "+ FOnlineSingle (shared library)" )
		list( APPEND FO_APPLICATIONS_GROUP "FOnlineSingle" )
		add_library( FOnlineSingle SHARED "Source/Applications/ClientApp.cpp" ${FO_SINGLE_SOURCE} )
		set_target_properties( FOnlineSingle PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${FO_SINGLE_OUTPUT} )
	endif()
	set_target_properties( FOnlineSingle PROPERTIES OUTPUT_NAME "FOnlineSingle" )
	set_target_properties( FOnlineSingle PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=1;FO_API_MULTIPLAYER_ONLY=0;FO_API_SINGLEPLAYER_ONLY=1" )
	target_link_libraries( FOnlineSingle "SingleScriptingLib" "bson_static" "${FO_COMMON_SYSTEM_LIBS}" "${FO_COMMON_LIBS}" "${FO_CLIENT_SYSTEM_LIBS}" "${FO_CLIENT_LIBS}" "${FO_RENDER_SYSTEM_LIBS}" "${FO_RENDER_LIBS}" "${CMAKE_DL_LIBS}" )
endif()

if( FONLINE_BUILD_MAPPER )
	StatusMessage( "+ FOnlineMapper" )
	list( APPEND FO_APPLICATIONS_GROUP "FOnlineMapper" )
	add_executable( FOnlineMapper WIN32 "Source/Applications/MapperApp.cpp" "Source/Common/Application.cpp" "Source/Tools/Mapper.h" "Source/Tools/Mapper.cpp" "Resources/Default.rc" "Source/Tools/MapperScripting.h" )
	set_target_properties( FOnlineMapper PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_TOOLS_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_DEBUG ${FO_TOOLS_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_RELEASE ${FO_TOOLS_OUTPUT} )
	set_target_properties( FOnlineMapper PROPERTIES OUTPUT_NAME "FOnlineMapper" )
	set_target_properties( FOnlineMapper PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=1" )
	target_link_libraries( FOnlineMapper "MapperScriptingLib" "ClientScriptingLib" "ClientLib" "CommonLib" "${FO_RENDER_SYSTEM_LIBS}" "${FO_RENDER_LIBS}" "${CMAKE_DL_LIBS}" )
endif()

if( FONLINE_BUILD_BAKER )
	StatusMessage( "+ FOnlineBaker" )
	list( APPEND FO_APPLICATIONS_GROUP "FOnlineBaker" )
	add_executable( FOnlineBaker "Source/Applications/BakerApp.cpp" "Source/Common/ApplicationHeadless.cpp" "Source/Tools/ImageBaker.h" "Source/Tools/ImageBaker.cpp" "Source/Tools/ModelBaker.h" "Source/Tools/ModelBaker.cpp" "Source/Tools/EffectBaker.h" "Source/Tools/EffectBaker.cpp" "Source/Tools/F2Palette-Include.h" )
	set_target_properties( FOnlineBaker PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_TOOLS_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_DEBUG ${FO_TOOLS_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_RELEASE ${FO_TOOLS_OUTPUT} )
	set_target_properties( FOnlineBaker PROPERTIES OUTPUT_NAME "FOnlineBaker" )
	set_target_properties( FOnlineBaker PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0" )
	target_link_libraries( FOnlineBaker "CommonLib" "${FO_BAKER_SYSTEM_LIBS}" "${FO_BAKER_LIBS}" "${CMAKE_DL_LIBS}" )
endif()

if( FONLINE_BUILD_ASCOMPILER )
	StatusMessage( "+ FOnlineASCompiler" )
	list( APPEND FO_APPLICATIONS_GROUP "FOnlineASCompiler" )
	add_executable( FOnlineASCompiler "Source/Applications/ASCompilerApp.cpp" "Source/Common/ApplicationHeadless.cpp" )
	set_target_properties( FOnlineASCompiler PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_TOOLS_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_DEBUG ${FO_TOOLS_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_RELEASE ${FO_TOOLS_OUTPUT} )
	set_target_properties( FOnlineASCompiler PROPERTIES OUTPUT_NAME "FOnlineASCompiler" )
	set_target_properties( FOnlineASCompiler PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0" )
	target_link_libraries( FOnlineASCompiler "CommonLib" "${CMAKE_DL_LIBS}" )

	add_library( ASCompilerServerLib STATIC "${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ServerCompiler.cpp" )
	set_target_properties( ASCompilerServerLib PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_SERVER_SCRIPTING=1;FO_CLIENT_SCRIPTING=0;FO_MAPPER_SCRIPTING=0;FO_SINGLEPLAYER_SCRIPTING=0;FO_ANGELSCRIPT_COMPILER=1;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=0" )
	add_library( ASCompilerClientLib STATIC "${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ClientCompiler.cpp" )
	set_target_properties( ASCompilerClientLib PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_SERVER_SCRIPTING=0;FO_CLIENT_SCRIPTING=1;FO_MAPPER_SCRIPTING=0;FO_SINGLEPLAYER_SCRIPTING=0;FO_ANGELSCRIPT_COMPILER=1;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=0" )
	add_library( ASCompilerMapperLib STATIC "${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-MapperCompiler.cpp" )
	set_target_properties( ASCompilerMapperLib PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=0;FO_SINGLEPLAYER=0;FO_SERVER_SCRIPTING=0;FO_CLIENT_SCRIPTING=0;FO_MAPPER_SCRIPTING=1;FO_SINGLEPLAYER_SCRIPTING=0;FO_ANGELSCRIPT_COMPILER=1;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=0" )
	target_link_libraries( FOnlineASCompiler "ASCompilerServerLib" "ASCompilerClientLib" "ASCompilerMapperLib" "Angelscript" "AngelscriptExt" )
	list( APPEND FO_CORE_LIBS_GROUP "ASCompilerServerLib" "ASCompilerClientLib" "ASCompilerMapperLib" )
endif()

if( FONLINE_UNIT_TESTS )
	StatusMessage( "+ FOnlineUnitTests" )
	list( APPEND FO_APPLICATIONS_GROUP "FOnlineUnitTests" )
	add_executable( FOnlineUnitTests "Source/Applications/TestingApp.cpp" ${FO_LINT_SOURCE} ${FO_TESTS_SOURCE} ${FO_TOOLS_SOURCE} ${FO_SERVER_SOURCE} "Source/Server/AppGuiStub.cpp" ${FO_CLIENT_SOURCE} ${FO_COMMON_SOURCE} "Source/Common/ApplicationHeadless.cpp" "Source/Applications/ServerApp.cpp" "Source/Applications/ServerServiceApp.cpp" "Source/Applications/ServerDaemonApp.cpp" "Source/Applications/ServerHeadlessApp.cpp" "Source/Applications/ClientApp.cpp" "Source/Applications/MapperApp.cpp" "Source/Applications/BakerApp.cpp" )
	set_target_properties( FOnlineUnitTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_TESTS_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_DEBUG ${FO_TESTS_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_RELEASE ${FO_TESTS_OUTPUT} )
	set_target_properties( FOnlineUnitTests PROPERTIES OUTPUT_NAME "FOnlineUnitTests" )
	set_target_properties( FOnlineUnitTests PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=1;FO_SINGLEPLAYER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=1" )
	target_link_libraries( FOnlineUnitTests "ServerScriptingLib" "ClientScriptingLib" "MapperScriptingLib" "${FO_COMMON_SYSTEM_LIBS}" "${FO_COMMON_LIBS}" "${FO_BAKER_SYSTEM_LIBS}" "${FO_BAKER_LIBS}" "${FO_SERVER_SYSTEM_LIBS}" "${FO_SERVER_LIBS}" "${FO_CLIENT_SYSTEM_LIBS}" "${FO_CLIENT_LIBS}" "${FO_RENDER_LIBS}" "${CMAKE_DL_LIBS}" )
endif()

if( FONLINE_CODE_COVERAGE )
	StatusMessage( "+ FOnlineCodeCoverage" )
	list( APPEND FO_APPLICATIONS_GROUP "FOnlineCodeCoverage" )
	add_executable( FOnlineCodeCoverage "Source/Applications/TestingApp.cpp" ${FO_TESTS_SOURCE} ${FO_TOOLS_SOURCE} ${FO_SERVER_SOURCE} "Source/Server/AppGuiStub.cpp" ${FO_CLIENT_SOURCE} ${FO_COMMON_SOURCE} "Source/Common/ApplicationHeadless.cpp" "Source/Applications/ServerApp.cpp" "Source/Applications/ServerServiceApp.cpp" "Source/Applications/ServerDaemonApp.cpp" "Source/Applications/ServerHeadlessApp.cpp" "Source/Applications/ClientApp.cpp" "Source/Applications/MapperApp.cpp" "Source/Applications/BakerApp.cpp" )
	set_target_properties( FOnlineCodeCoverage PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_TESTS_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_DEBUG ${FO_TESTS_OUTPUT} RUNTIME_OUTPUT_DIRECTORY_RELEASE ${FO_TESTS_OUTPUT} )
	set_target_properties( FOnlineCodeCoverage PROPERTIES OUTPUT_NAME "FOnlineCodeCoverage" )
	set_target_properties( FOnlineCodeCoverage PROPERTIES COMPILE_DEFINITIONS "${FO_BUILD_DEFINES};FO_TESTING=1;FO_SINGLEPLAYER=0;FO_API_MULTIPLAYER_ONLY=1;FO_API_SINGLEPLAYER_ONLY=1" )
	if( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
		target_compile_options( FOnlineCodeCoverage PRIVATE "-O0;-fprofile-instr-generate;-fcoverage-mapping" )
	elseif( CMAKE_CXX_COMPILER_ID MATCHES "GNU" )
		target_compile_options( FOnlineCodeCoverage PRIVATE "-O0;--coverage" )
		set_target_properties( FOnlineCodeCoverage PROPERTIES LINK_FLAGS "--coverage" )
	endif()
	target_link_libraries( FOnlineCodeCoverage "ServerScriptingLib" "ClientScriptingLib" "MapperScriptingLib" "${FO_COMMON_SYSTEM_LIBS}" "${FO_COMMON_LIBS}" "${FO_BAKER_SYSTEM_LIBS}" "${FO_BAKER_LIBS}" "${FO_SERVER_SYSTEM_LIBS}" "${FO_SERVER_LIBS}" "${FO_CLIENT_SYSTEM_LIBS}" "${FO_CLIENT_LIBS}" "${FO_RENDER_LIBS}" "${CMAKE_DL_LIBS}" )
endif()

# Scripts
set( genScriptApi "" )
set( compileNativeScripts "" )
set( compileASScripts "" )
set( compileMonoScripts "" )
set( genApiDepends "" )
if( FO_NATIVE_SCRIPTING OR FO_ANGELSCRIPT_SCRIPTING OR FO_MONO_SCRIPTING )
	# Script API meta
	get_property( idirs DIRECTORY PROPERTY INCLUDE_DIRECTORIES )
	if( MSVC )
		list( JOIN idirs ";/I;" idirsStr )
		set( metaArgs "/nologo;/EP;/C;/I;${idirsStr}" )
	else()
		list( JOIN idirs ";-I;" idirsStr )
		set( metaArgs "-E;-C;-I;${idirsStr}" )
	endif()

	add_custom_command( OUTPUT "${FO_SCRIPTS_OUTPUT}/ScriptApiMeta.txt"
		COMMAND ${CMAKE_CXX_COMPILER} ${metaArgs} -DFO_API_MULTIPLAYER_ONLY=1 -DFO_API_SINGLEPLAYER_ONLY=1 -DFO_API_NATIVE_ONLY=1 -DFO_API_ANGELSCRIPT_ONLY=1 -DFO_API_MONO_ONLY=1
			"${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting/ScriptApiMeta.h" > "${FO_SCRIPTS_OUTPUT}/ScriptApiMeta.txt"
		COMMAND ${CMAKE_CXX_COMPILER} ${metaArgs} -DFO_API_MULTIPLAYER_ONLY=1 -DFO_API_SINGLEPLAYER_ONLY=0 -DFO_API_NATIVE_ONLY=1 -DFO_API_ANGELSCRIPT_ONLY=1 -DFO_API_MONO_ONLY=1
			"${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting/ScriptApiMeta.h" > "${FO_SCRIPTS_OUTPUT}/ScriptApiMeta-MP.txt"
		COMMAND ${CMAKE_CXX_COMPILER} ${metaArgs} -DFO_API_MULTIPLAYER_ONLY=0 -DFO_API_SINGLEPLAYER_ONLY=1 -DFO_API_NATIVE_ONLY=1 -DFO_API_ANGELSCRIPT_ONLY=1 -DFO_API_MONO_ONLY=1
			"${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting/ScriptApiMeta.h" > "${FO_SCRIPTS_OUTPUT}/ScriptApiMeta-SP.txt"
		COMMAND ${CMAKE_CXX_COMPILER} ${metaArgs} -DFO_API_MULTIPLAYER_ONLY=1 -DFO_API_SINGLEPLAYER_ONLY=0 -DFO_API_NATIVE_ONLY=1 -DFO_API_ANGELSCRIPT_ONLY=0 -DFO_API_MONO_ONLY=0
			"${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting/ScriptApiMeta.h" > "${FO_SCRIPTS_OUTPUT}/ScriptApiMeta-MP-Native.txt"
		COMMAND ${CMAKE_CXX_COMPILER} ${metaArgs} -DFO_API_MULTIPLAYER_ONLY=1 -DFO_API_SINGLEPLAYER_ONLY=0 -DFO_API_NATIVE_ONLY=0 -DFO_API_ANGELSCRIPT_ONLY=1 -DFO_API_MONO_ONLY=0
			"${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting/ScriptApiMeta.h" > "${FO_SCRIPTS_OUTPUT}/ScriptApiMeta-MP-AngelScript.txt"
		COMMAND ${CMAKE_CXX_COMPILER} ${metaArgs} -DFO_API_MULTIPLAYER_ONLY=1 -DFO_API_SINGLEPLAYER_ONLY=0 -DFO_API_NATIVE_ONLY=0 -DFO_API_ANGELSCRIPT_ONLY=0 -DFO_API_MONO_ONLY=1
			"${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting/ScriptApiMeta.h" > "${FO_SCRIPTS_OUTPUT}/ScriptApiMeta-MP-Mono.txt"
		COMMAND ${CMAKE_CXX_COMPILER} ${metaArgs} -DFO_API_MULTIPLAYER_ONLY=0 -DFO_API_SINGLEPLAYER_ONLY=1 -DFO_API_NATIVE_ONLY=1 -DFO_API_ANGELSCRIPT_ONLY=0 -DFO_API_MONO_ONLY=0
			"${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting/ScriptApiMeta.h" > "${FO_SCRIPTS_OUTPUT}/ScriptApiMeta-SP-Native.txt"
		COMMAND ${CMAKE_CXX_COMPILER} ${metaArgs} -DFO_API_MULTIPLAYER_ONLY=0 -DFO_API_SINGLEPLAYER_ONLY=1 -DFO_API_NATIVE_ONLY=0 -DFO_API_ANGELSCRIPT_ONLY=1 -DFO_API_MONO_ONLY=0
			"${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting/ScriptApiMeta.h" > "${FO_SCRIPTS_OUTPUT}/ScriptApiMeta-SP-AngelScript.txt"
		COMMAND ${CMAKE_CXX_COMPILER} ${metaArgs} -DFO_API_MULTIPLAYER_ONLY=0 -DFO_API_SINGLEPLAYER_ONLY=1 -DFO_API_NATIVE_ONLY=0 -DFO_API_ANGELSCRIPT_ONLY=0 -DFO_API_MONO_ONLY=1
			"${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting/ScriptApiMeta.h" > "${FO_SCRIPTS_OUTPUT}/ScriptApiMeta-SP-Mono.txt"
		MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting/ScriptApiMeta.h"
		DEPENDS ${FO_SCRIPT_API} ${FO_SCRIPTING_SOURCE}
		COMMENT "Generate script API meta file" )

	# Script API generation
	if( APPLE ) # Workaraund for xcode builds
		set( genApiDepends "" )
	else()
		set( genApiDepends "${FO_SCRIPTS_OUTPUT}/ScriptApiMeta.txt" )
	endif()

	set( genApiCommands -meta "${FO_SCRIPTS_OUTPUT}/ScriptApiMeta.txt" -output ${FO_SCRIPTS_OUTPUT} )

	list( APPEND genApiCommands -markdown )
	if( FONLINE_INFO_MARKDOWN_OUTPUT )
		list( APPEND genApiCommands -mdpath "${FONLINE_INFO_MARKDOWN_OUTPUT}" )
	endif()

	if( FO_NATIVE_SCRIPTING )
		list( APPEND genApiCommands -native )
	endif()
	if( FO_ANGELSCRIPT_SCRIPTING )
		list( APPEND genApiCommands -angelscript )
	endif()
	if( FO_MONO_SCRIPTING )
		list( APPEND genApiCommands -csharp )
	endif()
	if( FO_MULTIPLAYER_SCRIPTING )
		list( APPEND genApiCommands -multiplayer )
	endif()
	if( FO_SINGLEPLAYER_SCRIPTING )
		list( APPEND genApiCommands -singleplayer )
	endif()
	list( APPEND genApiCommands -mapper )

	foreach( src ${FO_ANGELSCRIPT_FILES} )
		list( APPEND genApiCommands -assource ${src} )
	endforeach()

	set( csFiles "" )
	foreach( entry ${FO_MONO_ASSEMBLIES} )
		list( APPEND genApiCommands -monoassembly ${entry} )
		foreach( ref ${MonoAssembly_${entry}_CommonRefs} )
			list( APPEND genApiCommands -monoserverref "${entry},${ref}" )
			list( APPEND genApiCommands -monoclientref "${entry},${ref}" )
			list( APPEND genApiCommands -monosingleref "${entry},${ref}" )
			list( APPEND genApiCommands -monomapperref "${entry},${ref}" )
		endforeach()
		foreach( ref ${MonoAssembly_${entry}_ServerRefs} )
			list( APPEND genApiCommands -monoserverref "${entry},${ref}" )
		endforeach()
		foreach( ref ${MonoAssembly_${entry}_ClientRefs} )
			list( APPEND genApiCommands -monoclientref "${entry},${ref}" )
		endforeach()
		foreach( ref ${MonoAssembly_${entry}_SingleRefs} )
			list( APPEND genApiCommands -monosingleref "${entry},${ref}" )
		endforeach()
		foreach( ref ${MonoAssembly_${entry}_MapperRefs} )
			list( APPEND genApiCommands -monomapperref "${entry},${ref}" )
		endforeach()
		foreach( src ${MonoAssembly_${entry}_CommonSource} )
			list( APPEND genApiCommands -monoserversource "${entry},${src}" )
			list( APPEND genApiCommands -monoclientsource "${entry},${src}" )
			list( APPEND genApiCommands -monosinglesource "${entry},${src}" )
			list( APPEND genApiCommands -monomappersource "${entry},${src}" )
			list( APPEND csFiles ${src} )
		endforeach()
		foreach( src ${MonoAssembly_${entry}_ServerSource} )
			list( APPEND genApiCommands -monoserversource "${entry},${src}" )
			list( APPEND csFiles ${src} )
		endforeach()
		foreach( src ${MonoAssembly_${entry}_ClientSource} )
			list( APPEND genApiCommands -monoclientsource "${entry},${src}" )
			list( APPEND csFiles ${src} )
		endforeach()
		foreach( src ${MonoAssembly_${entry}_SingleSource} )
			list( APPEND genApiCommands -monosinglesource "${entry},${src}" )
			list( APPEND csFiles ${src} )
		endforeach()
		foreach( src ${MonoAssembly_${entry}_MapperSource} )
			list( APPEND genApiCommands -monomappersource "${entry},${src}" )
			list( APPEND csFiles ${src} )
		endforeach()
	endforeach()

	foreach( entry ${FO_CONTENT} )
		list( APPEND genApiCommands -content ${entry} )
	endforeach()

	set( genScriptApi python "${CMAKE_CURRENT_SOURCE_DIR}/BuildTools/gen-script-api.py" ${genApiCommands} )
	add_custom_target( GenerateScriptApi
		COMMAND ${genScriptApi}
		DEPENDS ${genApiDepends}
		SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Source/Scripting/ScriptApiMeta.h"
		WORKING_DIRECTORY ${FO_SCRIPTS_OUTPUT}
		COMMENT "Generate script API" )
	list( APPEND FO_COMMANDS_GROUP "GenerateScriptApi" )

	# Compile Native scripts
	if( FO_NATIVE_SCRIPTING )
		set( compileNativeScripts "" )
		add_custom_target( CompileNativeScripts
			COMMAND ${genScriptApi}
			COMMAND ${compileNativeScripts}
			DEPENDS ${genApiDepends}
			# SOURCES ${FO_NATIVE_SOURCE}
			WORKING_DIRECTORY ${FO_SCRIPTS_OUTPUT}
			COMMENT "Compile Native scripts" )
		list( APPEND FO_COMMANDS_GROUP "CompileNativeScripts" )
	endif()

	# Compile AngelScript scripts
	if( FO_ANGELSCRIPT_SCRIPTING )
		set( compileASScripts FOnlineASCompiler
			-ASMapper "${FO_SCRIPTS_OUTPUT}/MapperRootModule.fos" )
		if( FO_MULTIPLAYER_SCRIPTING )
			list( APPEND compileASScripts
				-ASServer "${FO_SCRIPTS_OUTPUT}/ServerRootModule.fos"
				-ASClient "${FO_SCRIPTS_OUTPUT}/ClientRootModule.fos" )
		endif()
		if( FO_SINGLEPLAYER_SCRIPTING )
			list( APPEND compileASScripts
				-ASSingle "${FO_SCRIPTS_OUTPUT}/SingleRootModule.fos" )
		endif()

		add_custom_target( CompileAngelScripts
			COMMAND ${genScriptApi}
			COMMAND ${compileASScripts}
			DEPENDS ${genApiDepends}
			SOURCES ${FO_ANGELSCRIPT_FILES}
			WORKING_DIRECTORY ${FO_SCRIPTS_OUTPUT}
			COMMENT "Compile AngelScript scripts" )
		list( APPEND FO_COMMANDS_GROUP "CompileAngelScripts" )
	endif()

	# Compile Mono scripts
	if( FO_MONO_SCRIPTING )
		set( monoCompileCommands -scripts ${FO_SCRIPTS_OUTPUT} )
		foreach( entry ${FO_MONO_ASSEMBLIES} )
			list( APPEND monoCompileCommands -assembly ${entry} )
		endforeach()

		set( compileMonoScripts python "${CMAKE_CURRENT_SOURCE_DIR}/BuildTools/compile-mono-scripts.py" ${monoCompileCommands} )

		add_custom_target( CompileMonoScripts
			COMMAND ${genScriptApi}
			COMMAND ${compileMonoScripts}
			DEPENDS ${genApiDepends}
			SOURCES ${csFiles}
			WORKING_DIRECTORY ${FO_SCRIPTS_OUTPUT}
			COMMENT "Compile Mono scripts" )
		list( APPEND FO_COMMANDS_GROUP "CompileMonoScripts" )
	endif()

	# Compile all scripts
	add_custom_target( CompileAllScripts
		COMMAND ${genScriptApi}
		COMMAND ${compileNativeScripts}
		COMMAND ${compileASScripts}
		COMMAND ${compileMonoScripts}
		DEPENDS ${genApiDepends}
		SOURCES ${FO_ANGELSCRIPT_FILES} ${csFiles}
		WORKING_DIRECTORY ${FO_SCRIPTS_OUTPUT}
		COMMENT "Compile all scripts" )
	list( APPEND FO_COMMANDS_GROUP "CompileAllScripts" )
endif()

# Bakering
set( bakeContent "FOnlineBaker" -ResourcesOutput ${FO_RESOURCES_OUTPUT} )
foreach( entry ${FO_CONTENT} )
	list( APPEND bakeContent -ContentEntry ${entry} )
endforeach()

add_custom_target( BakeContent
	COMMAND ${bakeContent}
	WORKING_DIRECTORY ${FO_RESOURCES_OUTPUT}
	COMMENT "Bake resources" )
list( APPEND FO_COMMANDS_GROUP "BakeContent" )

set( bakeResources "FOnlineBaker" -ResourcesOutput ${FO_RESOURCES_OUTPUT} )
foreach( entry ${FO_RESOURCES} )
	list( APPEND bakeResources -ResourcesEntry ${entry} )
endforeach()

add_custom_target( BakeResources
	COMMAND ${bakeResources}
	WORKING_DIRECTORY ${FO_RESOURCES_OUTPUT}
	COMMENT "Bake resources" )
list( APPEND FO_COMMANDS_GROUP "BakeResources" )

add_custom_target( BakeAll
	COMMAND ${bakeContent}
	COMMAND ${bakeResources}
	WORKING_DIRECTORY ${FO_RESOURCES_OUTPUT}
	COMMENT "Bake all" )
list( APPEND FO_COMMANDS_GROUP "BakeAll" )

# Packaging
if( FO_PACKAGES )
	add_custom_target( MakeAllPackages
		COMMAND ${genScriptApi}
		COMMAND ${compileNativeScripts}
		COMMAND ${compileASScripts}
		COMMAND ${compileMonoScripts}
		COMMAND ${bakeContent}
		COMMAND ${bakeResources}
		DEPENDS ${genApiDepends}
		WORKING_DIRECTORY ${FO_PACKAGES_OUTPUT}
		COMMENT "Make all packages" )
	list( APPEND FO_COMMANDS_GROUP "MakeAllPackages" )

	StatusMessage( "Package parts:" )

	foreach( package ${FO_PACKAGES} )
		add_custom_target( MakePackage-${package}
			COMMAND ${genScriptApi}
			COMMAND ${compileNativeScripts}
			COMMAND ${compileASScripts}
			COMMAND ${compileMonoScripts}
			COMMAND ${bakeContent}
			COMMAND ${bakeResources}
			DEPENDS ${genApiDepends}
			WORKING_DIRECTORY ${FO_PACKAGES_OUTPUT}
			COMMENT "Make package ${package}" )
		list( APPEND FO_COMMANDS_GROUP "MakePackage-${package}" )

		foreach( entry ${Package_${package}_Parts} )
			string( REPLACE "," ";" entry ${entry} )
			list( GET entry 0 target )
			list( GET entry 1 platform )
			list( GET entry 2 arch )
			list( GET entry 3 packType )
			list( GET entry 4 customConfig )

			set( packageCommands python "${CMAKE_CURRENT_SOURCE_DIR}/BuildTools/package.py" )
			list( APPEND packageCommands -devname "${FO_DEV_NAME}" )
			list( APPEND packageCommands -nicename "${FO_NICE_NAME}" )
			list( APPEND packageCommands -author "${FO_COMPANY_NAME}" )
			list( APPEND packageCommands -version "${FO_VERSION}" )

			set( configuration "Release" )
			if( ${Package_${package}_Debug} )
				set( configuration "Debug" )
				list( APPEND packageCommands -debug )
			endif()

			list( APPEND packageCommands -binaries "${FONLINE_OUTPUT_PATH}" )
			list( APPEND packageCommands -scripts "${FO_SCRIPTS_OUTPUT}" )
			list( APPEND packageCommands -resources "${FO_RESOURCES_OUTPUT}" )

			if( customConfig )
				set( curConfig ${customConfig} )
			else()
				set( curConfig ${Package_${package}_Config} )
			endif()
			set( processedOptions "" )
			while( curConfig )
				foreach( option ${Config_${curConfig}_Options} )
					list( FIND processedOptions ${option} optionIndex )
					if( optionIndex EQUAL -1 )
						list( APPEND processedOptions ${option} )
						list( APPEND packageCommands -config "${option},${Config_${curConfig}___${option}}" )
					endif()
				endforeach()
				set( curConfig ${Config_${curConfig}_Inheriten} )
			endwhile()

			foreach( res ${FO_RESOURCES} )
				list( APPEND packageCommands -res ${res} )
			endforeach()

			set( packagePartName ${package}-${target}-${platform}-${arch}-${configuration}-${packType} )
			StatusMessage( "+ ${packagePartName}" )
			add_custom_command( TARGET MakePackage-${package} POST_BUILD
				COMMAND ${packageCommands}
				COMMENT "Process ${packagePartName}" )
			add_custom_command( TARGET MakeAllPackages POST_BUILD
				COMMAND ${packageCommands}
				COMMENT "Process ${packagePartName}" )
		endforeach()
	endforeach()
endif()

# Setup targets grouping
set_property( GLOBAL PROPERTY USE_FOLDERS ON )
set_property( TARGET ${FO_APPLICATIONS_GROUP} PROPERTY FOLDER "Applications" )
set_property( TARGET ${FO_CORE_LIBS_GROUP} PROPERTY FOLDER "CoreLibs" )
set_property( TARGET ${FO_COMMANDS_GROUP} PROPERTY FOLDER "Commands" )
set_property( TARGET ${FO_COMMON_LIBS} ${FO_BAKER_LIBS} ${FO_SERVER_LIBS} ${FO_CLIENT_LIBS} ${FO_RENDER_LIBS} PROPERTY FOLDER "ThirdParty" )

# Print cached variables
if( FONLINE_VERBOSE_BUILD )
	get_cmake_property( FO_CACHE_VARIABLES CACHE_VARIABLES )
	list( SORT FO_CACHE_VARIABLES )

	StatusMessage( "Forced variables:" )
	foreach( varName ${FO_CACHE_VARIABLES} )
		get_property( str CACHE ${varName} PROPERTY HELPSTRING )
		get_property( type CACHE ${varName} PROPERTY TYPE )
		string( FIND "${str}" "Forced by FOnline" forced )
		if( NOT "${forced}" STREQUAL "-1" )
			StatusMessage( "- ${varName}: '${${varName}}' type: '${type}'" )
		endif()
	endforeach()

	StatusMessage( "Default variables:" )
	foreach( varName ${FO_CACHE_VARIABLES} )
		get_property( str CACHE ${varName} PROPERTY HELPSTRING )
		get_property( type CACHE ${varName} PROPERTY TYPE )
		string( FIND "${str}" "Forced by FOnline" forced )
		if( "${forced}" STREQUAL "-1" AND NOT "${type}" STREQUAL "INTERNAL" AND NOT "${type}" STREQUAL "STATIC" AND NOT "${type}" STREQUAL "UNINITIALIZED" )
			StatusMessage( "- ${varName}: '${${varName}}' docstring: '${str}' type: '${type}'" )
		endif()
	endforeach()
endif()
