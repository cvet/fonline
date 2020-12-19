cmake_minimum_required( VERSION 2.8.12 )
project( Acm )

file( GLOB ACM_SOURCE "*.*" )
add_library( Acm ${ACM_SOURCE} )
