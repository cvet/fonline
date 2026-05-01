cmake_minimum_required(VERSION 3.22)

# === Stage: ScriptsAndBaking ===
# Custom targets for AngelScript / Mono script compilation and resource baking.
# Add or override behaviour via AddStageHook(ScriptsAndBaking Pre|Post <macro-name>).

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
SetValue(bakeResources "${FO_DEV_NAME}_Baker" ${foMainConfigArgs})
SetValue(resourceBuildHashCommand
    ${CMAKE_COMMAND}
    -DHASH_FILE="${FO_OUTPUT_PATH}/Baking/Resources.build-hash"
    -DGIT_ROOT="${FO_GIT_ROOT}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/cmake/WriteBuildHash.cmake")

AddCommandTarget(BakeResources
    COMMAND_ARGS
    COMMAND ${bakeResources} -ForceBaking False
    COMMAND ${resourceBuildHashCommand}
    DEPENDS ForceCodeGeneration
    WORKING_DIRECTORY ${FO_OUTPUT_PATH}
    COMMENT "Bake resources")

AddCommandTarget(ForceBakeResources
    COMMAND_ARGS
    COMMAND ${bakeResources} -ForceBaking True
    COMMAND ${resourceBuildHashCommand}
    DEPENDS ForceCodeGeneration
    WORKING_DIRECTORY ${FO_OUTPUT_PATH}
    COMMENT "Bake resources")
