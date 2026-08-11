if(NOT DEFINED FO_RUN_COMMAND)
	message(FATAL_ERROR "FO_RUN_COMMAND is not specified")
endif()

if(NOT DEFINED FO_RUN_LOG)
	message(FATAL_ERROR "FO_RUN_LOG is not specified")
endif()

if(NOT DEFINED FO_RUN_WORKING_DIRECTORY)
	set(FO_RUN_WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
endif()

execute_process(
	COMMAND "${FO_RUN_COMMAND}"
	WORKING_DIRECTORY "${FO_RUN_WORKING_DIRECTORY}"
	RESULT_VARIABLE run_result
	OUTPUT_VARIABLE run_output
	ERROR_VARIABLE run_error)

file(WRITE "${FO_RUN_LOG}" "${run_output}")

if(NOT "${run_error}" STREQUAL "")
	file(APPEND "${FO_RUN_LOG}" "${run_error}")
endif()

message(STATUS "Run log written to ${FO_RUN_LOG}")

# The log file stays on the machine that ran the command, so a failure has to carry its own output:
# on CI the exit code alone says nothing about which test or assertion actually failed.
if(NOT run_result EQUAL 0)
	message("${run_output}")

	if(NOT "${run_error}" STREQUAL "")
		message("${run_error}")
	endif()

	message(FATAL_ERROR "Command failed with exit code ${run_result}: ${FO_RUN_COMMAND}")
endif()
