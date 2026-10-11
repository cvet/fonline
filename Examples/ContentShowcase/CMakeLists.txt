cmake_minimum_required(VERSION 3.22)
project(FOnlineContentShowcase)

include(Engine/BuildTools/Init.cmake)

SetOption(FO_MAIN_CONFIG "FOnlineContentShowcase.fomain")
SetOption(FO_DEV_NAME "FOCS")
SetOption(FO_NICE_NAME "FOnline Content Showcase")
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
SetOption(FO_UNIT_TESTS OFF)
SetOption(FO_OUTPUT_PATH ${CMAKE_BINARY_DIR})

StartProjectGeneration()
RegisterProjectOptions()
AddThirdPartyLibraries()
RegisterEngineSources()

function(_focs_post_target_setup)
    if(TARGET FOCS_Client AND TARGET FOCS_BakerLib)
        CopyTargetRuntimeToTarget(FOCS_Client FOCS_BakerLib)
    endif()
endfunction()

AddStageHook(Finalize Pre _focs_post_target_setup)

SetupCodeGeneration()
BuildCoreLibraries()
BuildApplications()
SetupScriptsAndBaking()

if(FO_WINDOWS)
    DefinePackage(Showcase
        CONFIG ShowcaseRelease
        BINARY Client Windows win64 Raw+Zip+Headless
        BINARY Server Windows win64 Raw+Zip+Headless)
elseif(FO_LINUX)
    DefinePackage(Showcase
        CONFIG ShowcaseRelease
        BINARY Client Linux x64 Raw+TarGz+Headless
        BINARY Server Linux x64 Raw+TarGz+Headless)
elseif(FO_WEB)
    DefinePackage(ShowcaseWeb
        CONFIG ShowcaseRelease
        BINARY Client Web wasm Raw+Zip+WebServer)
endif()

BuildPackages()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_custom_target(CheckShowcaseSources
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/verify_showcase.py" --mode source
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
    COMMENT "Validate Content Showcase sources, rights, and budgets")

if(TARGET BakeResources)
    add_dependencies(BakeResources CheckShowcaseSources)
    add_dependencies(ForceBakeResources CheckShowcaseSources)
endif()

if(FO_BUILD_CLIENT AND FO_BUILD_SERVER AND FO_BUILD_BAKER)
    add_custom_target(RunShowcaseChecks
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/run_showcase_smoke.py"
            $<TARGET_FILE:FOCS_ServerHeadless>
            $<TARGET_FILE:FOCS_ClientHeadless>
            "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}"
        DEPENDS FOCS_ServerHeadless FOCS_Client FOCS_ClientHeadless BakeResources
        SOURCES showcase-smoke.json quality/performance-budget.json captures/capture-contract.json
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        VERBATIM
        USES_TERMINAL
        COMMENT "Run FOnline Content Showcase checks")

    add_custom_target(RunShowcaseCapture
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/capture_showcase.py"
            $<TARGET_FILE:FOCS_ServerHeadless>
            $<TARGET_FILE:FOCS_Client>
            "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}"
        DEPENDS FOCS_ServerHeadless FOCS_Client BakeResources
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        VERBATIM
        USES_TERMINAL
        COMMENT "Capture the deterministic Content Showcase frame")
endif()

if(FO_WEB AND FO_BUILD_CLIENT)
    add_custom_target(RunShowcaseWebChecks
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/verify_showcase.py"
            --mode web
            --client $<TARGET_FILE:FOCS_Client>
        DEPENDS FOCS_Client
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        VERBATIM
        USES_TERMINAL
        COMMENT "Validate the Content Showcase Web build")

    if(TARGET MakePackage-ShowcaseWeb)
        add_dependencies(MakePackage-ShowcaseWeb FOCS_Client CheckShowcaseSources)

        add_custom_target(RunShowcaseWebPackageChecks
            COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_showcase.py"
                --mode web-package
                --package-root "${FO_OUTPUT_PATH}/FOCS-ShowcaseWeb"
            DEPENDS MakePackage-ShowcaseWeb
            SOURCES showcase-web-package.json
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            VERBATIM
            USES_TERMINAL
            COMMENT "Validate the packaged Content Showcase Web payload")
    endif()
endif()

FinalizeProjectGeneration()
