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
    string(REGEX MATCH "test cases:[ ]+[0-9]+ \\|[ ]+[0-9]+ passed \\|[ ]+[0-9]+ skipped" test_summary "${test_output}")
endif()

if("${test_summary}" STREQUAL "")
    message(FATAL_ERROR "Test executable exited successfully without a Catch2 summary. Full output: ${TEST_LOG}")
endif()

message(STATUS "${test_summary}")
message(STATUS "Full test output: ${TEST_LOG}")

if(NOT DEFINED TEST_ISOLATED_TIMEOUT_SECONDS)
    set(TEST_ISOLATED_TIMEOUT_SECONDS 45)
endif()

if(NOT "${TEST_ISOLATED_TIMEOUT_SECONDS}" MATCHES "^[1-9][0-9]*$" OR TEST_ISOLATED_TIMEOUT_SECONDS GREATER 45)
    message(FATAL_ERROR "TEST_ISOLATED_TIMEOUT_SECONDS must be an integer between 1 and 45")
endif()

set(isolated_test_cases
    DataBaseInactivePanicCallbackCanRestore
    DataBaseInactivePanicCallbackCanAwaitFailedRead
    DataBaseInactivePanicDecisionSurvivesConcurrentStart
    DataBaseDirectRestoreDuringConcurrentOplogAppend
    DataBaseRestoreInsideAppliedBackendWritePreservesBothChanges
    DataBaseIndependentRecoveriesPreserveRecordAndJournal
    DataBaseRecoveryOwnerDoesNotBlockCommitHookCycle
    DataBaseRecoveryCommitHookWaitsForIndependentRecovery
    DataBaseRestoreKeepsAppendDuringLogCleanup
    DataBaseOldCleanupPreservesNewRecoveryProgress)

foreach(isolated_test_case IN LISTS isolated_test_cases)
    set(isolated_test_log "${TEST_LOG}.${isolated_test_case}.log")

    execute_process(
        COMMAND "${TEST_EXECUTABLE}" "${isolated_test_case}" --reporter console --colour-mode none
        WORKING_DIRECTORY "${TEST_WORKING_DIRECTORY}"
        TIMEOUT "${TEST_ISOLATED_TIMEOUT_SECONDS}"
        RESULT_VARIABLE isolated_test_result
        OUTPUT_FILE "${isolated_test_log}"
        ERROR_FILE "${isolated_test_log}")

    file(READ "${isolated_test_log}" isolated_test_output)

    if(NOT "${isolated_test_result}" STREQUAL "0")
        message("${isolated_test_output}")
        message(FATAL_ERROR "Isolated test ${isolated_test_case} failed or timed out: ${isolated_test_result}. Full output: ${isolated_test_log}")
    endif()

    string(FIND "${isolated_test_output}" "Filters: \"${isolated_test_case}\"" isolated_filter_position)
    string(REGEX MATCH "All tests passed \\([1-9][0-9]* assertions? in 1 test case\\)" isolated_test_summary "${isolated_test_output}")

    if(isolated_filter_position EQUAL -1 OR "${isolated_test_summary}" STREQUAL "")
        message("${isolated_test_output}")
        message(FATAL_ERROR "Isolated test ${isolated_test_case} did not report one passing test case with assertions. Full output: ${isolated_test_log}")
    endif()

    message(STATUS "${isolated_test_case}: ${isolated_test_summary}")
    message(STATUS "Full isolated test output: ${isolated_test_log}")
endforeach()
