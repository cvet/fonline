cmake_minimum_required( VERSION 2.8 )
project( SHA2 )

file( GLOB SHA2_SOURCE "*.*" )
add_library( SHA2 ${SHA2_SOURCE} )
