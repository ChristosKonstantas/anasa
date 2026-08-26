anasa_add_module(   
    engine
    SOURCES
        EngineSettings.hpp
        Engine.cpp
        Engine.hpp
)

target_link_libraries(engine PUBLIC audio-pipeline queues playback renderer executor scheduler)