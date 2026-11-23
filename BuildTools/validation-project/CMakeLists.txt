cmake_minimum_required(VERSION 3.22)
project(FOnlineTest)

include(Engine/BuildTools/Init.cmake)

SetOption(FO_MAIN_CONFIG "FOnlineTest.fomain")
SetOption(FO_DEV_NAME "FOTEST")
SetOption(FO_NICE_NAME "FOnlineTest")
SetOption(FO_ENABLE_3D ON)
SetOption(FO_SPARK_PARTICLES ON)
SetOption(FO_EFFEKSEER_PARTICLES OFF)
SetOption(FO_ANGELSCRIPT_SCRIPTING ON)
SetOption(FO_NATIVE_SCRIPTING OFF)
SetOption(FO_MANAGED_SCRIPTING OFF)
SetOption(FO_GEOMETRY HEXAGONAL)
SetOption(FO_MAP_HEX_WIDTH 32)
SetOption(FO_MAP_HEX_HEIGHT 16)
SetOption(FO_MAP_CAMERA_ANGLE 25.6589)

SetOption(FO_UNIT_TESTS ON)
SetOption(FO_BUILD_BAKER ON)
SetOptionValues(
    FO_DATA_SOURCE_DAT ON
    FO_DATA_SOURCE_ZIP ON
    FO_DATABASE_JSON ON
    FO_DATABASE_SQLITE ON
    FO_DATABASE_MONGO ON
    FO_IMAGE_FRM ON
    FO_IMAGE_RIX ON
    FO_IMAGE_ART ON
    FO_IMAGE_SPR ON
    FO_IMAGE_ZAR ON
    FO_IMAGE_TIL ON
    FO_IMAGE_MOS ON
    FO_IMAGE_BAM ON)
SetOption(FO_OUTPUT_PATH ${CMAKE_BINARY_DIR})

StartProjectGeneration()
RegisterProjectOptions()
AddThirdPartyLibraries()
AddEngineSources(
    SERVER TestServerExtension.cpp)
RegisterEngineSources()
SetupCodeGeneration()
BuildCoreLibraries()
BuildApplications()
SetupScriptsAndBaking()
BuildPackages()

if(FO_UNIT_TESTS AND FO_BUILD_BAKER AND NOT FO_HEADLESS_ONLY AND
        NOT FO_WEB AND NOT FO_ANDROID AND NOT FO_IOS)
    AddExecutableApplication(
        ${FO_DEV_NAME}_FrameUniformProbe
        "${CMAKE_CURRENT_SOURCE_DIR}/FrameUniformProbe.cpp"
        OUTPUT_DIR "${FO_OUTPUT_PATH}/Binaries/FrameUniformProbe"
        WORKING_DIRECTORY "${FO_OUTPUT_PATH}"
        OUTPUT_NAME ${FO_DEV_NAME}_FrameUniformProbe
        TESTING_APP 1
        LINK_LIBS BakerLib AppFrontend Catch2
        DEPENDS ${FO_GEN_DEPENDENCIES})
    TargetIncludeDirectories(${FO_DEV_NAME}_FrameUniformProbe PRIVATE
        "${FO_ENGINE_ROOT}/Source/Tests")
    set_target_properties(${FO_DEV_NAME}_FrameUniformProbe PROPERTIES EXCLUDE_FROM_ALL TRUE)

    AddCommandTarget(RunFrameUniformProbe
        COMMAND_ARGS COMMAND $<TARGET_FILE:${FO_DEV_NAME}_FrameUniformProbe>
            --ApplyConfig "${CMAKE_CURRENT_SOURCE_DIR}/FrameUniformProbe.fomain"
            --Common.NonInteractive True
            --Common.UserWritablePath "${FO_OUTPUT_PATH}/Testing/FrameUniformProbe"
        DEPENDS ${FO_DEV_NAME}_FrameUniformProbe
        WORKING_DIRECTORY "${FO_OUTPUT_PATH}"
        COMMENT "Verify frame uniforms across repeated Vulkan draws")
endif()

FinalizeProjectGeneration()
