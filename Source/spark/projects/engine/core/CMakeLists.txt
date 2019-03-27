# ############################################# #
#                                               #
#             SPARK Particle Engine             #
#                  Core Module                  #
#                                               #
# ############################################# #



# Project declaration
# ###############################################
cmake_minimum_required(VERSION 2.8)
project(SPARK_Core)
set(SPARK_STATIC_BUILD OFF CACHE BOOL "Store whether SPARK is built as a static library (ON) or a dynamic one OFF)")



# Sources
# ###############################################
set(SPARK_DIR ../../..)
get_filename_component(SPARK_DIR ${SPARK_DIR}/void REALPATH)
get_filename_component(SPARK_DIR ${SPARK_DIR} PATH)

file(GLOB_RECURSE TMP_SRC_FILES
	${SPARK_DIR}/include/Core/*.h
	${SPARK_DIR}/include/Extensions/*.h
)
foreach(f ${TMP_SRC_FILES})
	file(RELATIVE_PATH SRCGR ${SPARK_DIR}/include ${f})
	string(REGEX REPLACE "(.*)(/[^/]*)$" "\\1" SRCGR ${SRCGR})
	string(REPLACE / \\ SRCGR ${SRCGR})
	source_group(Headers\\${SRCGR} FILES ${f})
endforeach()
set(SRC_FILES ${TMP_SRC_FILES})

file(GLOB_RECURSE TMP_SRC_FILES
	${SPARK_DIR}/src/Core/*.cpp
	${SPARK_DIR}/src/Extensions/*.cpp
)
foreach(f ${TMP_SRC_FILES})
	file(RELATIVE_PATH SRCGR ${SPARK_DIR}/src ${f})
	string(REGEX REPLACE "(.*)(/[^/]*)$" "\\1" SRCGR ${SRCGR})
	string(REPLACE / \\ SRCGR ${SRCGR})
	source_group(Sources\\${SRCGR} FILES ${f})
endforeach()
set(SRC_FILES
	${SPARK_DIR}/include/SPARK.h
	${SPARK_DIR}/include/SPARK_Core.h
	${SRC_FILES}
	${TMP_SRC_FILES}
)
source_group(Headers FILES
	${SPARK_DIR}/include/SPARK.h
	${SPARK_DIR}/include/SPARK_Core.h
)



# Build step
# ###############################################
set(SPARK_GENERATOR "(${CMAKE_SYSTEM_NAME}@${CMAKE_GENERATOR})")
include_directories(${SPARK_DIR}/include)
include_directories(${SPARK_DIR}/external/pugixml/src)
link_directories(${SPARK_DIR}/lib/${SPARK_GENERATOR}/external/pugixml)
if(${SPARK_STATIC_BUILD})
	set(SPARK_OUTPUT_TYPE static)
	add_library(SPARK_Core STATIC ${SRC_FILES})
else()
	add_definitions(-DSPK_CORE_EXPORT)
	set(SPARK_OUTPUT_TYPE dynamic)
	add_library(SPARK_Core SHARED ${SRC_FILES})
endif()
if(MSVC)
	set_target_properties(SPARK_Core PROPERTIES COMPILE_FLAGS "/fp:fast")
endif()
target_link_libraries(SPARK_Core
	debug pugixml_d
	optimized pugixml
)
set_target_properties(SPARK_Core PROPERTIES
	OUTPUT_NAME SPARK
	DEBUG_POSTFIX _debug
	RUNTIME_OUTPUT_DIRECTORY ${SPARK_DIR}/lib/${SPARK_GENERATOR}/${SPARK_OUTPUT_TYPE}
	LIBRARY_OUTPUT_DIRECTORY ${SPARK_DIR}/lib/${SPARK_GENERATOR}/${SPARK_OUTPUT_TYPE}
	ARCHIVE_OUTPUT_DIRECTORY ${SPARK_DIR}/lib/${SPARK_GENERATOR}/${SPARK_OUTPUT_TYPE}
	RUNTIME_OUTPUT_DIRECTORY_DEBUG ${SPARK_DIR}/lib/${SPARK_GENERATOR}/${SPARK_OUTPUT_TYPE}
	LIBRARY_OUTPUT_DIRECTORY_DEBUG ${SPARK_DIR}/lib/${SPARK_GENERATOR}/${SPARK_OUTPUT_TYPE}
	ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${SPARK_DIR}/lib/${SPARK_GENERATOR}/${SPARK_OUTPUT_TYPE}
	RUNTIME_OUTPUT_DIRECTORY_RELEASE ${SPARK_DIR}/lib/${SPARK_GENERATOR}/${SPARK_OUTPUT_TYPE}
	LIBRARY_OUTPUT_DIRECTORY_RELEASE ${SPARK_DIR}/lib/${SPARK_GENERATOR}/${SPARK_OUTPUT_TYPE}
	ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${SPARK_DIR}/lib/${SPARK_GENERATOR}/${SPARK_OUTPUT_TYPE}
)

