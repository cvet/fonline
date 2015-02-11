cmake_minimum_required( VERSION 2.8 )
project( AngelscriptExt )

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}" )
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/sdk/angelscript/include" )
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/sdk/angelscript/source" )
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/../" )
file( GLOB AS_SOURCE "*.*" )
add_library( AngelscriptExt ${AS_SOURCE} )
