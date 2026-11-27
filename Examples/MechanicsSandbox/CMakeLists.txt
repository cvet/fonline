cmake_minimum_required(VERSION 3.22)
project(FOnlineMechanicsSandbox)

include(Engine/BuildTools/Init.cmake)

SetOption(FO_MAIN_CONFIG "FOnlineMechanicsSandbox.fomain")
SetOption(FO_DEV_NAME "FOMECH")
SetOption(FO_NICE_NAME "FOnline Mechanics Sandbox")
SetOption(FO_ENABLE_3D OFF)
SetOption(FO_ANGELSCRIPT_SCRIPTING ON)
SetOption(FO_NATIVE_SCRIPTING OFF)
SetOption(FO_MANAGED_SCRIPTING OFF)
SetOption(FO_GEOMETRY HEXAGONAL)
SetOption(FO_MAP_HEX_WIDTH 32)
SetOption(FO_MAP_HEX_HEIGHT 16)
SetOption(FO_MAP_CAMERA_ANGLE 25.6589)

# Restart scenarios stop the server and start it again on the same JSON database directory
SetOption(FO_DATABASE_JSON ON)

SetOption(FO_OUTPUT_PATH ${CMAKE_BINARY_DIR})

StartProjectGeneration()
RegisterProjectOptions()
AddThirdPartyLibraries()
RegisterEngineSources()
SetupCodeGeneration()
BuildCoreLibraries()
BuildApplications()
SetupScriptsAndBaking()
BuildPackages()

if(FO_BUILD_CLIENT AND FO_BUILD_SERVER AND FO_BUILD_BAKER)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_custom_target(CheckSandboxConfig
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/generate_config.py"
            --check
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
        COMMENT "Check generated mechanics sandbox config")

    add_dependencies(BakeResources CheckSandboxConfig)
    add_dependencies(ForceBakeResources CheckSandboxConfig)

    add_custom_target(RunMechanicsChecks
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/run_mechanics_smoke.py"
            $<TARGET_FILE:FOMECH_ServerHeadless>
            $<TARGET_FILE:FOMECH_ClientHeadless>
            "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}"
        # The forced bake is the single writer of FO_OUTPUT_PATH/Baking before any scenario reads it
        DEPENDS FOMECH_ServerHeadless FOMECH_ClientHeadless ForceBakeResources
        SOURCES mechanics-smoke.json
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        VERBATIM
        USES_TERMINAL
        COMMENT "Run FOnline mechanics sandbox recipe checks")
endif()

FinalizeProjectGeneration()
