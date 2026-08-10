set(TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(AnasaTests STATIC
    ${TESTS_DIR}/../utils/queues/SpscQueue.hpp
    ${TESTS_DIR}/../utils/queues/SpscQueue.cpp

)

target_include_directories(AnasaTests PUBLIC ${CMAKE_SOURCE_DIR})


# Build tests only if BUILD_TESTS is enabled
if(BUILD_TESTS)
  target_compile_definitions(AnasaTests PUBLIC ACTIVE_TEST=1)
  add_subdirectory(utils)
endif()