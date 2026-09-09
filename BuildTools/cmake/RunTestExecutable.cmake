foreach(required_var TEST_EXECUTABLE TEST_WORKING_DIRECTORY TEST_LOG)
    if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
        message(FATAL_ERROR "RunTestExecutable requires ${required_var}")
    endif()
endforeach()

get_filename_component(test_log_dir "${TEST_LOG}" DIRECTORY)
file(MAKE_DIRECTORY "${test_log_dir}")

execute_process(
    COMMAND "${TEST_EXECUTABLE}"
    WORKING_DIRECTORY "${TEST_WORKING_DIRECTORY}"
    RESULT_VARIABLE test_result
    OUTPUT_FILE "${TEST_LOG}"
    ERROR_FILE "${TEST_LOG}")

file(READ "${TEST_LOG}" test_output)

if(NOT "${test_result}" STREQUAL "0")
    message("${test_output}")
    message(FATAL_ERROR "Test executable failed with exit code ${test_result}. Full output: ${TEST_LOG}")
endif()

string(REGEX MATCH "All tests passed \\([^\r\n]*\\)" test_summary "${test_output}")

if("${test_summary}" STREQUAL "")
    message(FATAL_ERROR "Test executable exited successfully without a Catch2 summary. Full output: ${TEST_LOG}")
endif()

message(STATUS "${test_summary}")
message(STATUS "Full test output: ${TEST_LOG}")
