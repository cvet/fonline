function(SetupResourceUsageIntegrationTests)
    if(NOT FO_UNIT_TESTS OR CMAKE_CROSSCOMPILING OR NOT TARGET "${FO_DEV_NAME}_Baker")
        return()
    endif()

    get_filename_component(engine_root "${FO_ENGINE_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

    set(angelscript_arg)

    if(FO_ANGELSCRIPT_SCRIPTING)
        set(angelscript_arg --angelscript)
    endif()

    add_custom_target(RunResourceUsageIntegrationTests
        COMMAND "${Python3_EXECUTABLE}"
            "${engine_root}/BuildTools/tests/check_resource_usage_pipeline.py"
            --baker "$<TARGET_FILE:${FO_DEV_NAME}_Baker>"
            --workdir "${CMAKE_CURRENT_BINARY_DIR}/Testing/ResourceUsage"
            ${angelscript_arg}
        DEPENDS ${FO_DEV_NAME}_Baker
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Verify resource usage selection and native archive readback"
        VERBATIM)

    add_dependencies(RunUnitTests RunResourceUsageIntegrationTests)
endfunction()
