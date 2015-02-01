cmake_minimum_required( VERSION 3.1 )
project( FOnlineProject )

set( CMAKE_CONFIGURATION_TYPES "RelWithDebInfo;Debug;Release;MinSizeRel" CACHE STRING "" FORCE )
set( CMAKE_INSTALL_PREFIX "C:/Games/FOnline" CACHE STRING "" FORCE )

#
# Preprocessor definitions
#

if( MSVC_IDE )
	# Todo: -D_HAS_ITERATOR_DEBUGGING=0 -D_SECURE_SCL=0 -D_HAS_EXCEPTIONS=0
	set( COMPILER_DEFINES "-D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE" )
else()
	set( COMPILER_DEFINES "" )
endif()

set( COMMON_DEFINES "${COMPILER_DEFINES}")
set( SERVER_DEFINES "${COMMON_DEFINES} -DFONLINE_SERVER")
set( CLIENT_DEFINES "${COMMON_DEFINES} -DFONLINE_CLIENT")
set( MAPPER_DEFINES "${COMMON_DEFINES} -DFONLINE_MAPPER")
set( ASCPMPILER_DEFINES "${COMMON_DEFINES} -DFONLINE_SCRIPT_COMPILER")

#
# Sources
#

file( GLOB Acm_SOURCE Acm/*.* )
source_group( Acm FILES ${Acm_SOURCE} )
file( GLOB AngelScript_SOURCE AngelScript/*.* )
source_group( AngelScript FILES ${AngelScript_SOURCE} )
file( GLOB Assimp_SOURCE Assimp/*.* Assimp/Compiler/*.* )
source_group( Assimp FILES ${Assimp_SOURCE} )
file( GLOB event2_SOURCE event2/*.* )
source_group( event2 FILES ${event2_SOURCE} )
file( GLOB fbxsdk_SOURCE fbxsdk/*.* )
source_group( fbxsdk FILES ${fbxsdk_SOURCE} )
file( GLOB FL_SOURCE FL/*.* )
source_group( FL FILES ${FL_SOURCE} )
file( GLOB GL_SOURCE GL/*.* )
source_group( GL FILES ${GL_SOURCE} )
file( GLOB NCodeHook_SOURCE NCodeHook/*.* )
source_group( NCodeHook FILES ${NCodeHook_SOURCE} )
file( GLOB ogg_SOURCE ogg/*.* )
source_group( ogg FILES ${ogg_SOURCE} )
file( GLOB PNG_SOURCE PNG/*.* )
source_group( PNG FILES ${PNG_SOURCE} )
file( GLOB SDL_SOURCE SDL/*.* )
source_group( SDL FILES ${SDL_SOURCE} )
file( GLOB SHA2_SOURCE SHA2/*.c SHA2/*.h )
source_group( SHA2 FILES ${SHA2_SOURCE} )
file( GLOB Theora_SOURCE Theora/*.* )
source_group( Theora FILES ${Theora_SOURCE} )
file( GLOB zlib_SOURCE zlib/*.* )
source_group( zlib FILES ${zlib_SOURCE} )

set( COMMON_SOURCE
	${AngelScript_SOURCE}
	${SHA2_SOURCE}
	${zlib_SOURCE}
	DataFile.cpp DataFile.h
	Debugger.cpp Debugger.h
	Defines.h
	Common.cpp Common.h
	Crypt.cpp Crypt.h
	FileManager.cpp FileManager.h
	FileSystem.cpp FileSystem.h
	IniParser.cpp IniParser.h
	Log.cpp Log.h
	PlatformSpecific.h
	Randomizer.h
	ScriptBind.h
	ScriptFunctions.h
	ScriptPragmas.cpp ScriptPragmas.h
	StdAfx.cpp StdAfx.h
	Text.cpp Text.h
	Timer.cpp Timer.h
	Types.h
)
set( SERVER_CLIENT_MAPPER_SOURCE
	${PNG_SOURCE}
	ConstantsManager.cpp ConstantsManager.h
	CritterManager.h
	CritterType.cpp CritterType.h
	Exception.cpp Exception.h
	Item.cpp Item.h
	ItemManager.cpp ItemManager.h
	LineTracer.h
	MsgFiles.cpp MsgFiles.h
	MsgStr.h
	NetProtocol.h
	Script.cpp Script.h
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
	${GL_SOURCE}
	${SDL_SOURCE}
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
	ResourceClient.h
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
	${event2_SOURCE}
	${FL_SOURCE}
	${NCodeHook_SOURCE}
	AI.cpp AI.h
	Critter.cpp Critter.h
	CritterData.h
	Dialogs.cpp Dialogs.h
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
	Vars.cpp Vars.h
)
set( CLIENT_SOURCE
	${COMMON_SOURCE}
	${SERVER_CLIENT_MAPPER_SOURCE}
	${SERVER_CLIENT_SOURCE}
	${CLIENT_MAPPER_SOURCE}
	${Acm_SOURCE}
	${ogg_SOURCE}
	${Theora_SOURCE}
	Client.cpp Client.h
	ClientInterface.cpp
	Defence.h
	MainClient.cpp
	QuestManager.h
	SoundManager.cpp SoundManager.h
)
set( MAPPER_SOURCE
	${COMMON_SOURCE}
	${SERVER_CLIENT_MAPPER_SOURCE}
	${SERVER_MAPPER_SOURCE}
	${CLIENT_MAPPER_SOURCE}
	MainMapper.cpp
	Mapper.cpp Mapper.h
	ResourceMapper.h
)
set( ASCOMPILER_SOURCE
	${COMMON_SOURCE}
	${SERVER_ASCOMPILER_SOURCE}
	ASCompiler.cpp
	ScriptEngine.h
)

#
# Builds
#

include_directories( "." )
link_directories( "../Lib" )

set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /MP /Z7" )
set( CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_RELEASE} /MT /MP /Z7" )
set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} /MT /MP /Z7" )
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /MP /Z7" )
set( CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /INCREMENTAL:NO" )

set( RO1 RUNTIME_OUTPUT_DIRECTORY_DEBUG )
set( RO2 RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL )
set( RO3 RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO )
set( RO4 RUNTIME_OUTPUT_DIRECTORY_RELEASE )

add_executable( FOnlineServer WIN32 ${SERVER_SOURCE} )
set( SERVER_OUTPUT "${CMAKE_BINARY_DIR}/../sdk/Server/" )
set_target_properties( FOnlineServer PROPERTIES ${RO1} ${SERVER_OUTPUT} ${RO2} ${SERVER_OUTPUT} ${RO3} ${SERVER_OUTPUT} ${RO4} ${SERVER_OUTPUT} )
set_target_properties( FOnlineServer PROPERTIES COMPILE_FLAGS "${SERVER_DEFINES}" )

add_executable( FOnline WIN32 ${CLIENT_SOURCE} )
set( CLIENT_OUTPUT "${CMAKE_BINARY_DIR}/../sdk/Client/" )
set_target_properties( FOnline PROPERTIES ${RO1} ${CLIENT_OUTPUT} ${RO2} ${CLIENT_OUTPUT} ${RO3} ${CLIENT_OUTPUT} ${RO4} ${CLIENT_OUTPUT} )
set_target_properties( FOnline PROPERTIES COMPILE_FLAGS "${CLIENT_DEFINES}" )

add_executable( Mapper WIN32 ${MAPPER_SOURCE} )
set( MAPPER_OUTPUT "${CMAKE_BINARY_DIR}/../sdk/Tools/Mapper/" )
set_target_properties( Mapper PROPERTIES ${RO1} ${MAPPER_OUTPUT} ${RO2} ${MAPPER_OUTPUT} ${RO3} ${MAPPER_OUTPUT} ${RO4} ${MAPPER_OUTPUT} )
set_target_properties( Mapper PROPERTIES COMPILE_FLAGS "${MAPPER_DEFINES}" )

add_executable( ASCompiler ${ASCOMPILER_SOURCE} )
set( ASCOMPILER_OUTPUT "${CMAKE_BINARY_DIR}/../sdk/Tools/ASCompiler/" )
set_target_properties( ASCompiler PROPERTIES ${RO1} ${ASCOMPILER_OUTPUT} ${RO2} ${ASCOMPILER_OUTPUT} ${RO3} ${ASCOMPILER_OUTPUT} ${RO4} ${ASCOMPILER_OUTPUT} )
set_target_properties( ASCompiler PROPERTIES COMPILE_FLAGS "${ASCPMPILER_DEFINES}" )
