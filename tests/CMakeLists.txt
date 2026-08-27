# Build tests only if BUILD_TESTS is enabled
if(BUILD_TESTS)
    # Here you should add any new test .cpp files
    set(TEST_SOURCES
        functions/Functions.hpp
        benchmarks/Benchmark.hpp
        utils/queues/SpscQueueTests.cpp
        audio-pipeline/AudioSimulatorTests.cpp
        render/RenderTests.cpp
        execution/ExecutorTests.cpp
    )

    add_executable(tests ${TEST_SOURCES})
    
    add_subdirectory(functions)

    if(BENCHMARKS)
        target_compile_definitions(tests PRIVATE enable_benchmarks)
        add_subdirectory(benchmarks)
        target_link_libraries(tests PRIVATE benchmarks)
    endif()
    
    # Here you should add any new declared interface
    target_link_libraries(tests
                           PRIVATE 
                          Catch2::Catch2WithMain
                          funcs
                          queues
                          audio-pipeline
                          playback
                          renderer
                          executor
    )

    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}"
        PREFIX "tests"
        FILES ${TEST_SOURCES}
    )

    include(Catch)
    catch_discover_tests(tests DISCOVERY_MODE PRE_TEST)
endif()