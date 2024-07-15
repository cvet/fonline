set (GCH_SMALL_VECTOR_SOURCE_TEST_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "")

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  string (REGEX REPLACE "/[wW]([0-4deovX]|all) ?" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

  if (GCH_SMALL_VECTOR_TEST_DISABLE_RTTI)
    string (REGEX REPLACE "/GR ?" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  endif ()

  if (GCH_SMALL_VECTOR_TEST_DISABLE_EXCEPTIONS)
    string (REGEX REPLACE "/EH[ascr]+-?" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  endif ()
endif ()

if (NOT DEFINED CMAKE_CXX_STANDARD_REQUIRED)
  set (CMAKE_CXX_STANDARD_REQUIRED OFF)
endif ()

if (NOT DEFINED CMAKE_CXX_EXTENSIONS)
  set (CMAKE_CXX_EXTENSIONS OFF)
endif ()

add_library (small_vector.test.test_common INTERFACE)

target_include_directories (small_vector.test.test_common INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries (small_vector.test.test_common INTERFACE gch::small_vector)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options (
    small_vector.test.test_common
    INTERFACE
      -ftime-trace
      -Weverything
      -Wno-padded
      -Wno-c++98-compat
      -Wno-c++98-compat-pedantic
      -Wno-zero-as-null-pointer-constant
      -Wno-used-but-marked-unused
      -Wno-weak-vtables
      -Wno-exit-time-destructors
      $<$<COMPILE_LANG_AND_ID:CXX,Clang>:-Wno-unsafe-buffer-usage>
  )

  if (GCH_SMALL_VECTOR_TEST_DISABLE_RTTI)
    target_compile_options (
      small_vector.test.test_common
      INTERFACE
        -fno-rtti
    )
  endif ()

  if (GCH_SMALL_VECTOR_TEST_ENABLE_COVERAGE_TARGET)
    if (CMAKE_SYSTEM_NAME MATCHES "Windows")
      target_compile_options (small_vector.test.test_common INTERFACE --coverage)
      target_link_options (small_vector.test.test_common INTERFACE --coverage)
    else ()
      target_compile_options (
        small_vector.test.test_common
        INTERFACE
          -fprofile-instr-generate
          -fcoverage-mapping
      )

      target_link_options (
        small_vector.test.test_common
        INTERFACE
          -fprofile-instr-generate
          -fcoverage-mapping
      )
    endif ()
  endif ()

  if (GCH_USE_LIBCXX_WITH_CLANG)
    target_compile_options (small_vector.test.test_common INTERFACE -stdlib=libc++)
    target_link_options (small_vector.test.test_common INTERFACE -stdlib=libc++)
  endif ()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  target_compile_options (
    small_vector.test.test_common
    INTERFACE
      -pedantic
      -Wall
      -Wextra
      -Wcast-align
      -Wcast-qual
      -Wctor-dtor-privacy
      -Wdisabled-optimization
      -Wformat=2
      -Winit-self
      -Wlogical-op
      -Wmissing-declarations
      -Wmissing-include-dirs
      -Wnoexcept
      -Wold-style-cast
      -Woverloaded-virtual
      -Wredundant-decls
      -Wshadow
      -Wsign-conversion
      -Wsign-promo
      -Wstrict-null-sentinel
      -Wstrict-overflow=5
      -Wswitch-default
      -Wundef
      -Wno-unused
      -fconcepts-diagnostics-depth=10
  )

  if (GCH_SMALL_VECTOR_TEST_DISABLE_RTTI)
    target_compile_options (
      small_vector.test.test_common
      INTERFACE
        -fno-rtti
    )
  endif ()

  if (GCH_SMALL_VECTOR_TEST_ENABLE_COVERAGE_TARGET)
    target_compile_options (small_vector.test.test_common INTERFACE --coverage)
    target_link_options (small_vector.test.test_common INTERFACE --coverage)
  endif ()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options (
    small_vector.test.test_common
    INTERFACE
      /W4
      /bigobj
  )

  if (GCH_SMALL_VECTOR_TEST_DISABLE_RTTI)
    target_compile_options (
      small_vector.test.test_common
      INTERFACE
        /GR-
    )
  endif ()
endif ()

if (UNIX)
  target_compile_options (
    small_vector.test.test_common
    INTERFACE
      $<$<CONFIG:Debug>:-fsanitize=address>
  )

  target_link_options (
    small_vector.test.test_common
    INTERFACE
      $<$<CONFIG:Debug>:-fsanitize=address>
  )
endif ()

if (GCH_SMALL_VECTOR_TEST_ENABLE_REL_OPS_TESTS)
  target_compile_definitions (
    small_vector.test.test_common
    INTERFACE
      GCH_SMALL_VECTOR_TEST_REL_OPS_AMBIGUITY
      $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:_SILENCE_CXX20_REL_OPS_DEPRECATION_WARNING>
  )
endif ()

if (GCH_SMALL_VECTOR_TEST_USE_LLD)
  target_link_options (
    small_vector.test.test_common
    INTERFACE
      -fuse-ld=lld
  )
endif ()

add_custom_target (small_vector.ctest)

add_subdirectory (unit)
