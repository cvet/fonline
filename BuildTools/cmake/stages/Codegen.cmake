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

# Forward `FO_NATIVE_SCRIPTS_DIR` so codegen can tell which `///@ Export*`
# tags originate from the user native scripts tree (vs engine source) when
# emitting metadata registration. The native scripting SOURCE files are
# emitted by LF_NativeScriptSynth, not codegen. No-op when native scripting
# is off.
if(FO_NATIVE_SCRIPTING AND FO_NATIVE_SCRIPTS_DIR)
    AppendList(FO_CODEGEN_COMMAND_ARGS -nativescriptsdir "${FO_NATIVE_SCRIPTS_DIR}")
endif()

# Engine configuration macros, emitted into EngineConfig.gen.h instead of cluttering the compiler command
# line. The codegen args file is written with file(WRITE), which does not evaluate generator expressions, so
# resolve every value to a literal here.
# Scope: only value/shape config that is consumed *after* an engine header is included. The feature/backend
# toggles (FO_ENABLE_3D, FO_*_SCRIPTING, FO_*_PARTICLES) and per-config FO_DEBUG stay -D compiler defines — they gate whole
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
	-enginedefine "FO_STRING_INLINE_CAPACITY=${FO_STRING_INLINE_CAPACITY}"
	-enginedefine "FO_NO_EXTRA_ASSERTS=0"
	-enginedefine "FO_USE_NAMESPACE=${foUseNamespaceValue}"
	-enginedefine "FO_NO_TEXTURE_LOOKUP=0"
	-enginedefine "FO_DIRECT_SPRITES_DRAW=0"
	-enginedefine "FO_RENDER_32BIT_INDEX=0")

AppendList(FO_CODEGEN_META_SOURCE
    ${FO_SOURCE_META_FILES})

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

# All native scripting outputs (NativeApi_ContextRpcMethods.h,
# NativeApi.<Target>.cppm, and NativeBindings-<Target>.cpp) are emitted by
# LF_NativeScriptSynth via the NativeApiGeneration custom command below;
# codegen.py does not emit native scripting sources.

SetValue(codegenArgsPath "${CMAKE_CURRENT_BINARY_DIR}/codegen-args.txt")
string(JOIN "\n" codegenArgsContent ${FO_CODEGEN_COMMAND_ARGS})
string(APPEND codegenArgsContent "\n")

if(EXISTS "${codegenArgsPath}")
    file(READ "${codegenArgsPath}" previousCodegenArgsContent)
endif()
if(NOT EXISTS "${codegenArgsPath}" OR NOT codegenArgsContent STREQUAL previousCodegenArgsContent)
    FileWrite("${codegenArgsPath}" "${codegenArgsContent}")
endif()

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
    DEPENDS ${FO_CODEGEN_SCRIPT} ${FO_CODEGEN_META_SOURCE} "${codegenArgsPath}"
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

