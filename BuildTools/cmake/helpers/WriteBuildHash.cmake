# Writes the configured native build hash to HASH_FILE
# Usage: cmake -DHASH_FILE=<path> -DBUILD_HASH=<hash> -P WriteBuildHash.cmake

if(NOT DEFINED HASH_FILE)
    message(FATAL_ERROR "HASH_FILE not defined")
endif()

if(NOT DEFINED BUILD_HASH OR BUILD_HASH STREQUAL "")
    message(FATAL_ERROR "BUILD_HASH not defined or empty")
endif()

file(WRITE "${HASH_FILE}" "${BUILD_HASH}")
