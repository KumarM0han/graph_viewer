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

include(FetchContent)

# Resolve SDL3
find_library(SDL_LIB NAMES SDL3 PATHS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/")
if(SDL_LIB)
    message(STATUS "Found local static SDL3: ${SDL_LIB}")
else()
    message(STATUS "Local SDL3 not found. Fetching and building from source...")
    set(SDL_SHARED OFF CACHE BOOL "Build SDL shared" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build SDL static" FORCE)
    FetchContent_Declare(
        SDL
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG preview-3.1.6
    )
    FetchContent_MakeAvailable(SDL)
    set(SDL_LIB SDL3::SDL3-static)
endif()

# Resolve OGDF
find_library(OGDF_LIB NAMES OGDF PATHS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ogdf/lib")
find_library(COIN_LIB NAMES COIN PATHS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ogdf/lib")

if(OGDF_LIB)
    message(STATUS "Found local static OGDF: ${OGDF_LIB}")
    set(OGDF_LIBRARIES ${OGDF_LIB})
    if(COIN_LIB)
        list(APPEND OGDF_LIBRARIES ${COIN_LIB})
    endif()
    set(OGDF_INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ogdf/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ogdf/include/ogdf-release"
    )
else()
    message(STATUS "Local OGDF not found. Fetching and building from source (may take a while)...")
    FetchContent_Declare(
        ogdf
        GIT_REPOSITORY https://github.com/ogdf/ogdf.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(ogdf)
    set(OGDF_LIBRARIES ogdf)
    set(OGDF_INCLUDES "")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
	src/main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}/vendor/
		${OGDF_INCLUDES}
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		${SDL_LIB}
		${OGDF_LIBRARIES}
)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
	# Silence MSVC/clang-cl secure CRT deprecation noise (strncpy, sscanf, etc.).
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
