anasa_add_module(
    render-contracts
    HEADER_ONLY
    SOURCES
        IChunkVersionReader.hpp
        ITileRenderer.hpp
        IPhraseRenderer.hpp
        PhraseRenderTypes.hpp
        kernels/IRenderKernel.hpp
        RenderConstants.hpp
        RenderLayout.hpp
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
        RenderCancellation.cpp
        RenderCancellation.hpp
        kernels/SyntheticRenderKernel.cpp
        kernels/SyntheticRenderKernel.hpp
        kernels/PcmRenderKernel.hpp
        PreparedSingingTimeline.hpp
        PhraseTileRenderer.hpp
        PhraseTileRenderer.cpp
)

if(OPENVPI)
    add_subdirectory(openvpi)
endif()

target_link_libraries(render-contracts INTERFACE audio-buffer Threads::Threads)
target_link_libraries(renderer PUBLIC render-contracts)