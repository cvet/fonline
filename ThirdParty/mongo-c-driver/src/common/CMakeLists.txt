
set (MONGOC_ENABLE_DEBUG_ASSERTIONS 0)

if (ENABLE_DEBUG_ASSERTIONS)
    set (MONGOC_ENABLE_DEBUG_ASSERTIONS 1)
endif ()

configure_file (
        "${CMAKE_CURRENT_SOURCE_DIR}/src/common-config.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/src/common-config.h"
)

add_library(mongo-mlib INTERFACE)
add_library(mongo::mlib ALIAS mongo-mlib)
target_include_directories(mongo-mlib INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>)
set_property(TARGET mongo-mlib PROPERTY EXPORT_NAME mongo::mlib)
