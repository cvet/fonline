include(CheckIncludeFileCXX)

if (GCH_SMALL_VECTOR_BENCH_ENABLE_BOOST OR NOT DEFINED GCH_SMALL_VECTOR_BENCH_ENABLE_BOOST)
  set (Boost_NO_WARN_NEW_VERSIONS ON)

  function (check_boost_header var)
    if (ARGN STREQUAL "QUIET")
      set (CMAKE_REQUIRED_QUIET ON)
    endif ()

    list (APPEND CMAKE_REQUIRED_LIBRARIES Boost::container)
    check_include_file_cxx(boost/container/small_vector.hpp ${var})
  endfunction ()

  if (GCH_SMALL_VECTOR_BENCH_ENABLE_BOOST)
    find_package (Boost REQUIRED COMPONENTS container)

    check_boost_header (HAVE_BOOST_SMALL_VECTOR_HPP)

    if (NOT HAVE_BOOST_SMALL_VECTOR_HPP)
      message (FATAL_ERROR "Found Boost, but could not compile with boost/container/small_vector.hpp")
    endif ()
  else ()
    find_package (Boost QUIET COMPONENTS container)

    if (Boost_FOUND)
      check_boost_header (HAVE_BOOST_SMALL_VECTOR_HPP QUIET)
      set (Boost_FOUND "${HAVE_BOOST_SMALL_VECTOR_HPP}")
    endif ()

    option (
      GCH_SMALL_VECTOR_BENCH_ENABLE_BOOST
      "Enable benchmark comparisons with Boost."
      "${Boost_FOUND}"
    )
  endif ()
endif ()

if (GCH_SMALL_VECTOR_BENCH_ENABLE_LLVM OR NOT DEFINED GCH_SMALL_VECTOR_BENCH_ENABLE_LLVM)
  enable_language (C)

  function (check_llvm_header var)
    if (ARGN STREQUAL "QUIET")
      set (CMAKE_REQUIRED_QUIET ON)
    endif ()

    list (APPEND CMAKE_REQUIRED_LIBRARIES LLVMSupport)
    list (APPEND CMAKE_REQUIRED_INCLUDES ${LLVM_INCLUDE_DIRS})
    check_include_file_cxx(llvm/ADT/SmallVector.h ${var})
  endfunction ()

  if (GCH_SMALL_VECTOR_BENCH_ENABLE_LLVM)
    find_package (LLVM REQUIRED CONFIG)

    check_llvm_header (HAVE_LLVM_SMALL_VECTOR_H)

    if (NOT HAVE_LLVM_SMALL_VECTOR_H)
      message (FATAL_ERROR "Found LLVM, but could not compile with llvm/ADT/SmallVector.h")
    endif ()
  else ()
    find_package (LLVM QUIET CONFIG)

    if (LLVM_FOUND)
      check_llvm_header (HAVE_LLVM_SMALL_VECTOR_H QUIET)
      set (LLVM_FOUND "${HAVE_LLVM_SMALL_VECTOR_H}")
    endif ()

    option (
      GCH_SMALL_VECTOR_BENCH_ENABLE_LLVM
      "Enable benchmark comparisons with LLVM."
      "${LLVM_FOUND}"
    )
  endif ()
endif ()

if (GCH_SMALL_VECTOR_BENCH_ENABLE_FOLLY OR NOT DEFINED GCH_SMALL_VECTOR_BENCH_ENABLE_FOLLY)
  function (check_folly_header var)
    if (ARGN STREQUAL "QUIET")
      set (CMAKE_REQUIRED_QUIET ON)
    endif ()

    list (APPEND CMAKE_REQUIRED_LIBRARIES Folly::folly)
    check_include_file_cxx(folly/small_vector.h ${var})
  endfunction ()

  if (GCH_SMALL_VECTOR_BENCH_ENABLE_FOLLY)
    find_package (folly REQUIRED)

    check_folly_header (HAVE_FOLLY_SMALL_VECTOR_H)

    if (NOT HAVE_FOLLY_SMALL_VECTOR_H)
      message (FATAL_ERROR "Found folly, but could not compile with folly/small_vector.h")
    endif ()
  else ()
    find_package (folly QUIET)

    if (folly_FOUND)
      check_folly_header (HAVE_FOLLY_SMALL_VECTOR_H QUIET)
      set (folly_FOUND "${HAVE_FOLLY_SMALL_VECTOR_H}")
    endif ()

    option (
      GCH_SMALL_VECTOR_BENCH_ENABLE_FOLLY
      "Enable benchmark comparisons with folly."
      "${folly_FOUND}"
    )
  endif ()
endif ()

foreach (version ${GCH_SMALL_VECTOR_TEST_STANDARD_VERSIONS})
  add_executable (
    small_vector.bench.c++${version}
    bench.cpp
    graphs.cpp
    demangle.cpp
  )

  target_link_libraries (small_vector.bench.c++${version} PRIVATE small_vector.test.test_common)

  target_compile_definitions (
    small_vector.bench.c++${version}
    PRIVATE
      GCH_UNRESTRICTED_DEFAULT_BUFFER_SIZE
  )

  if (GCH_SMALL_VECTOR_BENCH_ENABLE_BOOST)
    target_link_libraries (small_vector.bench.c++${version} PRIVATE Boost::container)
    target_compile_definitions (small_vector.bench.c++${version} PRIVATE GCH_WITH_BOOST)
  endif ()

  if (GCH_SMALL_VECTOR_BENCH_ENABLE_LLVM AND version GREATER 11)
    target_link_libraries (small_vector.bench.c++${version} PRIVATE LLVMSupport)
    target_compile_definitions (small_vector.bench.c++${version} PRIVATE GCH_WITH_LLVM)
    if (LLVM_INCLUDE_DIRS)
      target_include_directories (
        small_vector.bench.c++${version}
        SYSTEM PRIVATE
          ${LLVM_INCLUDE_DIRS}
      )
    endif ()
  endif ()

  if (GCH_SMALL_VECTOR_BENCH_ENABLE_FOLLY)
    target_link_libraries (small_vector.bench.c++${version} PRIVATE Folly::folly)
    target_compile_definitions (small_vector.bench.c++${version} PRIVATE GCH_WITH_FOLLY)
  endif ()

  target_compile_options (
    small_vector.bench.c++${version}
    PRIVATE
      $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU>:-Wno-missing-field-initializers>
  )

  set_target_properties (
    small_vector.bench.c++${version}
    PROPERTIES
    CXX_STANDARD
      ${version}
    CXX_STANDARD_REQUIRED
      NO
    CXX_EXTENSIONS
      NO
  )
endforeach ()
