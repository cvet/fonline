cmake_minimum_required(VERSION 3.22)

# === Stage: ScriptsAndBaking ===
# Custom targets for AngelScript / Mono script compilation and resource baking.
# Add or override behaviour via AddStageHook(ScriptsAndBaking Pre|Post <macro-name>).

function(AddBakingTarget target)
    set(options FORCE)
    set(oneValueArgs SUB_CONFIG COMMENT)
    set(multiValueArgs)
    ParseArguments(BAKING_TARGET "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(BAKING_TARGET_UNPARSED_ARGUMENTS)
        AbortMessage("AddBakingTarget ${target} got unexpected arguments: ${BAKING_TARGET_UNPARSED_ARGUMENTS}")
    endif()
    if(BAKING_TARGET_KEYWORDS_MISSING_VALUES)
        AbortMessage("AddBakingTarget ${target} expects values for: ${BAKING_TARGET_KEYWORDS_MISSING_VALUES}")
    endif()

    if(NOT DEFINED BAKING_TARGET_SUB_CONFIG)
        set(BAKING_TARGET_SUB_CONFIG "NONE")
    endif()
    if(NOT DEFINED BAKING_TARGET_COMMENT)
        set(BAKING_TARGET_COMMENT "Bake resources")
    endif()

    if(BAKING_TARGET_FORCE)
        set(forceBaking "True")
    else()
        set(forceBaking "False")
    endif()

    SetValue(bakeResources
        "${FO_DEV_NAME}_Baker"
        -ApplyConfig "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}"
        -ApplySubConfig "${BAKING_TARGET_SUB_CONFIG}")
    SetValue(resourceBuildHashCommand
        ${CMAKE_COMMAND}
        -DHASH_FILE="${FO_OUTPUT_PATH}/Baking/Resources.build-hash"
        -DGIT_ROOT="${FO_GIT_ROOT}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/cmake/helpers/WriteBuildHash.cmake")

    AddCommandTarget(${target}
        COMMAND_ARGS
        COMMAND ${bakeResources} -ForceBaking ${forceBaking}
        COMMAND ${resourceBuildHashCommand}
        DEPENDS ForceCodeGeneration
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        COMMENT "${BAKING_TARGET_COMMENT}")
endfunction()

# Scripts compilation
SetValue(foMainConfigArgs -ApplyConfig "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}" -ApplySubConfig "NONE")
SetValue(compileASScripts "")
SetValue(compileMonoScripts "")

if(FO_NATIVE_SCRIPTING OR FO_ANGELSCRIPT_SCRIPTING OR FO_MONO_SCRIPTING)
    # Compile AngelScript scripts
    if(FO_ANGELSCRIPT_SCRIPTING)
        SetValue(compileASScripts ${FO_DEV_NAME}_ASCompiler ${foMainConfigArgs})

        AddCommandTarget(CompileAngelScript
            COMMAND_ARGS COMMAND ${compileASScripts}
            DEPENDS ForceCodeGeneration
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            COMMENT "Compile AngelScript scripts")
    endif()

    # Compile Mono scripts
    if(FO_MONO_SCRIPTING)
        SetValue(monoCompileCommands "")

        foreach(entry ${FO_MONO_ASSEMBLIES})
            AppendList(monoCompileCommands -assembly ${entry})
        endforeach()

        SetValue(compileMonoScripts
            ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/compile-mono-scripts.py"
            ${monoCompileCommands})

        AddCommandTarget(CompileMonoScripts
            COMMAND_ARGS COMMAND ${compileMonoScripts}
            SOURCES ${FO_MONO_SOURCE}
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            COMMENT "Compile Mono scripts")
    endif()
endif()

# Baking
AddBakingTarget(BakeResources)
AddBakingTarget(ForceBakeResources FORCE)
