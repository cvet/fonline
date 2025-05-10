cmake_minimum_required(VERSION 3.22)
project(FOnlineTest)

include(BuildTools/Init.cmake)

SetOption(FO_MAIN_CONFIG "BuildTools/FOnlineTest.fomain")
SetOption(FO_DEV_NAME "FOTEST")
SetOption(FO_NICE_NAME "FOnlineTest")
SetOption(FO_ENABLE_3D ON)
SetOption(FO_ANGELSCRIPT_SCRIPTING ON)
SetOption(FO_NATIVE_SCRIPTING OFF)
SetOption(FO_MONO_SCRIPTING OFF)
SetOption(FO_GEOMETRY HEXAGONAL)
SetOption(FO_APP_ICON "Resources/Radiation.ico")
SetOption(FO_MAKE_EXTERNAL_COMMANDS OFF)

SetOption(FO_UNIT_TESTS ON)
SetOption(FO_OUTPUT_PATH ${CMAKE_BINARY_DIR})

include(BuildTools/StartGeneration.cmake)
include(BuildTools/FinalizeGeneration.cmake)
