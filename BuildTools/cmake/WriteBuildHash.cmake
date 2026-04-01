# Evaluates git rev-parse HEAD at build time and writes the hash to HASH_FILE.
# Usage: cmake -DHASH_FILE=<path> -DGIT_ROOT=<path> -P WriteBuildHash.cmake

if(NOT DEFINED HASH_FILE)
    message(FATAL_ERROR "HASH_FILE not defined")
endif()

if(NOT DEFINED GIT_ROOT)
    message(FATAL_ERROR "GIT_ROOT not defined")
endif()

execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY "${GIT_ROOT}"
    RESULT_VARIABLE git_result
    OUTPUT_VARIABLE git_hash
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

if(NOT git_result STREQUAL "0")
    string(RANDOM LENGTH 40 ALPHABET "0123456789abcdef" git_hash)
    string(APPEND git_hash "-random")
endif()

file(WRITE "${HASH_FILE}" "${git_hash}")
