cmake_minimum_required(VERSION 3.10.0) # This is the current project minimum cmake version requirement.
project(protocol VERSION 0.1.0 LANGUAGES C CXX) # this is naming the project

# Ensure we use C++17 for features like std::clamp
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

INCLUDE_DIRECTORIES(include/raylib.h) # This is what will include raylib and other assets and libs. 

# Standard executable (Terminal-based)
add_executable(protocol src/main.cpp) # creates the executable
target_include_directories(protocol PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Apply static linking ONLY when cross-compiling for Windows
if(WIN32)
    target_link_options(protocol PRIVATE "-static" "-static-libgcc" "-static-libstdc++")
endif()

# Standard packaging setup
set(CPACK_PROJECT_NAME ${PROJECT_NAME}) # sets the project name in the packed version to the project name at the top of this file
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION}) # sets the project version. 
include(CPack)