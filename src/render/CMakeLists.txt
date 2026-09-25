anasa_add_module(
    render-contracts
    HEADER_ONLY
    SOURCES
        IChunkVersionReader.hpp
        ITileRenderer.hpp
        RenderConstants.hpp
        RenderTypes.hpp
)

anasa_add_module(
    renderer
    SOURCES
        RenderFrameUtils.hpp
        Renderer.cpp
        Renderer.hpp
        RenderSettings.hpp
        VersionTable.cpp
        VersionTable.hpp
)

target_link_libraries(renderer PUBLIC render-contracts)