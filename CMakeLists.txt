cmake_minimum_required(VERSION 3.16.3)
project(FOnlineTest)

include(BuildTools/Init.cmake)

SetOption(FO_DEV_NAME "FOTEST")
SetOption(FO_NICE_NAME "FOnlineTest")
SetOption(FO_AUTHOR_NAME "FOnlineDev")
SetOption(FO_GAME_VERSION "0.0.1")
SetOption(FO_ENABLE_3D YES)
SetOption(FO_ANGELSCRIPT_SCRIPTING YES)
SetOption(FO_GEOMETRY HEXAGONAL)
SetOption(FO_APP_ICON "Resources/Radiation.ico")

SetOption(FO_UNIT_TESTS ON)
SetOption(FO_OUTPUT_PATH ${CMAKE_BINARY_DIR})

SetOption(FO_SINGLEPLAYER ${FO_BUILD_SINGLE})

include(BuildTools/StartGeneration.cmake)
include(BuildTools/FinalizeGeneration.cmake)
