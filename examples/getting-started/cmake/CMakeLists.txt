cmake_minimum_required(VERSION 3.20)
project(geo LANGUAGES CXX)

add_library(geo src/point.cpp)
target_include_directories(geo PUBLIC include)

## MRDOCS_BUILD is set by MrDocs when it invokes CMake.
## You can use it to skip targets we don't need for the documentation.
if(MRDOCS_BUILD)
   return()
endif()

if (BUILD_TESTING)
    add_subdirectory(tests)
endif()
