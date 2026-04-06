cmake_minimum_required(VERSION 3.22)

StatusMessage("Start project generation")

# Options
DeclareRequiredValueOptions(
	FO_MAIN_CONFIG "Main project config"
	FO_DEV_NAME "Short name for project"
	FO_NICE_NAME "More readable name for project"
	FO_GEOMETRY "HEXAGONAL or SQUARE geometry mode"
	FO_APP_ICON "Executable file icon"
	FO_OUTPUT_PATH "Common output path")

DeclareValueOptions(
	FO_CXX_STANDARD "C++ standard for project compilation (must be at least 20)" 20
	FO_EMBEDDED_DATA_CAPACITY "Capacity for embedded data in binaries" 200000)

DeclareValueOption(FO_RESHARPER_SETTINGS "Path to ReSharper solution settings (empty is default config)" "")

DeclareBoolOptions(
	FO_ENABLE_3D "Supporting of 3d models" OFF
	FO_NATIVE_SCRIPTING "Supporting of Native scripting" OFF
	FO_ANGELSCRIPT_SCRIPTING "Supporting of AngelScript scripting" OFF
	FO_MONO_SCRIPTING "Supporting of Mono scripting" OFF
	FO_DISABLE_RPMALLOC "Force disable using of Rpmalloc" OFF
	FO_DISABLE_MONGO "Force disable using of Mongo" OFF
	FO_DISABLE_UNQLITE "Force disable using of Unqlite" OFF
	FO_DISABLE_ASIO "Force disable using of Asio" OFF
	FO_DISABLE_WEB_SOCKETS "Force disable using of WebSockets" OFF
	FO_DISABLE_NAMESPACE "Force disable using of FOnline namespace" OFF
	FO_VERBOSE_BUILD "Verbose build mode" OFF
	FO_BUILD_CLIENT "Build Client binaries" OFF
	FO_BUILD_SERVER "Build Server binaries" OFF
	FO_BUILD_MAPPER "Build Mapper binaries" OFF
	FO_BUILD_EDITOR "Build Editor binaries" OFF
	FO_BUILD_ASCOMPILER "Build AngelScript compiler" OFF
	FO_BUILD_BAKER "Build Baker binaries" OFF
	FO_UNIT_TESTS "Build only binaries for Unit Testing" OFF
	FO_CODE_COVERAGE "Build only binaries for Code Coverage reports" OFF)

# Quiet all non-error messages instead ourself
if(FO_VERBOSE_BUILD)
	StatusMessage("Verbose build mode")
	SetBoolCacheValues(CMAKE_VERBOSE_MAKEFILE ON)
else()
	SetBoolCacheValues(CMAKE_VERBOSE_MAKEFILE OFF)
endif()

# Global options
SetValue(CMAKE_POLICY_VERSION_MINIMUM 3.22)
SetBoolCacheValues(
	CMAKE_EXPORT_COMPILE_COMMANDS ON
	BUILD_SHARED_LIBS OFF
	BUILD_TESTING OFF
	SKIP_INSTALL_ALL ON) # Generate compile_commands.json
SetGlobalProperty(USE_FOLDERS ON)

# Check options
RequireNonEmptyVariables(
	FO_MAIN_CONFIG
	FO_DEV_NAME
	FO_NICE_NAME
	FO_GEOMETRY
	FO_APP_ICON
	FO_OUTPUT_PATH)

