anasa_add_module(   
    engine
    FOLDER "engine"
    SOURCES
        EngineSettings.hpp
        EngineTypes.hpp
        Engine.cpp
        Engine.hpp
)

target_link_libraries(engine PUBLIC audio-pipeline queues playback)