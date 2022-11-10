cmake_minimum_required( VERSION 3.16.3 )
project( FOnlineProject )

# Options
set( FONLINE_VERBOSE_BUILD OFF CACHE BOOL "Verbose build mode" )
set( FONLINE_OUTPUT_PATH "${CMAKE_BINARY_DIR}" CACHE PATH "Common output path" )
set( FONLINE_BUILD_CLIENT OFF CACHE BOOL "Build Multiplayer client binaries" )
set( FONLINE_BUILD_SERVER OFF CACHE BOOL "Build Multiplayer server binaries" )
set( FONLINE_BUILD_SINGLE OFF CACHE BOOL "Build Singleplayer binaries" )
set( FONLINE_BUILD_EDITOR OFF CACHE BOOL "Build Editor binaries" )
set( FONLINE_BUILD_MAPPER OFF CACHE BOOL "Build Mapper binaries" )
set( FONLINE_BUILD_ASCOMPILER OFF CACHE BOOL "Build AngelScript compiler" )
set( FONLINE_BUILD_BAKER OFF CACHE BOOL "Build Baker binaries" )
set( FONLINE_UNIT_TESTS ON CACHE BOOL "Build only binaries for Unit Testing" )
set( FONLINE_CODE_COVERAGE OFF CACHE BOOL "Build only binaries for Code Coverage reports" )
set( FONLINE_MAKE_EXTERNAL_COMMANDS OFF CACHE BOOL "Create shortcuts for working outside CMake runner" )
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

# Skip all install rules
function( install )
endfunction()
set( CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "Forced by FOnline" FORCE )

# Global options
StatusMessage( "Start project generation" )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Forced by FOnline" FORCE ) # Generate compile_commands.json
set( BUILD_SHARED_LIBS OFF CACHE BOOL "Forced by FOnline" FORCE )
set( BUILD_TESTING OFF CACHE BOOL "Forced by FOnline" FORCE )

# Temporary variables
set( FO_BUILD_HASH "" )
set( FO_GIT_HASH_RESULT "" )
set( FO_GIT_DIFF_RESULT "" )
set( FO_GIT_HASH "" )
set( FO_HEADLESS_ONLY YES )
set( FO_DEV_NAME "FOnline" )
set( FO_NICE_NAME "FOnline" )
set( FO_AUTHOR_NAME "FOnline" )
set( FO_GAME_VERSION "0.0.1" )
set( FO_SINGLEPLAYER ${FONLINE_BUILD_SINGLE} )
set( FO_ENABLE_3D YES )
set( FO_NATIVE_SCRIPTING NO )
set( FO_ANGELSCRIPT_SCRIPTING ${FONLINE_BUILD_ASCOMPILER} )
set( FO_MONO_SCRIPTING NO )
set( FO_GEOMETRY 0 )
set( FO_OS "" )
set( FO_OS_UPPER "" )
set( FO_BUILD_LIBRARY NO )
set( FO_CLIENT_OUTPUT "" )
set( FO_SERVER_OUTPUT "" )
set( FO_SINGLE_OUTPUT "" )
set( FO_EDITOR_OUTPUT "" )
set( FO_MAPPER_OUTPUT "" )
set( FO_ASCOMPILER_OUTPUT "" )
set( FO_BAKER_OUTPUT "" )
set( FO_TESTS_OUTPUT "" )
set( FO_BACKED_RESOURCES_OUTPUT "" )
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
set( FO_TESTING_LIBS "" )
set( FO_DUMMY_TRAGETS "" )
set( FO_USE_GLEW NO )
set( FO_SDL_DIR "" )
set( FO_ZLIB_DIR "" )
set( FO_PNG_DIR "" )
set( FO_OGG_DIR "" )
set( FO_VORBIS_DIR "" )
set( FO_THEORA_DIR "" )
set( FO_ACM_DIR "" )
set( FO_SHA_DIR "" )
set( FO_GLEW_DIR "" )
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
set( FO_SERVER_BASE_SOURCE "" )
set( FO_CLIENT_BASE_SOURCE "" )
set( FO_SERVER_SOURCE "" )
set( FO_CLIENT_SOURCE "" )
set( FO_SINGLE_SOURCE "" )
set( FO_EDITOR_SOURCE "" )
set( FO_MAPPER_SOURCE "" )
set( FO_ASCOMPILER_SOURCE "" )
set( FO_BAKER_SOURCE "" )
set( FO_SOURCE_META_FILES "" )
set( FO_CONTENT_META_FILES "" )
set( FO_TESTS_SOURCE "" )
set( FO_CORE_LIBS_GROUP "" )
set( FO_COMMANDS_GROUP "" )
set( FO_APPLICATIONS_GROUP "" )
set( FO_CACHE_VARIABLES "" )
set( FO_CONTRIBUTION_DIR "" )
set( FO_CONTENT "" )
set( FO_RESOURCES "" )
set( FO_SPRITE_ATLAS "" )
set( FO_CODEGEN_COMMAND "" )
set( FO_CODEGEN_COMMAND_ARGS "" )
set( FO_CODEGEN_META_SOURCE "" )
set( FO_CODEGEN_OUTPUT "" )
set( FO_COMMON_NATIVE_SOURCE "" )
set( FO_SERVER_NATIVE_SOURCE "" )
set( FO_CLIENT_NATIVE_SOURCE "" )
set( FO_EDITOR_NATIVE_SOURCE "" )
set( FO_MAPPER_NATIVE_SOURCE "" )
set( FO_MONO_ASSEMBLIES "" )
set( FO_MONO_SOURCE "" )
set( FO_DEBUGGING_CONFIG "" )
set( FO_MAPPER_CONFIG "" )
set( FO_GENERATE_ANGELSCRIPT_CONTENT "" )
set( FO_PACKAGES "" )
set( FO_CONFIGS "" )

# Evaluate build hash
execute_process( COMMAND git rev-parse HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} RESULT_VARIABLE FO_GIT_HASH_RESULT OUTPUT_VARIABLE FO_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE )
if( FO_GIT_HASH_RESULT STREQUAL "0" )
	execute_process( COMMAND git diff-index --quiet HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} RESULT_VARIABLE FO_GIT_DIFF_RESULT )
	if( FO_GIT_DIFF_RESULT STREQUAL "0" )
		set( FO_BUILD_HASH ${FO_GIT_HASH} )
	else()
		set( FO_BUILD_HASH ${FO_GIT_HASH}-dirty )
	endif()
else()
	string( RANDOM LENGTH 40 ALPHABET "0123456789abcdef" randomHash )
	set( FO_BUILD_HASH ${randomHash}-random )
endif()

StatusMessage( "Build hash: ${FO_BUILD_HASH}" )

macro( WriteBuildHash target )
	if( NOT FO_BUILD_LIBRARY )
		get_target_property( dir ${target} RUNTIME_OUTPUT_DIRECTORY )
	else()
		get_target_property( dir ${target} LIBRARY_OUTPUT_DIRECTORY )
	endif()
	add_custom_command( TARGET ${target} PRE_BUILD COMMAND ${CMAKE_COMMAND} -E remove -f "${dir}/${target}.build-hash" )
	add_custom_command( TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E echo_append ${FO_BUILD_HASH} > "${dir}/${target}.build-hash" )
endmacro()

# Some info about build
StatusMessage( "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}" )
StatusMessage( "Generator: ${CMAKE_GENERATOR}" )

# Build configuration
get_cmake_property( isMultiConfig GENERATOR_IS_MULTI_CONFIG )
if( isMultiConfig )
	string(REPLACE ";" " " configs "${CMAKE_CONFIGURATION_TYPES}")
	StatusMessage( "Configurations: ${configs}" )
else()
	StatusMessage( "Configuration: ${CMAKE_BUILD_TYPE}" )
endif()

# Basic setup
add_compile_definitions( $<$<CONFIG:Debug>:DEBUG> )
add_compile_definitions( $<$<CONFIG:Debug>:_DEBUG> )
add_compile_definitions( $<$<CONFIG:Debug>:FO_DEBUG=1> )
add_compile_definitions( $<$<NOT:$<CONFIG:Debug>>:NDEBUG> )
add_compile_definitions( $<$<NOT:$<CONFIG:Debug>>:FO_DEBUG=0> )

# Headless configuration (without video/audio/input)
if( FONLINE_BUILD_CLIENT OR FONLINE_BUILD_SERVER OR FONLINE_BUILD_SINGLE OR FONLINE_BUILD_EDITOR OR FONLINE_BUILD_MAPPER )
	set( FO_HEADLESS_ONLY NO )
else()
	set( FO_HEADLESS_ONLY YES )
endif()

if( WIN32 AND NOT WINRT )
	StatusMessage( "Operating system: Windows" )
	set( FO_OS "Windows" )
	add_compile_definitions( FO_WINDOWS=1 FO_UWP=0 FO_LINUX=0 FO_MAC=0 FO_ANDROID=0 FO_IOS=0 FO_WEB=0 FO_PS4=0 )
	add_compile_definitions( FO_HAVE_OPENGL=1 FO_OPENGL_ES=0 FO_HAVE_DIRECT_3D=1 FO_HAVE_METAL=0 FO_HAVE_VULKAN=0 FO_HAVE_GNM=0 )
	if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		set( FO_BUILD_PLATFORM "Windows-win64" )
	else()
		set( FO_BUILD_PLATFORM "Windows-win32" )
	endif()
	if( FONLINE_BUILD_CLIENT OR FONLINE_BUILD_SINGLE )
		add_compile_options( $<$<CONFIG:Debug>:/MTd> )
		add_compile_options( $<$<NOT:$<CONFIG:Debug>>:/MT> )
	else()
		add_compile_options( $<$<CONFIG:Debug>:/MDd> )
		add_compile_options( $<$<NOT:$<CONFIG:Debug>>:/MD> )
	endif()
	# Todo: debug /RTCc /sdl _ALLOW_RTCc_IN_STL release /GS-
	add_compile_options( $<$<CONFIG:Debug>:/RTC1> )
	add_compile_options( $<$<CONFIG:Debug>:/GS> )
	add_compile_options( $<$<CONFIG:Debug>:/JMC> )
	add_compile_options( $<$<NOT:$<CONFIG:Debug>>:/sdl-> )
	add_compile_options( /W4 /WX /MP /Zi /EHsc /utf-8 /volatile:iso /W3 /GR /bigobj /fp:fast )
	add_link_options( /DEBUG:FULL /INCREMENTAL:NO /OPT:REF /OPT:NOICF )
	add_compile_definitions( UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE _WINSOCK_DEPRECATED_NO_WARNINGS )
	list( APPEND FO_COMMON_SYSTEM_LIBS "user32" "ws2_32" "version" "winmm" "imm32" "dbghelp" "psapi" "xinput" )
	if( NOT FO_HEADLESS_ONLY )
		set( FO_USE_GLEW YES )
		list( APPEND FO_RENDER_SYSTEM_LIBS "glu32" "d3d9" "d3d11" "d3dcompiler" "gdi32" "opengl32" )
	endif()
elseif( WIN32 AND WINRT )
	StatusMessage( "Operating system: Universal Windows Platform" )
	set( FO_OS "Windows" )
	add_compile_definitions( FO_WINDOWS=1 FO_UWP=1 FO_LINUX=0 FO_MAC=0 FO_ANDROID=0 FO_IOS=0 FO_WEB=0 FO_PS4=0 )
	add_compile_definitions( FO_HAVE_OPENGL=0 FO_OPENGL_ES=0 FO_HAVE_DIRECT_3D=1 FO_HAVE_METAL=0 FO_HAVE_VULKAN=0 FO_HAVE_GNM=0 )
	if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		set( FO_BUILD_PLATFORM "UWP-win64" )
	else()
		set( FO_BUILD_PLATFORM "UWP-win32" )
	endif()
	# Todo: debug /RTCc /sdl _ALLOW_RTCc_IN_STL release /GS-
	add_compile_options( $<$<CONFIG:Debug>:/MDd> )
	add_compile_options( $<$<CONFIG:Debug>:/RTC1> )
	add_compile_options( $<$<CONFIG:Debug>:/GS> )
	add_compile_options( $<$<CONFIG:Debug>:/JMC> )
	add_compile_options( $<$<NOT:$<CONFIG:Debug>>:/MD> )
	add_compile_options( $<$<NOT:$<CONFIG:Debug>>:/sdl-> )
	add_compile_options( /W4 /WX /ZW /MP /Zi /EHsc /utf-8 /volatile:iso /W3 /GR /bigobj /fp:fast )
	add_link_options( /APPCONTAINER /DEBUG:FULL /INCREMENTAL:NO /OPT:REF /OPT:NOICF )
	add_compile_definitions( UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE _WINSOCK_DEPRECATED_NO_WARNINGS )
	list( APPEND FO_COMMON_SYSTEM_LIBS "user32" "ws2_32" "version" "winmm" "imm32" "dbghelp" "psapi" "xinput" )
	if( NOT FO_HEADLESS_ONLY )
		list( APPEND FO_RENDER_SYSTEM_LIBS "d3d9" "gdi32" )
	endif()
