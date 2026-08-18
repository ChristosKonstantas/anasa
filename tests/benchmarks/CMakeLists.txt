anasa_add_module(
    benchmarks
    HEADER_ONLY
    FOLDER "tests"
    SOURCES
        Benchmark.hpp
)

target_include_directories(benchmarks INTERFACE ${PROJECT_SOURCE_DIR}/tests)