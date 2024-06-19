cmake_minimum_required (VERSION 3.15)

project (
  small_vector
  VERSION
    0.9.2
  LANGUAGES
    CXX
)

string (COMPARE EQUAL "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}" _IS_PROJECT_ROOT)

option (
  GCH_SMALL_VECTOR_ENABLE_TESTS
  "Set to ON to build tests for gch::small_vector."
  ${_IS_PROJECT_ROOT}
)

option (
  GCH_SMALL_VECTOR_ENABLE_BENCHMARKS
  "Set to ON to build benchmarks for gch::small_vector."
  ${_IS_PROJECT_ROOT}
)

if (GCH_SMALL_VECTOR_ENABLE_TESTS)
  include (CMakeDependentOption)

  cmake_dependent_option (
    GCH_USE_LIBCXX_WITH_CLANG
    "Set to ON to use libc++ when compiling with Clang."
    OFF
    "CMAKE_CXX_COMPILER_ID MATCHES Clang"
    OFF
  )

  option (
    GCH_SMALL_VECTOR_TEST_ENABLE_PCH
    "Set to ON to speed up the building of tests by using precompile headers."
    OFF
  )

  option (
    GCH_SMALL_VECTOR_TEST_ENABLE_REL_OPS_TESTS
    "Set to ON to test for ambiguous resolution resulting from `using namespace std::rel_ops`."
    OFF
  )

  option (
    GCH_SMALL_VECTOR_TEST_ENABLE_COVERAGE_TARGET
    "Set to ON to hack together a union of the unit tests to test for coverage."
    OFF
  )

  option (
    GCH_SMALL_VECTOR_TEST_DISABLE_RTTI
    "Set to ON to disable runtime type information when building the tests."
    OFF
  )

  option (
    GCH_SMALL_VECTOR_TEST_DISABLE_EXCEPTION_SAFETY_TESTING
    "Set to ON to disable verification of exception safety."
    OFF
  )

  option (
    GCH_SMALL_VECTOR_TEST_ENABLE_EXCEPTIONS_DISABLED_TESTS
    "Set to ON to add test targets with code generation for exceptions disabled entirely."
    OFF
  )

  if (NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    include (CheckLinkerFlag)
    check_linker_flag (CXX -fuse-ld=lld HAVE_lld_LINK_OPTION)
  endif ()

  cmake_dependent_option (
    GCH_SMALL_VECTOR_TEST_USE_LLD
    "Set to ON to link with lld."
    ON
    HAVE_lld_LINK_OPTION
    OFF
  )

  set (
    GCH_SMALL_VECTOR_TEST_STANDARD_VERSIONS
    11
    14
    17
    20
    CACHE STRING
    "Set to a list of C++ standard versions to build and test with."
  )

  enable_testing ()
endif ()

set (
  GCH_SMALL_VECTOR_INSTALL_CMAKE_DIR
  "lib/cmake/small_vector"
  CACHE STRING
  "Specify where to install the CMake config files."
)

set (
  GCH_SMALL_VECTOR_INSTALL_INCLUDE_DIR
  "include"
  CACHE STRING
  "Specify where to install gch/small_vector.hpp."
)

set (
  GCH_SMALL_VECTOR_INSTALL_LICENSE_DIR
  "share/licenses/gch/small_vector"
  CACHE STRING
  "Specify where to install the license."
)

set (
  GCH_SMALL_VECTOR_INSTALL_SUPPORT_DIR
  "share/gch"
  CACHE STRING
  "Specify where to install support files like GDB prettyprinters."
)

foreach (dir CMAKE INCLUDE LICENSE SUPPORT)
  if (IS_ABSOLUTE "${GCH_SMALL_VECTOR_INSTALL_${dir}_DIR}")
    message (FATAL_ERROR "GCH_SMALL_VECTOR_INSTALL_${dir}_DIR cannot be absolute.")
  endif ()
endforeach ()

add_subdirectory (source)

export (
  EXPORT
    small_vector-targets
  NAMESPACE
    gch::
)

install (
  FILES
    LICENSE
  DESTINATION
    ${GCH_SMALL_VECTOR_INSTALL_LICENSE_DIR}
)