elseif( CMAKE_SYSTEM_NAME MATCHES "Linux" )
	StatusMessage( "Operating system: Linux" )
	set( FO_OS "Linux" )
	add_compile_definitions( FO_WINDOWS=0 FO_UWP=0 FO_LINUX=1 FO_MAC=0 FO_ANDROID=0 FO_IOS=0 FO_WEB=0 FO_PS4=0 )
	add_compile_definitions( FO_HAVE_OPENGL=1 FO_OPENGL_ES=0 FO_HAVE_DIRECT_3D=0 FO_HAVE_METAL=0 FO_HAVE_VULKAN=0 FO_HAVE_GNM=0 )
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
	add_compile_options( -g )
	add_compile_options( $<$<NOT:$<CONFIG:Debug>>:-O2> )
	add_link_options( -no-pie -rdynamic )
	if( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
		# Todo: rework and use only libc++ (we use libstdc++ because fbxsdk use it)
		if( FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE OR FONLINE_BUILD_BAKER )
			add_compile_options( -stdlib=libstdc++ )
		else()
			add_compile_options( -stdlib=libc++ )
			add_link_options( -stdlib=libc++ )
		endif()
	endif()
elseif( APPLE AND NOT PLATFORM )
	StatusMessage( "Operating system: macOS" )
	if( NOT CMAKE_SIZEOF_VOID_P EQUAL 8 )
		AbortMessage( "Invalid pointer size for macOS build" )
	endif()
	set( FO_OS "Mac" )
	add_compile_definitions( FO_WINDOWS=0 FO_UWP=0 FO_LINUX=0 FO_MAC=1 FO_ANDROID=0 FO_IOS=0 FO_WEB=0 FO_PS4=0 )
	add_compile_definitions( FO_HAVE_OPENGL=1 FO_OPENGL_ES=0 FO_HAVE_DIRECT_3D=0 FO_HAVE_METAL=1 FO_HAVE_VULKAN=0 FO_HAVE_GNM=0 )
	set( FO_BUILD_PLATFORM "macOS-x64" )
	if( NOT FO_HEADLESS_ONLY )
		find_package( OpenGL REQUIRED )
		set( FO_USE_GLEW YES )
		list( APPEND FO_RENDER_SYSTEM_LIBS ${OPENGL_LIBRARIES} )
	endif()
	add_compile_options( -stdlib=libc++ -g )
	add_compile_options( $<$<NOT:$<CONFIG:Debug>>:-O2> )
	add_link_options( -rdynamic )
elseif( APPLE AND PLATFORM )
	StatusMessage( "Operating system: iOS" )
	StatusMessage( "Deployment target: ${DEPLOYMENT_TARGET}" )
	if( NOT CMAKE_SIZEOF_VOID_P EQUAL 8 )
		AbortMessage( "Invalid pointer size for iOS build" )
	endif()
	set( FO_OS "iOS" )
	add_compile_definitions( FO_WINDOWS=0 FO_UWP=0 FO_LINUX=0 FO_MAC=0 FO_ANDROID=0 FO_IOS=1 FO_WEB=0 FO_PS4=0 )
	add_compile_definitions( FO_HAVE_OPENGL=1 FO_OPENGL_ES=1 FO_HAVE_DIRECT_3D=0 FO_HAVE_METAL=1 FO_HAVE_VULKAN=0 FO_HAVE_GNM=0 )
	if( PLATFORM STREQUAL "OS64" )
		StatusMessage( "Platform: Device" )
		set( FO_BUILD_PLATFORM "iOS-arm64" )
	elseif( PLATFORM STREQUAL "SIMULATOR64" )
		StatusMessage( "Platform: Simulator" )
		set( FO_BUILD_PLATFORM "iOS-simulator" )
	else()
		AbortMessage( "Invalid iOS target platform ${PLATFORM}" )
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
	add_compile_options( -stdlib=libc++ )
	add_compile_options( $<$<CONFIG:Debug>:-g> )
	add_compile_options( $<$<NOT:$<CONFIG:Debug>>:-O3> )
	list( APPEND FO_COMMON_SYSTEM_LIBS "iconv" ) # Todo: ios iconv workaround for SDL, remove in future updates
elseif( ANDROID )
	StatusMessage( "Operating system: Android" )
	set( FO_OS "Android" )
	add_compile_definitions( FO_WINDOWS=0 FO_UWP=0 FO_LINUX=0 FO_MAC=0 FO_ANDROID=1 FO_IOS=0 FO_WEB=0 FO_PS4=0 )
	add_compile_definitions( FO_HAVE_OPENGL=1 FO_OPENGL_ES=1 FO_HAVE_DIRECT_3D=0 FO_HAVE_METAL=0 FO_HAVE_VULKAN=0 FO_HAVE_GNM=0 )
	set( FO_BUILD_PLATFORM "Android-${ANDROID_ABI}" )
	set( FO_BUILD_LIBRARY YES )
	if( NOT FO_HEADLESS_ONLY )
		list( APPEND FO_RENDER_SYSTEM_LIBS "GLESv1_CM" "GLESv2" )
	endif()
	list( APPEND FO_COMMON_SYSTEM_LIBS "android" "log" "atomic" )
	add_compile_options( $<$<CONFIG:Debug>:-g> )
	add_compile_options( $<$<NOT:$<CONFIG:Debug>>:-O3> )
	add_link_options( -pie )
elseif( EMSCRIPTEN )
	StatusMessage( "Operating system: Web" )
	set( FO_OS "Web" )
	add_compile_definitions( FO_WINDOWS=0 FO_UWP=0 FO_LINUX=0 FO_MAC=0 FO_ANDROID=0 FO_IOS=0 FO_WEB=1 FO_PS4=0 )
	add_compile_definitions( FO_HAVE_OPENGL=1 FO_OPENGL_ES=1 FO_HAVE_DIRECT_3D=0 FO_HAVE_METAL=0 FO_HAVE_VULKAN=0 FO_HAVE_GNM=0 )
	set( FO_BUILD_PLATFORM "Web-wasm" )
	set( CMAKE_EXECUTABLE_SUFFIX ".js" )
	add_compile_options( $<$<CONFIG:Debug>:-g1> ) # Todo: fix compilation with -g3
	add_compile_options( $<$<NOT:$<CONFIG:Debug>>:-O3> )
	add_compile_options( --no-heap-copy )
	add_link_options( -sSTRICT=1 )
	add_compile_options( -sSTRICT=1 )
	add_link_options( -sINITIAL_MEMORY=268435456 ) # 256 Mb
	add_link_options( -sABORT_ON_WASM_EXCEPTIONS=1 )
	add_link_options( -sERROR_ON_UNDEFINED_SYMBOLS=1 )
	add_link_options( -sALLOW_MEMORY_GROWTH=1 )
	add_link_options( -sMIN_WEBGL_VERSION=1 )
	add_link_options( -sMAX_WEBGL_VERSION=2 )
	add_link_options( -sUSE_SDL=0 )
	add_link_options( -sFORCE_FILESYSTEM=1 )
	add_link_options( -sDYNAMIC_EXECUTION=0 )
	add_link_options( -sEXIT_RUNTIME=0 )
	add_link_options( -sEXPORTED_RUNTIME_METHODS=['FS_createPath','FS_createDataFile','intArrayFromString','UTF8ToString','addRunDependency','removeRunDependency','stackTrace'] )
	add_link_options( -sLLD_REPORT_UNDEFINED=1 )
	add_link_options( -lGL )
	add_link_options( -legl.js )
	add_link_options( -lhtml5_webgl.js )
	# Todo: need?
	# add_compile_options( --js-library ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/mono/repo/sdks/wasm/library_mono.js )
	# add_link_options( -sWASM=2 )
	# add_link_options( -sDISABLE_EXCEPTION_CATCHING=0 )
	# add_compile_options( -sDISABLE_EXCEPTION_CATCHING=0 )
	# Todo: uncomment these optimizations later, they may give side effects
	# add_compile_options( --closure 1 ) - conflicts with NO_DYNAMIC_EXECUTION
	# add_link_options( -sCLOSURE_WARNINGS=error )
	# add_compile_options( --llvm-lto 1 )
	# add_link_options( -sINLINING_LIMIT=1 )
	# add_compile_options( -sINLINING_LIMIT=1 )
elseif( PS4 )
	StatusMessage( "Operating system: PS4" )
	set( FO_OS "PS4" )
	add_compile_definitions( FO_WINDOWS=0 FO_UWP=0 FO_LINUX=0 FO_MAC=0 FO_ANDROID=0 FO_IOS=0 FO_WEB=0 FO_PS4=1 )
	add_compile_definitions( FO_HAVE_OPENGL=0 FO_OPENGL_ES=0 FO_HAVE_DIRECT_3D=0 FO_HAVE_METAL=0 FO_HAVE_VULKAN=0 FO_HAVE_GNM=1 )
	set( FO_BUILD_PLATFORM "PS4-x64" )
	add_compile_options( -g )
	add_compile_options( $<$<CONFIG:Debug>:-O0> )
	add_compile_options( $<$<NOT:$<CONFIG:Debug>>:-O2> )
else()
	AbortMessage( "Unknown OS!" )
endif()

string( TOUPPER "${FO_OS}" FO_OS_UPPER )
add_compile_definitions( FO_${FO_OS_UPPER}=1 )

# Information about CPU architecture
if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
	StatusMessage( "CPU architecture: 64-bit" )
elseif( CMAKE_SIZEOF_VOID_P EQUAL 4 )
	StatusMessage( "CPU architecture: 32-bit" )
else()
	AbortMessage( "Invalid pointer size, nor 8 or 4 bytes" )
endif()

# Output path
StatusMessage( "Output path: ${FONLINE_OUTPUT_PATH}" )
set( FO_CLIENT_OUTPUT "${FONLINE_OUTPUT_PATH}/Binaries/Client-${FO_BUILD_PLATFORM}$<$<CONFIG:Debug>:-Debug>" )
set( FO_SERVER_OUTPUT "${FONLINE_OUTPUT_PATH}/Binaries/Server-${FO_BUILD_PLATFORM}$<$<CONFIG:Debug>:-Debug>" )
set( FO_SINGLE_OUTPUT "${FONLINE_OUTPUT_PATH}/Binaries/Single-${FO_BUILD_PLATFORM}$<$<CONFIG:Debug>:-Debug>" )
set( FO_EDITOR_OUTPUT "${FONLINE_OUTPUT_PATH}/Binaries/Editor-${FO_BUILD_PLATFORM}$<$<CONFIG:Debug>:-Debug>" )
set( FO_MAPPER_OUTPUT "${FONLINE_OUTPUT_PATH}/Binaries/Mapper-${FO_BUILD_PLATFORM}$<$<CONFIG:Debug>:-Debug>" )
set( FO_ASCOMPILER_OUTPUT "${FONLINE_OUTPUT_PATH}/Binaries/ASCompiler-${FO_BUILD_PLATFORM}$<$<CONFIG:Debug>:-Debug>" )
set( FO_BAKER_OUTPUT "${FONLINE_OUTPUT_PATH}/Binaries/Baker-${FO_BUILD_PLATFORM}$<$<CONFIG:Debug>:-Debug>" )
set( FO_TESTS_OUTPUT "${FONLINE_OUTPUT_PATH}/Binaries/Tests-${FO_BUILD_PLATFORM}$<$<CONFIG:Debug>:-Debug>" )
set( FO_BACKED_RESOURCES_OUTPUT "${FONLINE_OUTPUT_PATH}/Bakering" )
file( MAKE_DIRECTORY "${FONLINE_OUTPUT_PATH}/Binaries" )
file( MAKE_DIRECTORY "${FO_BACKED_RESOURCES_OUTPUT}" )

# Contributions
macro( ResolveContributedFiles )
	set( result "" )
	foreach( file ${ARGN} )
		file( GLOB globFiles LIST_DIRECTORIES FALSE CONFIGURE_DEPENDS "${FO_CONTRIBUTION_DIR}/${file}/*.fos" )
		list( APPEND FO_CONTENT_META_FILES ${globFiles} )
		file( GLOB globFiles LIST_DIRECTORIES TRUE "${FO_CONTRIBUTION_DIR}/${file}" )
		foreach( globFile ${globFiles} )
			get_filename_component( globFile ${globFile} ABSOLUTE )
			list( APPEND result ${globFile} )
		endforeach()
	endforeach()
endmacro()

macro( SetupGame )
	set( allowedCommands "DEV_NAME" "NICE_NAME" "AUTHOR_NAME" "GAME_VERSION"
		"SINGLEPLAYER" "ENABLE_3D" "NATIVE_SCRIPTING" "ANGELSCRIPT_SCRIPTING" "MONO_SCRIPTING"
		"DEBUGGING_CONFIG" "MAPPER_CONFIG" "GENERATE_ANGELSCRIPT_CONTENT" "GEOMETRY" )
	unset( opt )
	foreach( arg ${ARGN} )
		if( NOT opt )
			set( opt ${arg} )
			continue()
		endif()
		list( FIND allowedCommands ${opt} optIndex )
		if( NOT optIndex EQUAL -1 )
			if( "${opt}" STREQUAL "GENERATE_ANGELSCRIPT_CONTENT" )
				ResolveContributedFiles( ${arg} )
				set( arg "${result}" )
			endif()
			if( "${opt}" STREQUAL "GEOMETRY" )
				if( "${arg}" STREQUAL "HEXAGONAL" )
					set( arg 1 )
				elseif( "${arg}" STREQUAL "SQUARE" )
					set( arg 2 )
				else()
					set( arg -1 )
				endif()
			endif()
			StatusMessage( "+ ${opt}: ${arg}" )
			set( FO_${opt} ${arg} )
		else()
			AbortMessage( "Invalid SetupGame option ${opt} = ${arg}" )
		endif()
		unset( opt )
	endforeach()
endmacro()
macro( AddContent )
	ResolveContributedFiles( ${ARGN} )
	foreach( resultEntry ${result} )
		StatusMessage( "+ Content at ${resultEntry}" )
		list( APPEND FO_CONTENT ${resultEntry} )
	endforeach()
endmacro()
macro( AddResources packName )
	ResolveContributedFiles( ${ARGN} )
	foreach( resultEntry ${result} )
		StatusMessage( "+ Resources to ${packName} at ${resultEntry}" )
		list( APPEND FO_RESOURCES "${packName},${resultEntry}" )
	endforeach()
endmacro()
macro( AddRawResources )
	ResolveContributedFiles( ${ARGN} )
	foreach( resultEntry ${result} )
		StatusMessage( "+ Raw resources at ${resultEntry}" )
		list( APPEND FO_RESOURCES "Raw,${resultEntry}" )
	endforeach()
endmacro()
macro( AddEmbeddedResources )
	ResolveContributedFiles( ${ARGN} )
	foreach( resultEntry ${result} )
		StatusMessage( "+ Embedded resources at ${resultEntry}" )
		list( APPEND FO_RESOURCES "Embedded,${resultEntry}" )
	endforeach()
endmacro()
macro( AddEngineSource target )
	ResolveContributedFiles( ${ARGN} )
	foreach( resultEntry ${result} )
		StatusMessage( "+ Engine source at ${resultEntry}" )
		list( APPEND FO_${target}_SOURCE ${resultEntry} )
		list( APPEND FO_SINGLE_SOURCE ${resultEntry} )
		list( APPEND FO_SOURCE_META_FILES ${resultEntry} )
	endforeach()
endmacro()
macro( AddNativeIncludeDir )
	if( FO_NATIVE_SCRIPTING )
		foreach( dir ${ARGN} )
			StatusMessage( "+ Native include dir at ${FO_CONTRIBUTION_DIR}/${dir}" )
			include_directories( "${FO_CONTRIBUTION_DIR}/${dir}" )
		endforeach()
	endif()
endmacro()
macro( AddNativeSource )
	if( FO_NATIVE_SCRIPTING )
		#ResolveContributedFiles( ${ARGN} )
		#foreach( resultEntry ${result} )
		#	StatusMessage( "+ Engine source at ${resultEntry}" )
		#	list( APPEND FO_${target}_SOURCE ${resultEntry} )
		#endforeach()
	endif()
endmacro()
macro( AddMonoAssembly assembly )
	if( FO_MONO_SCRIPTING )
		StatusMessage( "+ Mono assembly ${assembly}" )
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
	endif()
endmacro()
macro( AddMonoReference assembly target )
	if( FO_MONO_SCRIPTING )
		foreach( arg ${ARGN} )
			StatusMessage( "+ Mono assembly ${target}/${assembly} redefence to ${arg}" )
			list( APPEND MonoAssembly_${assembly}_${target}Refs ${arg} )
		endforeach()
	endif()
endmacro()
macro( AddMonoSource assembly target )
	if( FO_MONO_SCRIPTING )
		ResolveContributedFiles( ${ARGN} )
		foreach( resultEntry ${result} )
			StatusMessage( "+ Mono source for assembly ${target}/${assembly} at ${resultEntry}" )
			list( APPEND MonoAssembly_${assembly}_${target}Source ${resultEntry} )
		endforeach()
	endif()
endmacro()
macro( CreatePackage package config )
	list( APPEND FO_PACKAGES ${package} )
	set( Package_${package}_Config ${config} )
	set( Package_${package}_Parts "" )
	list( APPEND FO_CONFIGS ${config} )
	list( REMOVE_DUPLICATES FO_CONFIGS )
endmacro()
macro( AddToPackage package binary platform arch packType )
	list( APPEND Package_${package}_Parts "${binary},${platform},${arch},${packType},${ARGN}" )
endmacro()

function( DisableLibWarnings )
	foreach( lib ${ARGV} )
		target_compile_options( ${lib} PRIVATE
			$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-w>
			$<$<CXX_COMPILER_ID:MSVC>:/W0> )
	endforeach()
endfunction()

# User contribution
if( NOT FONLINE_CMAKE_CONTRIBUTION STREQUAL "" )
	StatusMessage( "Contributions:" )
	foreach( inc ${FONLINE_CMAKE_CONTRIBUTION} )
		get_filename_component( FO_CONTRIBUTION_DIR ${inc} DIRECTORY )
		StatusMessage( "+ ${inc}" )
		include( ${inc} )
	endforeach()
endif()

# Core contribution
set( FO_CONTRIBUTION_DIR ${CMAKE_CURRENT_SOURCE_DIR} )
AddNativeIncludeDir( "Source/Scripting/Native" )
if( FO_ANGELSCRIPT_SCRIPTING )
	AddContent( "Source/Scripting/AngelScript" )
endif()
AddMonoAssembly( "FOnline" )
AddMonoSource( "FOnline" "Common" "Source/Scripting/Mono/*.cs" )
AddResources( "Core" "Resources/Core" )
AddEmbeddedResources( "Resources/Embedded" )
AddResources( "Core" "Resources/Embedded" ) # Duplicate embedded to core for correct data updating

# Force some variables for internal debugging purposes
if( NOT ${FO_FORCE_SINGLEPLAYER} STREQUAL "" )
	set( FO_SINGLEPLAYER ${FO_FORCE_SINGLEPLAYER} )
endif()
if( NOT ${FO_FORCE_ENABLE_3D} STREQUAL "" )
	set( FO_ENABLE_3D ${FO_FORCE_ENABLE_3D} )
endif()

# Configuration checks
if( FONLINE_CODE_COVERAGE AND ( FONLINE_BUILD_CLIENT OR FONLINE_BUILD_SERVER OR FONLINE_BUILD_EDITOR OR FONLINE_BUILD_MAPPER OR FONLINE_BUILD_SINGLE OR FONLINE_BUILD_ASCOMPILER OR FONLINE_BUILD_BAKER OR FONLINE_UNIT_TESTS ) )
	AbortMessage( "Code coverge build can not be mixed with other builds!" )
endif()
if( FONLINE_BUILD_SINGLE AND ( FONLINE_BUILD_SERVER OR FONLINE_BUILD_CLIENT ) )
	AbortMessage( "Singleplayer/Multiplayer configuration mismatch!" )
endif()
if( FO_SINGLEPLAYER AND ( FONLINE_BUILD_SERVER OR FONLINE_BUILD_CLIENT ) )
	AbortMessage( "No client or server builds in singleplayer mode!" )
endif()
if( NOT FO_SINGLEPLAYER AND FONLINE_BUILD_SINGLE )
	AbortMessage( "No single build in multiplayer mode!" )
endif()
if( FONLINE_BUILD_ASCOMPILER AND NOT FO_ANGELSCRIPT_SCRIPTING )
	AbortMessage( "AngelScript compiler build can not be without AngelScript scripting enabled!" )
endif()

# Global defines
add_compile_definitions( FO_SINGLEPLAYER=$<BOOL:${FO_SINGLEPLAYER}> )
add_compile_definitions( FO_ENABLE_3D=$<BOOL:${FO_ENABLE_3D}> )
add_compile_definitions( FO_NATIVE_SCRIPTING=$<BOOL:${FO_NATIVE_SCRIPTING}> )
add_compile_definitions( FO_ANGELSCRIPT_SCRIPTING=$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}> )
add_compile_definitions( FO_MONO_SCRIPTING=$<BOOL:${FO_MONO_SCRIPTING}> )
add_compile_definitions( FO_GEOMETRY=${FO_GEOMETRY} )

