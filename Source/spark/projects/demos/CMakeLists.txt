# ############################################# #
#                                               #
#         SPARK Particle Engine : Demos         #
#                 Solution file                 #
#                                               #
# ############################################# #



# Solution
# ###############################################
cmake_minimum_required(VERSION 2.8)
project(SPARK_Demos)
set(DEMOS_USE_STATIC_LIBS OFF CACHE BOOL "Store whether to link against static (ON) or dynamic (OFF) SPARK libraries")
set(DEMOS_USE_IRRLICHT OFF CACHE BOOL "Store whether to include Irrlicht demos")



# Projects
# ###############################################
add_subdirectory(flakes flakes)
add_subdirectory(collision collision)
add_subdirectory(test test)
add_subdirectory(explosion explosion)
if(${DEMOS_USE_IRRLICHT})
	add_subdirectory(test_irr test_irr)
	add_subdirectory(test_irr_controllers test_irr_controllers)
endif()



# Dependencies
# ###############################################