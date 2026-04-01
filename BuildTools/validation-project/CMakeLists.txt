cmake_minimum_required(VERSION 3.22)
project(FOnlineTest)

include(Engine/BuildTools/Init.cmake)

SetOption(FO_MAIN_CONFIG "FOnlineTest.fomain")
SetOption(FO_DEV_NAME "FOTEST")
SetOption(FO_NICE_NAME "FOnlineTest")
SetOption(FO_ENABLE_3D ON)
SetOption(FO_ANGELSCRIPT_SCRIPTING ON)
SetOption(FO_NATIVE_SCRIPTING OFF)
SetOption(FO_MONO_SCRIPTING OFF)
SetOption(FO_GEOMETRY HEXAGONAL)
SetOption(FO_MAP_HEX_WIDTH 32)
SetOption(FO_MAP_HEX_HEIGHT 16)
SetOption(FO_MAP_CAMERA_ANGLE 25.6589)
SetOption(FO_APP_ICON "Engine/Resources/Radiation.ico")

SetOption(FO_UNIT_TESTS ON)
SetOption(FO_OUTPUT_PATH ${CMAKE_BINARY_DIR})

StartProjectGeneration()
FinalizeProjectGeneration()
