cmake_minimum_required(VERSION 3.22)

# === Stage: ScriptsAndBaking ===
# Custom targets for AngelScript / Managed script compilation and resource baking.
# Add or override behaviour via AddStageHook(ScriptsAndBaking Pre|Post <macro-name>).

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

# Managed scripting environment shared by the managed script baker and resource baking
if(FO_MANAGED_SCRIPTING AND NOT FO_MANAGED_ASSEMBLIES)
    AppendList(FO_MANAGED_ASSEMBLIES FOnline)
endif()
string(REPLACE ";" "~" foManagedAssembliesEnv "${FO_MANAGED_ASSEMBLIES}")
string(REPLACE ";" "~" foManagedReferencesEnv "${FO_MANAGED_REFERENCES}")
string(REPLACE ";" "~" foManagedSourceEnv "${FO_MANAGED_SOURCE}")
SetValue(foManagedProjectDir "${FO_OUTPUT_PATH}/Scripts/Managed")
SetValue(foManagedScriptBakerCommand "${FO_DEV_NAME}_ManagedScriptBaker")
SetValue(foManagedScriptBakerDependency "")

if(TARGET ${FO_DEV_NAME}_ManagedScriptBaker)
    SetValue(foManagedScriptBakerCommand "$<TARGET_FILE:${FO_DEV_NAME}_ManagedScriptBaker>")
    SetValue(foManagedScriptBakerDependency ${FO_DEV_NAME}_ManagedScriptBaker)
endif()

SetValue(foManagedBakingEnv
    ${CMAKE_COMMAND} -E env
    "FO_MANAGED_ASSEMBLIES=${foManagedAssembliesEnv}"
    "FO_MANAGED_REFERENCES=${foManagedReferencesEnv}"
    "FO_MANAGED_SOURCE=${foManagedSourceEnv}"
    "FO_MANAGED_MSBUILD=${FO_MANAGED_MSBUILD}"
    "FO_MANAGED_TARGET_FRAMEWORK=${FO_MANAGED_TARGET_FRAMEWORK}"
    "FO_MANAGED_PROJECT_NAME=${FO_NICE_NAME}"
    "FO_MANAGED_PROJECT_DIR=${foManagedProjectDir}")

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
        SetValue(compileManagedScripts ${foManagedBakingEnv} ${foManagedScriptBakerCommand} ${foMainConfigArgs})

        AddCommandTarget(CompileManagedScripts
            COMMAND_ARGS COMMAND ${compileManagedScripts}
            DEPENDS ForceCodeGeneration ${foManagedScriptBakerDependency}
            SOURCES ${FO_MANAGED_SOURCE_FILES}
            WORKING_DIRECTORY ${FO_OUTPUT_PATH}
            COMMENT "Generate and bake Managed scripts")
    endif()
endif()

# Baking
if(FO_MANAGED_SCRIPTING)
    SetValue(bakeResources ${foManagedBakingEnv} ${foBakerCommand} ${foMainConfigArgs})
else()
    SetValue(bakeResources ${foBakerCommand} ${foMainConfigArgs})
endif()
SetValue(resourceBuildHashCommand
    ${CMAKE_COMMAND}
    -DHASH_FILE="${FO_OUTPUT_PATH}/Baking/Resources.build-hash"
    -DGIT_ROOT="${FO_GIT_ROOT}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/cmake/helpers/WriteBuildHash.cmake")

AddCommandTarget(BakeResources
    COMMAND_ARGS
    COMMAND ${bakeResources} -ForceBaking False
    COMMAND ${resourceBuildHashCommand}
    DEPENDS ForceCodeGeneration ${foBakerDependency}
    WORKING_DIRECTORY ${FO_OUTPUT_PATH}
    COMMENT "Bake resources")

AddCommandTarget(ForceBakeResources
    COMMAND_ARGS
    COMMAND ${bakeResources} -ForceBaking True
    COMMAND ${resourceBuildHashCommand}
    DEPENDS ForceCodeGeneration ${foBakerDependency}
    WORKING_DIRECTORY ${FO_OUTPUT_PATH}
    COMMENT "Bake resources")
