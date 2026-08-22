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
file(REMOVE_RECURSE "${OUTPUT_DIR}/ManagedRuntime")
file(COPY "${INPUT_DIR}/" DESTINATION "${OUTPUT_DIR}/ManagedRuntime")

file(GLOB runtime_files
    "${INPUT_DIR}/*.dll"
    "${INPUT_DIR}/*.dylib"
    "${INPUT_DIR}/*.so"
    "${INPUT_DIR}/*.so.*"
    "${INPUT_DIR}/bin/*.dll"
    "${INPUT_DIR}/bin/*.dylib"
    "${INPUT_DIR}/bin/*.so"
    "${INPUT_DIR}/bin/*.so.*"
    "${INPUT_DIR}/lib/*.dll"
    "${INPUT_DIR}/lib/*.dylib"
    "${INPUT_DIR}/lib/*.so"
    "${INPUT_DIR}/lib/*.so.*")

foreach(runtime_file ${runtime_files})
    if(NOT IS_DIRECTORY "${runtime_file}")
        get_filename_component(runtime_file_name "${runtime_file}" NAME)
        file(COPY_FILE "${runtime_file}" "${OUTPUT_DIR}/${runtime_file_name}" ONLY_IF_DIFFERENT)
    endif()
endforeach()
