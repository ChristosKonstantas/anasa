add_library(AnasaQueues INTERFACE)

target_include_directories(AnasaQueues INTERFACE ${CMAKE_SOURCE_DIR})

target_sources(AnasaQueues INTERFACE ${CMAKE_SOURCE_DIR}/utils/queues/SpscQueue.hpp)