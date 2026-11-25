cmake_minimum_required(VERSION 3.22)

project(anasa LANGUAGES CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_SOURCE_DIR}/cmake/AnasaSanitizers.cmake)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(BUILD_TESTS "Build unit tests" OFF)

include(CMakeDependentOption)

cmake_dependent_option(
    BENCHMARKS
    "Enable benchmark test cases"
    OFF
    "BUILD_TESTS"
    OFF
)

# Anasa CMake helpers
include(${CMAKE_SOURCE_DIR}/cmake/AnasaModule.cmake)


# External dependencies
if(BUILD_TESTS)
    enable_testing()

    message(STATUS "BUILD_TESTS is ON. Configuring Catch2...")

    if(DEFINED ENV{CATCH2_LIB})
        cmake_path(
            CONVERT "$ENV{CATCH2_LIB}"
            TO_CMAKE_PATH_LIST catch2_SOURCE_DIR
            NORMALIZE
        )

        if(NOT EXISTS "${catch2_SOURCE_DIR}/CMakeLists.txt")
            message(
                FATAL_ERROR
                "CATCH2_LIB is set to '$ENV{CATCH2_LIB}', "
                "but that does not look like the Catch2 source root."
            )
        endif()

        message(STATUS "Catch2: using local checkout at ${catch2_SOURCE_DIR}")

        add_subdirectory(
            "${catch2_SOURCE_DIR}"
            "${CMAKE_BINARY_DIR}/_deps/catch2-build"
            EXCLUDE_FROM_ALL
        )
    else()
        message(STATUS "Catch2: fetching via FetchContent")

        include(FetchContent)

        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.4
        )

        FetchContent_MakeAvailable(Catch2)
    endif()

    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    list(REMOVE_DUPLICATES CMAKE_MODULE_PATH)

    set_target_properties(
        Catch2
        Catch2WithMain
        PROPERTIES
            FOLDER "ThirdParty"
    )

    add_subdirectory(tests)
endif()

# Code
option(OPENVPI "Build the TIGER v106 ONNX renderer" OFF)
option(NN_BENCHMARK "Build the standalone inference benchmark" OFF)

if(NN_BENCHMARK AND NOT OPENVPI)
    message(FATAL_ERROR "NN_BENCHMARK requires OPENVPI=ON")
endif()

add_subdirectory(src)
add_subdirectory(runtime-demo)

if(NN_BENCHMARK)
    add_subdirectory(apps)
endif()