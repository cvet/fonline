cmake_minimum_required(VERSION 3.22)

# === Stage: Packages ===
# Auto-extracted from FinalizeGeneration.cmake by the staged-pipeline refactor.
# Add or override behaviour via AddStageHook(Packages Pre|Post <macro-name>).

# Packaging
StatusMessage("Packages:")

foreach(package ${FO_PACKAGES})
    StatusMessage("+ Package ${package}")

    SetValue(packageBaseCommands
        ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/package.py"
        -maincfg "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}"
        -buildhash "${FO_BUILD_HASH}"
        -devname "${FO_DEV_NAME}"
        -nicename "${FO_NICE_NAME}")

    AddCommandTarget(MakePackage-${package}
        COMMAND_ARGS COMMAND ${CMAKE_COMMAND} -E rm -rf "${FO_OUTPUT_PATH}/${FO_DEV_NAME}-${package}"
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        COMMENT "Make package ${package}")

    foreach(entry ${Package_${package}_Parts})
        SetValue(packageCommands ${packageBaseCommands})

        StringReplace("," ";" entry ${entry})
        ListGet(entry 0 target)
        ListGet(entry 1 platform)
        ListGet(entry 2 arch)
        ListGet(entry 3 pack)
        ListGet(entry 4 customConfig)
        ListGet(entry 5 binaryOutputPostfix)

        AppendList(packageCommands -target "${target}")
        AppendList(packageCommands -platform "${platform}")
        AppendList(packageCommands -arch "${arch}")
        AppendList(packageCommands -pack "${pack}")

        if(customConfig)
            SetValue(config ${customConfig})
        else()
            SetValue(config ${Package_${package}_Config})
        endif()

        AppendList(packageCommands -config "${config}")

        AppendList(packageCommands -input "${FO_OUTPUT_PATH}")

        if(NOT "${binaryOutputPostfix}" STREQUAL "")
            AppendList(packageCommands -binary-output-postfix "${binaryOutputPostfix}")
        endif()

        AppendList(packageCommands -output "${FO_OUTPUT_PATH}/${FO_DEV_NAME}-${package}")

        StatusMessage("  ${target} for ${platform}-${arch} in ${pack} with ${config} config")
        AddCustomCommand(TARGET MakePackage-${package} POST_BUILD
            COMMAND ${packageCommands})
    endforeach()
endforeach()