# Evaluate build hash
SetValue(FO_GIT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
ExecuteProcess(COMMAND git rev-parse HEAD
	WORKING_DIRECTORY ${FO_GIT_ROOT}
	RESULT_VARIABLE FO_GIT_HASH_RESULT
	OUTPUT_VARIABLE FO_GIT_HASH
	OUTPUT_STRIP_TRAILING_WHITESPACE)

if(FO_GIT_HASH_RESULT STREQUAL "0")
	SetValue(FO_BUILD_HASH ${FO_GIT_HASH})
else()
	StringRandom(randomHash LENGTH 40 ALPHABET "0123456789abcdef")
	SetValue(FO_BUILD_HASH ${randomHash}-random)
endif()

StatusMessage("Build hash: ${FO_BUILD_HASH}")

# Some info about build
StatusMessage("Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
StatusMessage("Generator: ${CMAKE_GENERATOR}")
StatusMessage("Operating system: ${CMAKE_SYSTEM_NAME}")

DetectProcessorArchitecture(FO_PROCESSOR_ARCHITECTURE CMAKE_SYSTEM_PROCESSOR)

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
GetCMakeProperty(FO_MULTICONFIG GENERATOR_IS_MULTI_CONFIG)

if(FO_MULTICONFIG)
	SetValue(CMAKE_CONFIGURATION_TYPES ${FO_CONFIGURATION_TYPES} CACHE STRING "Forced by FOnline" FORCE)
endif()

AddConfiguration(Profiling_Total RelWithDebInfo)
AddConfiguration(Profiling_OnDemand RelWithDebInfo)
AddConfiguration(Debug_Profiling_Total Debug)
AddConfiguration(Debug_Profiling_OnDemand Debug)
AddConfiguration(Release_Ext Release)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	AddConfiguration(Release_Debugging RelWithDebInfo)
	AddConfiguration(San_Address RelWithDebInfo)
	AddConfiguration(Debug_San_Address Debug)
	SetValue(expr_SanitizerConfigs $<CONFIG:San_Address,Debug_San_Address>)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
	AddConfiguration(San_Address RelWithDebInfo)
	AddConfiguration(San_Memory RelWithDebInfo)
	AddConfiguration(San_MemoryWithOrigins RelWithDebInfo)
	AddConfiguration(San_Undefined RelWithDebInfo)
	AddConfiguration(San_Thread RelWithDebInfo)
	AddConfiguration(San_DataFlow RelWithDebInfo)
	AddConfiguration(San_Address_Undefined RelWithDebInfo)
	SetValue(expr_SanitizerConfigs
		$<CONFIG:San_Address,San_Memory,San_MemoryWithOrigins,San_Undefined,San_Thread,San_DataFlow,San_Address_Undefined>)
else()
	SetValue(expr_SanitizerConfigs 0)
endif()

if(FO_MULTICONFIG)
	StringReplace(";" " " configs "${CMAKE_CONFIGURATION_TYPES}")
	StatusMessage("Configurations: ${configs}")
else()
	StatusMessage("Configuration: ${CMAKE_BUILD_TYPE}")

	ListFind(FO_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE} configurationIndex)

	if(configurationIndex EQUAL -1)
		AbortMessage("Invalid requested configuration type")
	endif()
endif()

# Basic compiler setup
SetValue(CMAKE_CXX_STANDARD ${FO_CXX_STANDARD})

SetValue(expr_DebugBuild $<CONFIG:Debug,Debug_Profiling_Total,Debug_Profiling_OnDemand,Debug_San_Address>)
SetValue(expr_FullOptimization $<CONFIG:Release_Ext>)
SetValue(expr_WebFullOptimization $<CONFIG:Release,Release_Ext>)
SetValue(expr_DebugInfo $<NOT:$<CONFIG:MinSizeRel>>)
SetValue(expr_WebDebugInfo $<NOT:$<CONFIG:Release,Release_Ext,MinSizeRel>>)
SetValue(expr_PrefixConfig $<NOT:$<CONFIG:Release,RelWithDebInfo,MinSizeRel,Release_Ext>>)
SetValue(expr_TracyEnabled $<CONFIG:Profiling_Total,Profiling_OnDemand,Debug_Profiling_Total,Debug_Profiling_OnDemand>)
SetValue(expr_TracyOnDemand $<CONFIG:Profiling_OnDemand,Debug_Profiling_OnDemand>)
SetValue(expr_RpmallocEnabled $<NOT:${expr_SanitizerConfigs}>)
SetValue(expr_StandaloneRpmallocEnabled $<AND:${expr_RpmallocEnabled},$<NOT:${expr_TracyEnabled}>>)

AddCompileDefinitionsList(
	$<${expr_DebugBuild}:DEBUG>
	$<${expr_DebugBuild}:_DEBUG>
	$<${expr_DebugBuild}:FO_DEBUG=1>
	$<$<NOT:${expr_DebugBuild}>:NDEBUG>
	$<$<NOT:${expr_DebugBuild}>:FO_DEBUG=0>)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	AddCompileOptionsList(
		$<$<CONFIG:San_Address>:/fsanitize=address>
		$<$<CONFIG:Release_Debugging>:/dynamicdeopt>)
	AddLinkOptionsList($<$<CONFIG:Release_Debugging>:/DYNAMICDEOPT>)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
	AddCompileDefinitionsList(
		$<$<CONFIG:San_Address>:LLVM_USE_SANITIZER=Address>
		$<$<CONFIG:San_Memory>:LLVM_USE_SANITIZER=Memory>
		$<$<CONFIG:San_MemoryWithOrigins>:LLVM_USE_SANITIZER=MemoryWithOrigins>
		$<$<CONFIG:San_Undefined>:LLVM_USE_SANITIZER=Undefined>
		$<$<CONFIG:San_Thread>:LLVM_USE_SANITIZER=Thread>
		$<$<CONFIG:San_DataFlow>:LLVM_USE_SANITIZER=DataFlow>
		$<$<CONFIG:San_Address_Undefined>:LLVM_USE_SANITIZER=Address$<SEMICOLON>Undefined>)
endif()

# Engine settings
AddCompileDefinitionsList(
	"FO_MAIN_CONFIG=\"${FO_MAIN_CONFIG}\""
	FO_ENABLE_3D=$<BOOL:${FO_ENABLE_3D}>
	FO_NATIVE_SCRIPTING=$<BOOL:${FO_NATIVE_SCRIPTING}>
	FO_ANGELSCRIPT_SCRIPTING=$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}>
	FO_MONO_SCRIPTING=$<BOOL:${FO_MONO_SCRIPTING}>
	FO_GEOMETRY=$<IF:$<STREQUAL:${FO_GEOMETRY},HEXAGONAL>,1,$<IF:$<STREQUAL:${FO_GEOMETRY},SQUARE>,2,0>>
	FO_MAP_HEX_WIDTH=${FO_MAP_HEX_WIDTH}
	FO_MAP_HEX_HEIGHT=${FO_MAP_HEX_HEIGHT}
	FO_MAP_CAMERA_ANGLE=${FO_MAP_CAMERA_ANGLE}
	FO_NO_MANUAL_STACK_TRACE=$<CONFIG:Release_Ext>
	FO_NO_EXTRA_ASSERTS=0
	FO_USE_NAMESPACE=$<NOT:$<BOOL:${FO_DISABLE_NAMESPACE}>>)