# Compiler options
set( CMAKE_CXX_STANDARD 17 )

if( MSVC )
	add_compile_options( $<$<COMPILE_LANGUAGE:CXX>:/std:c++17> )
	add_compile_options( /permissive- )
else()
	add_compile_options( $<$<COMPILE_LANGUAGE:CXX>:-std=c++17> )
endif()
if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
	add_compile_options( --param=max-vartrack-size=1000000 )
endif()

if( FONLINE_CODE_COVERAGE )
	if( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
		add_compile_options( -O0 -fprofile -instr-generate -fcoverage-mapping )
	elseif( CMAKE_CXX_COMPILER_ID MATCHES "GNU" )
		add_compile_options( -O0 --coverage )
		add_link_options( --coverage )
	endif()
endif()

# Basic includes
include_directories( "Source/Common" )
include_directories( "Source/Server" )
include_directories( "Source/Client" )
include_directories( "Source/Tools" )
include_directories( "Source/Scripting" )
include_directories( "Source/Frontend" )
include_directories( "Source/Singleplayer" )
include_directories( "${CMAKE_BINARY_DIR}/GeneratedSource" )

# Third-party libs
StatusMessage( "Third-party libs:" )

# SDL2
StatusMessage( "+ SDL2" )
set( FO_SDL_DIR "ThirdParty/SDL2" )
set( SDL2_DISABLE_INSTALL ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL2_DISABLE_UNINSTALL ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_SHARED OFF CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_STATIC ON CACHE BOOL "Forced by FOnline" FORCE )
set( SDL_TEST OFF CACHE BOOL "Forced by FOnline" FORCE )
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
add_compile_definitions( GL_GLEXT_PROTOTYPES )
target_compile_definitions( SDL2main PRIVATE "GL_GLEXT_PROTOTYPES" )
target_compile_definitions( SDL2-static PRIVATE "GL_GLEXT_PROTOTYPES" )
list( APPEND FO_RENDER_LIBS "SDL2main" "SDL2-static" )
list( APPEND FO_DUMMY_TRAGETS "sdl_headers_copy" )
DisableLibWarnings( SDL2main SDL2-static )

# Zlib
StatusMessage( "+ Zlib" )
set( FO_ZLIB_DIR "ThirdParty/zlib" )
add_subdirectory( "${FO_ZLIB_DIR}" )
include_directories( "${FO_ZLIB_DIR}" "${FO_ZLIB_DIR}/contrib" "${FOnlineProject_BINARY_DIR}/${FO_ZLIB_DIR}" )
list( APPEND FO_COMMON_LIBS "zlibstatic" )
DisableLibWarnings( zlibstatic )

# PNG
if( FONLINE_BUILD_BAKER OR FONLINE_BUILD_EDITOR OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
	StatusMessage( "+ PNG" )
	set( FO_PNG_DIR "ThirdParty/PNG" )
	set( SKIP_INSTALL_ALL ON CACHE BOOL "Forced by FOnline" FORCE )
	set( ZLIB_LIBRARY "zlibstatic" CACHE STRING "Forced by FOnline" FORCE )
	set( ZLIB_INCLUDE_DIR "../${FO_ZLIB_DIR}" "${FOnlineProject_BINARY_DIR}/${FO_ZLIB_DIR}" CACHE STRING "Forced by FOnline" FORCE )
	set( PNG_SHARED OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( PNG_STATIC ON CACHE BOOL "Forced by FOnline" FORCE )
	add_subdirectory( "${FO_PNG_DIR}" )
	include_directories( "${FO_PNG_DIR}" "${FOnlineProject_BINARY_DIR}/${FO_PNG_DIR}" )
	list( APPEND FO_BAKER_LIBS "png16_static" )
	DisableLibWarnings( png16_static )
endif()

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
	add_compile_definitions( GLEW_STATIC )
	target_compile_definitions( GLEW PRIVATE "GLEW_STATIC" )
	list( APPEND FO_RENDER_LIBS "GLEW" )
	DisableLibWarnings( GLEW )
endif()

# Assimp
StatusMessage( "+ Assimp (math headers)" )
include_directories( "ThirdParty/AssimpMath" )

# Fbx SDK
if( FO_ENABLE_3D )
	if( ( FONLINE_BUILD_BAKER OR FONLINE_BUILD_EDITOR ) AND NOT ( ( WIN32 OR CMAKE_SYSTEM_NAME MATCHES "Linux" ) AND CMAKE_SIZEOF_VOID_P EQUAL 8 ) )
		AbortMessage( "Using of FBX SDK for non 64 bit Linux & Windows builds is not supported" )
	endif()

	if( ( FONLINE_BUILD_BAKER OR FONLINE_BUILD_EDITOR OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE ) AND ( WIN32 OR CMAKE_SYSTEM_NAME MATCHES "Linux" ) AND CMAKE_SIZEOF_VOID_P EQUAL 8 )
		StatusMessage( "+ Fbx SDK" )

		set( FO_FBXSDK_DIR "ThirdParty/fbxsdk" )
		include_directories( "${FO_FBXSDK_DIR}" )

		if( WIN32 )
			list( APPEND FO_BAKER_SYSTEM_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/fbxsdk/libfbxsdk.lib" )
		else()
			list( APPEND FO_BAKER_SYSTEM_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/fbxsdk/libfbxsdk.so" )
		endif()

		add_compile_definitions( FO_HAVE_FBXSDK=1 )

	else()
		add_compile_definitions( FO_HAVE_FBXSDK=0 )
	endif()
endif()

macro( CopyFbxSdkLib target )
	if( FO_ENABLE_3D )
		if( WIN32 )
			get_target_property( dir ${target} RUNTIME_OUTPUT_DIRECTORY )
			add_custom_command( TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/fbxsdk/libfbxsdk.dll" ${dir} )
		endif()
	endif()
endmacro()

# Nlohmann Json
StatusMessage( "+ Nlohmann Json" )
set( FO_JSON_DIR "ThirdParty/Json" )
include_directories( "${FO_JSON_DIR}" )
add_compile_definitions( FO_HAVE_JSON=1 )

# Fmt
StatusMessage( "+ Fmt" )
set( FO_FMT_DIR "ThirdParty/fmt" )
add_subdirectory( "${FO_FMT_DIR}" )
include_directories( "${FO_FMT_DIR}/include" )
list( APPEND FO_COMMON_LIBS "fmt" )
DisableLibWarnings( fmt )

# LibreSSL
if( ( FONLINE_BUILD_SERVER OR FONLINE_BUILD_EDITOR OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE ) AND NOT FO_SINGLEPLAYER )
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
	include_directories( "${FOnlineProject_BINARY_DIR}/${FO_LIBRESSL_DIR}/crypto" )
	include_directories( "${FOnlineProject_BINARY_DIR}/${FO_LIBRESSL_DIR}/ssl" )
	list( APPEND FO_SERVER_LIBS "ssl" "crypto" "tls" )
	DisableLibWarnings( ssl crypto tls )
endif()

# Asio & Websockets
if( ( FONLINE_BUILD_SERVER OR FONLINE_BUILD_EDITOR OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE ) AND NOT ANDROID AND NOT FO_SINGLEPLAYER )
	StatusMessage( "+ Asio" )
	set( FO_ASIO_DIR "ThirdParty/Asio" )
	include_directories( "${FO_ASIO_DIR}/include" )

	StatusMessage( "+ Websockets" )
	set( FO_WEBSOCKETS_DIR "ThirdParty/websocketpp" )
	include_directories( "${FO_WEBSOCKETS_DIR}" )

	add_compile_definitions( FO_HAVE_ASIO=1 )
else()
	add_compile_definitions( FO_HAVE_ASIO=0 )
endif()

# MongoDB & Bson
if( FONLINE_BUILD_SERVER OR FONLINE_BUILD_EDITOR OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE OR FONLINE_BUILD_SINGLE )
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
	set( MONGO_USE_CCACHE OFF CACHE STRING "Forced by FOnline" FORCE )

	if( ( FONLINE_BUILD_SERVER OR FONLINE_BUILD_EDITOR OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE ) AND NOT FO_SINGLEPLAYER )
		StatusMessage( "+ MongoDB" )
		set( ENABLE_MONGOC ON CACHE STRING "Forced by FOnline" FORCE )
		add_compile_definitions( FO_HAVE_MONGO=1 )
	else()
		set( ENABLE_MONGOC OFF CACHE STRING "Forced by FOnline" FORCE )
		add_compile_definitions( FO_HAVE_MONGO=0 )
	endif()

	add_subdirectory( "${FO_MONGODB_DIR}" )

	include_directories( "${FOnlineProject_BINARY_DIR}/${FO_MONGODB_DIR}/src/libbson/src/bson" )
	include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MONGODB_DIR}/src/libbson/src" )
	target_compile_definitions( bson_static PRIVATE "BSON_COMPILATION;BSON_STATIC;JSONSL_PARSE_NAN" )
	add_compile_definitions( BSON_COMPILATION BSON_STATIC JSONSL_PARSE_NAN )
	list( APPEND FO_SERVER_LIBS "bson_static" )
	DisableLibWarnings( bson_static )

	if( ENABLE_MONGOC )
		include_directories( "${FOnlineProject_BINARY_DIR}/${FO_MONGODB_DIR}/src/libmongoc/src/mongoc" )
		include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MONGODB_DIR}/src/libmongoc/src" )
		target_compile_definitions( mongoc_static PRIVATE "BSON_COMPILATION;BSON_STATIC;JSONSL_PARSE_NAN" )
		list( APPEND FO_SERVER_LIBS "mongoc_static" )
		list( APPEND FO_DUMMY_TRAGETS "mongoc-cxx-check" "dist" "distcheck" )
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
	target_compile_definitions( unqlite PRIVATE "JX9_DISABLE_BUILTIN_FUNC" )
	add_compile_definitions( FO_HAVE_UNQLITE=1 )
else()
	add_compile_definitions( FO_HAVE_UNQLITE=0 )
endif()

# Dear ImGui
StatusMessage( "+ Dear ImGui" )
set( FO_DEAR_IMGUI_DIR "ThirdParty/imgui" )
file( GLOB FO_IMGUI_SOURCE "${FO_DEAR_IMGUI_DIR}/*.cpp" "${FO_DEAR_IMGUI_DIR}/*.h" )
include_directories( "${FO_DEAR_IMGUI_DIR}" )
add_library( ImGui ${FO_IMGUI_SOURCE} )
target_compile_definitions( ImGui PRIVATE "IMGUI_DISABLE_OBSOLETE_FUNCTIONS" "IMGUI_DISABLE_DEMO_WINDOWS" "IMGUI_DISABLE_DEBUG_TOOLS" )
add_compile_definitions( IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DISABLE_DEBUG_TOOLS )
list( APPEND FO_COMMON_LIBS "ImGui" )
if( WIN32 AND WINRT )
	target_compile_definitions( ImGui PRIVATE "IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS" )
	add_compile_definitions( IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS )
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
		elseif( haveBFD )
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
add_subdirectory( "${FO_SPARK_DIR}/projects/external/pugi" )
include_directories( "${FO_SPARK_DIR}/spark/include" )
include_directories( "${FO_SPARK_DIR}/thirdparty/PugiXML" )
list( APPEND FO_CLIENT_LIBS "SPARK_Core" "PugiXML" )
DisableLibWarnings( SPARK_Core PugiXML )

# glslang & SPIRV-Cross
if( FONLINE_BUILD_BAKER OR FONLINE_BUILD_EDITOR OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
	StatusMessage( "+ glslang" )
	set( FO_GLSLANG_DIR "ThirdParty/glslang" )
	set( BUILD_EXTERNAL OFF CACHE BOOL "Forced by FOnline" FORCE )
	set( OVERRIDE_MSVCCRT OFF CACHE BOOL "Forced by FOnline" FORCE )
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
	list( APPEND FO_BAKER_LIBS "glslang" "OGLCompiler" "OSDependent" "SPIRV" "GenericCodeGen" "MachineIndependent" )
	DisableLibWarnings( glslang OGLCompiler OSDependent SPIRV GenericCodeGen MachineIndependent )

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

# span
include_directories( "ThirdParty/span/include/tcb" )

# AngelScript scripting
if( FO_ANGELSCRIPT_SCRIPTING )
	# AngelScript
	StatusMessage( "+ AngelScript" )
	set( FO_ANGELSCRIPT_DIR "ThirdParty/AngelScript" )
	# add_compile_definitions( $<$<CONFIG:Debug>:AS_DEBUG=1> )
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
		add_compile_definitions( AS_NO_COMPILER )
	endif()
	if( EMSCRIPTEN OR ( APPLE AND PLATFORM ) OR ( ANDROID AND CMAKE_SIZEOF_VOID_P EQUAL 8 ) )
		target_compile_definitions( Angelscript PRIVATE "AS_MAX_PORTABILITY" )
		target_compile_definitions( AngelscriptExt PRIVATE "AS_MAX_PORTABILITY" )
		add_compile_definitions( AS_MAX_PORTABILITY )
	endif()
	if( EMSCRIPTEN )
		target_compile_definitions( Angelscript PRIVATE "WIP_16BYTE_ALIGN" )
		target_compile_definitions( AngelscriptExt PRIVATE "WIP_16BYTE_ALIGN" )
		add_compile_definitions( WIP_16BYTE_ALIGN )
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
	add_compile_definitions( HAVE_EXTERN_DEFINED_WINAPI_SUPPORT )
	DisableLibWarnings( libmono )
endif()

# Engine sources
list( APPEND FO_COMMON_SOURCE
	"Source/Common/AnyData.cpp"
	"Source/Common/AnyData.h"
	"Source/Common/CacheStorage.cpp"
	"Source/Common/CacheStorage.h"
	"Source/Common/Common.cpp"
	"Source/Common/Common.h"
	"Source/Common/ConfigFile.cpp"
	"Source/Common/ConfigFile.h"
	"Source/Common/DataSource.cpp"
	"Source/Common/DataSource.h"
	"Source/Common/DeferredCalls.cpp"
	"Source/Common/DeferredCalls.h"
	"Source/Common/Dialogs.cpp"
	"Source/Common/Dialogs.h"
	"Source/Common/DiskFileSystem.cpp"
	"Source/Common/DiskFileSystem.h"
	"Source/Common/EngineBase.cpp"
	"Source/Common/EngineBase.h"
	"Source/Common/Entity.cpp"
	"Source/Common/Entity.h"
	"Source/Common/EntityProtos.cpp"
	"Source/Common/EntityProtos.h"
	"Source/Common/EntityProperties.h"
	"Source/Common/FileSystem.cpp"
	"Source/Common/FileSystem.h"
	"Source/Common/GenericUtils.cpp"
	"Source/Common/GenericUtils.h"
	"Source/Common/GeometryHelper.cpp"
	"Source/Common/GeometryHelper.h"
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
	"Source/Common/PropertiesSerializator.cpp"
	"Source/Common/PropertiesSerializator.h"
	"Source/Common/ProtoManager.cpp"
	"Source/Common/ProtoManager.h"
	"Source/Common/ScriptSystem.cpp"
	"Source/Common/ScriptSystem.h"
	"Source/Common/Settings.cpp"
	"Source/Common/Settings.h"
	"Source/Common/Settings-Include.h"
	"Source/Common/StringUtils.cpp"
	"Source/Common/StringUtils.h"
	"Source/Common/Timer.cpp"
	"Source/Common/Timer.h"
	"Source/Common/TwoBitMask.cpp"
	"Source/Common/TwoBitMask.h"
	"Source/Common/UcsTables-Include.h"
	"Source/Common/WinApi-Include.h"
	"Source/Scripting/CommonGlobalScriptMethods.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/Version-Include.h"
	"${CMAKE_BINARY_DIR}/GeneratedSource/DebugSettings-Include.h"
	"${CMAKE_BINARY_DIR}/GeneratedSource/EmbeddedResources-Include.h"
	"${CMAKE_BINARY_DIR}/GeneratedSource/GenericCode-Common.cpp" )

list( APPEND FO_SERVER_BASE_SOURCE
	"Source/Server/AdminPanel.cpp"
	"Source/Server/AdminPanel.h"
	"Source/Server/ClientConnection.cpp"
	"Source/Server/ClientConnection.h"
	"Source/Server/Critter.cpp"
	"Source/Server/Critter.h"
	"Source/Server/CritterManager.cpp"
	"Source/Server/CritterManager.h"
	"Source/Server/DataBase.cpp"
	"Source/Server/DataBase.h"
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
	"Source/Server/Server.cpp"
	"Source/Server/Server.h"
	"Source/Server/ServerDeferredCalls.cpp"
	"Source/Server/ServerDeferredCalls.h"
	"Source/Server/ServerEntity.cpp"
	"Source/Server/ServerEntity.h"
	"Source/Scripting/ServerGlobalScriptMethods.cpp"
	"Source/Scripting/ServerPlayerScriptMethods.cpp"
	"Source/Scripting/ServerItemScriptMethods.cpp"
	"Source/Scripting/ServerCritterScriptMethods.cpp"
	"Source/Scripting/ServerMapScriptMethods.cpp"
	"Source/Scripting/ServerLocationScriptMethods.cpp" )

list( APPEND FO_CLIENT_BASE_SOURCE
	"Source/Client/3dAnimation.cpp"
	"Source/Client/3dAnimation.h"
	"Source/Client/3dStuff.cpp"
	"Source/Client/3dStuff.h"
	"Source/Client/Client.cpp"
	"Source/Client/Client.h"
	"Source/Client/ClientEntity.cpp"
	"Source/Client/ClientEntity.h"
	"Source/Client/CritterHexView.cpp"
	"Source/Client/CritterHexView.h"
	"Source/Client/CritterView.cpp"
	"Source/Client/CritterView.h"
	"Source/Client/EffectManager.cpp"
	"Source/Client/EffectManager.h"
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
	"Source/Client/ServerConnection.cpp"
	"Source/Client/ServerConnection.h"
	"Source/Client/SoundManager.cpp"
	"Source/Client/SoundManager.h"
	"Source/Client/SpriteManager.cpp"
	"Source/Client/SpriteManager.h"
	"Source/Client/Sprites.cpp"
	"Source/Client/Sprites.h"
	"Source/Client/Updater.cpp"
	"Source/Client/Updater.h"
	"Source/Client/VisualParticles.cpp"
	"Source/Client/VisualParticles.h"
	"Source/Client/SparkExtension.cpp"
	"Source/Client/SparkExtension.h"
	"Source/Scripting/ClientGlobalScriptMethods.cpp"
	"Source/Scripting/ClientPlayerScriptMethods.cpp"
	"Source/Scripting/ClientItemScriptMethods.cpp"
	"Source/Scripting/ClientCritterScriptMethods.cpp"
	"Source/Scripting/ClientMapScriptMethods.cpp"
	"Source/Scripting/ClientLocationScriptMethods.cpp" )

if( NOT FO_SINGLEPLAYER )
	list( APPEND FO_SERVER_SOURCE
		${FO_SERVER_BASE_SOURCE}
		"Source/Scripting/ServerScripting.h"
		"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-Server.cpp"
		"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Server.cpp"
		"${CMAKE_BINARY_DIR}/GeneratedSource/MonoScripting-Server.cpp"
		"${CMAKE_BINARY_DIR}/GeneratedSource/NativeScripting-Server.cpp" )
		
	list( APPEND FO_CLIENT_SOURCE
		${FO_CLIENT_BASE_SOURCE}
		"Source/Scripting/ClientScripting.h"
		"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-Client.cpp"
		"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Client.cpp"
		"${CMAKE_BINARY_DIR}/GeneratedSource/MonoScripting-Client.cpp"
		"${CMAKE_BINARY_DIR}/GeneratedSource/NativeScripting-Client.cpp" )
		
else()
	list( APPEND FO_SINGLE_SOURCE
		${FO_SERVER_BASE_SOURCE}
		${FO_CLIENT_BASE_SOURCE}
		"Source/Singleplayer/Single.cpp"
		"Source/Singleplayer/Single.h"
		"Source/Scripting/SingleScripting.h"
		"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-Single.cpp"
		"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Single.cpp"
		"${CMAKE_BINARY_DIR}/GeneratedSource/MonoScripting-Single.cpp"
		"${CMAKE_BINARY_DIR}/GeneratedSource/NativeScripting-Single.cpp" )
endif()

list( APPEND FO_EDITOR_SOURCE
	"Source/Tools/Editor.h"
	"Source/Tools/Editor.cpp"
	"Source/Tools/AssetExplorer.h"
	"Source/Tools/AssetExplorer.cpp"
	# "Source/Tools/DialogEditor.h"
	# "Source/Tools/DialogEditor.cpp"
	# "Source/Tools/InterfaceEditor.h"
	# "Source/Tools/InterfaceEditor.cpp"
	# "Source/Tools/ProtoEditor.h"
	# "Source/Tools/ProtoEditor.cpp"
	# "Source/Tools/ModelEditor.h"
	# "Source/Tools/ModelEditor.cpp"
	# "Source/Tools/EffectEditor.h"
	# "Source/Tools/EffectEditor.cpp"
	"Source/Tools/ParticleEditor.h"
	"Source/Tools/ParticleEditor.cpp" )

list( APPEND FO_MAPPER_SOURCE
	"Source/Tools/Mapper.h"
	"Source/Tools/Mapper.cpp"
	"Source/Scripting/MapperScripting.h"
	"Source/Scripting/MapperGlobalScriptMethods.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-Mapper.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Mapper.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/MonoScripting-Mapper.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/NativeScripting-Mapper.cpp" )

list( APPEND FO_BAKER_SOURCE
	"Source/Tools/Baker.h"
	"Source/Tools/Baker.cpp"
	"Source/Tools/EffectBaker.h"
	"Source/Tools/EffectBaker.cpp"
	"Source/Tools/F2Palette-Include.h"
	"Source/Tools/ImageBaker.h"
	"Source/Tools/ImageBaker.cpp"
	"Source/Tools/ModelBaker.h"
	"Source/Tools/ModelBaker.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-Baker.cpp" )

if( FO_ANGELSCRIPT_SCRIPTING )
	list( APPEND FO_ASCOMPILER_SOURCE
		"Source/Scripting/MapperScripting.h"
		"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-MapperCompiler.cpp"
		"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-MapperCompiler.cpp" )
	if( NOT FO_SINGLEPLAYER )
		list( APPEND FO_ASCOMPILER_SOURCE
			"Source/Scripting/ServerScripting.h"
			"Source/Scripting/ClientScripting.h"
			"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ServerCompiler.cpp"
			"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ServerCompilerValidation.cpp"
			"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ClientCompiler.cpp"
			"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-ServerCompiler.cpp"
			"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-ClientCompiler.cpp" )
	else()
		list( APPEND FO_ASCOMPILER_SOURCE
			"Source/Scripting/SingleScripting.h"
			"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-SingleCompiler.cpp"
			"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-SingleCompilerValidation.cpp"
			"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-SingleCompiler.cpp" )
	endif()
endif()

list( APPEND FO_SOURCE_META_FILES
	"Source/Singleplayer/Single.h"
	"Source/Frontend/Application.h"
	"Source/Frontend/Rendering.h"
	"Source/Common/Common.h"
	"Source/Common/Entity.h"
	"Source/Common/EntityProperties.h"
	"Source/Common/Settings-Include.h"
	"Source/Common/DataRegistration-Template.cpp"
	"Source/Common/GenericCode-Template.cpp"
	"Source/Client/Client.h"
	"Source/Client/Sprites.h"
	"Source/Server/Critter.h"
	"Source/Server/Item.h"
	"Source/Server/Location.h"
	"Source/Server/Map.h"
	"Source/Server/Player.h"
	"Source/Server/Server.h"
	"Source/Tools/Mapper.h"
	"Source/Scripting/AngelScriptScripting-Template.cpp"
	"Source/Scripting/MonoScripting-Template.cpp"
	"Source/Scripting/NativeScripting-Template.cpp"
	"Source/Scripting/ServerGlobalScriptMethods.cpp"
	"Source/Scripting/ServerPlayerScriptMethods.cpp"
	"Source/Scripting/ServerItemScriptMethods.cpp"
	"Source/Scripting/ServerCritterScriptMethods.cpp"
	"Source/Scripting/ServerMapScriptMethods.cpp"
	"Source/Scripting/ServerLocationScriptMethods.cpp"
	"Source/Scripting/ClientGlobalScriptMethods.cpp"
	"Source/Scripting/ClientPlayerScriptMethods.cpp"
	"Source/Scripting/ClientItemScriptMethods.cpp"
	"Source/Scripting/ClientCritterScriptMethods.cpp"
	"Source/Scripting/ClientMapScriptMethods.cpp"
	"Source/Scripting/ClientLocationScriptMethods.cpp"
	"Source/Scripting/MapperGlobalScriptMethods.cpp"
	"Source/Scripting/CommonGlobalScriptMethods.cpp" )

list( APPEND FO_TESTS_SOURCE
	"Source/Tests/Test_AnyData.cpp"
	"Source/Tests/Test_GenericUtils.cpp" )

# Code generation
include( FindPython3 )
find_package( Python3 REQUIRED COMPONENTS Interpreter )

list( APPEND FO_CODEGEN_COMMAND_ARGS -buildhash "${FO_BUILD_HASH}" )
list( APPEND FO_CODEGEN_COMMAND_ARGS -genoutput "${CMAKE_BINARY_DIR}/GeneratedSource" )
list( APPEND FO_CODEGEN_COMMAND_ARGS -devname "${FO_DEV_NAME}" )
list( APPEND FO_CODEGEN_COMMAND_ARGS -gamename "${FO_NICE_NAME} ${FO_GAME_VERSION}" )

list( APPEND FO_CODEGEN_COMMAND_ARGS -markdown )
if( FONLINE_INFO_MARKDOWN_OUTPUT )
	list( APPEND FO_CODEGEN_COMMAND_ARGS -mdpath "${FONLINE_INFO_MARKDOWN_OUTPUT}" )
endif()

if( FO_NATIVE_SCRIPTING )
	list( APPEND FO_CODEGEN_COMMAND_ARGS -native )
endif()
if( FO_ANGELSCRIPT_SCRIPTING )
	list( APPEND FO_CODEGEN_COMMAND_ARGS -angelscript )
	if( FO_GENERATE_ANGELSCRIPT_CONTENT )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -ascontentoutput "${FO_GENERATE_ANGELSCRIPT_CONTENT}" )
	endif()
endif()
if( FO_MONO_SCRIPTING )
	list( APPEND FO_CODEGEN_COMMAND_ARGS -csharp )
endif()
if( NOT FO_SINGLEPLAYER )
	list( APPEND FO_CODEGEN_COMMAND_ARGS -multiplayer )
else()
	list( APPEND FO_CODEGEN_COMMAND_ARGS -singleplayer )
endif()
list( APPEND FO_CODEGEN_COMMAND_ARGS -mapper )

foreach( entry ${FO_MONO_ASSEMBLIES} )
	list( APPEND FO_CODEGEN_COMMAND_ARGS -monoassembly ${entry} )
	foreach( ref ${MonoAssembly_${entry}_CommonRefs} )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monoserverref "${entry},${ref}" )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monoclientref "${entry},${ref}" )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monosingleref "${entry},${ref}" )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monomapperref "${entry},${ref}" )
	endforeach()
	foreach( ref ${MonoAssembly_${entry}_ServerRefs} )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monoserverref "${entry},${ref}" )
	endforeach()
	foreach( ref ${MonoAssembly_${entry}_ClientRefs} )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monoclientref "${entry},${ref}" )
	endforeach()
	foreach( ref ${MonoAssembly_${entry}_SingleRefs} )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monosingleref "${entry},${ref}" )
	endforeach()
	foreach( ref ${MonoAssembly_${entry}_MapperRefs} )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monomapperref "${entry},${ref}" )
	endforeach()
	foreach( src ${MonoAssembly_${entry}_CommonSource} )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monoserversource "${entry},${src}" )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monoclientsource "${entry},${src}" )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monosinglesource "${entry},${src}" )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monomappersource "${entry},${src}" )
		list( APPEND FO_MONO_SOURCE ${src} )
	endforeach()
	foreach( src ${MonoAssembly_${entry}_ServerSource} )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monoserversource "${entry},${src}" )
		list( APPEND FO_MONO_SOURCE ${src} )
	endforeach()
	foreach( src ${MonoAssembly_${entry}_ClientSource} )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monoclientsource "${entry},${src}" )
		list( APPEND FO_MONO_SOURCE ${src} )
	endforeach()
	foreach( src ${MonoAssembly_${entry}_SingleSource} )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monosinglesource "${entry},${src}" )
		list( APPEND FO_MONO_SOURCE ${src} )
	endforeach()
	foreach( src ${MonoAssembly_${entry}_MapperSource} )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -monomappersource "${entry},${src}" )
		list( APPEND FO_MONO_SOURCE ${src} )
	endforeach()
