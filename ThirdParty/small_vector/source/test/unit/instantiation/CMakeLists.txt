add_small_vector_unit_tests (
  fwddecl-stdlib.cpp
  COMPILATION_TEST
  NO_CONSTEXPR
  COMPILE_DEFINITIONS
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:_SILENCE_CXX20_REL_OPS_DEPRECATION_WARNING>
  COMPILE_OPTIONS
    $<$<COMPILE_LANG_AND_ID:CXX,Clang>:-Wno-header-hygiene>
)

add_small_vector_unit_tests (
  incomplete-type.cpp
  COMPILATION_TEST
  NO_CONSTEXPR
)

add_small_vector_unit_tests (
  expect-fail/incomplete-type.cpp
  COMPILATION_TEST
  NO_CONSTEXPR
  EXPECT_FAIL
)
