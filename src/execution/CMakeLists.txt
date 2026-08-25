anasa_add_module(   
    executor
    SOURCES
        ExecutorTypes.hpp
        ExecutorSettings.hpp
        ExecutorConstants.hpp
        Executor.cpp
        Executor.hpp
)

target_link_libraries(executor PUBLIC renderer)