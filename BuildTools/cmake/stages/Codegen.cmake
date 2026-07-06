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

# Engine configuration macros, emitted into EngineConfig.gen.h instead of cluttering the compiler command
# line. The codegen args file is written with file(WRITE), which does not evaluate generator expressions, so
# resolve every value to a literal here.
# Scope: only value/shape config that is consumed *after* an engine header is included. The feature/backend
# toggles (FO_ENABLE_3D, FO_*_SCRIPTING) and per-config FO_DEBUG stay -D compiler defines — they gate whole
# files/headers and are evaluated before any engine header is pulled in (see Init.cmake).
if(FO_GEOMETRY STREQUAL "HEXAGONAL")
	SetValue(foGeometryValue 1)
elseif(FO_GEOMETRY STREQUAL "SQUARE")
	SetValue(foGeometryValue 2)
else()
	SetValue(foGeometryValue 0)
endif()

if(FO_DISABLE_NAMESPACE)
	SetValue(foUseNamespaceValue 0)
else()
	SetValue(foUseNamespaceValue 1)
endif()

AppendList(FO_CODEGEN_COMMAND_ARGS
	-enginedefine "FO_MAIN_CONFIG=\"${FO_MAIN_CONFIG}\""
	-enginedefine "FO_GEOMETRY=${foGeometryValue}"
	-enginedefine "FO_MAP_HEX_WIDTH=${FO_MAP_HEX_WIDTH}"
	-enginedefine "FO_MAP_HEX_HEIGHT=${FO_MAP_HEX_HEIGHT}"
	-enginedefine "FO_MAP_CAMERA_ANGLE=${FO_MAP_CAMERA_ANGLE}"
	-enginedefine "FO_EFFECT_SCRIPT_VALUES=${FO_EFFECT_SCRIPT_VALUES}"
	-enginedefine "FO_EFFECT_MAX_PASSES=${FO_EFFECT_MAX_PASSES}"
	-enginedefine "FO_MODEL_LAYERS_COUNT=${FO_MODEL_LAYERS_COUNT}"
	-enginedefine "FO_MODEL_MAX_TEXTURES=${FO_MODEL_MAX_TEXTURES}"
	-enginedefine "FO_MODEL_MAX_BONES=${FO_MODEL_MAX_BONES}"
	-enginedefine "FO_MODEL_BONES_PER_VERTEX=${FO_MODEL_BONES_PER_VERTEX}"
	-enginedefine "FO_NO_EXTRA_ASSERTS=0"
	-enginedefine "FO_USE_NAMESPACE=${foUseNamespaceValue}"
	-enginedefine "FO_NO_TEXTURE_LOOKUP=0"
	-enginedefine "FO_DIRECT_SPRITES_DRAW=0"
	-enginedefine "FO_RENDER_32BIT_INDEX=0")

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
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/EngineConfig.gen.h"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/EmbeddedResources.gen.inc"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/InternalConfig.gen.inc"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-Server.gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-Client.gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-Mapper.gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-ServerStub.gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-ClientStub.gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/MetadataRegistration-MapperStub.gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/GenericCode-Common.gen.cpp")

FileWrite("${CMAKE_CURRENT_BINARY_DIR}/codegen-args.txt" "")

foreach(entry ${FO_CODEGEN_COMMAND_ARGS})
    FileAppend("${CMAKE_CURRENT_BINARY_DIR}/codegen-args.txt" "${entry}\n")
endforeach()

SetValue(FO_CODEGEN_COMMAND
    ${Python3_EXECUTABLE}
    "${FO_CODEGEN_SCRIPT}"
    "@${CMAKE_CURRENT_BINARY_DIR}/codegen-args.txt")
SetValue(codegenTouchCommand
    ${CMAKE_COMMAND}
    -E touch
    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/CodeGenTouch")

AddCustomCommand(OUTPUT ${FO_CODEGEN_OUTPUT}
    COMMAND ${FO_CODEGEN_COMMAND}
    COMMAND ${codegenTouchCommand}
    DEPENDS ${FO_CODEGEN_SCRIPT} ${FO_CODEGEN_META_SOURCE}
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
