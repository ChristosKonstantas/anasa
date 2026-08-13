anasa_add_module(
    audio-pipeline
    SOURCES
        AudioSimulator.hpp
        AudioSimulator.cpp
        AudioConstants.hpp
        AudioSettings.hpp
        AudioTypes.hpp
)

target_link_libraries(audio-pipeline PUBLIC queues playback)