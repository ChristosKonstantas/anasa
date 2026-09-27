anasa_add_module(
    audio-pipeline
    SOURCES
        simulator/AudioSimulator.hpp
        simulator/AudioSimulator.cpp
        AudioConstants.hpp
        AudioSettings.hpp
        AudioTypes.hpp
)

target_link_libraries(audio-pipeline PUBLIC queues playback)