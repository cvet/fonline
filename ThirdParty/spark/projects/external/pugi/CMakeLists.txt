# ############################################# #
#                                               #
#             SPARK Particle Engine             #
#              External -- PugiXML              #
#                                               #
# ############################################# #



# Project declaration
# ###############################################
cmake_minimum_required(VERSION 2.8.12...3.16.3)
project(PugiXML)



# Sources
# ###############################################
set(SPARK_DIR ../../../spark) #!
#!set(SPARK_GENERATOR "(${CMAKE_SYSTEM_NAME}@${CMAKE_GENERATOR})")
get_filename_component(PUGIXML_DIR ${SPARK_DIR}/../thirdparty/PugiXML REALPATH) #!
#! get_filename_component(PUGIXML_DIR ${PUGIXML_DIR} PATH)
set(SRC_FILES #!
	${PUGIXML_DIR}/pugixml.cpp #!
	${PUGIXML_DIR}/pugixml.hpp #!
	${PUGIXML_DIR}/pugiconfig.hpp #!
)
add_definitions("-DPUGIXML_NO_EXCEPTIONS") #!



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

