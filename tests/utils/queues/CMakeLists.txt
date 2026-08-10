cmake_minimum_required(VERSION 3.22)

set(QUEUES_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(QueueTests
    SpscQueueTests.cpp
    
)

target_link_libraries(QueueTests PRIVATE AnasaTests Catch2::Catch2WithMain)

target_compile_definitions(QueueTests PRIVATE ACTIVE_TEST=1)

source_group(
    TREE ${QUEUES_TEST_DIR}
    PREFIX "Queue Tests"
    FILES ${QUEUES_TEST_DIR}/SpscQueueTests.cpp
)

include(Catch)
catch_discover_tests(QueueTests)