cmake_minimum_required(VERSION 3.22)

StatusMessage("Start project generation")

# Options
macro(DeclareOption var desc value)
	if(NOT DEFINED ${var}) # Prevent moving to cache
		set(${var} ${value})
	endif()

	option(${var} ${desc} ${value})

	StatusMessage("${var} = ${${var}}")
endmacro()

DeclareOption(FO_MAIN_CONFIG "Main project config" "") # Required
DeclareOption(FO_DEV_NAME "Short name for project" "") # Required
DeclareOption(FO_NICE_NAME "More readable name for project" "") # Required
DeclareOption(FO_ENABLE_3D "Supporting of 3d models" OFF)
DeclareOption(FO_NATIVE_SCRIPTING "Supporting of Native scripting" OFF)
DeclareOption(FO_ANGELSCRIPT_SCRIPTING "Supporting of AngelScript scripting" OFF)
DeclareOption(FO_MONO_SCRIPTING "Supporting of Mono scripting" OFF)
DeclareOption(FO_GEOMETRY "HEXAGONAL or SQUARE gemetry mode" "") # Required
DeclareOption(FO_APP_ICON "Executable file icon" "") # Required
DeclareOption(FO_CXX_STANDARD "C++ standard for project compilation (must be at least 20)" 20)
DeclareOption(FO_RESHARPER_SETTINGS "Path to ReSharper solution settings (empty is default config)" "")
DeclareOption(FO_DISABLE_RPMALLOC "Force disable using of Rpmalloc" OFF)
DeclareOption(FO_DISABLE_JSON "Force disable using of Json" OFF)
DeclareOption(FO_DISABLE_MONGO "Force disable using of Mongo" OFF)
DeclareOption(FO_DISABLE_UNQLITE "Force disable using of Unqlite" OFF)
DeclareOption(FO_DISABLE_ASIO "Force disable using of Asio" OFF)
DeclareOption(FO_DISABLE_WEB_SOCKETS "Force disable using of WebSockets" OFF)
DeclareOption(FO_DISABLE_NAMESPACE "Force disable using of FOnline namespace" OFF)

DeclareOption(FO_VERBOSE_BUILD "Verbose build mode" OFF)
DeclareOption(FO_BUILD_CLIENT "Build Client binaries" OFF)
DeclareOption(FO_BUILD_SERVER "Build Server binaries" OFF)
DeclareOption(FO_BUILD_MAPPER "Build Mapper binaries" OFF)
DeclareOption(FO_BUILD_EDITOR "Build Editor binaries" OFF)
DeclareOption(FO_BUILD_ASCOMPILER "Build AngelScript compiler" OFF)
DeclareOption(FO_BUILD_BAKER "Build Baker binaries" OFF)
DeclareOption(FO_UNIT_TESTS "Build only binaries for Unit Testing" OFF)
DeclareOption(FO_CODE_COVERAGE "Build only binaries for Code Coverage reports" OFF)
DeclareOption(FO_OUTPUT_PATH "Common output path" "") # Required

# Quiet all non-error messages instead ourself
if(FO_VERBOSE_BUILD)
	StatusMessage("Verbose build mode")
	set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "Forced by FOnline" FORCE)
else()
	set(CMAKE_VERBOSE_MAKEFILE OFF CACHE BOOL "Forced by FOnline" FORCE)
endif()

# Global options
set(CMAKE_POLICY_VERSION_MINIMUM 3.22)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Forced by FOnline" FORCE) # Generate compile_commands.json
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Forced by FOnline" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Forced by FOnline" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "Forced by FOnline" FORCE)

# Check options
set(requiredOptions "FO_MAIN_CONFIG" "FO_DEV_NAME" "FO_NICE_NAME" "FO_GEOMETRY" "FO_APP_ICON" "FO_OUTPUT_PATH")

foreach(opt ${requiredOptions})
	if("${${opt}}" STREQUAL "")
		AbortMessage("${opt} not specified")
	endif()
endforeach()

