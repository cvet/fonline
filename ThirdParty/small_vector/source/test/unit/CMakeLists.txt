set (GCH_SMALL_VECTOR_SOURCE_TEST_UNIT_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "")

if (GCH_SMALL_VECTOR_TEST_ENABLE_COVERAGE_TARGET)
  set (COVERAGE_TARGET_HEADER ${CMAKE_CURRENT_BINARY_DIR}/coverage-common.hpp)
  set (COVERAGE_TARGET_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/coverage-main.cpp)

  set (COVERAGE_TARGET_HEADER_CONTENT [=[
#include "test_common.hpp"
#include "test_types.hpp"
#include "test_allocators.hpp"

#include <array>
#include <memory>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>
]=])

  if (EXISTS ${COVERAGE_TARGET_HEADER})
    file (READ ${COVERAGE_TARGET_HEADER} _CURRENT_HEADER_CONTENT)
  else ()
    set (_CURRENT_HEADER_CONTENT "")
  endif ()

  if (NOT COVERAGE_TARGET_HEADER_CONTENT STREQUAL _CURRENT_HEADER_CONTENT)
    file (WRITE ${COVERAGE_TARGET_HEADER} "${COVERAGE_TARGET_HEADER_CONTENT}")
  endif ()
endif ()

add_library (small_vector.test.unit.unit_test_common INTERFACE)

