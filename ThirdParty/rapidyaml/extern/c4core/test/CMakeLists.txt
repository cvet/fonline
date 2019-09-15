c4_setup_testing(C4CORE)

set(testsrc
    c4/test.hpp
    c4/libtest/test.cpp
    c4/libtest/archetypes.cpp
    c4/libtest/archetypes.hpp
    c4/libtest/supprwarn_push.hpp
    c4/libtest/supprwarn_pop.hpp
    )
c4_add_library(C4CORE c4core-libtest
    SOURCES ${testsrc}
    LIBS c4core gtest
    INC_DIRS ${CMAKE_CURRENT_LIST_DIR}
    FOLDER c4core-test
    )

function(c4core_add_test name)
    c4_add_executable(C4CORE c4core-test-${name}
        SOURCES ${ARGN} main.cpp
        INC_DIRS ${CMAKE_CURRENT_LIST_DIR}
        LIBS c4core c4core-libtest gtest
        FOLDER c4core-test)
    c4_add_test(C4CORE c4core-test-${name} ON)
endfunction(c4core_add_test)

c4core_add_test(preprocessor     preprocessor.cpp)
c4core_add_test(type_name        type_name.cpp)
c4core_add_test(types            types.cpp)
c4core_add_test(szconv           szconv.cpp)
c4core_add_test(error            error.cpp)
c4core_add_test(blob             blob.cpp)
c4core_add_test(memory_util      memory_util.cpp)
c4core_add_test(memory_resource  memory_resource.cpp)
c4core_add_test(allocator        allocator.cpp)
c4core_add_test(ctor_dtor        ctor_dtor.cpp)
c4core_add_test(chartraits       char_traits.cpp)
c4core_add_test(enum             enum.cpp)
c4core_add_test(bitmask          bitmask.cpp)
c4core_add_test(span             span.cpp)
c4core_add_test(substr           substr.cpp)
c4core_add_test(to_chars         to_chars.cpp)
c4core_add_test(format           format.cpp)
c4core_add_test(base64           base64.cpp)
c4core_add_test(std_string       std_string.cpp)
c4core_add_test(std_vector       std_vector.cpp)
