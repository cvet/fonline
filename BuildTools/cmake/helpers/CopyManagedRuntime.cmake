cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED INPUT_DIR OR "${INPUT_DIR}" STREQUAL "")
    message(FATAL_ERROR "INPUT_DIR is not specified")
endif()

if(NOT DEFINED OUTPUT_DIR OR "${OUTPUT_DIR}" STREQUAL "")
    message(FATAL_ERROR "OUTPUT_DIR is not specified")
endif()

if(NOT EXISTS "${INPUT_DIR}")
    message(FATAL_ERROR "Managed runtime directory does not exist: ${INPUT_DIR}")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

# Every application runs this as its own post-build step and the ones of a kind share an output
# directory, so without the lock a parallel build deletes the tree while a sibling copies into it
file(LOCK "${OUTPUT_DIR}/ManagedRuntime.lock" GUARD PROCESS TIMEOUT 600)

file(REMOVE_RECURSE "${OUTPUT_DIR}/ManagedRuntime")
file(COPY "${INPUT_DIR}/" DESTINATION "${OUTPUT_DIR}/ManagedRuntime")
