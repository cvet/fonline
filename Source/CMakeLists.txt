cmake_minimum_required( VERSION 2.8 )
project( FOnlineProject )
set( CMAKE_CONFIGURATION_TYPES "RelWithDebInfo" CACHE STRING "" FORCE )

#
# Preprocessor definitions
#

set( BUILD_ONLY_CLIENT NO )

if( WIN32 )
	set( OS "Windows" )
elseif( ${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" )
	set( OS "Linux" )
	find_package( X11 REQUIRED )
	find_package( OpenGL REQUIRED )
	set( GL "GL" )
elseif( APPLE )
	set( OS "OSX" )
	find_package( OpenGL REQUIRED )
	set( GL "${OPENGL_LIBRARIES}" )
	set( BUILD_ONLY_CLIENT YES )
else()
	message( FATAL_ERROR "Unknown OS!" )
endif()

if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
	set( ARCH_X64 TRUE )
	set( ARCH_STR "x64" )
else()
	set( ARCH_X64 FALSE )
	set( ARCH_STR "x86" )
endif()

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
	if( CMAKE_COMPILER_IS_GNUCXX )
		set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x" )
		set_source_files_properties( "${CMAKE_CURRENT_SOURCE_DIR}/Debugger.cpp" PROPERTIES COMPILE_FLAGS "-Wdeprecated-declarations" )
	endif()
	set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" )
	set( CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}" )
	set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}" )
	set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" )
	set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}" )
	set( CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL}" )
	set( CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}" )
	set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}" )
	set( CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}" )
endif()

set( SERVER_DEFINES "-DFONLINE_SERVER")
set( CLIENT_DEFINES "-DFONLINE_CLIENT")
set( MAPPER_DEFINES "-DFONLINE_MAPPER")
set( ASCPMPILER_DEFINES "-DFONLINE_SCRIPT_COMPILER")

#
# Sources
#

set( SKIP_INSTALL_ALL ON )

set( SDL_DIR "SDL2" ) # SDL2-2.0.3
set( SDL_SHARED OFF CACHE STRING "" FORCE )
set( RENDER_D3D OFF CACHE STRING "" FORCE )
add_subdirectory( "${SDL_DIR}" )

set( FLTK_DIR "FLTK" ) # fltk-1.3.3
set( OPTION_BUILD_EXAMPLES OFF CACHE STRING "" FORCE )
set( OPENGL_FOUND OFF CACHE STRING "" FORCE )
add_subdirectory( "${FLTK_DIR}" )

set( ZLIB_DIR "zlib" ) # zlib-1.2.8
add_subdirectory( "${ZLIB_DIR}" )

set( PNG_DIR "PNG" ) # lpng1616
set( ZLIB_LIBRARY "zlibstatic" )
set( ZLIB_INCLUDE_DIR "../${ZLIB_DIR}" "${CMAKE_BINARY_DIR}/${ZLIB_DIR}" )
set( PNG_SHARED OFF )
set( PNG_STATIC ON )
add_subdirectory( "${PNG_DIR}" )

set( OGG_DIR "ogg" ) # libogg-1.3.1
file( GLOB OGG_SOURCE "${OGG_DIR}/src/*.c" )
source_group( ogg FILES ${OGG_SOURCE} )

