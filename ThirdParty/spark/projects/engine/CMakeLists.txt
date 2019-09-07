# ############################################# #
#                                               #
#             SPARK Particle Engine             #
#                 Solution file                 #
#                                               #
# ############################################# #



# Solution
# ###############################################
cmake_minimum_required(VERSION 2.8)
project(SPARK)



# Projects
# ###############################################
add_subdirectory(core core)
add_subdirectory(irr irr)
add_subdirectory(ogl ogl)
add_subdirectory(dx9 dx9)
add_subdirectory(../external external)



# Dependencies
# ###############################################
add_dependencies(SPARK_Core PugiXML)
add_dependencies(SPARK_OpenGL GLEW)
add_dependencies(SPARK_Irrlicht SPARK_Core)
add_dependencies(SPARK_OpenGL SPARK_Core)
add_dependencies(SPARK_DirectX9 SPARK_Core)