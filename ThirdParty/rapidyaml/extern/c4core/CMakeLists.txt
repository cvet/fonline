cmake_minimum_required(VERSION 3.2)
project(c4core)

include(./cmake/c4Project.cmake)

c4_declare_project(C4CORE
    DESC "Multiplatform low-level C++ utilities"
    AUTHOR "Joao Paulo Magalhaes <dev@jpmag.me>"
    MAJOR 0 MINOR 0 RELEASE 1)

set(C4CORE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(C4CORE_EXT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern)

set(C4CORE_SRC
    c4/allocator.hpp
    c4/base64.hpp
    c4/base64.cpp
    c4/blob.hpp
    c4/bitmask.hpp
    c4/c4_pop.hpp
    c4/c4_push.hpp
    c4/char_traits.cpp
    c4/char_traits.hpp
    c4/common.hpp
    c4/compiler.hpp
    c4/config.hpp
    c4/cpu.hpp
    c4/ctor_dtor.hpp
    c4/enum.hpp
    c4/error.cpp
    c4/error.hpp
    c4/format.hpp
    c4/format.cpp
    c4/hash.hpp
    c4/language.hpp
    c4/language.cpp
    c4/memory_resource.cpp
    c4/memory_resource.hpp
    c4/memory_util.cpp
    c4/memory_util.hpp
    c4/platform.hpp
    c4/preprocessor.hpp
    c4/restrict.hpp
    c4/span.hpp
    c4/std/std.hpp
    c4/std/string.hpp
    c4/std/tuple.hpp
    c4/std/vector.hpp
    c4/substr.hpp
    c4/szconv.hpp
    c4/time.hpp
    c4/time.cpp
    c4/to_chars.hpp
    c4/type_name.hpp
    c4/types.hpp
    c4/unrestrict.hpp
    c4/windows.hpp
    c4/windows_pop.hpp
    c4/windows_push.hpp
    c4/c4core.natvis
)

c4core_add_library(c4core
    SOURCES ${C4CORE_SRC} SOURCE_ROOT ${C4CORE_SRC_DIR}
    INC_DIRS ${C4CORE_SRC_DIR} ${C4CORE_EXT_DIR}
)

c4core_add_library(c4core-interface
    LIBRARY_TYPE INTERFACE
    SOURCES ${C4CORE_SRC} SOURCE_ROOT ${C4CORE_SRC_DIR}
    INTERFACE_INC_DIRS ${C4CORE_SRC_DIR} ${C4CORE_EXT_DIR}
)

c4core_add_doxygen(doc
    PROJ c4core
    DOXYFILE ${CMAKE_CURRENT_LIST_DIR}/cmake/Doxyfile.in
    INPUT ${C4CORE_SRC_DIR} ${C4CORE_EXT_DIR}
    STRIP_FROM_PATH ${C4CORE_SRC_DIR} ${C4CORE_EXT_DIR})
c4core_add_doxygen(doc-full
    PROJ c4core
    DOXYFILE ${CMAKE_CURRENT_LIST_DIR}/cmake/Doxyfile.full.in
    INPUT ${C4CORE_SRC_DIR} ${C4CORE_EXT_DIR}
    STRIP_FROM_PATH ${C4CORE_SRC_DIR} ${C4CORE_EXT_DIR})


if(C4CORE_BUILD_TESTS)
    enable_testing() # this must be done here (and not inside the test dir)
                     # so that the test targets are available at the top level
    add_subdirectory(test)
endif()

if(C4CORE_BUILD_BENCHMARKS)
    add_subdirectory(bm)
endif()
