cmake_minimum_required( VERSION 2.8 )
project( FOnlineProject )

#
# Preprocessor definitions
#

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
	set( OS "WINDOWS" )
	set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/../Client" )
	if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		set( CLIENT_POSTFIX "64" )
	else()
		set( NON_CLIENT_POSTFIX "32" )
	endif()
	set( CMAKE_CONFIGURATION_TYPES "${CONFIGURATION_NAME_DESKTOP}" CACHE STRING "" FORCE )
	set( CMAKE_BUILD_TYPE "${CONFIGURATION_NAME_DESKTOP}" CACHE STRING "" FORCE )
	link_directories( "${CMAKE_CURRENT_SOURCE_DIR}/Lib/Windows/${CONFIGURATION_NAME}/${ARCH_STR}" )
	set( USE_GLEW YES )
elseif( ${CMAKE_SYSTEM_NAME} MATCHES "Linux" )
	set( OS "LINUX" )
	set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/../Client" )
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
	link_directories( "${CMAKE_CURRENT_SOURCE_DIR}/Lib/Linux/${CONFIGURATION_NAME}/${ARCH_STR}" )
elseif( APPLE AND NOT IOS_PLATFORM )
	set( OS "MAC" )
	set( BUILD_ONLY_CLIENT YES )
	set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/Mac" )
	set( CMAKE_CONFIGURATION_TYPES "${CONFIGURATION_NAME_DESKTOP}" CACHE STRING "" FORCE )
	set( CMAKE_BUILD_TYPE "${CONFIGURATION_NAME_DESKTOP}" CACHE STRING "" FORCE )
	find_package( OpenGL REQUIRED )
	set( RENDER_LIBS "${OPENGL_LIBRARIES}" )
	set( USE_GLEW YES )
elseif( APPLE AND IOS_PLATFORM )
	set( OS "IOS" )
	set( BUILD_ONLY_CLIENT YES )
	set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/iOS" )
	find_package( OpenGL REQUIRED )
	set( RENDER_LIBS "${OPENGL_LIBRARIES}" )
elseif( ANDROID )
	set( OS "ANDROID" )
	set( BUILD_ONLY_CLIENT YES )
	set( BUILD_CLIENT_LIBRARY YES )
	set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/../Android/libs/${ANDROID_ABI}" )
	set( RENDER_LIBS "GLESv1_CM" "GLESv2" )
	set( CLIENT_LIBS "android" "log" )
elseif( EMSCRIPTEN )
	set( OS "WEB" )
	set( BUILD_ONLY_CLIENT YES )
	set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/../Web" )
	set( CMAKE_EXECUTABLE_SUFFIX ".js" )
	set( WEB_OPTIONS "-s TOTAL_MEMORY=268435456" ) # 256 Mb
	#set( WEB_OPTIONS "${WEB_OPTIONS} --memory-init-file on" )
	if( FO_DEBUG )
		set( WEB_OPTIONS "${WEB_OPTIONS} -g4 -s SAFE_HEAP=1 -s ASSERTIONS=2" )
	else
		set( WEB_OPTIONS "${WEB_OPTIONS} -Oz" )
	endif()
	set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${WEB_OPTIONS}" )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WEB_OPTIONS}" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WEB_OPTIONS}" )
	add_definitions( "-DAS_MAX_PORTABILITY" )
else()
	message( FATAL_ERROR "Unknown OS!" )
endif()

if( FO_DEBUG )
	set( CLIENT_POSTFIX "${CLIENT_POSTFIX}_Debug" )
	set( NON_CLIENT_POSTFIX "${NON_CLIENT_POSTFIX}_Debug" )
endif()

set( SERVER_OUTPUT "${CMAKE_BINARY_DIR}/../Server" )
set( MAPPER_OUTPUT "${CMAKE_BINARY_DIR}/../Mapper" )
set( ASCOMPILER_OUTPUT "${CMAKE_BINARY_DIR}/../ASCompiler" )

