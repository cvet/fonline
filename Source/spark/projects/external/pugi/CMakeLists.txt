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
get_filename_component(SPARK_DIR ${SPARK_DIR}/void REALPATH)
get_filename_component(SPARK_DIR ${SPARK_DIR} PATH)
file(GLOB_RECURSE SRC_FILES
	${SPARK_DIR}/../thirdparty/PugiXML/*.c #!
	${SPARK_DIR}/../thirdparty/PugiXML/*.cpp #!
	${SPARK_DIR}/../thirdparty/PugiXML/*.cxx #!
	${SPARK_DIR}/../thirdparty/PugiXML/*.h #!
	${SPARK_DIR}/../thirdparty/PugiXML/*.hpp #!
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

