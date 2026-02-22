# The following lines of boilerplate have to be in your project's CMakeLists in
# this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test1_demo)

# Add the following line to include the ESP RainMaker component
# set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/managed_components)
