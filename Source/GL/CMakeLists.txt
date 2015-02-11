cmake_minimum_required( VERSION 2.8 )
project( GLExt )

file( GLOB GLEXT_SOURCE "*.*" )
add_library( GLExt ${GLEXT_SOURCE} )
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/../" )
