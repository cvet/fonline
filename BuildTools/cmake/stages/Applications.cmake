cmake_minimum_required(VERSION 3.22)

# === Stage: Applications ===
# Auto-extracted from FinalizeGeneration.cmake by the staged-pipeline refactor.
# Add or override behaviour via AddStageHook(Applications Pre|Post <macro-name>).

# Applications
StatusMessage("Applications:")

if(FO_BUILD_CLIENT)
    if(NOT FO_BUILD_LIBRARY)
        AddExecutableApplication(${FO_DEV_NAME}_Client "${FO_ENGINE_ROOT}/Source/Applications/ClientApp.cpp"
            WIN32
            OUTPUT_DIR ${FO_CLIENT_OUTPUT}
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            OUTPUT_NAME ${FO_DEV_NAME}_Client
            TESTING_APP 0
            LINK_LIBS AppFrontend ClientLib
            EXTRA_SOURCES ${FO_RC_FILE}
            WRITE_BUILD_HASH)

        if(FO_WINDOWS OR FO_LINUX OR FO_MAC)
            AddSharedApplication(${FO_DEV_NAME}_ClientLib "${FO_ENGINE_ROOT}/Source/Applications/ClientLib.cpp"
                OUTPUT_DIR ${FO_CLIENT_OUTPUT}
                OUTPUT_NAME ${FO_DEV_NAME}_ClientLib
                TESTING_APP 0
                LINK_LIBS PRIVATE AppFrontend ClientLib
                EXTRA_PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_CLIENT_OUTPUT}
                NO_PREFIX
                WRITE_BUILD_HASH)

            add_custom_command(TARGET ${FO_DEV_NAME}_ClientLib POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${FO_DEV_NAME}_ClientLib>"
                    "${FO_CLIENT_OUTPUT}/${FO_DEV_NAME}_Client${CMAKE_SHARED_LIBRARY_SUFFIX}"
                COMMENT "Copy client runtime library to host-derived module name")

            AddExecutableApplication(${FO_DEV_NAME}_ClientHeadless "${FO_ENGINE_ROOT}/Source/Applications/ClientApp.cpp"
                OUTPUT_DIR ${FO_CLIENT_OUTPUT}
                WORKING_DIRECTORY ${FO_OUTPUT_PATH}
                OUTPUT_NAME ${FO_DEV_NAME}_ClientHeadless
                TESTING_APP 0
                HEADLESS_APP 1
                LINK_LIBS AppHeadless ClientLib
                WRITE_BUILD_HASH)

            AddSharedApplication(${FO_DEV_NAME}_ClientLibHeadless "${FO_ENGINE_ROOT}/Source/Applications/ClientLib.cpp"
                OUTPUT_DIR ${FO_CLIENT_OUTPUT}
                OUTPUT_NAME ${FO_DEV_NAME}_ClientLibHeadless
                TESTING_APP 0
                HEADLESS_APP 1
                LINK_LIBS PRIVATE AppHeadless ClientLib
                EXTRA_PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_CLIENT_OUTPUT}
                NO_PREFIX
                WRITE_BUILD_HASH)

            add_custom_command(TARGET ${FO_DEV_NAME}_ClientLibHeadless POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${FO_DEV_NAME}_ClientLibHeadless>"
                    "${FO_CLIENT_OUTPUT}/${FO_DEV_NAME}_ClientHeadless${CMAKE_SHARED_LIBRARY_SUFFIX}"
                COMMENT "Copy headless client runtime library to host-derived module name")
        endif()
    else()
        AddSharedApplication(${FO_DEV_NAME}_Client "${FO_ENGINE_ROOT}/Source/Applications/ClientApp.cpp"
            OUTPUT_DIR ${FO_CLIENT_OUTPUT}
            OUTPUT_NAME ${FO_DEV_NAME}_Client
            TESTING_APP 0
            LINK_LIBS AppFrontend ClientLib
            WRITE_BUILD_HASH)
    endif()
endif()

