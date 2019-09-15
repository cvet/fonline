cmake_minimum_required(VERSION 2.8)

project(c4stl-extern)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
include(ExternalProject)
include(ExternalProjectUtils)

ExternalProject_GetFwdArgs(FWD_ARGS)
# this is needed for multi-configuration generators.
# see http://stackoverflow.com/questions/24460486/cmake-build-type-not-being-used-in-cmakelists-txt
set(BUILD_COMMAND ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --)

set(BDIR ${CMAKE_BINARY_DIR})
set(IDIR ${CMAKE_INSTALL_PREFIX})

#------------------------------------------------------------------------------
if(MSVC)
    set(gtopts -Dgtest_force_shared_crt=1)
endif()
ExternalProject_Add(googletest
    PREFIX          googletest
    GIT_REPOSITORY  https://github.com/google/googletest
    GIT_TAG         release-1.8.0
    BINARY_DIR      ${BDIR}/googletest
    INSTALL_DIR     ${IDIR}
    CMAKE_ARGS      ${FWD_ARGS} ${gtopts}
    BUILD_COMMAND   ${BUILD_COMMAND})

#------------------------------------------------------------------------------
ExternalProject_Add(googlebenchmark
    PREFIX          googlebenchmark
    GIT_REPOSITORY  https://github.com/google/benchmark
    BINARY_DIR      ${BDIR}/googlebenchmark
    INSTALL_DIR     ${IDIR}
    CMAKE_ARGS      ${FWD_ARGS} -DBENCHMARK_ENABLE_TESTING=0
    BUILD_COMMAND   ${BUILD_COMMAND})