endforeach()

foreach( entry ${FO_CONTENT} )
	list( APPEND FO_CODEGEN_COMMAND_ARGS -content "${entry}" )
endforeach()
foreach( entry ${FO_RESOURCES} )
	list( APPEND FO_CODEGEN_COMMAND_ARGS -resource "${entry}" )
endforeach()

if( FO_DEBUGGING_CONFIG )
	list( APPEND FO_CODEGEN_COMMAND_ARGS -config "ExternalConfig,${FO_BACKED_RESOURCES_OUTPUT}/Configs/${FO_DEBUGGING_CONFIG}.focfg" )
	list( APPEND FO_CODEGEN_COMMAND_ARGS -config "ResourcesDir,${FO_BACKED_RESOURCES_OUTPUT}" )
	list( APPEND FO_CODEGEN_COMMAND_ARGS -config "EmbeddedResources,${FO_BACKED_RESOURCES_OUTPUT}/Embedded" )

	list( APPEND FO_CODEGEN_COMMAND_ARGS -config "BakeOutput,${FO_BACKED_RESOURCES_OUTPUT}" )
	foreach( entry ${FO_CONTENT} )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -config "BakeContentEntries,+${entry}" )
	endforeach()
	foreach( entry ${FO_RESOURCES} )
		list( APPEND FO_CODEGEN_COMMAND_ARGS -config "BakeResourceEntries,+${entry}" )
	endforeach()