# Todo: FO_NO_EXTRA_ASSERTS=$<CONFIG:Release_Ext> after separating asserts from handled errors.
AddCompileDefinitionsList(FO_NO_TEXTURE_LOOKUP=0) # Todo: FO_NO_TEXTURE_LOOKUP need option for enable
AddCompileDefinitionsList(FO_DIRECT_SPRITES_DRAW=0) # Todo: FO_DIRECT_SPRITES_DRAW need option for enable
AddCompileDefinitionsList(FO_RENDER_32BIT_INDEX=0) # Todo: FO_RENDER_32BIT_INDEX need option for enable

# Basic includes
AddIncludeDirectories(
	"${FO_ENGINE_ROOT}/Source/Essentials"
	"${FO_ENGINE_ROOT}/Source/Common"
	"${FO_ENGINE_ROOT}/Source/Server"
	"${FO_ENGINE_ROOT}/Source/Client"
	"${FO_ENGINE_ROOT}/Source/Tools"
	"${FO_ENGINE_ROOT}/Source/Scripting"
	"${FO_ENGINE_ROOT}/Source/Frontend"
	"${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource")

# Headless configuration (without video/audio/input)
if(FO_BUILD_CLIENT OR FO_BUILD_SERVER OR FO_BUILD_MAPPER OR FO_BUILD_EDITOR)
	SetValue(FO_HEADLESS_ONLY OFF)
else()
	SetValue(FO_HEADLESS_ONLY ON)
endif()

# Core libs to build
EnableCoreLibraryIfAny(
	FO_BUILD_COMMON_LIB
	FO_BUILD_CLIENT
	FO_BUILD_SERVER
	FO_BUILD_MAPPER
	FO_BUILD_EDITOR
	FO_BUILD_ASCOMPILER
	FO_BUILD_BAKER
	FO_UNIT_TESTS
	FO_CODE_COVERAGE)
EnableCoreLibraryIfAny(
	FO_BUILD_CLIENT_LIB
	FO_BUILD_CLIENT
	FO_BUILD_MAPPER
	FO_BUILD_SERVER
	FO_BUILD_EDITOR
	FO_UNIT_TESTS
	FO_CODE_COVERAGE)
EnableCoreLibraryIfAny(
	FO_BUILD_SERVER_LIB
	FO_BUILD_SERVER
	FO_BUILD_EDITOR
	FO_UNIT_TESTS
	FO_CODE_COVERAGE)
