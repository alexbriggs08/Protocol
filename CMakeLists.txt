cmake_minimum_required(VERSION 3.10.0)
project(protocol VERSION 0.1.0 LANGUAGES C CXX)

# Ensure we use C++17 for features like std::clamp
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Standard executable (Terminal-based)
add_executable(protocol src/main.cpp)
target_include_directories(protocol PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Apply static linking ONLY when cross-compiling for Windows
if(WIN32)
    target_link_options(protocol PRIVATE "-static" "-static-libgcc" "-static-libstdc++")
endif()

# Standard packaging setup
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)