# Evaluate build hash
set(FO_GIT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
execute_process(COMMAND git rev-parse HEAD
	WORKING_DIRECTORY ${FO_GIT_ROOT}
	RESULT_VARIABLE FO_GIT_HASH_RESULT
	OUTPUT_VARIABLE FO_GIT_HASH
	OUTPUT_STRIP_TRAILING_WHITESPACE)

if(FO_GIT_HASH_RESULT STREQUAL "0")
	set(FO_BUILD_HASH ${FO_GIT_HASH})
else()
	string(RANDOM LENGTH 40 ALPHABET "0123456789abcdef" randomHash)
	set(FO_BUILD_HASH ${randomHash}-random)
endif()

StatusMessage("Build hash: ${FO_BUILD_HASH}")

macro(WriteBuildHash target)
	if(NOT FO_BUILD_LIBRARY)
		get_target_property(dir ${target} RUNTIME_OUTPUT_DIRECTORY)
	else()
		get_target_property(dir ${target} LIBRARY_OUTPUT_DIRECTORY)
	endif()

	add_custom_command(TARGET ${target} PRE_BUILD COMMAND ${CMAKE_COMMAND} -E remove -f "${dir}/${target}.build-hash")
	add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E echo_append ${FO_BUILD_HASH} > "${dir}/${target}.build-hash")
endmacro()

# Some info about build
StatusMessage("Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
StatusMessage("Generator: ${CMAKE_GENERATOR}")
StatusMessage("Operating system: ${CMAKE_SYSTEM_NAME}")

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	set(FO_PROCESSOR_ARCHITECTURE "x64")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "i386" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "i486" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "i586" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "i686")
	set(FO_PROCESSOR_ARCHITECTURE "x86")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7-a")
	set(FO_PROCESSOR_ARCHITECTURE "arm")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
	set(FO_PROCESSOR_ARCHITECTURE "arm64")
else()
	set(FO_PROCESSOR_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL FO_PROCESSOR_ARCHITECTURE)
	StatusMessage("Processor: ${FO_PROCESSOR_ARCHITECTURE}")
else()
	StatusMessage("Processor: ${FO_PROCESSOR_ARCHITECTURE} (${CMAKE_SYSTEM_PROCESSOR})")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	StatusMessage("CPU architecture: 64-bit")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	StatusMessage("CPU architecture: 32-bit")
else()
	AbortMessage("Invalid pointer size, nor 8 or 4 bytes")
endif()

# Build configuration
get_cmake_property(FO_MULTICONFIG GENERATOR_IS_MULTI_CONFIG)

macro(AddConfiguration name parent)
	string(TOUPPER ${name} nameUpper)
	string(TOUPPER ${parent} parentUpper)

	list(APPEND FO_CONFIGURATION_TYPES ${name})

	if(FO_MULTICONFIG)
		set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES};${name}" CACHE STRING "Forced by FOnline" FORCE)
	endif()

	CopyConfigurationType(${parent} ${name})
endmacro()

if(FO_MULTICONFIG)
	set(CMAKE_CONFIGURATION_TYPES ${FO_CONFIGURATION_TYPES} CACHE STRING "Forced by FOnline" FORCE)
endif()

AddConfiguration(Profiling_Total RelWithDebInfo)
AddConfiguration(Profiling_OnDemand RelWithDebInfo)
AddConfiguration(Debug_Profiling_Total Debug)
AddConfiguration(Debug_Profiling_OnDemand Debug)
AddConfiguration(Release_Ext Release)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	AddConfiguration(Release_Debugging RelWithDebInfo)
	AddConfiguration(San_Address RelWithDebInfo)
	set(expr_SanitizerConfigs $<CONFIG:San_Address>)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
	AddConfiguration(San_Address RelWithDebInfo)
	AddConfiguration(San_Memory RelWithDebInfo)
	AddConfiguration(San_MemoryWithOrigins RelWithDebInfo)
	AddConfiguration(San_Undefined RelWithDebInfo)
	AddConfiguration(San_Thread RelWithDebInfo)
	AddConfiguration(San_DataFlow RelWithDebInfo)
	AddConfiguration(San_Address_Undefined RelWithDebInfo)
	set(expr_SanitizerConfigs $<CONFIG:San_Address,San_Memory,San_MemoryWithOrigins,San_Undefined,San_Thread,San_DataFlow,San_Address_Undefined>)
else()
	set(expr_SanitizerConfigs 0)
endif()

if(FO_MULTICONFIG)
	string(REPLACE ";" " " configs "${CMAKE_CONFIGURATION_TYPES}")
	StatusMessage("Configurations: ${configs}")
else()
	StatusMessage("Configuration: ${CMAKE_BUILD_TYPE}")

	list(FIND FO_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE} configurationIndex)

	if(configurationIndex EQUAL -1)
		AbortMessage("Invalid requested configuration type")
	endif()