set( VORBIS_DIR "Vorbis" ) # libvorbis-1.3.3
file( GLOB VORBIS_SOURCE "${VORBIS_DIR}/lib/*.c" )
list( REMOVE_ITEM VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${VORBIS_DIR}/lib/barkmel.c" )
list( REMOVE_ITEM VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${VORBIS_DIR}/lib/psytune.c" )
list( REMOVE_ITEM VORBIS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${VORBIS_DIR}/lib/tone.c" )
source_group( vorbis FILES ${VORBIS_SOURCE} )

set( THEORA_DIR "Theora" ) # libtheora-1.1.1
file( GLOB THEORA_SOURCE "${THEORA_DIR}/lib/*.c" )
list( REMOVE_ITEM THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${THEORA_DIR}/lib/analyze.c" )
list( REMOVE_ITEM THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${THEORA_DIR}/lib/encode.c" )
list( REMOVE_ITEM THEORA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${THEORA_DIR}/lib/encapiwrapper.c" )
source_group( theora FILES ${THEORA_SOURCE} )

set( ANGEL_SCRIPT_DIR "AngelScript" )
add_subdirectory( "${ANGEL_SCRIPT_DIR}/sdk/angelscript/projects/cmake" )
add_subdirectory( "${ANGEL_SCRIPT_DIR}" )

add_subdirectory( "Acm" )
add_subdirectory( "SHA2" )
add_subdirectory( "GL" )
if( WIN32 )
	add_subdirectory( "NCodeHook" )
	set( NCODEHOOK "NCodeHook" )
endif()
if( NOT WIN32 )
	set( EVENT2 "libevent.a" )
	set( EVENT2_PTHREADS "libevent_pthreads.a" )
endif()

file( GLOB "Assimp_SOURCE Assimp/*.* Assimp/Compiler/*.*" )
source_group( Assimp FILES ${Assimp_SOURCE} )

file( GLOB "fbxsdk_SOURCE fbxsdk/*.*" )
source_group( fbxsdk FILES ${fbxsdk_SOURCE} )
if( WIN32 )
	set( FBXSDK "libfbxsdk" )
else()
	set( FBXSDK "libfbxsdk.a")
endif()
if( MSVC_IDE )
	set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DELAYLOAD:libfbxsdk.dll" )
endif()

set( COMMON_SOURCE
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
	ScriptBind.h
	ScriptFunctions.h
	ScriptInvoker.cpp ScriptInvoker.h
	ScriptPragmas.cpp ScriptPragmas.h
	ScriptProfiler.cpp ScriptProfiler.h
	Text.cpp Text.h
	Timer.cpp Timer.h
	Types.h
)
set( SERVER_CLIENT_MAPPER_SOURCE
	ConstantsManager.cpp ConstantsManager.h
	CritterManager.h
	CritterType.cpp CritterType.h
	Item.cpp Item.h
	ItemManager.cpp ItemManager.h
	LineTracer.h
	MsgFiles.cpp MsgFiles.h
	MsgStr.h
	NetProtocol.h
)
set( SERVER_CLIENT_SOURCE
	Access.h
	BufferManager.cpp BufferManager.h
	CraftManager.cpp CraftManager.h
	DataMask.h
)
set( SERVER_MAPPER_SOURCE
	CritterManager.cpp
	ProtoMap.cpp ProtoMap.h
)
set( SERVER_ASCOMPILER_SOURCE
	DummyData.h
)
set( CLIENT_MAPPER_SOURCE
	${Assimp_SOURCE}
	${fbxsdk_SOURCE}
	3dAnimation.h
	3dStuff.cpp 3dStuff.h
	CritterCl.cpp CritterCl.h
	F2Palette.h
	FlexRect.h
	GraphicLoader.cpp GraphicLoader.h
	GraphicStructures.cpp GraphicStructures.h
	HexManager.cpp HexManager.h
	ItemHex.cpp ItemHex.h
	Keyboard.cpp Keyboard.h
	ResourceManager.cpp ResourceManager.h
	SpriteManager.cpp SpriteManager.h
	SpriteManagerFont.cpp
	Sprites.cpp Sprites.h
)

set( SERVER_SOURCE
	${COMMON_SOURCE}
	${SERVER_CLIENT_MAPPER_SOURCE}
	${SERVER_CLIENT_SOURCE}
	${SERVER_MAPPER_SOURCE}
	${SERVER_ASCOMPILER_SOURCE}
	AI.cpp AI.h
	Critter.cpp Critter.h
	CritterData.h
	Dialogs.cpp Dialogs.h
	EntityManager.cpp EntityManager.h
	Jobs.cpp Jobs.h
	MainServer.cpp
	Map.cpp Map.h
	MapManager.cpp MapManager.h
	Mutex.h
	Server.cpp Server.h
	ServerClient.cpp
	ServerItem.cpp
	ServerNpc.cpp
	ServerScript.cpp
	ThreadSync.cpp ThreadSync.h
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
include_directories( "${SDL_DIR}/include" )
include_directories( "${FLTK_DIR}" )
include_directories( "${ZLIB_DIR}" "${ZLIB_DIR}/contrib" "${CMAKE_BINARY_DIR}/${ZLIB_DIR}" )
include_directories( "${PNG_DIR}" "${CMAKE_BINARY_DIR}/${PNG_DIR}" )
include_directories( "${OGG_DIR}/include" )
include_directories( "${VORBIS_DIR}/include" )
include_directories( "${VORBIS_DIR}/lib" )
include_directories( "${THEORA_DIR}/include" )
include_directories( "${ANGEL_SCRIPT_DIR}/sdk/angelscript/include" "${ANGEL_SCRIPT_DIR}" )
link_directories( "${CMAKE_CURRENT_SOURCE_DIR}/Lib/${OS}/${ARCH_STR}" )
link_directories( "$ENV{DXSDK_DIR}/lib/${ARCH_STR}" )

if( MSVC_IDE )
	set( RO1 RUNTIME_OUTPUT_DIRECTORY_DEBUG )
	set( RO2 RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL )
	set( RO3 RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO )
	set( RO4 RUNTIME_OUTPUT_DIRECTORY_RELEASE )
else()
	set( RO1 RUNTIME_OUTPUT_DIRECTORY )
	set( RO2 RUNTIME_OUTPUT_DIRECTORY )
	set( RO3 RUNTIME_OUTPUT_DIRECTORY )
	set( RO4 RUNTIME_OUTPUT_DIRECTORY )
endif()

add_executable( FOnline WIN32 ${CLIENT_SOURCE} "Client.rc" )
set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/../SDK/Client/" )
set_target_properties( FOnline PROPERTIES ${RO1} ${CLIENT_OUTPUT} ${RO2} ${CLIENT_OUTPUT} ${RO3} ${CLIENT_OUTPUT} ${RO4} ${CLIENT_OUTPUT} )
set_target_properties( FOnline PROPERTIES COMPILE_FLAGS "${CLIENT_DEFINES}" )
target_link_libraries( FOnline "Angelscript" "AngelscriptExt" "zlibstatic" "png16_static" "SDL2main" "SDL2-static" "Acm" "SHA2" "${NCODEHOOK}" "${GL}" "GLExt" "${FBXSDK}" "${CMAKE_DL_LIBS}" )

if( NOT BUILD_ONLY_CLIENT )
	add_executable( FOnlineServer WIN32 ${SERVER_SOURCE} "Server.rc" )
	set( SERVER_OUTPUT "${CMAKE_BINARY_DIR}/../SDK/Server/" )
	set_target_properties( FOnlineServer PROPERTIES ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
	set_target_properties( FOnlineServer PROPERTIES COMPILE_FLAGS "${SERVER_DEFINES}" )
	target_link_libraries( FOnlineServer "Angelscript" "AngelscriptExt" "zlibstatic" "png16_static" "SHA2" "${NCODEHOOK}" "fltk" "${EVENT2}" "${EVENT2_PTHREADS}" "${CMAKE_DL_LIBS}" )

	if( NOT WIN32 )
		add_executable( FOnlineServerDaemon ${SERVER_SOURCE} "Server.rc" )
		set( SERVER_OUTPUT "${CMAKE_BINARY_DIR}/../SDK/Server/" )
		set_target_properties( FOnlineServerDaemon PROPERTIES ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
		set_target_properties( FOnlineServerDaemon PROPERTIES COMPILE_FLAGS "${SERVER_DEFINES} -DSERVER_DAEMON" )
		target_link_libraries( FOnlineServerDaemon "Angelscript" "AngelscriptExt" "zlibstatic" "png16_static" "SHA2" "${NCODEHOOK}" "${EVENT2}" "${EVENT2_PTHREADS}" "${CMAKE_DL_LIBS}" )
	endif()

	add_executable( Mapper WIN32 ${MAPPER_SOURCE} "Mapper.rc" )
	set( MAPPER_OUTPUT "${CMAKE_BINARY_DIR}/../SDK/Tools/Mapper/" )
	set_target_properties( Mapper PROPERTIES ${RO1} ${MAPPER_OUTPUT} ${RO2} ${MAPPER_OUTPUT} ${RO3} ${MAPPER_OUTPUT} ${RO4} ${MAPPER_OUTPUT} )
	set_target_properties( Mapper PROPERTIES COMPILE_FLAGS "${MAPPER_DEFINES}" )
	target_link_libraries( Mapper "Angelscript" "AngelscriptExt" "zlibstatic" "png16_static" "SDL2main" "SDL2-static" "SHA2" "${NCODEHOOK}" "${GL}" "GLExt" "${FBXSDK}" "${CMAKE_DL_LIBS}" )

	add_executable( ASCompiler ${ASCOMPILER_SOURCE} )
	set( ASCOMPILER_OUTPUT "${CMAKE_BINARY_DIR}/../SDK/Tools/ASCompiler/" )
	set_target_properties( ASCompiler PROPERTIES ${RO1} ${ASCOMPILER_OUTPUT} ${RO2} ${ASCOMPILER_OUTPUT} ${RO3} ${ASCOMPILER_OUTPUT} ${RO4} ${ASCOMPILER_OUTPUT} )
	set_target_properties( ASCompiler PROPERTIES COMPILE_FLAGS "${ASCPMPILER_DEFINES}" )
	target_link_libraries( ASCompiler "Angelscript" "AngelscriptExt" "zlibstatic" "SHA2" "${NCODEHOOK}" "${CMAKE_DL_LIBS}" )

	set_property( GLOBAL PROPERTY USE_FOLDERS ON )
	set_property( TARGET FOnlineServer FOnline Mapper ASCompiler PROPERTY FOLDER "Binaries")
	set_property( TARGET Acm Angelscript AngelscriptExt fltk GLExt ${NCODEHOOK} png16_static SDL2main SDL2-static SHA2 zlibstatic PROPERTY FOLDER "Libraries")
endif()