target_include_directories (
  small_vector.test.unit.unit_test_common
  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries (
  small_vector.test.unit.unit_test_common
  INTERFACE
    small_vector.test.test_common
)

if (GCH_SMALL_VECTOR_TEST_DISABLE_EXCEPTION_SAFETY_TESTING)
  target_compile_definitions (
    small_vector.test.unit.unit_test_common
    INTERFACE
      GCH_SMALL_VECTOR_TEST_DISABLE_EXCEPTION_SAFETY_TESTING
  )
endif ()

foreach (version ${GCH_SMALL_VECTOR_TEST_STANDARD_VERSIONS})
  set (_TARGET_NAME small_vector.test.unit.main.c++${version})
  add_library (${_TARGET_NAME} OBJECT)
  target_sources (${_TARGET_NAME} PRIVATE main.cpp)
  target_link_libraries (${_TARGET_NAME} PUBLIC small_vector.test.unit.unit_test_common)
  set_target_properties (
    ${_TARGET_NAME}
    PROPERTIES
    CXX_STANDARD
      ${version}
    CXX_STANDARD_REQUIRED
      NO
    CXX_EXTENSIONS
      NO
  )

  if (GCH_SMALL_VECTOR_TEST_ENABLE_PCH)
    target_precompile_headers (
      ${_TARGET_NAME}
      PUBLIC
        [["gch/small_vector.hpp"]]
        [["test_types.hpp"]]
        [["test_allocators.hpp"]]
    )
  endif ()

  if (GCH_SMALL_VECTOR_TEST_ENABLE_COVERAGE_TARGET)
    add_executable (small_vector.test.coverage.c++${version} ${COVERAGE_TARGET_SOURCE})

    target_link_libraries (
      small_vector.test.coverage.c++${version}
      PRIVATE
        small_vector.test.unit.unit_test_common
    )

    set_target_properties (
      small_vector.test.coverage.c++${version}
      PROPERTIES
      CXX_STANDARD
        ${version}
      CXX_STANDARD_REQUIRED
        NO
      CXX_EXTENSIONS
        NO
    )

    if (GCH_SMALL_VECTOR_TEST_ENABLE_PCH)
      set_target_properties (
        small_vector.test.coverage.c++${version}
        PROPERTIES
        PRECOMPILE_HEADERS_REUSE_FROM
          small_vector.test.unit.main.c++${version}
      )
    else ()
      set_target_properties (
        small_vector.test.coverage.c++${version}
        PROPERTIES
        DISABLE_PRECOMPILE_HEADERS
          ON
      )
    endif ()
  endif ()
endforeach ()

# Check if constexpr small_vector is available.
if (NOT DEFINED GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR)
  file (WRITE ${CMAKE_CURRENT_BINARY_DIR}/test.cpp [=[
  #include "gch/small_vector.hpp"
  constexpr int test (void) { gch::small_vector<int, 2> v { 2, 1, 0 }; return v.back (); }
  ]=])

  set (
    _TEST_INCLUDE_DIRS
    ${GCH_SMALL_VECTOR_SOURCE_INCLUDE_DIR}
    ${GCH_SMALL_VECTOR_SOURCE_TEST_DIR}
    ${GCH_SMALL_VECTOR_SOURCE_TEST_UNIT_DIR}
  )

  message (CHECK_START "Checking if the compiler supports constexpr gch::small_vector")

  try_compile (
    GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR
    ${CMAKE_CURRENT_BINARY_DIR}
    SOURCES
      ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    CMAKE_FLAGS
      "-DINCLUDE_DIRECTORIES=${_TEST_INCLUDE_DIRS}"
    COMPILE_DEFINITIONS
      -DGCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR
      -DGCH_SMALL_VECTOR_TEST_FILE=${CMAKE_CURRENT_BINARY_DIR}/test.cpp
    OUTPUT_VARIABLE
      GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR_OUTPUT
    CXX_STANDARD
      20
    CXX_STANDARD_REQUIRED
      OFF
    CXX_EXTENSIONS
      OFF
  )

  if (GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR)
    message (CHECK_PASS "Success")

    set (GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR 1 CACHE INTERNAL "")
    file (
      APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
      "Test of GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR succeeded with the following output:\n"
      "${GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR_OUTPUT}\n"
    )
  else ()
    message (CHECK_PASS "Failed")

    set (GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR "" CACHE INTERNAL "")
    file (
      APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
      "Test of GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR failed with the following output:\n"
      "${GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR_OUTPUT}\n"
    )
  endif ()
endif ()

function (add_small_vector_unit_tests)

  set (_OPTIONS
       COMPILATION_TEST
       EXPECT_FAIL
       NO_CONSTEXPR
       NO_EXCEPTIONS_DISABLED
       ONLY_CONSTEXPR
       ONLY_EXCEPTIONS_DISABLED
       )

  set (_SINGLE_VALUE_ARGS
       MINIMUM_VERSION
       SUFFIX
       )

  set (_MULTI_VALUE_ARGS
       COMPILE_DEFINITIONS
       COMPILE_OPTIONS
       FILES
       LINK_LIBRARIES
       )

  cmake_parse_arguments (asvut "${_OPTIONS}" "${_SINGLE_VALUE_ARGS}" "${_MULTI_VALUE_ARGS}" ${ARGN})

  list (APPEND asvut_FILES ${asvut_UNPARSED_ARGUMENTS})

  if (asvut_ONLY_CONSTEXPR)
    set (asvut_COMPILATION_TEST    ON)
    set (asvut_SUFFIX              .constexpr)
    set (asvut_MINIMUM_VERSION     20)
    list (APPEND asvut_COMPILE_DEFINITIONS GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR)
  endif ()

  if (asvut_ONLY_EXCEPTIONS_DISABLED)
    set (asvut_SUFFIX .exceptions-disabled)
    list (
      APPEND asvut_COMPILE_OPTIONS
      $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fno-exceptions>
      $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/EHsc->
    )
  endif ()

  set (_asvut_VERSIONS ${GCH_SMALL_VECTOR_TEST_STANDARD_VERSIONS})
  if (DEFINED asvut_MINIMUM_VERSION)
    list (GET _asvut_VERSIONS -1 _asvut_LAST)
    if (_asvut_LAST LESS asvut_MINIMUM_VERSION)
      return ()
    endif ()

    list (GET _asvut_VERSIONS 0 _asvut_FIRST)
    while (_asvut_FIRST LESS asvut_MINIMUM_VERSION)
      list (POP_FRONT _asvut_VERSIONS)
      list (GET _asvut_VERSIONS 0 _asvut_FIRST)
    endwhile ()
  endif ()

  file (
    RELATIVE_PATH _DIR_RELPATH
    ${GCH_SMALL_VECTOR_SOURCE_TEST_UNIT_DIR}
    ${CMAKE_CURRENT_LIST_DIR}
  )

  string (REPLACE "/" "." _DIR_TARGET_BASE "${_DIR_RELPATH}")

  foreach (file ${asvut_FILES})
    set (_TARGET_NAME_BASE "small_vector.test.unit.${_DIR_TARGET_BASE}")
    if (NOT file STREQUAL "test.cpp")
      get_filename_component (_FILENAME_WE "${file}" NAME_WE)
      get_filename_component (_FILEDIR "${file}" DIRECTORY)
      if (_FILEDIR)
        string (APPEND _TARGET_NAME_BASE ".${_FILEDIR}")
      endif ()
      string (APPEND _TARGET_NAME_BASE ".${_FILENAME_WE}")
    endif ()

    foreach (version ${_asvut_VERSIONS})
      set (_TARGET_NAME "${_TARGET_NAME_BASE}.c++${version}${asvut_SUFFIX}")

      if (asvut_COMPILATION_TEST)
        if (asvut_ONLY_CONSTEXPR)
          add_executable (${_TARGET_NAME})

          target_sources (${_TARGET_NAME} PRIVATE ${GCH_SMALL_VECTOR_SOURCE_TEST_UNIT_DIR}/main.cpp)
          get_source_file_property (_FILE_LOCATION ${file} LOCATION)
          target_compile_definitions (
            ${_TARGET_NAME}
            PRIVATE
              GCH_SMALL_VECTOR_TEST_FILE=${_FILE_LOCATION}
          )

          target_compile_options (
            ${_TARGET_NAME}
            PRIVATE
              $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-fconstexpr-ops-limit=2147483647>
              $<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>:-fconstexpr-steps=2147483647>
              $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/constexpr:steps2147483647>
          )
        else ()
          add_library (${_TARGET_NAME} OBJECT)
          target_sources (${_TARGET_NAME} PRIVATE ${file})
        endif ()

        target_link_libraries (${_TARGET_NAME} PRIVATE small_vector.test.unit.unit_test_common)
        set_target_properties (${_TARGET_NAME} PROPERTIES EXCLUDE_FROM_ALL ON)

        add_test (
          NAME
            ${_TARGET_NAME}
          COMMAND
            ${CMAKE_COMMAND}
            --build ${CMAKE_BINARY_DIR}
            --config $<CONFIG>
            --target ${_TARGET_NAME}
            --verbose
        )

        set_tests_properties (
          ${_TARGET_NAME}
          PROPERTIES
          RESOURCE_LOCK
            BUILD_SYSTEM
        )
      else ()
        add_executable (${_TARGET_NAME} ${file})
        target_link_libraries (${_TARGET_NAME} PRIVATE small_vector.test.unit.main.c++${version})

        add_dependencies (small_vector.ctest ${_TARGET_NAME})

        add_test (
          NAME
            ${_TARGET_NAME}
          COMMAND
            ${_TARGET_NAME}
        )
      endif ()

      set_target_properties (
        ${_TARGET_NAME}
        PROPERTIES
        CXX_STANDARD
          ${version}
        CXX_STANDARD_REQUIRED
          NO
        CXX_EXTENSIONS
          NO
      )
      target_compile_definitions (${_TARGET_NAME} PRIVATE ${asvut_COMPILE_DEFINITIONS})
      target_compile_options (${_TARGET_NAME} PRIVATE ${asvut_COMPILE_OPTIONS})
      target_link_libraries (${_TARGET_NAME} PRIVATE ${asvut_LINK_LIBRARIES})

      if (NOT GCH_SMALL_VECTOR_TEST_ENABLE_PCH
          OR DEFINED asvut_COMPILE_DEFINITIONS
          OR DEFINED asvut_COMPILE_OPTIONS)
        set_target_properties (${_TARGET_NAME} PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
      else ()
        set_target_properties (
          ${_TARGET_NAME}
          PROPERTIES
          PRECOMPILE_HEADERS_REUSE_FROM
            small_vector.test.unit.main.c++${version}
        )
      endif ()

      if (asvut_EXPECT_FAIL)
        set_tests_properties (
          ${_TARGET_NAME}
          PROPERTIES
          WILL_FAIL
            ON
        )
      endif ()
    endforeach ()

    if (GCH_SMALL_VECTOR_TEST_ENABLE_COVERAGE_TARGET AND NOT asvut_COMPILATION_TEST)
      get_source_file_property (_FILE_LOCATION ${file} LOCATION)
      string (MAKE_C_IDENTIFIER "${_TARGET_NAME_BASE}" _NAMESPACE)

      string (
        CONCAT _CONTENT
        "#include \"${COVERAGE_TARGET_HEADER}\"\n"
        "namespace ${_NAMESPACE}\n"
        "{\n"
        "#include \"${_FILE_LOCATION}\"\n"
        "}\n"
      )

      if (EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${file}-coverage.cpp)
        file (READ ${CMAKE_CURRENT_BINARY_DIR}/${file}-coverage.cpp _CURRENT_CONTENT)
      else ()
        set (_CURRENT_CONTENT "")
      endif ()

      if (NOT _CONTENT STREQUAL _CURRENT_CONTENT)
        file (WRITE ${CMAKE_CURRENT_BINARY_DIR}/${file}-coverage.cpp "${_CONTENT}")
      endif ()

      foreach (version ${_asvut_VERSIONS})
        target_sources (
          small_vector.test.coverage.c++${version}
          PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}/${file}-coverage.cpp
        )
      endforeach ()

      set_property (GLOBAL APPEND PROPERTY COVERAGE_TARGET_NAMESPACES ${_NAMESPACE})
    endif ()

    if (NOT asvut_ONLY_CONSTEXPR AND NOT asvut_ONLY_EXCEPTIONS_DISABLED)
      if (GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR AND NOT asvut_NO_CONSTEXPR)
        add_small_vector_unit_tests (${file} ONLY_CONSTEXPR)
      endif ()

      if (GCH_SMALL_VECTOR_TEST_ENABLE_EXCEPTIONS_DISABLED_TESTS
          AND NOT asvut_NO_EXCEPTIONS_DISABLED
          AND NOT asvut_COMPILATION_TEST)
        add_small_vector_unit_tests (${file} ONLY_EXCEPTIONS_DISABLED)
      endif ()
    endif ()
  endforeach ()
endfunction ()

add_subdirectory (instantiation)
add_subdirectory (member)
add_subdirectory (non-member)

if (GCH_SMALL_VECTOR_TEST_ENABLE_COVERAGE_TARGET)
  get_property (COVERAGE_TARGET_NAMESPACES GLOBAL PROPERTY COVERAGE_TARGET_NAMESPACES)

  set (_DECLARATIONS_LIST ${COVERAGE_TARGET_NAMESPACES})
  list (TRANSFORM _DECLARATIONS_LIST PREPEND "namespace ")
  list (TRANSFORM _DECLARATIONS_LIST APPEND " { int test (void)\; }")

  set (_INVOCATIONS_LIST ${COVERAGE_TARGET_NAMESPACES})
  list (TRANSFORM _INVOCATIONS_LIST PREPEND "  ")
  list (TRANSFORM _INVOCATIONS_LIST APPEND "::test ()\;")

  string (
    JOIN "\n" COVERAGE_TARGET_SOURCE_CONTENT
    ${_DECLARATIONS_LIST}
    "int"
    "main (void)"
    "{"
    ${_INVOCATIONS_LIST}
    "  return 0;"
    "}\n"
  )

  if (EXISTS ${COVERAGE_TARGET_SOURCE})
    file (READ ${COVERAGE_TARGET_SOURCE} _CURRENT_SOURCE_CONTENT)
  else ()
    set (_CURRENT_SOURCE_CONTENT "")
  endif ()

  if (NOT COVERAGE_TARGET_SOURCE_CONTENT STREQUAL _CURRENT_SOURCE_CONTENT)
    file (WRITE ${COVERAGE_TARGET_SOURCE} "${COVERAGE_TARGET_SOURCE_CONTENT}")
  endif ()
endif ()
