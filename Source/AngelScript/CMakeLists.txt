cmake_minimum_required( VERSION 2.8 )
project( AngelscriptExt )

include_directories( "sdk/angelscript/include" )
include_directories( "sdk/angelscript/source" )
include_directories( "../" )
file( GLOB AS_SOURCE "*.*" )
add_library( AngelscriptExt ${AS_SOURCE} )