endif()

# Basic compiler setup
set(CMAKE_CXX_STANDARD ${FO_CXX_STANDARD})

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)
add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)
add_compile_definitions($<$<CONFIG:Debug>:FO_DEBUG=1>)
add_compile_definitions($<$<NOT:$<CONFIG:Debug>>:NDEBUG>)
add_compile_definitions($<$<NOT:$<CONFIG:Debug>>:FO_DEBUG=0>)

set(expr_DebugBuild $<OR:$<CONFIG:Debug>,$<CONFIG:Debug_Profiling_Total>,$<CONFIG:Debug_Profiling_OnDemand>>)
set(expr_FullOptimization $<CONFIG:Release_Ext>)
set(expr_DebugInfo $<NOT:$<CONFIG:MinSizeRel>>)
set(expr_PrefixConfig $<NOT:$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>>)
set(expr_TracyEnabled $<OR:$<CONFIG:Profiling_Total>,$<CONFIG:Profiling_OnDemand>,$<CONFIG:Debug_Profiling_Total>,$<CONFIG:Debug_Profiling_OnDemand>>)
set(expr_TracyOnDemand $<OR:$<CONFIG:Profiling_OnDemand>,$<CONFIG:Debug_Profiling_OnDemand>>)
set(expr_RpmallocEnabled $<NOT:${expr_SanitizerConfigs}>)
set(expr_StandaloneRpmallocEnabled $<AND:${expr_RpmallocEnabled},$<NOT:${expr_TracyEnabled}>>)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	add_compile_options_C_CXX($<$<CONFIG:San_Address>:/fsanitize=address>)
	add_compile_options_C_CXX($<$<CONFIG:Release_Debugging>:/dynamicdeopt>)
	add_link_options($<$<CONFIG:Release_Debugging>:/DYNAMICDEOPT>)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
	add_compile_definitions($<$<CONFIG:San_Address>:LLVM_USE_SANITIZER=Address>)
	add_compile_definitions($<$<CONFIG:San_Memory>:LLVM_USE_SANITIZER=Memory>)
	add_compile_definitions($<$<CONFIG:San_MemoryWithOrigins>:LLVM_USE_SANITIZER=MemoryWithOrigins>)
	add_compile_definitions($<$<CONFIG:San_Undefined>:LLVM_USE_SANITIZER=Undefined>)
	add_compile_definitions($<$<CONFIG:San_Thread>:LLVM_USE_SANITIZER=Thread>)
	add_compile_definitions($<$<CONFIG:San_DataFlow>:LLVM_USE_SANITIZER=DataFlow>)
	add_compile_definitions($<$<CONFIG:San_Address_Undefined>:LLVM_USE_SANITIZER=Address$<SEMICOLON>Undefined>)
endif()

# Engine settings
add_compile_definitions("FO_MAIN_CONFIG=\"${FO_MAIN_CONFIG}\"")
add_compile_definitions(FO_ENABLE_3D=$<BOOL:${FO_ENABLE_3D}>)
add_compile_definitions(FO_NATIVE_SCRIPTING=$<BOOL:${FO_NATIVE_SCRIPTING}>)
add_compile_definitions(FO_ANGELSCRIPT_SCRIPTING=$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}>)
add_compile_definitions(FO_MONO_SCRIPTING=$<BOOL:${FO_MONO_SCRIPTING}>)
add_compile_definitions(FO_GEOMETRY=$<IF:$<STREQUAL:${FO_GEOMETRY},HEXAGONAL>,1,$<IF:$<STREQUAL:${FO_GEOMETRY},SQUARE>,2,0>>)
add_compile_definitions(FO_NO_MANUAL_STACK_TRACE=$<CONFIG:Release_Ext>)
add_compile_definitions(FO_NO_EXTRA_ASSERTS=0) # Todo: FO_NO_EXTRA_ASSERTS=$<CONFIG:Release_Ext> for first need separate asserts from valid error
add_compile_definitions(FO_NO_TEXTURE_LOOKUP=$<CONFIG:Release_Ext>)
add_compile_definitions(FO_DIRECT_SPRITES_DRAW=$<CONFIG:Release_Ext>)
add_compile_definitions(FO_RENDER_32BIT_INDEX=0)
add_compile_definitions(FO_USE_NAMESPACE=$<NOT:$<BOOL:${FO_DISABLE_NAMESPACE}>>)

