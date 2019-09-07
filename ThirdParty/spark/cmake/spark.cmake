set(SPARK_DIR ${PROJECT_SOURCE_DIR}/spark)

set(CMAKE_CXX_FLAGS "-Wall -Wno-unused-parameter -std=c++11 -Wsuggest-override")

file(GLOB_RECURSE SRC_FILES
	${SPARK_DIR}/include/Core/*.h
	${SPARK_DIR}/include/Extensions/*.h
	${SPARK_DIR}/src/Core/*.cpp
	${SPARK_DIR}/src/Extensions/*.cpp
)

include_directories(${SPARK_DIR}/include)
include_directories(${SPARK_DIR}/../thirdparty/PugiXml)

add_library(spark STATIC ${SRC_FILES})

SET(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})
install( DIRECTORY ${SPARK_DIR}/include/ DESTINATION include )
