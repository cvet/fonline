cmake_minimum_required( VERSION 2.8 )
project( GLExt )


if( NOT ANDROID )
	file( GLOB GLEXT_SOURCE "*.*" )
else()
	set( GLEXT_SOURCE glu_stuff.cpp glu_stuff.h )
endif()

add_library( GLExt ${GLEXT_SOURCE} )
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/../" )