# Basic includes
include_directories("${FO_ENGINE_ROOT}/Source/Essentials")
include_directories("${FO_ENGINE_ROOT}/Source/Common")
include_directories("${FO_ENGINE_ROOT}/Source/Server")
include_directories("${FO_ENGINE_ROOT}/Source/Client")
include_directories("${FO_ENGINE_ROOT}/Source/Tools")
include_directories("${FO_ENGINE_ROOT}/Source/Scripting")
include_directories("${FO_ENGINE_ROOT}/Source/Frontend")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource")

# Headless configuration (without video/audio/input)
if(FO_BUILD_CLIENT OR FO_BUILD_SERVER OR FO_BUILD_MAPPER OR FO_BUILD_EDITOR)
	set(FO_HEADLESS_ONLY OFF)
else()
	set(FO_HEADLESS_ONLY ON)
endif()

# Core libs to build
if(FO_BUILD_CLIENT OR FO_BUILD_SERVER OR FO_BUILD_MAPPER OR FO_BUILD_EDITOR OR FO_BUILD_ASCOMPILER OR FO_BUILD_BAKER OR FO_UNIT_TESTS OR FO_CODE_COVERAGE)
	set(FO_BUILD_COMMON_LIB ON)
endif()

if(FO_BUILD_CLIENT OR FO_BUILD_MAPPER OR FO_BUILD_SERVER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE)
	set(FO_BUILD_CLIENT_LIB ON)
endif()

if(FO_BUILD_SERVER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE)
	set(FO_BUILD_SERVER_LIB ON)
endif()

if(FO_BUILD_MAPPER OR FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE)
	set(FO_BUILD_MAPPER_LIB ON)
endif()

if(FO_BUILD_SERVER OR FO_BUILD_MAPPER OR FO_BUILD_EDITOR OR FO_BUILD_BAKER OR FO_UNIT_TESTS OR FO_CODE_COVERAGE)
	set(FO_BUILD_BAKER_LIB ON)
endif()

if(FO_BUILD_ASCOMPILER OR FO_BUILD_BAKER_LIB)
	set(FO_BUILD_ASCOMPILER_LIB ON)
endif()

if(FO_BUILD_EDITOR OR FO_UNIT_TESTS OR FO_CODE_COVERAGE)
	set(FO_BUILD_EDITOR_LIB ON)
endif()

# Per OS configurations
if(WIN32)
	set(FO_WINDOWS 1)
	set(FO_HAVE_OPENGL 1)
	set(FO_HAVE_DIRECT_3D 1)

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(FO_BUILD_PLATFORM "Windows-win64")
		set(FO_MONO_OS "windows")
		set(FO_MONO_ARCH "x64")
	else()
		set(FO_BUILD_PLATFORM "Windows-win32")
		set(FO_MONO_OS "windows")
		set(FO_MONO_ARCH "x86")
	endif()

	set(CMAKE_SYSTEM_VERSION 6.1)
	add_compile_definitions(_WIN32_WINNT=0x0601)
	add_compile_definitions(UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE _WINSOCK_DEPRECATED_NO_WARNINGS)

	# Todo: debug /RTCc /sdl _ALLOW_RTCc_IN_STL release /GS-
	add_compile_options_C_CXX(/permissive-)
	add_compile_options_C_CXX(/Zc:__cplusplus)
	add_compile_options_C_CXX($<$<CONFIG:Debug>:/RTC1>)
	add_compile_options_C_CXX($<$<CONFIG:Debug>:/GS>)
	add_compile_options_C_CXX($<$<CONFIG:Debug,RelWithDebInfo>:/JMC>)
	add_compile_options_C_CXX($<$<NOT:$<CONFIG:Debug>>:/sdl->)
	add_compile_options_C_CXX(/W4)
	add_compile_options_C_CXX(/MP)
	add_compile_options_C_CXX(/EHsc)
	add_compile_options_C_CXX(/utf-8)
	add_compile_options_C_CXX(/volatile:iso)
	add_compile_options_C_CXX(/GR)
	add_compile_options_C_CXX(/bigobj)
	add_compile_options_C_CXX(/fp:fast)
	add_compile_options_C_CXX($<${expr_FullOptimization}:/GL>) # Todo: GL/LTCG leads to crashes
	add_compile_options_C_CXX($<${expr_DebugInfo}:/Zi>)

	add_link_options(/INCREMENTAL:NO)
	add_link_options(/OPT:REF)
	add_link_options(/OPT:NOICF)
	add_link_options($<${expr_FullOptimization}:/LTCG>)
	add_link_options($<IF:${expr_DebugInfo},/DEBUG:FULL,/DEBUG:NONE>)

	if(FO_BUILD_CLIENT)
		add_compile_options_C_CXX($<$<CONFIG:Debug>:/MTd>)
		add_compile_options_C_CXX($<$<NOT:$<CONFIG:Debug>>:/MT>)
	else()
		add_compile_options_C_CXX($<$<CONFIG:Debug>:/MDd>)
		add_compile_options_C_CXX($<$<NOT:$<CONFIG:Debug>>:/MD>)
	endif()

	list(APPEND FO_COMMON_SYSTEM_LIBS "user32" "ws2_32" "version" "winmm" "imm32" "dbghelp" "psapi" "xinput")

	if(NOT FO_HEADLESS_ONLY)
		set(FO_USE_GLEW ON)
		list(APPEND FO_RENDER_SYSTEM_LIBS "d3d9" "gdi32" "dxgi" "windowscodecs" "dxguid")
		list(APPEND FO_RENDER_SYSTEM_LIBS "glu32" "d3d11" "d3dcompiler" "opengl32")
	endif()

elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
	set(FO_LINUX 1)
	set(FO_HAVE_OPENGL 1)
	set(FO_BUILD_PLATFORM "Linux-${FO_PROCESSOR_ARCHITECTURE}")
	set(FO_MONO_OS "linux")
	set(FO_MONO_ARCH ${FO_PROCESSOR_ARCHITECTURE})

	if(NOT FO_HEADLESS_ONLY)
		find_package(X11 REQUIRED)
		find_package(OpenGL REQUIRED)
		set(FO_USE_GLEW ON)
		list(APPEND FO_RENDER_SYSTEM_LIBS "GL")
	endif()

	add_compile_options_C_CXX($<${expr_DebugInfo}:-g>)
	add_compile_options_C_CXX($<${expr_FullOptimization}:-O3>)
	add_compile_options_C_CXX($<${expr_FullOptimization}:-flto>)
	add_link_options($<${expr_FullOptimization}:-flto>)
	add_link_options(-rdynamic)

	if(NOT FO_BUILD_BAKER)
		add_link_options(-no-pie)
	else()
		add_compile_options_C_CXX(-fPIC)
	endif()

	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		# Todo: using of libc++ leads to crash on any exception when trying to call free() with invalid pointer
		# Bug somehow connected with rpmalloc new operators overloading
		# add_compile_options_C_CXX(-stdlib=libc++)
		# add_link_options(-stdlib=libc++)
	endif()

elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
	set(FO_MAC 1)
	set(FO_HAVE_OPENGL 1)
	set(FO_HAVE_METAL 1)
	set(FO_BUILD_PLATFORM "macOS-${FO_PROCESSOR_ARCHITECTURE}")
	set(FO_MONO_OS "osx")
	set(FO_MONO_ARCH ${FO_PROCESSOR_ARCHITECTURE})

	if(NOT FO_HEADLESS_ONLY)
		find_package(OpenGL REQUIRED)
		set(FO_USE_GLEW ON)
		list(APPEND FO_RENDER_SYSTEM_LIBS ${OPENGL_LIBRARIES})
	endif()

	add_compile_options_C_CXX($<${expr_DebugInfo}:-g>)
	add_compile_options_C_CXX($<${expr_FullOptimization}:-O3>)
	add_compile_options_C_CXX($<${expr_FullOptimization}:-flto>)
	add_link_options($<${expr_FullOptimization}:-flto>)
	add_link_options(-rdynamic)

