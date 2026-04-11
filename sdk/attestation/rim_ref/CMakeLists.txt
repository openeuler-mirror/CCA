# CMake version
cmake_minimum_required(VERSION 3.16)
# Project
project(gen_rim_ref LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# Build mode
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Build options
option(BUILD_RIM_GEN "Compile rim reference generator" ON)

# Set source file
file(GLOB SOURCES
    src/gen_rim_ref.c
)

# Set include directory
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${GLIB_INCLUDE_DIRS}
)

# Generate excutable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link library
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    ${GLIB_LIBRARIES}
)
target_compile_definitions(${PROJECT_NAME} PRIVATE LOG_PRINT=0)

# Set install path
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set_directory_properties(PROPERTIES
    ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

if(BUILD_RIM_GEN)
    message(STATUS "Build gen_rim_ref: on")
endif()
