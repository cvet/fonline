# ############################################# #
#                                               #
#             SPARK Particle Engine             #
#                DirectX9 Module                #
#                                               #
# ############################################# #



# Project declaration
# ###############################################
cmake_minimum_required(VERSION 2.8)
project(SPARK_DirectX9)
set(SPARK_DIRECTX_DIR "" CACHE PATH "Store the path to the DirectX SDK (folder containing 'Lib' and 'Include')")
set(SPARK_DIRECTX_LIB_X86 ON CACHE BOOL "Store whether to use 32bits (ON) or 64bits (OFF) libraries of DirectX")



# Sources
# ###############################################
set(SPARK_DIR ../../..)
get_filename_component(SPARK_DIR ${SPARK_DIR}/void REALPATH)
get_filename_component(SPARK_DIR ${SPARK_DIR} PATH)
file(GLOB_RECURSE SRC_FILES
	${SPARK_DIR}/include/SPARK_DX9.h
	${SPARK_DIR}/include/Rendering/DX9/*.h
	${SPARK_DIR}/src/Rendering/DX9/*.cpp
)



# Build step
# ###############################################
set(SPARK_GENERATOR "(${CMAKE_SYSTEM_NAME}@${CMAKE_GENERATOR})")
include_directories(${SPARK_DIR}/include)
include_directories(${SPARK_DIRECTX_DIR}/Include)
if(${SPARK_DIRECTX_LIB_X86})
	link_directories(${SPARK_DIRECTX_DIR}/Lib/x86)
else()
	link_directories(${SPARK_DIRECTX_DIR}/Lib/x64)
endif()
if(${SPARK_STATIC_BUILD})
	set(SPARK_OUTPUT_TYPE static)
else()
	set(SPARK_OUTPUT_TYPE dynamic)
endif()
link_directories(${SPARK_DIR}/lib/${SPARK_GENERATOR}/${SPARK_OUTPUT_TYPE})
if(${SPARK_STATIC_BUILD})
	add_library(SPARK_DirectX9 STATIC ${SRC_FILES})
else()
	add_definitions(-DSPK_CORE_IMPORT)
	add_definitions(-DSPK_DX9_EXPORT)
	add_library(SPARK_DirectX9 SHARED ${SRC_FILES})
endif()
if(MSVC)
	set_target_properties(SPARK_Core PROPERTIES COMPILE_FLAGS "/fp:fast")
endif()
target_link_libraries(SPARK_DirectX9
	debug SPARK_debug
	optimized SPARK
	general d3d9
	general d3dx9
)
set_target_properties(SPARK_DirectX9 PROPERTIES
	OUTPUT_NAME SPARK_DX9
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











