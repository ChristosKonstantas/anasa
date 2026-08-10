cmake_minimum_required(VERSION 3.22)
project(anasa LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Testing -> add -DBUILD_TESTS=ON
# Catch2 (env var override otherwise FetchContent)
option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)

  enable_testing()
  include(CTest)

  message(STATUS "BUILD_TESTS is ON. Configuring Catch2...")

  if(DEFINED ENV{CATCH2_LIB} AND NOT EXISTS "$ENV{CATCH2_LIB}/CMakeLists.txt")
    message(FATAL_ERROR "CATCH2_LIB is set to '$ENV{CATCH2_LIB}', but that does not look like the Catch2 source root.")
  endif()

  if(DEFINED ENV{CATCH2_LIB} AND EXISTS "$ENV{CATCH2_LIB}/CMakeLists.txt")
    message(STATUS "Catch2: using local checkout at $ENV{CATCH2_LIB}")
    set(catch2_SOURCE_DIR "$ENV{CATCH2_LIB}")
    add_subdirectory("$ENV{CATCH2_LIB}" "${CMAKE_BINARY_DIR}/_deps/catch2-build" EXCLUDE_FROM_ALL)
  else()
    message(STATUS "Catch2: fetching via FetchContent")
    include(FetchContent)
    FetchContent_Declare(Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        v3.5.4
    )
    FetchContent_MakeAvailable(Catch2)
  endif()

  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

endif()


add_subdirectory(utils)

add_subdirectory(tests)