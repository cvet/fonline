cmake_minimum_required(VERSION 3.22)

# === Stage: Finalize ===
# Auto-extracted from FinalizeGeneration.cmake by the staged-pipeline refactor.
# Add or override behaviour via AddStageHook(Finalize Pre|Post <macro-name>).

# Copy ReSharper config
if(MSVC)
    if(FO_RESHARPER_SETTINGS)
        FileCreateLink(
            "${CMAKE_CURRENT_SOURCE_DIR}/${FO_RESHARPER_SETTINGS}"
            "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.sln.DotSettings"
            COPY_ON_ERROR)
    else()
        FileCreateLink(
            "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/ReSharper.sln.DotSettings"
            "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.sln.DotSettings"
            COPY_ON_ERROR)
    endif()
endif()

# Setup targets grouping
SetGlobalProperty(USE_FOLDERS ON)
SetTargetsFolder("Applications" ${FO_APPLICATIONS_GROUP})
SetTargetsFolder("CoreLibs" ${FO_CORE_LIBS_GROUP})
SetTargetsFolder("Commands" ${FO_COMMANDS_GROUP})
SetTargetsFolder(
    "ThirdParty"
    ${FO_ESSENTIALS_LIBS}
    ${FO_COMMON_LIBS}
    ${FO_BAKER_LIBS}
    ${FO_SERVER_LIBS}
    ${FO_CLIENT_LIBS}
    ${FO_RENDER_LIBS}
    ${FO_TESTING_LIBS})
SetTargetsFolder("ThirdParty/Dummy" ${FO_DUMMY_TARGETS})

# Print cached variables
if(FO_VERBOSE_BUILD)
    GetCMakeProperty(FO_CACHE_VARIABLES CACHE_VARIABLES)
    ListSort(FO_CACHE_VARIABLES)

    StatusMessage("Forced variables:")

    foreach(varName ${FO_CACHE_VARIABLES})
        GetCacheProperty(str ${varName} HELPSTRING)
        GetCacheProperty(type ${varName} TYPE)
        StringFind("${str}" "Forced by FOnline" forced)

        if(NOT "${forced}" STREQUAL "-1")
            StatusMessage("- ${varName}: '${${varName}}' type: '${type}'")
        endif()
    endforeach()

    StatusMessage("Default variables:")

    foreach(varName ${FO_CACHE_VARIABLES})
        GetCacheProperty(str ${varName} HELPSTRING)
        GetCacheProperty(type ${varName} TYPE)
        StringFind("${str}" "Forced by FOnline" forced)

        if("${forced}" STREQUAL "-1" AND
            NOT "${type}" STREQUAL "INTERNAL" AND
            NOT "${type}" STREQUAL "STATIC" AND
            NOT "${type}" STREQUAL "UNINITIALIZED")
            StatusMessage("- ${varName}: '${${varName}}' docstring: '${str}' type: '${type}'")
        endif()
    endforeach()
endif()
