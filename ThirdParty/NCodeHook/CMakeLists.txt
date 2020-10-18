cmake_minimum_required( VERSION 2.8 )
project( NCodeHook )

file( GLOB NCodeHook_SOURCE "*.*" )
file( GLOB DISTORM_SOURCE "distorm3/src/*.*" )
add_library( NCodeHook ${NCodeHook_SOURCE} ${DISTORM_SOURCE} )
source_group( distorm FILES ${DISTORM_SOURCE} )
