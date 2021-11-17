set (INCLUDE_INSTALL_DIRS ${MONGOC_HEADER_INSTALL_DIR})
set (LIBRARY_INSTALL_DIRS ${CMAKE_INSTALL_LIBDIR})
set (PACKAGE_LIBRARIES mongoc-1.0)

include (CMakePackageConfigHelpers)

# These aren't pkg-config files, they're CMake package configuration files.
function (install_package_config_file prefix)
   foreach (suffix "config.cmake" "config-version.cmake")
      configure_package_config_file (
         build/cmake/libmongoc-${prefix}-${suffix}.in
         ${CMAKE_CURRENT_BINARY_DIR}/libmongoc-${prefix}-${suffix}
         INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libmongoc-${prefix}
         PATH_VARS INCLUDE_INSTALL_DIRS LIBRARY_INSTALL_DIRS
      )

      install (
         FILES
            ${CMAKE_CURRENT_BINARY_DIR}/libmongoc-${prefix}-${suffix}
         DESTINATION
            ${CMAKE_INSTALL_LIBDIR}/cmake/libmongoc-${prefix}
      )
   endforeach ()
endfunction ()

install_package_config_file ("1.0")

if (ENABLE_STATIC)
   install_package_config_file ("static-1.0")
endif ()