EnableCoreLibraryIfAny(
	FO_BUILD_MAPPER_LIB
	FO_BUILD_MAPPER
	FO_BUILD_EDITOR
	FO_UNIT_TESTS
	FO_CODE_COVERAGE)
EnableCoreLibraryIfAny(
	FO_BUILD_BAKER_LIB
	FO_BUILD_SERVER
	FO_BUILD_MAPPER
	FO_BUILD_EDITOR
	FO_BUILD_ASCOMPILER
	FO_BUILD_BAKER
	FO_UNIT_TESTS
	FO_CODE_COVERAGE)
EnableCoreLibraryIfAny(
	FO_BUILD_EDITOR_LIB
	FO_BUILD_EDITOR
	FO_UNIT_TESTS
	FO_CODE_COVERAGE)

# Per OS configurations
if(WIN32)
	SetValue(FO_WINDOWS 1)
	SetValue(FO_HAVE_OPENGL 1)
	SetValue(FO_HAVE_DIRECT_3D 1)

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		SetBuildPlatformInfo("Windows-win64" "windows" "x64")
	else()
		SetBuildPlatformInfo("Windows-win32" "windows" "x86")
	endif()

	SetValue(CMAKE_SYSTEM_VERSION 6.1)
	AddCompileDefinitionsList(_WIN32_WINNT=0x0601)
	AddCompileDefinitionsList(
		UNICODE
		_UNICODE
		_CRT_SECURE_NO_WARNINGS
		_CRT_SECURE_NO_DEPRECATE
		_WINSOCK_DEPRECATED_NO_WARNINGS)

	AddCompileOptionsList(
		/permissive-
		/Zc:__cplusplus
		/Zc:preprocessor
		$<${expr_DebugBuild}:/RTC1>
		$<${expr_DebugBuild}:/GS>
		$<$<OR:${expr_DebugBuild},$<CONFIG:RelWithDebInfo>>:/JMC>
		$<$<NOT:${expr_DebugBuild}>:/sdl->
		/W4
		/MP
		/EHsc
		/utf-8
		/volatile:iso
		/GR
		/bigobj
		/fp:fast
		$<${expr_FullOptimization}:/GL>
		$<${expr_DebugInfo}:/Zi>)
	AddLinkOptionsList(
		/INCREMENTAL:NO
		/OPT:REF
		/OPT:NOICF
		$<${expr_FullOptimization}:/LTCG>
		$<IF:${expr_DebugInfo},/DEBUG:FULL,/DEBUG:NONE>)

	if(FO_BUILD_CLIENT)
		AddCompileOptionsList($<${expr_DebugBuild}:/MTd> $<$<NOT:${expr_DebugBuild}>:/MT>)
	else()
		AddCompileOptionsList($<${expr_DebugBuild}:/MDd> $<$<NOT:${expr_DebugBuild}>:/MD>)
	endif()

	AppendList(FO_ESSENTIALS_SYSTEM_LIBS "user32" "ws2_32" "version" "winmm" "imm32" "dbghelp" "psapi" "xinput")

	if(NOT FO_HEADLESS_ONLY)
		SetValue(FO_USE_GLEW ON)
		AppendList(FO_RENDER_SYSTEM_LIBS "d3d9" "gdi32" "dxgi" "windowscodecs" "dxguid")
		AppendList(FO_RENDER_SYSTEM_LIBS "glu32" "d3d11" "d3dcompiler" "opengl32")
	endif()

elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
	SetValue(FO_LINUX 1)
	SetValue(FO_HAVE_OPENGL 1)
	SetBuildPlatformInfo("Linux-${FO_PROCESSOR_ARCHITECTURE}" "linux" "${FO_PROCESSOR_ARCHITECTURE}")

	if(NOT FO_HEADLESS_ONLY)
		RequirePackage(X11 REQUIRED)
		RequirePackage(OpenGL REQUIRED)
		SetValue(FO_USE_GLEW ON)
		AppendList(FO_RENDER_SYSTEM_LIBS "GL")
	endif()

	AddNativeOptimizationFlags()
	AddLinkOptionsList(-rdynamic)

	if(NOT FO_BUILD_BAKER)
		AddLinkOptionsList(-no-pie)
	else()
		AddCompileOptionsList(-fPIC)
	endif()

	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		# Todo: using of libc++ leads to crash on any exception when trying to call free() with invalid pointer
		# Bug somehow connected with rpmalloc new operators overloading
		# add_compile_options_C_CXX(-stdlib=libc++)
		# add_link_options(-stdlib=libc++)
	endif()

elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
	SetValue(FO_MAC 1)
	SetValue(FO_HAVE_OPENGL 1)
	SetValue(FO_HAVE_METAL 1)
	SetBuildPlatformInfo("macOS-${FO_PROCESSOR_ARCHITECTURE}" "osx" "${FO_PROCESSOR_ARCHITECTURE}")

	if(NOT FO_HEADLESS_ONLY)
		RequirePackage(OpenGL REQUIRED)
		SetValue(FO_USE_GLEW ON)
		AppendList(FO_RENDER_SYSTEM_LIBS ${OPENGL_LIBRARIES})
	endif()

	AddNativeOptimizationFlags()
	AddLinkOptionsList(-rdynamic)

elseif(CMAKE_SYSTEM_NAME MATCHES "iOS")
	StatusMessage("Deployment target: ${DEPLOYMENT_TARGET}")

	SetValue(FO_IOS 1)
	SetValue(FO_HAVE_OPENGL 1)
	SetValue(FO_OPENGL_ES 1)
	SetValue(FO_HAVE_METAL 1)

	if(PLATFORM STREQUAL "OS64")
		StatusMessage("Platform: Device")
		SetBuildPlatformInfo("iOS-arm64" "ios" "arm64")
	elseif(PLATFORM STREQUAL "SIMULATOR64")
		StatusMessage("Platform: Simulator")
		SetBuildPlatformInfo("iOS-simulator" "iossimulator" "arm64")
	else()
		AbortMessage("Invalid iOS target platform ${PLATFORM}")
	endif()

	if(NOT FO_HEADLESS_ONLY)
		FindAndAppendLibraries(
			FO_RENDER_SYSTEM_LIBS
			OpenGLES
			Metal
			CoreGraphics
			QuartzCore
			UIKit
			AVFoundation
			GameController
			CoreMotion)
	endif()

	AddNativeOptimizationFlags()
	AppendList(FO_COMMON_SYSTEM_LIBS "iconv") # Todo: ios iconv workaround for SDL, remove in future updates

elseif(CMAKE_SYSTEM_NAME MATCHES "Android")
	SetValue(FO_ANDROID 1)
	SetValue(FO_HAVE_OPENGL 1)
	SetValue(FO_OPENGL_ES 1)
	SetValue(FO_BUILD_PLATFORM "Android-${ANDROID_ABI}")
	SetValue(FO_BUILD_LIBRARY ON)
	SetValue(FO_MONO_OS "android")

	if(${ANDROID_ABI} STREQUAL "armeabi-v7a")
		SetBuildPlatformInfo("Android-${ANDROID_ABI}" "android" "arm")
	elseif(${ANDROID_ABI} STREQUAL "arm64-v8a")
		SetBuildPlatformInfo("Android-${ANDROID_ABI}" "android" "arm64")
	elseif(${ANDROID_ABI} STREQUAL "x86")
		SetBuildPlatformInfo("Android-${ANDROID_ABI}" "android" "x86")
	endif()

	if(NOT FO_HEADLESS_ONLY)
		AppendList(FO_RENDER_SYSTEM_LIBS "GLESv3")
	endif()

	AppendList(FO_ESSENTIALS_SYSTEM_LIBS "android" "log" "atomic")
	AddNativeOptimizationFlags()
	AddLinkOptionsList(-pie)

