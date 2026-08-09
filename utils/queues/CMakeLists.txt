# libs/math/CMakeLists.txt
add_library(queues)

target_sources(queues PRIVATE SpscQueue.cpp)

target_include_directories(queues PUBLIC SpscQueue.hpp)

target_compile_definitions(queues PRIVATE QUEUES_INTERNAL_BUILD)