if( MSVC_IDE )
	set( COMPILER_DEFINES "-D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE -D_WINSOCK_DEPRECATED_NO_WARNINGS" )
	set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${COMPILER_DEFINES} /MTd /MP" )
	set( CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} ${COMPILER_DEFINES} /MT /MP" )
	set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${COMPILER_DEFINES} /MT /MP" )
	set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${COMPILER_DEFINES} /MT /MP" )
	set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${COMPILER_DEFINES} /MTd /MP" )
	set( CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} ${COMPILER_DEFINES} /MT /MP" )
	set( CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${COMPILER_DEFINES} /MT /MP" )
	set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${COMPILER_DEFINES} /MT /MP" )
	set( CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /INCREMENTAL:NO" )
	set( CMAKE_VS_PLATFORM_TOOLSET "v120_xp" CACHE STRING "Platform toolset" FORCE )
else()
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
	if( CMAKE_COMPILER_IS_GNUCXX )
		set_source_files_properties( "${CMAKE_CURRENT_SOURCE_DIR}/Debugger.cpp" PROPERTIES COMPILE_FLAGS "-Wdeprecated-declarations" )
	endif()
endif()

set( SERVER_DEFINES "-DFONLINE_SERVER -DFO_${OS}" )
set( CLIENT_DEFINES "-DFONLINE_CLIENT -DFO_${OS}" )
set( MAPPER_DEFINES "-DFONLINE_MAPPER -DFO_${OS}" )
set( ASCPMPILER_DEFINES "-DFONLINE_SCRIPT_COMPILER -DFO_${OS}" )

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
add_definitions( "-DGL_GLEXT_PROTOTYPES" )
add_subdirectory( "${SDL_DIR}" )
include_directories( "${SDL_DIR}/include" )

if( NOT BUILD_ONLY_CLIENT )
	set( FLTK_DIR "FLTK" ) # fltk-1.3.3
	set( OPTION_BUILD_EXAMPLES OFF CACHE STRING "" FORCE )
	set( OPENGL_FOUND OFF CACHE STRING "" FORCE )
	add_subdirectory( "${FLTK_DIR}" )
	include_directories( "${FLTK_DIR}" )
endif()

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
	set( CURL_DIR "cURL" ) # curl-7.52.1
	set( CURL_STATICLIB ON CACHE STRING "" FORCE )
	set( BUILD_CURL_EXE OFF CACHE STRING "" FORCE )
	set( BUILD_TESTING OFF CACHE STRING "" FORCE )
	set( CMAKE_USE_OPENSSL OFF CACHE STRING "" FORCE )
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
set( SERVER_CLIENT_MAPPER_SOURCE
	${Assimp_SOURCE}
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
set( SERVER_ASCOMPILER_SOURCE
	DummyData.h
)
set( CLIENT_MAPPER_SOURCE
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
set( SERVER_MAPPER_SOURCE
	ResourceConverter.h ResourceConverter.cpp
)

set( SERVER_SOURCE
	${COMMON_SOURCE}
	${SERVER_CLIENT_MAPPER_SOURCE}
	${SERVER_MAPPER_SOURCE}
	${SERVER_CLIENT_SOURCE}
	${SERVER_ASCOMPILER_SOURCE}
	${fbxsdk_SOURCE}
	AI.h
	Critter.cpp Critter.h
	CritterData.h
	CritterManager.h CritterManager.cpp
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
	${SERVER_CLIENT_MAPPER_SOURCE}
	${SERVER_CLIENT_SOURCE}
	${CLIENT_MAPPER_SOURCE}
	${OGG_SOURCE}
	${VORBIS_SOURCE}
	${THEORA_SOURCE}
	Client.cpp Client.h
	ClientInterface.cpp
	Defence.h
	MainClient.cpp
	SoundManager.cpp SoundManager.h
)
set( MAPPER_SOURCE
	${COMMON_SOURCE}
	${SERVER_CLIENT_MAPPER_SOURCE}
	${SERVER_MAPPER_SOURCE}
	${CLIENT_MAPPER_SOURCE}
	${fbxsdk_SOURCE}
	MainMapper.cpp
	Mapper.cpp Mapper.h
)
set( ASCOMPILER_SOURCE
	${COMMON_SOURCE}
	${SERVER_ASCOMPILER_SOURCE}
	ASCompiler.cpp
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
	set_target_properties( FOnline PROPERTIES ${RO0} ${CLIENT_OUTPUT} ${RO1} ${CLIENT_OUTPUT} ${RO2} ${CLIENT_OUTPUT} ${RO3} ${CLIENT_OUTPUT} ${RO4} ${CLIENT_OUTPUT} )
else()
	add_library( FOnline SHARED ${CLIENT_SOURCE} )
	set_target_properties( FOnline PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CLIENT_OUTPUT} )
endif()
set_target_properties( FOnline PROPERTIES OUTPUT_NAME "FOnline${CLIENT_POSTFIX}" COMPILE_FLAGS "${CLIENT_DEFINES}" )
target_link_libraries( FOnline "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SDL2main" "SDL2-static" "Acm" "SHA" "${NCODEHOOK}" "${RENDER_LIBS}" "${CMAKE_DL_LIBS}" "${CLIENT_LIBS}" )

if( NOT BUILD_ONLY_CLIENT )
	add_executable( FOnlineServer WIN32 ${SERVER_SOURCE} "Server.rc" )
	set_target_properties( FOnlineServer PROPERTIES ${RO0} ${SERVER_OUTPUT} ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
	set_target_properties( FOnlineServer PROPERTIES OUTPUT_NAME "FOnlineServer${NON_CLIENT_POSTFIX}" COMPILE_FLAGS "${SERVER_DEFINES}" )
	target_link_libraries( FOnlineServer "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SHA" "libcurl" "${NCODEHOOK}" "fltk" "${FBXSDK}" "assimp" "${CMAKE_DL_LIBS}" )

	if( NOT WIN32 )
		add_executable( FOnlineServerDaemon ${SERVER_SOURCE} "Server.rc" )
		set_target_properties( FOnlineServerDaemon PROPERTIES ${RO0} ${SERVER_OUTPUT} ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
		set_target_properties( FOnlineServerDaemon PROPERTIES OUTPUT_NAME "FOnlineServerDaemon${NON_CLIENT_POSTFIX}" COMPILE_FLAGS "${SERVER_DEFINES} -DSERVER_DAEMON" )
		target_link_libraries( FOnlineServerDaemon "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SHA" "libcurl" "${NCODEHOOK}" "${FBXSDK}" "assimp" "${CMAKE_DL_LIBS}" )
	endif()

	add_executable( Mapper WIN32 ${MAPPER_SOURCE} "Mapper.rc" )
	set_target_properties( Mapper PROPERTIES ${RO0} ${MAPPER_OUTPUT} ${RO1} ${MAPPER_OUTPUT} ${RO2} ${MAPPER_OUTPUT} ${RO3} ${MAPPER_OUTPUT} ${RO4} ${MAPPER_OUTPUT} )
	set_target_properties( Mapper PROPERTIES OUTPUT_NAME "Mapper${NON_CLIENT_POSTFIX}" COMPILE_FLAGS "${MAPPER_DEFINES}" )
	target_link_libraries( Mapper "Angelscript" "AngelscriptExt" "zlibstatic" "${PNG16}" "SDL2main" "SDL2-static" "SHA" "${NCODEHOOK}" "${FBXSDK}" "assimp" "${RENDER_LIBS}" "${CMAKE_DL_LIBS}" )

	add_executable( ASCompiler ${ASCOMPILER_SOURCE} )
	set_target_properties( ASCompiler PROPERTIES ${RO0} ${ASCOMPILER_OUTPUT} ${RO1} ${ASCOMPILER_OUTPUT} ${RO2} ${ASCOMPILER_OUTPUT} ${RO3} ${ASCOMPILER_OUTPUT} ${RO4} ${ASCOMPILER_OUTPUT} )
	set_target_properties( ASCompiler PROPERTIES OUTPUT_NAME "ASCompiler${NON_CLIENT_POSTFIX}" COMPILE_FLAGS "${ASCPMPILER_DEFINES}" )
	target_link_libraries( ASCompiler "Angelscript" "AngelscriptExt" "zlibstatic" "SHA" "${NCODEHOOK}" "${CMAKE_DL_LIBS}" )

	set_property( GLOBAL PROPERTY USE_FOLDERS ON )
	set_property( TARGET FOnlineServer FOnline Mapper ASCompiler PROPERTY FOLDER "Binaries")
	set_property( TARGET Acm Angelscript AngelscriptExt fltk ${NCODEHOOK} png16_static SDL2main SDL2-static SHA zlibstatic libcurl assimp PROPERTY FOLDER "Libraries")
endif()