if(FO_BUILD_SERVER)
    AddExecutableApplication(
        ${FO_DEV_NAME}_Server
        "${FO_ENGINE_ROOT}/Source/Applications/ServerApp.cpp"
        WIN32
        OUTPUT_DIR ${FO_SERVER_OUTPUT}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        OUTPUT_NAME ${FO_DEV_NAME}_Server
        TESTING_APP 0
        LINK_LIBS ServerLib ClientLib AppFrontend
        EXTRA_SOURCES ${FO_RC_FILE}
        WRITE_BUILD_HASH)

    AddExecutableApplication(
        ${FO_DEV_NAME}_ServerHeadless
        "${FO_ENGINE_ROOT}/Source/Applications/ServerHeadlessApp.cpp"
        OUTPUT_DIR ${FO_SERVER_OUTPUT}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        OUTPUT_NAME ${FO_DEV_NAME}_ServerHeadless
        TESTING_APP 0
        LINK_LIBS ServerLib ClientLib AppHeadless
        WRITE_BUILD_HASH)

    if(FO_WINDOWS)
        AddExecutableApplication(
            ${FO_DEV_NAME}_ServerService
            "${FO_ENGINE_ROOT}/Source/Applications/ServerServiceApp.cpp"
            OUTPUT_DIR ${FO_SERVER_OUTPUT}
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            OUTPUT_NAME ${FO_DEV_NAME}_ServerService
            TESTING_APP 0
            LINK_LIBS ServerLib ClientLib AppHeadless
            WRITE_BUILD_HASH)
    else()
        AddExecutableApplication(
            ${FO_DEV_NAME}_ServerDaemon
            "${FO_ENGINE_ROOT}/Source/Applications/ServerDaemonApp.cpp"
            OUTPUT_DIR ${FO_SERVER_OUTPUT}
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            OUTPUT_NAME ${FO_DEV_NAME}_ServerDaemon
            TESTING_APP 0
            LINK_LIBS ServerLib ClientLib AppHeadless
            WRITE_BUILD_HASH)
    endif()
endif()

if(FO_BUILD_EDITOR)
    AddExecutableApplication(${FO_DEV_NAME}_Editor "${FO_ENGINE_ROOT}/Source/Applications/EditorApp.cpp"
        WIN32
        OUTPUT_DIR ${FO_EDITOR_OUTPUT}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        OUTPUT_NAME ${FO_DEV_NAME}_Editor
        TESTING_APP 0
        LINK_LIBS AppFrontend EditorLib MapperLib BakerLib ClientLib ServerLib
        EXTRA_SOURCES ${FO_RC_FILE}
        WRITE_BUILD_HASH)
endif()

if(FO_BUILD_MAPPER)
    AddExecutableApplication(${FO_DEV_NAME}_Mapper "${FO_ENGINE_ROOT}/Source/Applications/MapperApp.cpp"
        WIN32
        OUTPUT_DIR ${FO_MAPPER_OUTPUT}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        OUTPUT_NAME ${FO_DEV_NAME}_Mapper
        TESTING_APP 0
        LINK_LIBS AppFrontend MapperLib ClientLib BakerLib
        EXTRA_SOURCES ${FO_RC_FILE}
        WRITE_BUILD_HASH)
endif()

if(FO_BUILD_ASCOMPILER)
    AddExecutableApplication(
        ${FO_DEV_NAME}_ASCompiler
        "${FO_ENGINE_ROOT}/Source/Applications/ASCompilerApp.cpp"
        OUTPUT_DIR ${FO_ASCOMPILER_OUTPUT}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        OUTPUT_NAME ${FO_DEV_NAME}_ASCompiler
        TESTING_APP 0
        LINK_LIBS AppHeadless BakerLib
        DEPENDS ${FO_GEN_DEPENDENCIES}
        WRITE_BUILD_HASH)
endif()

if(FO_MANAGED_SCRIPTING AND FO_BUILD_BAKER_LIB)
    AddExecutableApplication(
        ${FO_DEV_NAME}_ManagedScriptBaker
        "${FO_ENGINE_ROOT}/Source/Applications/ManagedScriptBakerApp.cpp"
        OUTPUT_DIR ${FO_BAKER_OUTPUT}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        OUTPUT_NAME ${FO_DEV_NAME}_ManagedScriptBaker
        TESTING_APP 0
        LINK_LIBS AppHeadless BakerLib
        DEPENDS ${FO_GEN_DEPENDENCIES}
        WRITE_BUILD_HASH)
endif()

if(FO_BUILD_BAKER)
    AddExecutableApplication(${FO_DEV_NAME}_Baker "${FO_ENGINE_ROOT}/Source/Applications/BakerApp.cpp"
        OUTPUT_DIR ${FO_BAKER_OUTPUT}
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        OUTPUT_NAME ${FO_DEV_NAME}_Baker
        TESTING_APP 0
        LINK_LIBS AppHeadless BakerLib
        WRITE_BUILD_HASH)

    if(NOT FO_WEB)
        AddSharedApplication(${FO_DEV_NAME}_BakerLib "${FO_ENGINE_ROOT}/Source/Applications/BakerLib.cpp"
            OUTPUT_DIR ${FO_BAKER_OUTPUT}
            OUTPUT_NAME ${FO_DEV_NAME}_BakerLib
            TESTING_APP 0
            LINK_LIBS PRIVATE BakerLib
            EXTRA_PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FO_BAKER_OUTPUT}
            NO_PREFIX
            WRITE_BUILD_HASH)
    endif()
