cmake_minimum_required( VERSION 2.8 )
project( SHA )

file( GLOB SHA_SOURCE "*.*" )
add_library( SHA ${SHA_SOURCE} )
