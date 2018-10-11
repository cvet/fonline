cmake_minimum_required( VERSION 2.8 )
project( ipc )

file( GLOB IPC_SOURCE "src/*.h" "src/*.c" "cpp/*.h" "cpp/*.cpp" "common/*.h" "common/*.c" )
include_directories( "src" )
include_directories( "common" )
add_library( ipc ${IPC_SOURCE} )