endif()

if(FO_UNIT_TESTS OR FO_CODE_COVERAGE)
    SetValue(FO_CODE_COVERAGE_BACKEND "")

    if(FO_CODE_COVERAGE)
        if(MSVC)
            SetValue(FO_CODE_COVERAGE_BACKEND "msvc")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            SetValue(FO_CODE_COVERAGE_BACKEND "llvm")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            SetValue(FO_CODE_COVERAGE_BACKEND "gcc")
        else()
            AbortMessage("Code coverage backend is not configured for the selected compiler")
        endif()
    endif()

    macro(SetupTestBuild name)
        SetValue(target ${FO_DEV_NAME}_${name})
        SetValue(testBuildSources
            ${FO_TESTS_SOURCE}
            "${FO_ENGINE_ROOT}/Source/Applications/EditorApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/MapperApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/BakerApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ServerApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ServerServiceApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ServerDaemonApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ServerHeadlessApp.cpp"
            "${FO_ENGINE_ROOT}/Source/Applications/ClientApp.cpp")

        AddExecutableApplication(
            ${target}
            "${FO_ENGINE_ROOT}/Source/Applications/TestingApp.cpp"
            OUTPUT_DIR ${FO_TESTS_OUTPUT}
            WORKING_DIRECTORY ${FO_TESTS_OUTPUT}
            OUTPUT_NAME ${target}
            TESTING_APP 1
            LINK_LIBS BakerLib EditorLib MapperLib ${FO_TESTING_LIBS} ClientLib ServerLib AppHeadless
            DEPENDS ${FO_GEN_DEPENDENCIES}
            EXTRA_SOURCES ${testBuildSources})

        if("${name}" STREQUAL "CodeCoverage")
            SetValue(coverageTool
                ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/codecoverage.py")
            SetValue(coverageToolArgs
                --workspace-root "${CMAKE_CURRENT_SOURCE_DIR}"
                --build-dir "${CMAKE_CURRENT_BINARY_DIR}"
                --binary "$<TARGET_FILE:${target}>"
                --backend "${FO_CODE_COVERAGE_BACKEND}"
                --output-dir "${FO_COVERAGE_OUTPUT}")

            AddCommandTarget(CleanCodeCoverageData
                COMMAND_ARGS COMMAND ${coverageTool} clean ${coverageToolArgs}
                DEPENDS ${target}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Clean code coverage data")

            AddCommandTarget(Run${name}
                COMMAND_ARGS COMMAND ${coverageTool} run ${coverageToolArgs}
                DEPENDS ${target}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Run ${name}")

            AddCommandTarget(GenerateCodeCoverageReport
                COMMAND_ARGS COMMAND ${coverageTool} report ${coverageToolArgs}
                DEPENDS Run${name}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Generate code coverage report")

            AddCommandTarget(AnalyzeCodeCoverage
                COMMAND_ARGS COMMAND ${coverageTool} full ${coverageToolArgs}
                DEPENDS ${target}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Run code coverage and generate report")
        else()
            if(MSVC)
                AddCommandTarget(Run${name}
                    COMMAND_ARGS
                    COMMAND "${CMAKE_COMMAND}"
                        "-DFO_RUN_COMMAND=$<TARGET_FILE:${target}>"
                        "-DFO_RUN_WORKING_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}"
                        "-DFO_RUN_LOG=${CMAKE_CURRENT_BINARY_DIR}/${target}.log"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/cmake/helpers/RunAndLog.cmake"
                    DEPENDS ${target}
                    COMMENT "Run ${name}")
            else()
                AddCommandTarget(Run${name}
                    COMMAND_ARGS COMMAND ${target}
                    DEPENDS ${target}
                    COMMENT "Run ${name}")
            endif()
        endif()
    endmacro()

    if(FO_UNIT_TESTS)
        SetupTestBuild(UnitTests)
    endif()

    if(FO_CODE_COVERAGE)
        SetupTestBuild(CodeCoverage)
    endif()
endif()
