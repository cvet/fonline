cmake_minimum_required(VERSION 3.22)

# === Stage: ScriptsAndBaking ===
# Custom targets for AngelScript / Managed script compilation and resource baking.
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
SetValue(compileManagedScripts "")
SetValue(foASCompilerCommand "${FO_DEV_NAME}_ASCompiler")
SetValue(foBakerCommand "${FO_DEV_NAME}_Baker")
SetValue(foASCompilerDependency "")
SetValue(foBakerDependency "")

if(TARGET ${FO_DEV_NAME}_ASCompiler)
    SetValue(foASCompilerCommand "$<TARGET_FILE:${FO_DEV_NAME}_ASCompiler>")
    SetValue(foASCompilerDependency ${FO_DEV_NAME}_ASCompiler)
endif()
if(TARGET ${FO_DEV_NAME}_Baker)
    SetValue(foBakerCommand "$<TARGET_FILE:${FO_DEV_NAME}_Baker>")
    SetValue(foBakerDependency ${FO_DEV_NAME}_Baker)
endif()

SetValue(foManagedScriptBakerCommand "${FO_DEV_NAME}_ManagedScriptBaker")
SetValue(foManagedScriptBakerDependency "")

if(TARGET ${FO_DEV_NAME}_ManagedScriptBaker)
    SetValue(foManagedScriptBakerCommand "$<TARGET_FILE:${FO_DEV_NAME}_ManagedScriptBaker>")
    SetValue(foManagedScriptBakerDependency ${FO_DEV_NAME}_ManagedScriptBaker)
endif()

if(FO_NATIVE_SCRIPTING OR FO_ANGELSCRIPT_SCRIPTING OR FO_MANAGED_SCRIPTING)
    # Compile AngelScript scripts
    if(FO_ANGELSCRIPT_SCRIPTING)
        SetValue(compileASScripts ${foASCompilerCommand} ${foMainConfigArgs})

        AddCommandTarget(CompileAngelScript
            COMMAND_ARGS COMMAND ${compileASScripts}
            DEPENDS ForceCodeGeneration ${foASCompilerDependency}
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            COMMENT "Compile AngelScript scripts")
    endif()

    # Generate and bake Managed scripts (metadata + C# API generation + assembly compilation).
    # Runs the standalone ManagedScriptBaker app, so the managed project environment can be
    # regenerated without a full resource bake (usable as a pre-build / manual task step).
    if(FO_MANAGED_SCRIPTING)
        SetValue(compileManagedScripts ${foManagedScriptBakerCommand} ${foMainConfigArgs})

        AddCommandTarget(CompileManagedScripts
            COMMAND_ARGS COMMAND ${compileManagedScripts}
            DEPENDS ForceCodeGeneration ${foManagedScriptBakerDependency}
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            COMMENT "Generate and bake Managed scripts")
    endif()
endif()

# Baking
AddBakingTarget(BakeResources)
AddBakingTarget(ForceBakeResources FORCE)
