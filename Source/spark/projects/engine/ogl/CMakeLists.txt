# ############################################# #
#                                               #
#             SPARK Particle Engine             #
#                 OpenGL Module                 #
#                                               #
# ############################################# #



# Project declaration
# ###############################################
cmake_minimum_required(VERSION 2.8)
project(SPARK_OpenGL)
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(SPARK_OPENGL_LIB "GL" CACHE STRING "The name of the library file of OpenGL.")
else()
	set(SPARK_OPENGL_LIB "opengl32" CACHE STRING "The name of the library file of OpenGL.")
endif()



# Sources
# ###############################################
set(SPARK_DIR ../../..)
get_filename_component(SPARK_DIR ${SPARK_DIR}/void REALPATH)
get_filename_component(SPARK_DIR ${SPARK_DIR} PATH)
file(GLOB_RECURSE SRC_FILES
	${SPARK_DIR}/include/SPARK_GL.h
	${SPARK_DIR}/include/Rendering/OpenGL/*.h
	${SPARK_DIR}/src/Rendering/OpenGL/*.cpp
)



# Build step
# ###############################################
set(SPARK_GENERATOR "(${CMAKE_SYSTEM_NAME}@${CMAKE_GENERATOR})")
add_definitions(-DGLEW_STATIC)
include_directories(${SPARK_DIR}/include)
include_directories(${SPARK_DIR}/external/glew/src)
link_directories(${SPARK_DIR}/lib/${SPARK_GENERATOR}/external/glew)
if(${SPARK_STATIC_BUILD})
	set(SPARK_OUTPUT_TYPE static)
else()
	add_definitions(-DSPK_CORE_IMPORT)
	add_definitions(-DSPK_GL_EXPORT)
	set(SPARK_OUTPUT_TYPE dynamic)
endif()
link_directories(${SPARK_DIR}/lib/${SPARK_GENERATOR}/${SPARK_OUTPUT_TYPE})
if(${SPARK_STATIC_BUILD})
	add_library(SPARK_OpenGL STATIC ${SRC_FILES})
else()
	add_library(SPARK_OpenGL SHARED ${SRC_FILES})
endif()
if(MSVC)
	set_target_properties(SPARK_Core PROPERTIES COMPILE_FLAGS "/fp:fast")
endif()
target_link_libraries(SPARK_OpenGL
	debug glew_d
	debug SPARK_debug
	optimized glew
	optimized SPARK
	general ${SPARK_OPENGL_LIB}
)
set_target_properties(SPARK_OpenGL PROPERTIES
	OUTPUT_NAME SPARK_GL
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