elseif(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
	SetValue(FO_WEB 1)
	SetValue(FO_HAVE_OPENGL 1)
	SetValue(FO_OPENGL_ES 1)
	SetBuildPlatformInfo("Web-wasm" "browser" "wasm")
	SetValue(CMAKE_EXECUTABLE_SUFFIX ".js")
	SetValue(webRuntimeMethods
		"['FS_createPath','FS_createDataFile','intArrayFromString','UTF8ToString','addRunDependency','removeRunDependency','autoResumeAudioContext','dynCall']")
	AddCompileOptionsList(
		$<${expr_WebDebugInfo}:-g3>
		$<${expr_WebFullOptimization}:-O3>
		$<${expr_WebFullOptimization}:-flto>
		-sDISABLE_EXCEPTION_CATCHING=0
		-sMAIN_MODULE=2
		-sSTRICT=0)
	AddLinkOptionsList(
		$<${expr_WebDebugInfo}:-g3>
		$<${expr_WebFullOptimization}:-O3>
		$<${expr_WebFullOptimization}:-flto>
		$<IF:${expr_DebugBuild},-O0,-O2>
		-sASSERTIONS=$<IF:${expr_DebugBuild},2,0>
		-sSTACK_OVERFLOW_CHECK=$<IF:${expr_DebugBuild},2,0>
		-sINITIAL_MEMORY=268435456
		-sABORT_ON_WASM_EXCEPTIONS=1
		-sERROR_ON_UNDEFINED_SYMBOLS=1
		-sALLOW_MEMORY_GROWTH=1
		-sMIN_WEBGL_VERSION=2
		-sMAX_WEBGL_VERSION=2
		-sUSE_SDL=0
		-sFORCE_FILESYSTEM=1
		-sDYNAMIC_EXECUTION=0
		-sEXIT_RUNTIME=0
		-sEXPORTED_RUNTIME_METHODS=${webRuntimeMethods}
		-sDISABLE_EXCEPTION_CATCHING=0
		-sWASM_BIGINT=1
		-sMAIN_MODULE=2
		-sSTRICT=0
		-sSTRICT_JS=1
		-sIGNORE_MISSING_MAIN=0
		-sAUTO_JS_LIBRARIES=0
		-sAUTO_NATIVE_LIBRARIES=0
		-sDEFAULT_TO_CXX=0
		-sUSE_GLFW=0
		-sALLOW_UNIMPLEMENTED_SYSCALLS=0
		-lhtml5
		-lGL
		-legl.js
		-lhtml5_webgl.js
		-lidbfs.js)

else()
	AbortMessage("Unknown OS")
endif()

AddCompileDefinitionsList(
	FO_WINDOWS=${FO_WINDOWS}
	FO_LINUX=${FO_LINUX}
	FO_MAC=${FO_MAC}
	FO_ANDROID=${FO_ANDROID}
	FO_IOS=${FO_IOS}
	FO_WEB=${FO_WEB})
AddCompileDefinitionsList(
	FO_HAVE_OPENGL=${FO_HAVE_OPENGL}
	FO_OPENGL_ES=${FO_OPENGL_ES}
	FO_HAVE_DIRECT_3D=${FO_HAVE_DIRECT_3D}
	FO_HAVE_METAL=${FO_HAVE_METAL}
	FO_HAVE_VULKAN=${FO_HAVE_VULKAN})

if(FO_CODE_COVERAGE)
	SetValue(FO_COVERAGE_VARIANT "")

	if(MSVC)
		AddCompileOptionsList(/Od /Zi)
		AddLinkOptionsList(/PROFILE)

		if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			SetValue(FO_COVERAGE_VARIANT "ClangCl")
		else()
			SetValue(FO_COVERAGE_VARIANT "MSVC")
		endif()
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		AddCompileOptionsList(-O0 -fprofile-instr-generate -fcoverage-mapping)
		AddLinkOptionsList(-fprofile-instr-generate)
		SetValue(FO_COVERAGE_VARIANT "Clang")
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		AddCompileOptionsList(-O0 --coverage)
		AddLinkOptionsList(--coverage)
		SetValue(FO_COVERAGE_VARIANT "GCC")
	else()
		AbortMessage("Code coverage is not configured for the selected compiler")
	endif()

	SetValue(FO_COVERAGE_OUTPUT
		"${FO_OUTPUT_PATH}/CodeCoverage/${FO_COVERAGE_VARIANT}/${FO_BUILD_PLATFORM}$<${expr_PrefixConfig}:-$<CONFIG>>")
endif()

# Output path
StatusMessage("Output path: ${FO_OUTPUT_PATH}")
SetBinaryOutputPath(FO_CLIENT_OUTPUT Client)
SetBinaryOutputPath(FO_SERVER_OUTPUT Server)
SetBinaryOutputPath(FO_EDITOR_OUTPUT Editor)
SetBinaryOutputPath(FO_MAPPER_OUTPUT Mapper)
SetBinaryOutputPath(FO_ASCOMPILER_OUTPUT ASCompiler)
SetBinaryOutputPath(FO_BAKER_OUTPUT Baker)
SetBinaryOutputPath(FO_TESTS_OUTPUT Tests)

SetValue(FO_CONTRIBUTION_DIR ${CMAKE_CURRENT_SOURCE_DIR})