# Native scripting code emission — LF_NativeScriptSynth (engine C++
# tool, see Engine/Source/Applications/NativeScriptSynthApp.cpp) is
# the SOLE generator of the native scripting surface. codegen.py
# emits nothing for native scripting anymore. The tool produces:
#   - NativeApi_ContextRpcMethods.h / per-target NativeApi.<Target>.cppm
#     — driven by EngineMetadata.
#   - NativeBindings-<Target>.cpp  — the per-role dispatcher,
#     built by scanning the user `.cppm` tree
#     (FO_NATIVE_SCRIPTS_DIR, passed as argv[2]) for
#     `export module NativeScripts.User.<Role>.<Name>;` + the
#     module's `void <Init>(const ModuleInitContext&)` entry.
#
# The CMake target cycle that previously blocked this wiring was
# broken in CoreLibs.cmake by moving the Common dispatcher link
# from CommonLib to the role-specific engine libs.
if(FO_NATIVE_SCRIPTING)
    SetValue(FO_NATIVE_API_GEN_OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeApi_ContextRpcMethods.h"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeApi.Common.cppm"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeApi.Server.cppm"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeApi.Client.cppm"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeApi.Mapper.cppm"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeApi.Baker.cppm"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeBindings-Common.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeBindings-Server.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeBindings-Client.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeBindings-Mapper.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeBindings-Baker.cpp")

    # Collect user `.cppm` module files across all roles so the
    # custom command re-runs when a user module is added / edited /
    # removed (the tool scans the dir at runtime, but CMake needs
    # the file-level dep to know when to invoke it).
    SetValue(FO_NATIVE_API_GEN_DEPS "")
    foreach(role IN ITEMS Common Server Client Mapper Baker)
        if(FO_NATIVE_SCRIPTS_${role}_MODULE_FILES)
            AppendList(FO_NATIVE_API_GEN_DEPS ${FO_NATIVE_SCRIPTS_${role}_MODULE_FILES})
        endif()
    endforeach()

    # argv[2] = native scripts dir (omitted when the project has
    # no user native script tree — the tool then emits empty
    # dispatcher bodies so the symbols still resolve).
    SetValue(FO_NATIVE_SCRIPT_SYNTH_SCRIPTS_ARG "")
    if(FO_NATIVE_SCRIPTS_DIR)
        SetValue(FO_NATIVE_SCRIPT_SYNTH_SCRIPTS_ARG "${FO_NATIVE_SCRIPTS_DIR}")
    endif()

    AddCustomCommand(OUTPUT ${FO_NATIVE_API_GEN_OUTPUT}
        COMMAND $<TARGET_FILE:${FO_DEV_NAME}_NativeScriptSynth>
                "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource"
                "${FO_NATIVE_SCRIPT_SYNTH_SCRIPTS_ARG}"
        DEPENDS ${FO_DEV_NAME}_NativeScriptSynth ${FO_NATIVE_API_GEN_DEPS}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "NativeApi generation (via LF_NativeScriptSynth)")

    AddCommandTarget(NativeApiGeneration
        DEPENDS ${FO_NATIVE_API_GEN_OUTPUT}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    # Don't add to FO_GEN_DEPENDENCIES — that list is consumed by
    # every engine target's `AddDependencies` call, and
    # LF_NativeScriptSynth itself transitively links AppHeadless /
    # CommonLib / NativeScriptSynth which are members of that list.
    # File-level dep tracking via AddCustomCommand's OUTPUT is
    # enough: any target listing NativeApi.<Role>.cppm or
    # NativeBindings-<Role>.cpp as a source automatically waits
    # for this command.
endif()

# The interop shim table is generated here rather than in ThirdParty because it needs the Python
# interpreter this stage resolves, while the archives it reads come from the Managed runtime setup
if(FO_MANAGED_SCRIPTING)
    SetValue(FO_MANAGED_PINVOKE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/generate_pinvoke_table.py")

    # The archives carry target objects, so the reader must be the target toolchain's nm. Most toolchains
    # set CMAKE_NM; the Emscripten one does not, and only its own llvm-nm understands wasm objects
    if(CMAKE_NM)
        SetValue(FO_MANAGED_NM "${CMAKE_NM}")
    elseif(FO_WEB)
        SetValue(FO_MANAGED_NM "$ENV{EMSDK}/upstream/bin/llvm-nm")
    else()
        # MSVC ships no nm, so the search also covers the LLVM the Visual Studio installer places beside
        # the toolset and a standalone LLVM install
        find_program(FO_MANAGED_NM
            NAMES llvm-nm nm
            HINTS
                "$ENV{VCINSTALLDIR}/Tools/Llvm/x64/bin"
                "$ENV{VCINSTALLDIR}/Tools/Llvm/bin"
                "$ENV{ProgramFiles}/LLVM/bin"
                "$ENV{ProgramW6432}/LLVM/bin")
    endif()

    if(NOT FO_MANAGED_NM OR NOT EXISTS "${FO_MANAGED_NM}")
        AbortMessage("Symbol reader for the Managed interop shim table not found. Install LLVM (llvm-nm) or the Visual Studio \"C++ Clang tools for Windows\" component, or set FO_MANAGED_NM")
    endif()

    AddCustomCommand(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/ManagedPInvokeTable.gen.cpp"
        COMMAND ${Python3_EXECUTABLE}
            "${FO_MANAGED_PINVOKE_SCRIPT}"
            --nm "${FO_MANAGED_NM}"
            --output "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/ManagedPInvokeTable.gen.cpp"
            ${FO_MANAGED_PINVOKE_ARGS}
        DEPENDS "${FO_MANAGED_PINVOKE_SCRIPT}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generate Managed interop shim table")

    AddCommandTarget(ManagedPInvokeTable
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/ManagedPInvokeTable.gen.cpp"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    # Ordered against the runtime setup as a target, not by depending on its marker file: a second
    # DEPENDS on that output duplicates the rule and races two dotnet/runtime builds in one tree
    AddDependencies(ManagedPInvokeTable SetupManagedRuntime)
    AppendList(FO_GEN_DEPENDENCIES ManagedPInvokeTable)
endif()
