target_sources(
    DX12MiniRenderer PRIVATE
    RendererBackend.h
    RendererBackend.cpp
    HWRTRenderBackend.h
    HWRTRenderBackend.cpp
    ForwardRenderBackend.h
    ForwardRenderBackend.cpp
    GpuPipeline.h
    GpuPipeline.cpp
)

add_subdirectory(./RTShaders)