elseif(CMAKE_SYSTEM_NAME MATCHES "iOS")
	StatusMessage("Deployment target: ${DEPLOYMENT_TARGET}")

	set(FO_IOS 1)
	set(FO_HAVE_OPENGL 1)
	set(FO_OPENGL_ES 1)
	set(FO_HAVE_METAL 1)

	if(PLATFORM STREQUAL "OS64")
		StatusMessage("Platform: Device")
		set(FO_BUILD_PLATFORM "iOS-arm64")
		set(FO_MONO_OS "ios")
		set(FO_MONO_ARCH "arm64")
	elseif(PLATFORM STREQUAL "SIMULATOR64")
		StatusMessage("Platform: Simulator")
		set(FO_BUILD_PLATFORM "iOS-simulator")
		set(FO_MONO_OS "iossimulator")
		set(FO_MONO_ARCH "arm64")
	else()
		AbortMessage("Invalid iOS target platform ${PLATFORM}")
	endif()

	if(NOT FO_HEADLESS_ONLY)
		find_library(OPENGLES OpenGLES)
		find_library(METAL Metal)
		find_library(COREGRAPGHICS CoreGraphics)
		find_library(QUARTZCORE QuartzCore)
		find_library(UIKIT UIKit)
		find_library(AVFOUNDATION AVFoundation)
		find_library(GAMECONTROLLER GameController)
		find_library(COREMOTION CoreMotion)
		list(APPEND FO_RENDER_SYSTEM_LIBS ${OPENGLES} ${METAL} ${COREGRAPGHICS} ${QUARTZCORE} ${UIKIT} ${AVFOUNDATION} ${GAMECONTROLLER} ${COREMOTION})
		unset(OPENGLES)
		unset(METAL)
		unset(COREGRAPGHICS)
		unset(QUARTZCORE)
		unset(UIKIT)
		unset(AVFOUNDATION)
		unset(GAMECONTROLLER)
		unset(COREMOTION)
	endif()

	add_compile_options_C_CXX($<${expr_DebugInfo}:-g>)
	add_compile_options_C_CXX($<${expr_FullOptimization}:-O3>)
	add_compile_options_C_CXX($<${expr_FullOptimization}:-flto>)
	add_link_options($<${expr_FullOptimization}:-flto>)
	list(APPEND FO_COMMON_SYSTEM_LIBS "iconv") # Todo: ios iconv workaround for SDL, remove in future updates

elseif(CMAKE_SYSTEM_NAME MATCHES "Android")
	set(FO_ANDROID 1)
	set(FO_HAVE_OPENGL 1)
	set(FO_OPENGL_ES 1)
	set(FO_BUILD_PLATFORM "Android-${ANDROID_ABI}")
	set(FO_BUILD_LIBRARY ON)
	set(FO_MONO_OS "android")

	if(${ANDROID_ABI} STREQUAL "armeabi-v7a")
		set(FO_MONO_ARCH "arm")
	elseif(${ANDROID_ABI} STREQUAL "arm64-v8a")
		set(FO_MONO_ARCH "arm64")
	elseif(${ANDROID_ABI} STREQUAL "x86")
		set(FO_MONO_ARCH "x86")
	endif()

	if(NOT FO_HEADLESS_ONLY)
		list(APPEND FO_RENDER_SYSTEM_LIBS "GLESv3")
	endif()

	list(APPEND FO_COMMON_SYSTEM_LIBS "android" "log" "atomic")
	add_compile_options_C_CXX($<${expr_DebugInfo}:-g>)
	add_compile_options_C_CXX($<${expr_FullOptimization}:-O3>)
	add_compile_options_C_CXX($<${expr_FullOptimization}:-flto>)
	add_link_options($<${expr_FullOptimization}:-flto>)
	add_link_options(-pie)

