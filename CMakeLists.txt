cmake_minimum_required(VERSION 3.24)

project(viewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build types (default to Debug for single-config generators)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()
if(NOT CMAKE_CONFIGURATION_TYPES)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

find_library(SDL_LIB NAMES SDL3 PATHS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/")
find_library(OGDF_LIB NAMES OGDF PATHS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ogdf/lib")
find_library(COIN_LIB NAMES COIN PATHS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ogdf/lib")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
	src/main.cpp
)

#if(WIN32)
#	add_executable(${PROJECT_NAME} WIN32 ${SOURCES})
#else()
add_executable(${PROJECT_NAME} ${SOURCES})
#endif()

target_include_directories(${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}/vendor/
		${CMAKE_CURRENT_SOURCE_DIR}/vendor/ogdf/include
		${CMAKE_CURRENT_SOURCE_DIR}/vendor/ogdf/include/ogdf-release
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		${SDL_LIB}
		${OGDF_LIB}
		${COIN_LIB}
)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
	# Silence MSVC/clang-cl secure CRT deprecation noise (strncpy, sscanf, etc.).
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
