set( CMAKE_SYSTEM_NAME Linux )
set( CMAKE_SYSTEM_VERSION 1 )

set( PI_TOOLS_DIR arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64 )

set( CMAKE_C_COMPILER "$ENV{PI_TOOLS_HOME}/${PI_TOOLS_DIR}/bin/arm-linux-gnueabihf-gcc" )
set( CMAKE_CXX_COMPILER "$ENV{PI_TOOLS_HOME}/${PI_TOOLS_DIR}/bin/arm-linux-gnueabihf-g++" )
set( CMAKE_FIND_ROOT_PATH "$ENV{PI_TOOLS_HOME}/${PI_TOOLS_DIR}" )
set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