elseif(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
	set(FO_WEB 1)
	set(FO_HAVE_OPENGL 1)
	set(FO_OPENGL_ES 1)
	set(FO_BUILD_PLATFORM "Web-wasm")
	set(FO_MONO_OS "browser")
	set(FO_MONO_ARCH "wasm")
	set(CMAKE_EXECUTABLE_SUFFIX ".js")
	add_compile_options_C_CXX($<${expr_DebugInfo}:-g3>)
	add_link_options($<${expr_DebugInfo}:-g3>)
	add_compile_options_C_CXX($<${expr_FullOptimization}:-O3>)
	add_link_options($<${expr_FullOptimization}:-O3>)
	add_compile_options_C_CXX($<${expr_FullOptimization}:-flto>)
	add_link_options($<${expr_FullOptimization}:-flto>)
	add_compile_options_C_CXX(--no-heap-copy)
	add_link_options($<IF:$<CONFIG:Debug>,-O0,-O2>)
	add_link_options(-sASSERTIONS=$<IF:$<CONFIG:Debug>,2,0>)
	add_link_options(-sSTACK_OVERFLOW_CHECK=$<IF:$<CONFIG:Debug>,2,0>)
	add_link_options(-sINITIAL_MEMORY=268435456) # 256 Mb
	add_link_options(-sABORT_ON_WASM_EXCEPTIONS=1)
	add_link_options(-sERROR_ON_UNDEFINED_SYMBOLS=1)
	add_link_options(-sALLOW_MEMORY_GROWTH=1)
	add_link_options(-sMIN_WEBGL_VERSION=2)
	add_link_options(-sMAX_WEBGL_VERSION=2)
	add_link_options(-sUSE_SDL=0)
	add_link_options(-sFORCE_FILESYSTEM=1)
	add_link_options(-sDYNAMIC_EXECUTION=0)
	add_link_options(-sEXIT_RUNTIME=0)
	add_link_options(-sEXPORTED_RUNTIME_METHODS=['FS_createPath','FS_createDataFile','intArrayFromString','UTF8ToString','addRunDependency','removeRunDependency','autoResumeAudioContext','dynCall'])
	add_link_options(-sDISABLE_EXCEPTION_CATCHING=0)
	add_compile_options_C_CXX(-sDISABLE_EXCEPTION_CATCHING=0)
	add_link_options(-sWASM_BIGINT=1)
	add_link_options(-sMAIN_MODULE=2)
	add_compile_options_C_CXX(-sMAIN_MODULE=2)

	add_link_options(-sSTRICT=0)
	add_compile_options_C_CXX(-sSTRICT=0)
	add_link_options(-sSTRICT_JS=1)
	add_link_options(-sIGNORE_MISSING_MAIN=0)
	add_link_options(-sAUTO_JS_LIBRARIES=0)
	add_link_options(-sAUTO_NATIVE_LIBRARIES=0)
	add_link_options(-sDEFAULT_TO_CXX=0)
	add_link_options(-sUSE_GLFW=0)
	add_link_options(-sALLOW_UNIMPLEMENTED_SYSCALLS=0)

	add_link_options(-lhtml5)
	add_link_options(-lGL)
	add_link_options(-legl.js)
	add_link_options(-lhtml5_webgl.js)
	add_link_options(-lidbfs.js)

else()
	AbortMessage("Unknown OS")
endif()

add_compile_definitions(FO_WINDOWS=${FO_WINDOWS} FO_LINUX=${FO_LINUX} FO_MAC=${FO_MAC} FO_ANDROID=${FO_ANDROID} FO_IOS=${FO_IOS} FO_WEB=${FO_WEB})
add_compile_definitions(FO_HAVE_OPENGL=${FO_HAVE_OPENGL} FO_OPENGL_ES=${FO_OPENGL_ES} FO_HAVE_DIRECT_3D=${FO_HAVE_DIRECT_3D} FO_HAVE_METAL=${FO_HAVE_METAL} FO_HAVE_VULKAN=${FO_HAVE_VULKAN})

if(FO_CODE_COVERAGE)
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		add_compile_options_C_CXX(-O0)
		add_compile_options_C_CXX(-fprofile)
		add_compile_options_C_CXX(-instr-generate)
		add_compile_options_C_CXX(-fcoverage-mapping)
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		add_compile_options_C_CXX(-O0)
		add_compile_options_C_CXX(--coverage)
		add_link_options(--coverage)
	endif()
endif()

# Output path
StatusMessage("Output path: ${FO_OUTPUT_PATH}")
set(FO_CLIENT_OUTPUT "${FO_OUTPUT_PATH}/Binaries/Client-${FO_BUILD_PLATFORM}$<${expr_PrefixConfig}:-$<CONFIG>>")
set(FO_SERVER_OUTPUT "${FO_OUTPUT_PATH}/Binaries/Server-${FO_BUILD_PLATFORM}$<${expr_PrefixConfig}:-$<CONFIG>>")
set(FO_EDITOR_OUTPUT "${FO_OUTPUT_PATH}/Binaries/Editor-${FO_BUILD_PLATFORM}$<${expr_PrefixConfig}:-$<CONFIG>>")
set(FO_MAPPER_OUTPUT "${FO_OUTPUT_PATH}/Binaries/Mapper-${FO_BUILD_PLATFORM}$<${expr_PrefixConfig}:-$<CONFIG>>")
set(FO_ASCOMPILER_OUTPUT "${FO_OUTPUT_PATH}/Binaries/ASCompiler-${FO_BUILD_PLATFORM}$<${expr_PrefixConfig}:-$<CONFIG>>")
set(FO_BAKER_OUTPUT "${FO_OUTPUT_PATH}/Binaries/Baker-${FO_BUILD_PLATFORM}$<${expr_PrefixConfig}:-$<CONFIG>>")
set(FO_TESTS_OUTPUT "${FO_OUTPUT_PATH}/Binaries/Tests-${FO_BUILD_PLATFORM}$<${expr_PrefixConfig}:-$<CONFIG>>")

# Contributions
macro(ResolveContributedFiles)
	set(result "")

	foreach(file ${ARGN})
		file(GLOB globFiles LIST_DIRECTORIES TRUE "${FO_CONTRIBUTION_DIR}/${file}")

		foreach(globFile ${globFiles})
			get_filename_component(globFile ${globFile} ABSOLUTE)
			list(APPEND result ${globFile})
		endforeach()
	endforeach()
endmacro()

macro(AddEngineSource target)
	ResolveContributedFiles(${ARGN})

	foreach(resultEntry ${result})
		StatusMessage("Add engine source (${target}): ${resultEntry}")
		list(APPEND FO_${target}_SOURCE ${resultEntry})
		list(APPEND FO_SOURCE_META_FILES ${resultEntry})

		string(REGEX MATCH "\\.h$" isHeader "${resultEntry}")

		if(${target} STREQUAL "COMMON" AND isHeader)
			list(APPEND FO_ADDED_COMMON_HEADERS ${resultEntry})
		endif()
	endforeach()
endmacro()

macro(AddNativeIncludeDir)
	if(FO_NATIVE_SCRIPTING)
		foreach(dir ${ARGN})
			StatusMessage("Add native include dir: ${FO_CONTRIBUTION_DIR}/${dir}")
			include_directories("${FO_CONTRIBUTION_DIR}/${dir}")
		endforeach()
	endif()
endmacro()

macro(AddNativeSource)
	if(FO_NATIVE_SCRIPTING)
		# ResolveContributedFiles( ${ARGN} )
		# foreach( resultEntry ${result} )
		# StatusMessage( "Add engine source: ${resultEntry}" )
		# list( APPEND FO_${target}_SOURCE ${resultEntry} )
		# endforeach()
	endif()
endmacro()

macro(AddMonoAssembly assembly)
	if(FO_MONO_SCRIPTING)
		StatusMessage("Add mono assembly ${assembly}")
		list(APPEND FO_MONO_ASSEMBLIES ${assembly})
		set(MonoAssembly_${assembly}_CommonRefs "")
		set(MonoAssembly_${assembly}_ServerRefs "")
		set(MonoAssembly_${assembly}_ClientRefs "")
		set(MonoAssembly_${assembly}_MapperRefs "")
		set(MonoAssembly_${assembly}_CommonSource "")
		set(MonoAssembly_${assembly}_ServerSource "")
		set(MonoAssembly_${assembly}_ClientSource "")
		set(MonoAssembly_${assembly}_MapperSource "")
	endif()
endmacro()

macro(AddMonoReference assembly target)
	if(FO_MONO_SCRIPTING)
		foreach(arg ${ARGN})
			StatusMessage("Add mono assembly ${target}/${assembly} redefence to ${arg}")
			list(APPEND MonoAssembly_${assembly}_${target}Refs ${arg})
		endforeach()
	endif()
endmacro()

macro(AddMonoSource assembly target)
	if(FO_MONO_SCRIPTING)
		ResolveContributedFiles(${ARGN})

		foreach(resultEntry ${result})
			StatusMessage("Add mono source for assembly ${target}/${assembly} at ${resultEntry}")
			list(APPEND MonoAssembly_${assembly}_${target}Source ${resultEntry})
		endforeach()
	endif()
endmacro()

macro(CreatePackage package config)
	list(APPEND FO_PACKAGES ${package})
	set(Package_${package}_Config ${config})
	set(Package_${package}_Parts "")
endmacro()

macro(AddToPackage package binary platform arch packType)
	list(APPEND Package_${package}_Parts "${binary},${platform},${arch},${packType},${ARGN}")
endmacro()

# Core contribution
set(FO_CONTRIBUTION_DIR ${FO_ENGINE_ROOT})

AddNativeIncludeDir("Source/Scripting/Native")

AddMonoAssembly("FOnline")
AddMonoSource("FOnline" "Common" "Source/Scripting/Mono/*.cs")

set(FO_CONTRIBUTION_DIR ${CMAKE_CURRENT_SOURCE_DIR})
