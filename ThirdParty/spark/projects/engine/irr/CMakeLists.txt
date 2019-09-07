# ############################################# #
#                                               #
#             SPARK Particle Engine             #
#                Irrlicht module                #
#                                               #
# ############################################# #



# Project declaration
# ###############################################
cmake_minimum_required(VERSION 2.8)
project(SPARK_Irrlicht)
set(SPARK_IRRLICHT_LIB "" CACHE STRING "Store the name of the Irrlicht library file (release)")
set(SPARK_IRRLICHT_LIB_DEBUG "" CACHE STRING "Store the name of the Irrlicht library file (debug)")
set(SPARK_IRRLICHT_INCLUDE_DIR "" CACHE PATH "Store the path to the 'include' folder of Irrlicht")
set(SPARK_IRRLICHT_LIB_DIR "" CACHE PATH "Store the path to the library folder of Irrlicht, ie 'lib/<config>'")



# Sources
# ###############################################
set(SPARK_DIR ../../..)
get_filename_component(SPARK_DIR ${SPARK_DIR}/void REALPATH)
get_filename_component(SPARK_DIR ${SPARK_DIR} PATH)
file(GLOB_RECURSE SRC_FILES
	${SPARK_DIR}/include/SPARK_IRR.h
	${SPARK_DIR}/include/Rendering/Irrlicht/*.h
	${SPARK_DIR}/src/Rendering/Irrlicht/*.cpp
)



# Build step
# ###############################################
set(SPARK_GENERATOR "(${CMAKE_SYSTEM_NAME}@${CMAKE_GENERATOR})")
include_directories(${SPARK_DIR}/include)
include_directories(${SPARK_IRRLICHT_INCLUDE_DIR})
link_directories(${SPARK_IRRLICHT_LIB_DIR})
if(${SPARK_STATIC_BUILD})
	set(SPARK_OUTPUT_TYPE static)
else()
	add_definitions(-DSPK_IRR_EXPORT)
	add_definitions(-DSPK_CORE_IMPORT)
	set(SPARK_OUTPUT_TYPE dynamic)
endif()
link_directories(${SPARK_DIR}/lib/${SPARK_GENERATOR}/${SPARK_OUTPUT_TYPE})
if(${SPARK_STATIC_BUILD})
	add_library(SPARK_Irrlicht STATIC ${SRC_FILES})
else()
	add_library(SPARK_Irrlicht SHARED ${SRC_FILES})
endif()
if(("${SPARK_IRRLICHT_LIB}" STREQUAL "") OR
   ("${SPARK_IRRLICHT_LIB_DEBUG}" STREQUAL ""))
	message("	You have to set both SPARK_IRRLICHT_LIB and SPARK_IRRLICHT_LIB_DEBUG for the Irrlicht build to work.
	Ignore this message if you're not using Irrlicht.")
else()
	target_link_libraries(SPARK_Irrlicht
		debug SPARK_debug
		debug ${SPARK_IRRLICHT_LIB_DEBUG}
		optimized SPARK
		optimized ${SPARK_IRRLICHT_LIB}
	)
endif()
if(MSVC)
	set_target_properties(SPARK_Core PROPERTIES COMPILE_FLAGS "/fp:fast")
endif()
set_target_properties(SPARK_Irrlicht PROPERTIES
	OUTPUT_NAME SPARK_IRR
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












