# ############################################# #
#                                               #
#         SPARK Particle Engine : Demos         #
#                Collision  demo                #
#                                               #
# ############################################# #



# Project declaration
# ###############################################
cmake_minimum_required(VERSION 2.8)
project(Collision)



# Sources
# ###############################################
set(SPARK_DIR ../../..)
get_filename_component(SPARK_DIR ${SPARK_DIR}/void REALPATH)
get_filename_component(SPARK_DIR ${SPARK_DIR} PATH)
set(SRC_FILES
	${SPARK_DIR}/demos/src/SPKCollision.cpp
)



# Build step
# ###############################################
set(SPARK_GENERATOR "(${CMAKE_SYSTEM_NAME}@${CMAKE_GENERATOR})")
include_directories(${SPARK_DIR}/include)
include_directories(${SPARK_DIR}/demos/external/SDL/include)
link_directories(${SPARK_DIR}/demos/external/SDL/lib)
if(${DEMOS_USE_STATIC_LIBS})
	link_directories(${SPARK_DIR}/lib/${SPARK_GENERATOR}/static)
else()
	add_definitions(-DSPK_IMPORT)
	link_directories(${SPARK_DIR}/lib/${SPARK_GENERATOR}/dynamic)
endif()
add_executable(Collision
	${SRC_FILES}
)
target_link_libraries(Collision	
	general opengl32.lib
	general glu32.lib
	general SDL.lib
	general SDLmain.lib
	debug SPARK_debug
	debug SPARK_GL_debug
	optimized SPARK
	optimized SPARK_GL
)
set_target_properties(Collision PROPERTIES
	DEBUG_POSTFIX _debug
	RUNTIME_OUTPUT_DIRECTORY ${SPARK_DIR}/demos/bin
	RUNTIME_OUTPUT_DIRECTORY_DEBUG ${SPARK_DIR}/demos/bin
	RUNTIME_OUTPUT_DIRECTORY_RELEASE ${SPARK_DIR}/demos/bin
)













