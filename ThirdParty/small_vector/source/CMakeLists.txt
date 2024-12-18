set (GCH_SMALL_VECTOR_SOURCE_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include CACHE INTERNAL "")

add_library (small_vector INTERFACE)

target_sources (
  small_vector
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/gch/small_vector.hpp>
)

target_include_directories (
  small_vector
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)

target_include_directories (
  small_vector
  SYSTEM INTERFACE
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${GCH_SMALL_VECTOR_INSTALL_INCLUDE_DIR}>
)

set_target_properties (
  small_vector
  PROPERTIES
  PUBLIC_HEADER
    include/gch/small_vector.hpp
)

target_sources (
  small_vector
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/support/visualstudio/small_vector.natvis>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${GCH_SMALL_VECTOR_INSTALL_SUPPORT_DIR}/visualstudio/small_vector.natvis>
)

add_library (gch::small_vector ALIAS small_vector)

install (
  TARGETS
    small_vector
  EXPORT
    small_vector-targets
  PUBLIC_HEADER
    DESTINATION ${GCH_SMALL_VECTOR_INSTALL_INCLUDE_DIR}/gch
)

file (
  RELATIVE_PATH
  _PACKAGE_PREFIX_DIR
  "${CMAKE_INSTALL_PREFIX}/${GCH_SMALL_VECTOR_INSTALL_CMAKE_DIR}"
  "${CMAKE_INSTALL_PREFIX}"
)
string (PREPEND _PACKAGE_PREFIX_DIR \${CMAKE_CURRENT_LIST_DIR}/)

file (WRITE ${CMAKE_CURRENT_BINARY_DIR}/small_vector-config.cmake "\
get_filename_component (PACKAGE_PREFIX_DIR \"${_PACKAGE_PREFIX_DIR}\" ABSOLUTE)
include (\"\${CMAKE_CURRENT_LIST_DIR}/small_vector-targets.cmake\")
")

include (CMakePackageConfigHelpers)
write_basic_package_version_file (
  small_vector-version.cmake
  COMPATIBILITY
    SameMinorVersion
)

install (
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/small_vector-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/small_vector-version.cmake
  DESTINATION
    ${GCH_SMALL_VECTOR_INSTALL_CMAKE_DIR}
)

install (
  EXPORT
    small_vector-targets
  DESTINATION
    ${GCH_SMALL_VECTOR_INSTALL_CMAKE_DIR}
  NAMESPACE
    gch::
)

install (
  DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/support/
  DESTINATION
    ${GCH_SMALL_VECTOR_INSTALL_SUPPORT_DIR}
  PATTERN "__pycache__" EXCLUDE
)

if (GCH_SMALL_VECTOR_ENABLE_TESTS)
  add_subdirectory (test)
endif ()

if (GCH_SMALL_VECTOR_ENABLE_BENCHMARKS)
  add_subdirectory (bench)
endif ()