endif()

list( APPEND FO_CODEGEN_META_SOURCE
	${FONLINE_CMAKE_CONTRIBUTION}
	${FO_SOURCE_META_FILES}
	${FO_CONTENT_META_FILES}
	${FO_MONO_SOURCE} )

foreach( entry ${FO_CODEGEN_META_SOURCE} )
	list( APPEND FO_CODEGEN_COMMAND_ARGS -meta ${entry} )
endforeach()

list( APPEND FO_CODEGEN_OUTPUT
	"${CMAKE_BINARY_DIR}/GeneratedSource/CodeGenTouch"
	"${CMAKE_BINARY_DIR}/GeneratedSource/Version-Include.h"
	"${CMAKE_BINARY_DIR}/GeneratedSource/DebugSettings-Include.h"
	"${CMAKE_BINARY_DIR}/GeneratedSource/EmbeddedResources-Include.h"
	"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-Server.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-Client.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-Single.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-Mapper.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-Baker.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-ServerCompiler.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-ClientCompiler.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-SingleCompiler.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/DataRegistration-MapperCompiler.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/GenericCode-Common.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Server.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Client.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Mapper.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-Single.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ServerCompiler.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ServerCompilerValidation.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-ClientCompiler.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-MapperCompiler.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-SingleCompiler.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/AngelScriptScripting-SingleCompilerValidation.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/MonoScripting-Server.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/MonoScripting-Client.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/MonoScripting-Mapper.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/MonoScripting-Single.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/NativeScripting-Server.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/NativeScripting-Client.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/NativeScripting-Mapper.cpp"
	"${CMAKE_BINARY_DIR}/GeneratedSource/NativeScripting-Single.cpp" )

