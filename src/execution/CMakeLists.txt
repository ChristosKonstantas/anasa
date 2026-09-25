anasa_add_module(
    executor-contracts
    HEADER_ONLY
    SOURCES
        IRenderExecutor.hpp
        ExecutorTypes.hpp
)

target_link_libraries(executor-contracts INTERFACE render-contracts)

anasa_add_module(
    executor
    SOURCES
        ExecutorSettings.hpp
        ExecutorConstants.hpp
        Executor.cpp
        Executor.hpp
)

target_link_libraries(executor PUBLIC executor-contracts)