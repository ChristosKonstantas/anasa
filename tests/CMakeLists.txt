set(TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Build tests only if BUILD_TESTS is enabled
if(BUILD_TESTS)
    # Here you should add any new test .cpp files
    set(TEST_SOURCES
        utils/queues/SpscQueueTests.cpp
        audio-pipeline/AudioSimulatorTests.cpp
    )

    add_executable(AnasaTests ${TEST_SOURCES})

    # Here you should add any new declared interface
    target_link_libraries(AnasaTests
                           PRIVATE 
                          Catch2::Catch2WithMain
                          queues
                          audio-pipeline
                          playback
    )

    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}"
        PREFIX "Tests"
        FILES ${TEST_SOURCES}
    )

    include(Catch)
    catch_discover_tests(AnasaTests)
    
    if(BENCHMARKS)
        target_compile_definitions(AnasaTests PRIVATE enable_benchmarks)
    endif()
endif()