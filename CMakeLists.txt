cmake_minimum_required(VERSION 3.16.3)
project(FOnlineTest)

set(FO_DEV_NAME "FOTEST")
set(FO_NICE_NAME "FOnlineTest")
set(FO_AUTHOR_NAME "FOnlineDev")
set(FO_GAME_VERSION "0.0.1")
set(FO_ENABLE_3D YES)
set(FO_ANGELSCRIPT_SCRIPTING YES)
set(FO_GEOMETRY HEXAGONAL)
set(FO_APP_ICON "Resources/Radiation.ico")

option(FO_UNIT_TESTS "" ON)
option(FO_OUTPUT_PATH "" ${CMAKE_BINARY_DIR})

include(BuildTools/Init.cmake)
include(BuildTools/Finalize.cmake)
