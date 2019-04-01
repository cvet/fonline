# ############################################# #
#                                               #
#             SPARK Particle Engine             #
#              External -- PugiXML              #
#                                               #
# ############################################# #



# Project declaration
# ###############################################
cmake_minimum_required(VERSION 2.8)
project(PugiXML)



# Sources
# ###############################################
set(SPARK_DIR ../../../spark) #!
#!set(SPARK_GENERATOR "(${CMAKE_SYSTEM_NAME}@${CMAKE_GENERATOR})")
message(${SPARK_DIR})
get_filename_component(PUGIXML_DIR ${SPARK_DIR}/../thirdparty/PugiXML/void REALPATH)
message(${PUGIXML_DIR})
get_filename_component(PUGIXML_DIR ${PUGIXML_DIR} PATH)
message(${PUGIXML_DIR})
file(GLOB_RECURSE SRC_FILES
	"${PUGIXML_DIR}/*.c"
	"${PUGIXML_DIR}/*.cpp"
	"${PUGIXML_DIR}/*.cxx"
	"${PUGIXML_DIR}/*.h"
	"${PUGIXML_DIR}/*.hpp"
)



# Build step
# ###############################################
add_library(PugiXML STATIC ${SRC_FILES})
#!set_target_properties(PugiXML PROPERTIES
#!	OUTPUT_NAME pugixml
#!	DEBUG_POSTFIX _d
#!	RUNTIME_OUTPUT_DIRECTORY ${SPARK_DIR}/lib/${SPARK_GENERATOR}/external/pugixml
#!	LIBRARY_OUTPUT_DIRECTORY ${SPARK_DIR}/lib/${SPARK_GENERATOR}/external/pugixml
#!	ARCHIVE_OUTPUT_DIRECTORY ${SPARK_DIR}/lib/${SPARK_GENERATOR}/external/pugixml
#!	RUNTIME_OUTPUT_DIRECTORY_DEBUG ${SPARK_DIR}/lib/${SPARK_GENERATOR}/external/pugixml
#!	LIBRARY_OUTPUT_DIRECTORY_DEBUG ${SPARK_DIR}/lib/${SPARK_GENERATOR}/external/pugixml
#!	ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${SPARK_DIR}/lib/${SPARK_GENERATOR}/external/pugixml
#!	RUNTIME_OUTPUT_DIRECTORY_RELEASE ${SPARK_DIR}/lib/${SPARK_GENERATOR}/external/pugixml
#!	LIBRARY_OUTPUT_DIRECTORY_RELEASE ${SPARK_DIR}/lib/${SPARK_GENERATOR}/external/pugixml
#!	ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${SPARK_DIR}/lib/${SPARK_GENERATOR}/external/pugixml
#!)

