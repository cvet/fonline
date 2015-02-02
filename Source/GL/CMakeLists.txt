cmake_minimum_required( VERSION 2.8 )
project( GL )

file( GLOB GL_SOURCE "*.*" )
add_library( GL ${GL_SOURCE} )
include_directories( "../" )
