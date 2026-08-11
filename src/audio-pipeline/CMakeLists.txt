anasa_add_module(
    audio-pipeline
    SOURCES
        AudioSimulator.hpp
        AudioSimulator.cpp
)

target_link_libraries(audio-pipeline PUBLIC queues)