anasa_add_module(
    funcs
    HEADER_ONLY
    SOURCES
        Functions.hpp
)

target_include_directories(funcs INTERFACE ${PROJECT_SOURCE_DIR}/tests)