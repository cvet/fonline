cmake_minimum_required(VERSION 3.22)
project(FOnlinePackagingMatrix)

include(Engine/BuildTools/Init.cmake)

SetOption(FO_MAIN_CONFIG "FOnlinePackagingMatrix.fomain")
SetOption(FO_DEV_NAME "FOPKG")
SetOption(FO_NICE_NAME "FOnline Packaging Matrix")
SetOption(FO_ENABLE_3D OFF)
SetOption(FO_ANGELSCRIPT_SCRIPTING ON)
SetOption(FO_NATIVE_SCRIPTING OFF)
SetOption(FO_MONO_SCRIPTING OFF)
SetOption(FO_SPARK_PARTICLES OFF)
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
SetupCodeGeneration()
BuildCoreLibraries()
BuildApplications()
SetupScriptsAndBaking()

if(FO_WINDOWS)
    DefinePackage(PackageSmoke
        CONFIG PackageSmoke
        BINARY Client Windows win64 Raw+Zip+Headless
        BINARY Server Windows win64 Raw+Zip+Headless+Service)
elseif(FO_LINUX)
    DefinePackage(PackageSmoke
        CONFIG PackageSmoke
        BINARY Client Linux x64 Raw+TarGz+Headless
        BINARY Server Linux x64 Raw+TarGz+Headless+Daemon)
endif()

BuildPackages()

if(FO_WINDOWS OR FO_LINUX)
    add_custom_target(CheckPackagingMatrixConfig
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/generate_config.py"
            --check
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
        COMMENT "Check generated packaging-matrix config")

    add_dependencies(ForceBakeResources CheckPackagingMatrixConfig)
    add_dependencies(MakePackage-PackageSmoke
        ForceBakeResources
        FOPKG_Baker
        FOPKG_Client
        FOPKG_ClientHeadless
        FOPKG_Server
        FOPKG_ServerHeadless)

    if(FO_WINDOWS)
        add_dependencies(MakePackage-PackageSmoke FOPKG_ServerService)
        set(packageSmokePlatform Windows)
    else()
        add_dependencies(MakePackage-PackageSmoke FOPKG_ServerDaemon)
        set(packageSmokePlatform Linux)
    endif()

    add_custom_target(RunPackagingChecks
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/verify_package.py"
            --package-root "${FO_OUTPUT_PATH}/FOPKG-PackageSmoke"
            --engine-root "${CMAKE_CURRENT_SOURCE_DIR}/Engine"
            --platform ${packageSmokePlatform}
        DEPENDS MakePackage-PackageSmoke
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        VERBATIM
        USES_TERMINAL
        COMMENT "Build, inventory, and run packaged client/server payloads")
endif()

FinalizeProjectGeneration()
