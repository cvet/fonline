cmake_minimum_required(VERSION 3.22)
project(FOnlineMinimalMultiplayer)

include(Engine/BuildTools/Init.cmake)

SetOption(FO_MAIN_CONFIG "FOnlineMinimalMultiplayer.fomain")
SetOption(FO_DEV_NAME "FOMM")
SetOption(FO_NICE_NAME "FOnline Minimal Multiplayer")
SetOption(FO_ENABLE_3D OFF)
SetOption(FO_ANGELSCRIPT_SCRIPTING ON)
SetOption(FO_NATIVE_SCRIPTING OFF)
SetOption(FO_MONO_SCRIPTING OFF)
SetOption(FO_SPARK_PARTICLES ON)
SetOption(FO_GEOMETRY HEXAGONAL)
SetOption(FO_MAP_HEX_WIDTH 32)
SetOption(FO_MAP_HEX_HEIGHT 16)
SetOption(FO_MAP_CAMERA_ANGLE 25.6589)
SetOption(FO_APP_ICON "Engine/Resources/Radiation.ico")

SetOption(FO_UNIT_TESTS ON)
SetOption(FO_OUTPUT_PATH ${CMAKE_BINARY_DIR})

StartProjectGeneration()
RegisterProjectOptions()
AddThirdPartyLibraries()
RegisterEngineSources()

function(_fomm_post_target_setup)
    CopyTargetRuntimeToTarget(${FO_DEV_NAME}_Client ${FO_DEV_NAME}_BakerLib)
endfunction()

AddStageHook(Finalize Pre _fomm_post_target_setup)

SetupCodeGeneration()
BuildCoreLibraries()
BuildApplications()
SetupScriptsAndBaking()

if(FO_WINDOWS)
    DefinePackage(Tutorial
        CONFIG TutorialSmoke
        BINARY Client Windows win64 Raw+Zip+Headless
        BINARY Server Windows win64 Raw+Zip+Headless)
elseif(FO_LINUX)
    DefinePackage(Tutorial
        CONFIG TutorialSmoke
        BINARY Client Linux x64 Raw+TarGz+Headless
        BINARY Server Linux x64 Raw+TarGz+Headless)
endif()

BuildPackages()

if(FO_BUILD_CLIENT AND FO_BUILD_SERVER AND FO_BUILD_BAKER)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_custom_target(CheckTutorialConfig
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/generate_config.py"
            --check
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
        COMMENT "Check generated Minimal Multiplayer config")

    add_dependencies(BakeResources CheckTutorialConfig)
    add_dependencies(ForceBakeResources CheckTutorialConfig)

    add_custom_target(RunTutorialChecks
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/run_tutorial_smoke.py"
            $<TARGET_FILE:FOMM_ServerHeadless>
            $<TARGET_FILE:FOMM_ClientHeadless>
            "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}"
            "${FO_OUTPUT_PATH}/Baking/Metadata/Metadata.fometa-server"
            "${FO_OUTPUT_PATH}/Baking/Metadata/Metadata.fometa-client"
        DEPENDS FOMM_ServerHeadless FOMM_Client FOMM_ClientHeadless BakeResources
        SOURCES tutorial-smoke.json
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        VERBATIM
        USES_TERMINAL
        COMMENT "Run FOnline minimal multiplayer tutorial checks")

    if(FO_WINDOWS OR FO_LINUX)
        add_dependencies(MakePackage-Tutorial
            ForceBakeResources
            FOMM_Baker
            FOMM_Client
            FOMM_ClientHeadless
            FOMM_Server
            FOMM_ServerHeadless)

        if(FO_WINDOWS)
            set(tutorialPackagePlatform Windows)
        else()
            set(tutorialPackagePlatform Linux)
        endif()

        add_custom_target(RunTutorialPackageChecks
            COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_tutorial_package.py"
                --package-root "${FO_OUTPUT_PATH}/FOMM-Tutorial"
                --engine-root "${CMAKE_CURRENT_SOURCE_DIR}/Engine"
                --platform ${tutorialPackagePlatform}
            DEPENDS MakePackage-Tutorial RunTutorialChecks
            SOURCES package-smoke.json generate_config.py verify_tutorial_package.py
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            VERBATIM
            USES_TERMINAL
            COMMENT "Build, inventory, and run packaged FOnline tutorial")
    endif()
endif()

FinalizeProjectGeneration()
