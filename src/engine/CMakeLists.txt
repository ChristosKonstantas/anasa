anasa_add_module(   
    engine
    SOURCES
        EngineSettings.hpp
        EngineTypes.hpp
        Engine.cpp
        Engine.hpp
)

target_link_libraries(engine PUBLIC audio-processing audio-simulator queues playback renderer executor scheduler)