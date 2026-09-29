anasa_add_module(
    audio-buffer
    SOURCES
        AudioBuffer.hpp
        AudioBuffer.cpp
)

anasa_add_module(
    audio-contracts
    HEADER_ONLY
    SOURCES
        AudioConstants.hpp
        AudioSettings.hpp
        AudioTypes.hpp
        IAudioBlockProcessor.hpp
        AudioOutputBuffer.hpp
)

anasa_add_module(
    audio-processing
    SOURCES
        AudioBlockProcessor.hpp
        AudioBlockProcessor.cpp
)

target_link_libraries(audio-processing PUBLIC audio-contracts queues playback)

anasa_add_module(
    audio-simulator
    SOURCES
        simulator/AudioSimulator.hpp
        simulator/AudioSimulator.cpp
)

target_link_libraries(audio-simulator PUBLIC audio-contracts audio-buffer)