cmake_minimum_required(VERSION 3.14)
project(protocol VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find raylib (installed via brew on macOS, or any system install)
find_package(raylib REQUIRED)

# Generate version.h from version.h.in
configure_file(src/version.h.in src/version.h)

add_executable(protocol src/main.cpp)

target_include_directories(protocol PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/src
)

target_link_libraries(protocol PRIVATE raylib)

# macOS system frameworks required by raylib
if(APPLE)
    target_link_libraries(protocol PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework OpenGL"
    )
endif()

# Windows static linking
# NOTE: for cross-compilation from macOS, CMAKE_PREFIX_PATH must point to a
# MinGW-built raylib — the macOS brew install is not usable for cross-compile.
if(WIN32)
    target_link_options(protocol PRIVATE "-static" "-static-libgcc" "-static-libstdc++")
endif()

# Warn at configure time if the font asset is missing (run Task 2 to download it)
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/assets/fonts/ShareTechMono-Regular.ttf")
    message(WARNING "Font file missing: assets/fonts/ShareTechMono-Regular.ttf — download it before running the game.")
endif()

# Copy assets/ directory next to the built executable after each build
add_custom_command(TARGET protocol POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:protocol>/assets
    COMMENT "Copying assets"
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
