# Writes the configured native build hash to HASH_FILE
# Specify exactly one of BUILD_HASH (literal) or BUILD_HASH_INPUT_FILE

if(NOT DEFINED HASH_FILE)
    message(FATAL_ERROR "HASH_FILE not defined")
endif()

if(DEFINED BUILD_HASH AND DEFINED BUILD_HASH_INPUT_FILE)
    message(FATAL_ERROR "Specify only one build hash input")
endif()

if(DEFINED BUILD_HASH_INPUT_FILE)
    if(NOT EXISTS "${BUILD_HASH_INPUT_FILE}" OR IS_DIRECTORY "${BUILD_HASH_INPUT_FILE}")
        message(FATAL_ERROR "BUILD_HASH_INPUT_FILE must name an existing file")
    endif()

    file(READ "${BUILD_HASH_INPUT_FILE}" buildHash)
    if(buildHash STREQUAL "")
        message(FATAL_ERROR "BUILD_HASH_INPUT_FILE is empty")
    endif()
else()
    if(NOT DEFINED BUILD_HASH OR BUILD_HASH STREQUAL "")
        message(FATAL_ERROR "BUILD_HASH not defined or empty")
    endif()
    set(buildHash "${BUILD_HASH}")
endif()

file(WRITE "${HASH_FILE}" "${buildHash}")
