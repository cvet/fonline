cmake_minimum_required(VERSION 3.22)

# === Stage: Codegen ===
# Auto-extracted from FinalizeGeneration.cmake by the staged-pipeline refactor.
# Add or override behaviour via AddStageHook(Codegen Pre|Post <macro-name>).

# Code generation
IncludeFile(FindPython3)
RequirePackage(Python3 REQUIRED COMPONENTS Interpreter)

AppendList(FO_CODEGEN_COMMAND_ARGS -maincfg "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}")
AppendList(FO_CODEGEN_COMMAND_ARGS -buildhash "${FO_BUILD_HASH}")
AppendList(FO_CODEGEN_COMMAND_ARGS -genoutput "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource")
AppendList(FO_CODEGEN_COMMAND_ARGS -devname "${FO_DEV_NAME}")
AppendList(FO_CODEGEN_COMMAND_ARGS -nicename "${FO_NICE_NAME}")
AppendList(FO_CODEGEN_COMMAND_ARGS -embedded "${FO_EMBEDDED_DATA_CAPACITY}")
AppendList(FO_CODEGEN_COMMAND_ARGS -internalcfg "${FO_INTERNAL_CONFIG_CAPACITY}")

AppendList(FO_CODEGEN_META_SOURCE
    ${FO_SOURCE_META_FILES}
    ${FO_MONO_SOURCE})

foreach(entry ${FO_CODEGEN_META_SOURCE})
    AppendList(FO_CODEGEN_COMMAND_ARGS -meta ${entry})
endforeach()

foreach(entry ${FO_ADDED_COMMON_HEADERS})
    AppendList(FO_CODEGEN_COMMAND_ARGS -commonheader ${entry})
endforeach()

AppendList(FO_CODEGEN_OUTPUT
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/CodeGenTouch"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/Version-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/EmbeddedResources-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/InternalConfig-Include.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-Server.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-Client.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-Mapper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-ServerStub.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-ClientStub.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-MapperStub.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/GenericCode-Common.cpp")

FileWrite("${CMAKE_CURRENT_BINARY_DIR}/codegen-args.txt" "")

foreach(entry ${FO_CODEGEN_COMMAND_ARGS})
    FileAppend("${CMAKE_CURRENT_BINARY_DIR}/codegen-args.txt" "${entry}\n")
endforeach()

SetValue(FO_CODEGEN_COMMAND
    ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/codegen.py"
    "@${CMAKE_CURRENT_BINARY_DIR}/codegen-args.txt")
SetValue(codegenTouchCommand
    ${CMAKE_COMMAND}
    -E touch
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/CodeGenTouch")

AddCustomCommand(OUTPUT ${FO_CODEGEN_OUTPUT}
    COMMAND ${FO_CODEGEN_COMMAND}
    COMMAND ${codegenTouchCommand}
    DEPENDS ${FO_CODEGEN_META_SOURCE}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Code generation")

AddCommandTarget(CodeGeneration
    DEPENDS ${FO_CODEGEN_OUTPUT}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
AppendList(FO_GEN_DEPENDENCIES CodeGeneration)

AddCommandTarget(ForceCodeGeneration
    COMMAND_ARGS
    COMMAND ${FO_CODEGEN_COMMAND}
    COMMAND ${codegenTouchCommand}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