file( WRITE "${CMAKE_BINARY_DIR}/codegen-args.txt" "" )
foreach( entry ${FO_CODEGEN_COMMAND_ARGS} )
	file( APPEND "${CMAKE_BINARY_DIR}/codegen-args.txt" "${entry}\n" )
endforeach()

set( FO_CODEGEN_COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/BuildTools/codegen.py" "@${CMAKE_BINARY_DIR}/codegen-args.txt" )

add_custom_command( OUTPUT ${FO_CODEGEN_OUTPUT}
	COMMAND ${FO_CODEGEN_COMMAND}
	COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/GeneratedSource/CodeGenTouch"
	DEPENDS ${FO_CODEGEN_META_SOURCE}
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	COMMENT "Code generation" )

add_custom_target( CodeGeneration
	DEPENDS ${FO_CODEGEN_OUTPUT}
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" )
list( APPEND FO_COMMANDS_GROUP "CodeGeneration" )

add_custom_target( ForceCodeGeneration
	COMMAND ${FO_CODEGEN_COMMAND}
	COMMAND ${CMAKE_COMMAND} -E touch ${FO_CODEGEN_OUTPUT}
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" )
list( APPEND FO_COMMANDS_GROUP "ForceCodeGeneration" )

# Core libs
StatusMessage( "Core libs:" )

if( FONLINE_BUILD_CLIENT OR FONLINE_BUILD_SERVER OR FONLINE_BUILD_EDITOR OR FONLINE_BUILD_MAPPER OR FONLINE_BUILD_SINGLE OR FONLINE_BUILD_ASCOMPILER OR FONLINE_BUILD_BAKER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
	StatusMessage( "+ AppHeadless" )
	add_library( AppHeadless STATIC
		"Source/Frontend/Application.h"
		"Source/Frontend/ApplicationHeadless.cpp" )
	add_dependencies( AppHeadless CodeGeneration )
	list( APPEND FO_CORE_LIBS_GROUP "AppHeadless" )

	if( NOT FO_HEADLESS_ONLY )
		StatusMessage( "+ AppFrontend" )
		add_library( AppFrontend STATIC
			"Source/Frontend/Application.h"
			"Source/Frontend/Application.cpp"
			"Source/Frontend/Rendering.cpp"
			"Source/Frontend/Rendering.h"
			"Source/Frontend/Rendering-Direct3D.cpp"
			"Source/Frontend/Rendering-OpenGL.cpp" )
		add_dependencies( AppFrontend CodeGeneration )
		target_link_libraries( AppFrontend ${FO_RENDER_SYSTEM_LIBS} ${FO_RENDER_LIBS} )
		list( APPEND FO_CORE_LIBS_GROUP "AppFrontend" )
	endif()

	StatusMessage( "+ CommonLib" )
	add_library( CommonLib STATIC ${FO_COMMON_SOURCE} )
	add_dependencies( CommonLib CodeGeneration )
	target_link_libraries( CommonLib ${FO_COMMON_SYSTEM_LIBS} ${FO_COMMON_LIBS} ${CMAKE_DL_LIBS} )
	list( APPEND FO_CORE_LIBS_GROUP "CommonLib" )
endif()

if( NOT FO_SINGLEPLAYER AND ( FONLINE_BUILD_CLIENT OR FONLINE_BUILD_EDITOR OR FONLINE_BUILD_MAPPER OR FONLINE_BUILD_SERVER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE ) )
	StatusMessage( "+ ClientLib" )
	add_library( ClientLib STATIC ${FO_CLIENT_SOURCE} )
	add_dependencies( ClientLib CodeGeneration )
	target_link_libraries( ClientLib CommonLib ${FO_CLIENT_SYSTEM_LIBS} ${FO_CLIENT_LIBS} )
	list( APPEND FO_CORE_LIBS_GROUP "ClientLib" )
endif()

if( NOT FO_SINGLEPLAYER AND ( FONLINE_BUILD_SERVER OR FONLINE_BUILD_EDITOR OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE ) )
	StatusMessage( "+ ServerLib" )
	add_library( ServerLib STATIC ${FO_SERVER_SOURCE} )
	add_dependencies( ServerLib CodeGeneration )
	target_link_libraries( ServerLib CommonLib ${FO_SERVER_SYSTEM_LIBS} ${FO_SERVER_LIBS} )
	list( APPEND FO_CORE_LIBS_GROUP "ServerLib" )
endif()

if( FO_SINGLEPLAYER AND ( FONLINE_BUILD_SINGLE OR FONLINE_BUILD_EDITOR OR FONLINE_BUILD_MAPPER OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE ) )
	StatusMessage( "+ SingleLib" )
	add_library( SingleLib STATIC ${FO_SINGLE_SOURCE} )
	add_dependencies( SingleLib CodeGeneration )
	target_link_libraries( SingleLib CommonLib ${FO_SERVER_SYSTEM_LIBS} ${FO_SERVER_LIBS} ${FO_CLIENT_SYSTEM_LIBS} ${FO_CLIENT_LIBS} )
	list( APPEND FO_CORE_LIBS_GROUP "SingleLib" )
endif()

if( FONLINE_BUILD_MAPPER OR FONLINE_BUILD_EDITOR OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
	StatusMessage( "+ MapperLib" )
	add_library( MapperLib STATIC ${FO_MAPPER_SOURCE} )
	add_dependencies( MapperLib CodeGeneration )
	target_link_libraries( MapperLib CommonLib )
	list( APPEND FO_CORE_LIBS_GROUP "MapperLib" )
endif()

if( FO_ANGELSCRIPT_SCRIPTING AND ( FONLINE_BUILD_ASCOMPILER OR FONLINE_BUILD_BAKER OR FONLINE_BUILD_EDITOR OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE ) )
	StatusMessage( "+ ASCompilerLib" )
	add_library( ASCompilerLib STATIC ${FO_ASCOMPILER_SOURCE} )
	add_dependencies( ASCompilerLib CodeGeneration )
	target_link_libraries( ASCompilerLib CommonLib )
	list( APPEND FO_CORE_LIBS_GROUP "ASCompilerLib" )
endif()

if( FONLINE_BUILD_BAKER OR FONLINE_BUILD_EDITOR OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
	StatusMessage( "+ BakerLib" )
	add_library( BakerLib STATIC ${FO_BAKER_SOURCE} )
	add_dependencies( BakerLib CodeGeneration )
	set_target_properties( BakerLib PROPERTIES COMPILE_DEFINITIONS "FO_ASYNC_BAKE=0" )
	target_link_libraries( BakerLib CommonLib ${FO_BAKER_SYSTEM_LIBS} ${FO_BAKER_LIBS} )
	list( APPEND FO_CORE_LIBS_GROUP "BakerLib" )
endif()

if( FONLINE_BUILD_EDITOR OR FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
	StatusMessage( "+ EditorLib" )
	add_library( EditorLib STATIC ${FO_EDITOR_SOURCE} )
	add_dependencies( EditorLib CodeGeneration )
	target_link_libraries( EditorLib CommonLib )
	list( APPEND FO_CORE_LIBS_GROUP "EditorLib" )
endif()

# Applications
StatusMessage( "Applications:" )

if( NOT FO_SINGLEPLAYER AND FONLINE_BUILD_CLIENT )
	if( NOT FO_BUILD_LIBRARY )
		StatusMessage( "+ ${FO_DEV_NAME}_Client" )
		list( APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Client" )
		add_executable( ${FO_DEV_NAME}_Client WIN32 "Source/Applications/ClientApp.cpp" "Resources/Default.rc" )
		# Todo: cmake make bundles for Mac and iOS
		# add_executable( ${FO_DEV_NAME}_Client MACOSX_BUNDLE ... "Default.rc" )
		set_target_properties( ${FO_DEV_NAME}_Client PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_CLIENT_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FONLINE_OUTPUT_PATH} )
	else()
		StatusMessage( "+ ${FO_DEV_NAME}_Client (shared library)" )
		list( APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Client" )
		add_library( ${FO_DEV_NAME}_Client SHARED "Source/Applications/ClientApp.cpp" )
		set_target_properties( ${FO_DEV_NAME}_Client PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${FO_CLIENT_OUTPUT} )
	endif()
	set_target_properties( ${FO_DEV_NAME}_Client PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_Client" )
	set_target_properties( ${FO_DEV_NAME}_Client PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0" )
	target_link_libraries( ${FO_DEV_NAME}_Client "AppFrontend" "ClientLib" )
	WriteBuildHash( ${FO_DEV_NAME}_Client )
endif()

if( NOT FO_SINGLEPLAYER AND FONLINE_BUILD_SERVER )
	StatusMessage( "+ ${FO_DEV_NAME}_Server" )
	list( APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Server" )
	add_executable( ${FO_DEV_NAME}_Server WIN32 "Source/Applications/ServerApp.cpp" "Resources/Default.rc" )
	set_target_properties( ${FO_DEV_NAME}_Server PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SERVER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FONLINE_OUTPUT_PATH} )
	set_target_properties( ${FO_DEV_NAME}_Server PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_Server" )
	set_target_properties( ${FO_DEV_NAME}_Server PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0" )
	target_link_libraries( ${FO_DEV_NAME}_Server "AppFrontend" "ServerLib" "ClientLib" )
	WriteBuildHash( ${FO_DEV_NAME}_Server )

	StatusMessage( "+ ${FO_DEV_NAME}_ServerHeadless" )
	list( APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_ServerHeadless" )
	add_executable( ${FO_DEV_NAME}_ServerHeadless "Source/Applications/ServerHeadlessApp.cpp" )
	set_target_properties( ${FO_DEV_NAME}_ServerHeadless PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SERVER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FONLINE_OUTPUT_PATH} )
	set_target_properties( ${FO_DEV_NAME}_ServerHeadless PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_ServerHeadless" )
	set_target_properties( ${FO_DEV_NAME}_ServerHeadless PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0" )
	target_link_libraries( ${FO_DEV_NAME}_ServerHeadless "AppHeadless" "ServerLib" )
	WriteBuildHash( ${FO_DEV_NAME}_ServerHeadless )

	if( WIN32 )
		StatusMessage( "+ ${FO_DEV_NAME}_ServerService" )
		list( APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_ServerService" )
		add_executable( ${FO_DEV_NAME}_ServerService "Source/Applications/ServerServiceApp.cpp" )
		set_target_properties( ${FO_DEV_NAME}_ServerService PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SERVER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FONLINE_OUTPUT_PATH} )
		set_target_properties( ${FO_DEV_NAME}_ServerService PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_ServerService" )
		set_target_properties( ${FO_DEV_NAME}_ServerService PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0" )
		target_link_libraries( ${FO_DEV_NAME}_ServerService "AppHeadless" "ServerLib" )
		WriteBuildHash( ${FO_DEV_NAME}_ServerService )
	else()
		StatusMessage( "+ ${FO_DEV_NAME}_ServerDaemon" )
		list( APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_ServerDaemon" )
		add_executable( ${FO_DEV_NAME}_ServerDaemon "Source/Applications/ServerDaemonApp.cpp" )
		set_target_properties( ${FO_DEV_NAME}_ServerDaemon PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SERVER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FONLINE_OUTPUT_PATH} )
		set_target_properties( ${FO_DEV_NAME}_ServerDaemon PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_ServerDaemon" )
		set_target_properties( ${FO_DEV_NAME}_ServerDaemon PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0" )
		target_link_libraries( ${FO_DEV_NAME}_ServerDaemon "AppHeadless" "ServerLib" )
		WriteBuildHash( ${FO_DEV_NAME}_ServerDaemon )
	endif()
endif()

if( FO_SINGLEPLAYER AND FONLINE_BUILD_SINGLE )
	if( NOT FO_BUILD_LIBRARY )
		StatusMessage( "+ ${FO_DEV_NAME}" )
		list( APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}" )
		add_executable( ${FO_DEV_NAME} WIN32 "Source/Applications/SingleApp.cpp" "Resources/Default.rc" )
		set_target_properties( ${FO_DEV_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_SINGLE_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FONLINE_OUTPUT_PATH} )
	else()
		StatusMessage( "+ ${FO_DEV_NAME} (shared library)" )
		list( APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}" )
		add_library( ${FO_DEV_NAME} SHARED "Source/Applications/SingleApp.cpp" )
		set_target_properties( ${FO_DEV_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${FO_SINGLE_OUTPUT} )
	endif()
	add_dependencies( ${FO_DEV_NAME} CodeGeneration )
	set_target_properties( ${FO_DEV_NAME} PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}" )
	set_target_properties( ${FO_DEV_NAME} PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0" )
	target_link_libraries( ${FO_DEV_NAME} "AppFrontend" "SingleLib" )
	WriteBuildHash( ${FO_DEV_NAME} )
endif()

if( FONLINE_BUILD_EDITOR )
	StatusMessage( "+ ${FO_DEV_NAME}_Editor" )
	list( APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Editor" )
	add_executable( ${FO_DEV_NAME}_Editor WIN32 "Source/Applications/EditorApp.cpp" "Resources/Default.rc" )
	set_target_properties( ${FO_DEV_NAME}_Editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_EDITOR_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FONLINE_OUTPUT_PATH} )
	set_target_properties( ${FO_DEV_NAME}_Editor PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_Editor" )
	set_target_properties( ${FO_DEV_NAME}_Editor PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0" )
	target_link_libraries( ${FO_DEV_NAME}_Editor "AppFrontend" "EditorLib" "MapperLib" "BakerLib" )
	if( NOT FO_SINGLEPLAYER )
		target_link_libraries( ${FO_DEV_NAME}_Editor "ClientLib" "ServerLib" )
	else()
		target_link_libraries( ${FO_DEV_NAME}_Editor "SingleLib" )
	endif()
	if( FO_ANGELSCRIPT_SCRIPTING )
		target_link_libraries( ${FO_DEV_NAME}_Editor "ASCompilerLib" )
	endif()
	WriteBuildHash( ${FO_DEV_NAME}_Editor )
	CopyFbxSdkLib( ${FO_DEV_NAME}_Editor )
endif()

if( FONLINE_BUILD_MAPPER )
	StatusMessage( "+ ${FO_DEV_NAME}_Mapper" )
	list( APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Mapper" )
	add_executable( ${FO_DEV_NAME}_Mapper WIN32 "Source/Applications/MapperApp.cpp" "Resources/Default.rc" )
	set_target_properties( ${FO_DEV_NAME}_Mapper PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_MAPPER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FONLINE_OUTPUT_PATH} )
	set_target_properties( ${FO_DEV_NAME}_Mapper PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_Mapper" )
	set_target_properties( ${FO_DEV_NAME}_Mapper PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0" )
	target_link_libraries( ${FO_DEV_NAME}_Mapper "AppFrontend" "MapperLib" )
	if( NOT FO_SINGLEPLAYER )
		target_link_libraries( ${FO_DEV_NAME}_Mapper "ClientLib" )
	else()
		target_link_libraries( ${FO_DEV_NAME}_Mapper "SingleLib" )
	endif()
	WriteBuildHash( ${FO_DEV_NAME}_Mapper )
endif()

if( FONLINE_BUILD_ASCOMPILER )
	StatusMessage( "+ ${FO_DEV_NAME}_ASCompiler" )
	list( APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_ASCompiler" )
	add_executable( ${FO_DEV_NAME}_ASCompiler "Source/Applications/ASCompilerApp.cpp" )
	add_dependencies( ${FO_DEV_NAME}_ASCompiler CodeGeneration )
	set_target_properties( ${FO_DEV_NAME}_ASCompiler PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_ASCOMPILER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FONLINE_OUTPUT_PATH} )
	set_target_properties( ${FO_DEV_NAME}_ASCompiler PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_ASCompiler" )
	set_target_properties( ${FO_DEV_NAME}_ASCompiler PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0" )
	target_link_libraries( ${FO_DEV_NAME}_ASCompiler "AppHeadless" "ASCompilerLib" )
	WriteBuildHash( ${FO_DEV_NAME}_ASCompiler )
endif()

if( FONLINE_BUILD_BAKER )
	StatusMessage( "+ ${FO_DEV_NAME}_Baker" )
	list( APPEND FO_APPLICATIONS_GROUP "${FO_DEV_NAME}_Baker" )
	add_executable( ${FO_DEV_NAME}_Baker "Source/Applications/BakerApp.cpp" )
	set_target_properties( ${FO_DEV_NAME}_Baker PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_BAKER_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FONLINE_OUTPUT_PATH} )
	set_target_properties( ${FO_DEV_NAME}_Baker PROPERTIES OUTPUT_NAME "${FO_DEV_NAME}_Baker" )
	set_target_properties( ${FO_DEV_NAME}_Baker PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=0" )
	target_link_libraries( ${FO_DEV_NAME}_Baker "AppHeadless" "BakerLib" )
	if( FO_ANGELSCRIPT_SCRIPTING )
		target_link_libraries( ${FO_DEV_NAME}_Baker "ASCompilerLib" )
	endif()
	WriteBuildHash( ${FO_DEV_NAME}_Baker )
	CopyFbxSdkLib( ${FO_DEV_NAME}_Baker )
endif()

if( FONLINE_UNIT_TESTS OR FONLINE_CODE_COVERAGE )
	macro( SetupTestBuild name )
		set( target ${FO_DEV_NAME}_${name} )
		StatusMessage( "+ ${target}" )
		list( APPEND FO_APPLICATIONS_GROUP ${target} )
		add_executable( ${target}
			"Source/Applications/TestingApp.cpp"
			${FO_TESTS_SOURCE}
			"Source/Applications/EditorApp.cpp"
			"Source/Applications/MapperApp.cpp"
			"Source/Applications/BakerApp.cpp" )
		add_dependencies( ${target} CodeGeneration )
		set_target_properties( ${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_TESTS_OUTPUT} VS_DEBUGGER_WORKING_DIRECTORY ${FO_TESTS_OUTPUT} )
		set_target_properties( ${target} PROPERTIES OUTPUT_NAME ${target} )
		set_target_properties( ${target} PROPERTIES COMPILE_DEFINITIONS "FO_TESTING_APP=1" )
		target_link_libraries( ${target} "AppHeadless" "BakerLib" "EditorLib" "MapperLib" "${FO_TESTING_LIBS}" )
		if( NOT FO_SINGLEPLAYER )
			target_sources( ${target} PRIVATE
				"Source/Applications/ServerApp.cpp"
				"Source/Applications/ServerServiceApp.cpp"
				"Source/Applications/ServerDaemonApp.cpp"
				"Source/Applications/ServerHeadlessApp.cpp"
				"Source/Applications/ClientApp.cpp" )
			target_link_libraries( ${target} "ClientLib" "ServerLib" )
		else()
			target_sources( ${target} PRIVATE "Source/Applications/SingleApp.cpp" )
			target_link_libraries( ${target} "SingleLib" )
		endif()
		if( FO_ANGELSCRIPT_SCRIPTING )
			target_link_libraries( ${target} "ASCompilerLib" )
		endif()
		CopyFbxSdkLib( ${target} )
		add_custom_target( Run${name}
			COMMAND ${target}
			COMMENT "Run ${name}" )
		list( APPEND FO_COMMANDS_GROUP "Run${name}" )
	endmacro()

	if( FONLINE_UNIT_TESTS )
		SetupTestBuild( UnitTests )
	endif()
	if( FONLINE_CODE_COVERAGE )
		SetupTestBuild( CodeCoverage )
	endif()
endif()

# Scripts compilation
set( compileASScripts "" )
set( compileMonoScripts "" )
if( FO_NATIVE_SCRIPTING OR FO_ANGELSCRIPT_SCRIPTING OR FO_MONO_SCRIPTING )
	# Compile AngelScript scripts
	if( FO_ANGELSCRIPT_SCRIPTING )
		set( compileASScripts ${FO_DEV_NAME}_ASCompiler )

		list( APPEND compileASScripts -BakeOutput "${FO_BACKED_RESOURCES_OUTPUT}" )
		foreach( entry ${FO_CONTENT} )
			list( APPEND compileASScripts -BakeContentEntries "+${entry}" )
		endforeach()
		foreach( entry ${FO_RESOURCE} )
			list( APPEND compileASScripts -BakeResourceEntries "+${entry}" )
		endforeach()

		add_custom_target( CompileAngelScript
			COMMAND ${compileASScripts}
			COMMENT "Compile AngelScript scripts" )
		list( APPEND FO_COMMANDS_GROUP "CompileAngelScript" )
	endif()

	# Compile Mono scripts
	if( FO_MONO_SCRIPTING )
		set( monoCompileCommands "" )
		foreach( entry ${FO_MONO_ASSEMBLIES} )
			list( APPEND monoCompileCommands -assembly ${entry} )
		endforeach()

		set( compileMonoScripts ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/BuildTools/compile-mono-scripts.py" ${monoCompileCommands} )

		add_custom_target( CompileMonoScripts
			COMMAND ${compileMonoScripts}
			SOURCES ${FO_MONO_SOURCE}
			COMMENT "Compile Mono scripts" )
		list( APPEND FO_COMMANDS_GROUP "CompileMonoScripts" )
	endif()
endif()

# Bakering
set( bakeResources "${FO_DEV_NAME}_Baker" )

list( APPEND bakeResources -BakeOutput "${FO_BACKED_RESOURCES_OUTPUT}" )
foreach( entry ${FO_CONTENT} )
	list( APPEND bakeResources -BakeContentEntries "+${entry}" )
endforeach()
foreach( entry ${FO_RESOURCES} )
	list( APPEND bakeResources -BakeResourceEntries "+${entry}" )
endforeach()

add_custom_target( BakeResources
	COMMAND ${bakeResources} -ForceBakering False
	WORKING_DIRECTORY ${FONLINE_OUTPUT_PATH}
	COMMENT "Bake resources" )
list( APPEND FO_COMMANDS_GROUP "BakeResources" )

add_custom_target( ForceBakeResources
	COMMAND ${bakeResources} -ForceBakering True
	WORKING_DIRECTORY ${FONLINE_OUTPUT_PATH}
	COMMENT "Bake resources" )
list( APPEND FO_COMMANDS_GROUP "ForceBakeResources" )

# Packaging
StatusMessage( "Packages:" )

foreach( package ${FO_PACKAGES} )
	StatusMessage( "+ Package ${package}" )

	add_custom_target( MakePackage-${package}
		WORKING_DIRECTORY ${FONLINE_OUTPUT_PATH}
		COMMENT "Make package ${package}" )
	list( APPEND FO_COMMANDS_GROUP "MakePackage-${package}" )

	foreach( entry ${Package_${package}_Parts} )
		set( packageCommands ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/BuildTools/package.py" )

		list( APPEND packageCommands -buildhash "${FO_BUILD_HASH}" )
		list( APPEND packageCommands -devname "${FO_DEV_NAME}" )
		list( APPEND packageCommands -nicename "${FO_NICE_NAME}" )
		list( APPEND packageCommands -authorname "${FO_AUTHOR_NAME}" )
		list( APPEND packageCommands -gameversion "${FO_GAME_VERSION}" )

		string( REPLACE "," ";" entry ${entry} )
		list( GET entry 0 target )
		list( GET entry 1 platform )
		list( GET entry 2 arch )
		list( GET entry 3 pack )
		list( GET entry 4 customConfig )

		list( APPEND packageCommands -target "${target}" )
		list( APPEND packageCommands -platform "${platform}" )
		list( APPEND packageCommands -arch "${arch}" )
		list( APPEND packageCommands -pack "${pack}" )

		foreach( entry ${FO_RESOURCES} )
			string( REPLACE "," ";" entry ${entry} )
			list( GET entry 0 packName )
			list( APPEND packageCommands -respack ${packName} )
		endforeach()

		if( customConfig )
			set( config ${customConfig} )
		else()
			set( config ${Package_${package}_Config} )
		endif()
		list( APPEND packageCommands -config "${config}" )

		if( FO_ANGELSCRIPT_SCRIPTING )
			list( APPEND packageCommands -angelscript )
		endif()
		if( FO_MONO_SCRIPTING )
			list( APPEND packageCommands -mono )
		endif()

		list( APPEND packageCommands -input ${FONLINE_OUTPUT_PATH} )
		list( APPEND packageCommands -output ${FONLINE_OUTPUT_PATH}/${FO_DEV_NAME}-${package} )
		list( APPEND packageCommands -compresslevel 9 )

		StatusMessage( "  ${target} for ${platform}-${arch} in ${pack} with ${config} config" )
		add_custom_command( TARGET MakePackage-${package} POST_BUILD COMMAND ${packageCommands} )
	endforeach()
endforeach()

# External commands
if( FONLINE_MAKE_EXTERNAL_COMMANDS )
	if( WIN32 )
		set( prolog "@echo off\n\n" )
		set( start "start \"\" " )
		set( breakLine "^" )
		set( scriptExt "bat" )
		set( pause "pause" )
	else()
		set( prolog "#!/bin/bash -e\n\n" )
		set( start "" )
		set( breakLine "\\" )
		set( scriptExt "sh" )
		set( pause "read -p \"Press enter to continue\"" )
	endif()

	cmake_path( RELATIVE_PATH FO_BACKED_RESOURCES_OUTPUT BASE_DIRECTORY ${FONLINE_OUTPUT_PATH} OUTPUT_VARIABLE resourcesDir )

	set( bakeResourceEntries "" )
	list( APPEND bakeResourceEntries "-BakeOutput \"${resourcesDir}\"" )
	foreach( entry ${FO_CONTENT} )
		cmake_path( RELATIVE_PATH entry BASE_DIRECTORY ${FONLINE_OUTPUT_PATH} OUTPUT_VARIABLE entry )
		list( APPEND bakeResourceEntries "-BakeContentEntries \"+${entry}\"" )
	endforeach()
	foreach( entry ${FO_RESOURCES} )
		string( REPLACE "," ";" entry ${entry} )
		list( GET entry 0 packName )
		list( GET entry 1 packEntry )
		cmake_path( RELATIVE_PATH packEntry BASE_DIRECTORY ${FONLINE_OUTPUT_PATH} OUTPUT_VARIABLE packEntry )
		list( APPEND bakeResourceEntries "-BakeResourceEntries \"+${packName},${packEntry}\"" )
	endforeach()

	function( CreateSimpleExternalCommand cmdName program doPause )
		cmake_path( RELATIVE_PATH program BASE_DIRECTORY ${FONLINE_OUTPUT_PATH} OUTPUT_VARIABLE program )

		set( FO_GEN_FILE_CONTENT "${prolog}\"${program}\"" )
		foreach( arg ${ARGN} )
			set( FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT} ${breakLine}\n${arg}" )
		endforeach()

		if( doPause )
			set( FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT}\n\n${pause}" )
		endif()

		configure_file( "BuildTools/blank.cmake" "${FONLINE_OUTPUT_PATH}/${cmdName}.${scriptExt}" FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ )
	endfunction()

	function( CreateBakeredExternalCommand cmdName program )
		cmake_path( RELATIVE_PATH program BASE_DIRECTORY ${FONLINE_OUTPUT_PATH} OUTPUT_VARIABLE program )

		set( bakerProgram "${FONLINE_OUTPUT_PATH}/Binaries/Baker-${FO_BUILD_PLATFORM}/${FO_DEV_NAME}_Baker" )
		cmake_path( RELATIVE_PATH bakerProgram BASE_DIRECTORY ${FONLINE_OUTPUT_PATH} OUTPUT_VARIABLE bakerProgram )

		set( FO_GEN_FILE_CONTENT "${prolog}\"${bakerProgram}\" -ForceBakering False" )
		foreach( arg ${bakeResourceEntries} )
			set( FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT} ${breakLine}\n${arg}" )
		endforeach()
		set( FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT}\n\n" )

		set( FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT}${start}\"${program}\"" )
		foreach( arg ${ARGN} )
			set( FO_GEN_FILE_CONTENT "${FO_GEN_FILE_CONTENT} ${breakLine}\n${arg}" )
		endforeach()

		configure_file( "BuildTools/blank.cmake" "${FONLINE_OUTPUT_PATH}/${cmdName}.${scriptExt}" FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ )
	endfunction()

	if( FONLINE_BUILD_ASCOMPILER )
		CreateSimpleExternalCommand( CompileScripts "${FONLINE_OUTPUT_PATH}/Binaries/ASCompiler-${FO_BUILD_PLATFORM}/${FO_DEV_NAME}_ASCompiler" YES ${bakeResourceEntries} )
	endif()
	if( FONLINE_BUILD_BAKER )
		CreateSimpleExternalCommand( BakeResources "${FONLINE_OUTPUT_PATH}/Binaries/Baker-${FO_BUILD_PLATFORM}/${FO_DEV_NAME}_Baker" YES "-ForceBakering False" ${bakeResourceEntries} )
		CreateSimpleExternalCommand( ForceBakeResources "${FONLINE_OUTPUT_PATH}/Binaries/Baker-${FO_BUILD_PLATFORM}/${FO_DEV_NAME}_Baker" YES "-ForceBakering True" ${bakeResourceEntries} )
	endif()

	if( FONLINE_BUILD_EDITOR )
		CreateSimpleExternalCommand( Editor "${FONLINE_OUTPUT_PATH}/Binaries/Editor-${FO_BUILD_PLATFORM}/${FO_DEV_NAME}_Editor" NO "-ExternalConfig \"Configs/Default.focfg\"" "-EmbeddedResources \"@Disabled\"" ${bakeResourceEntries} )
	endif()
	if( FONLINE_BUILD_MAPPER )
		CreateBakeredExternalCommand( Mapper "${FONLINE_OUTPUT_PATH}/Binaries/Mapper-${FO_BUILD_PLATFORM}/${FO_DEV_NAME}_Mapper" "-ResourcesDir \"${resourcesDir}\"" "-ExternalConfig \"${resourcesDir}/Configs/${FO_MAPPER_CONFIG}.focfg\"" "-EmbeddedResources \"${resourcesDir}/Embedded\"" )
	endif()

	foreach( config ${FO_CONFIGS} )
		if( FONLINE_BUILD_SERVER )
			CreateBakeredExternalCommand( Server-${config} "${FONLINE_OUTPUT_PATH}/Binaries/Server-${FO_BUILD_PLATFORM}/${FO_DEV_NAME}_Server" "-ResourcesDir \"${resourcesDir}\"" "-ExternalConfig \"${resourcesDir}/Configs/${config}.focfg\"" "-EmbeddedResources \"${resourcesDir}/Embedded\"" )
		endif()
		if( FONLINE_BUILD_CLIENT )
			CreateBakeredExternalCommand( Client-${config} "${FONLINE_OUTPUT_PATH}/Binaries/Client-${FO_BUILD_PLATFORM}/${FO_DEV_NAME}_Client" "-ResourcesDir \"${resourcesDir}\"" "-ExternalConfig \"${resourcesDir}/Configs/Client_${config}.focfg\"" "-EmbeddedResources \"${resourcesDir}/Embedded\"" )
		endif()
	endforeach()
endif()

# Setup targets grouping
set_property( GLOBAL PROPERTY USE_FOLDERS ON )
set_property( TARGET ${FO_APPLICATIONS_GROUP} PROPERTY FOLDER "Applications" )
set_property( TARGET ${FO_CORE_LIBS_GROUP} PROPERTY FOLDER "CoreLibs" )
set_property( TARGET ${FO_COMMANDS_GROUP} PROPERTY FOLDER "Commands" )
set_property( TARGET ${FO_COMMON_LIBS} ${FO_BAKER_LIBS} ${FO_SERVER_LIBS} ${FO_CLIENT_LIBS} ${FO_RENDER_LIBS} ${FO_TESTING_LIBS} ${FO_DUMMY_TRAGETS} PROPERTY FOLDER "ThirdParty" )

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
