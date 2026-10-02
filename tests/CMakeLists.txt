# Build tests only if BUILD_TESTS is enabled
if(BUILD_TESTS)
    # Here you should add any new test .cpp files
    set(TEST_SOURCES
        functions/Functions.hpp
        benchmarks/Benchmark.hpp
        utils/queues/SpscQueueTests.cpp
        utils/queues/TestQueuePayload.cpp
        render/TestVersionReader.cpp
        render/TestRenderKernel.cpp
        render/RenderTests.cpp
        render/SyntheticRenderKernelTests.cpp
        render/RenderCancellationTests.cpp
        execution/TestTileRenderer.cpp
        execution/RenderTaskProcessorTests.cpp
        execution/ExecutorContractTests.cpp
        execution/ExecutorTests.cpp
        audio/AudioSimulatorTests.cpp
        audio/AudioBlockProcessorTests.cpp
        audio/AudioBufferTests.cpp
        playback/PlaybackProtocolTests.cpp
        playback/PlaybackControllerTests.cpp
        scheduler/SchedulingPolicyTests.cpp
        scheduler/SchedulerIntegrationTests.cpp
        scheduler/SchedulerBoundaryTests.cpp
        engine/EngineTests.cpp
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
                          audio-buffer
                          audio-processing 
                          audio-simulator
                          playback
                          renderer
                          executor
                          scheduler
                          engine
    )

    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}"
        PREFIX "tests"
        FILES ${TEST_SOURCES}
    )

    include(Catch)
    catch_discover_tests(tests DISCOVERY_MODE PRE_TEST)
endif()