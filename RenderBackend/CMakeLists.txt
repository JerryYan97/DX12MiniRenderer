set(RENDERBACKEND_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/RendererBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RendererBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/HWRTRenderBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/HWRTRenderBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ForwardRenderBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ForwardRenderBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GpuPipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/GpuPipeline.cpp
    PARENT_SCOPE)

target_sources(DX12MiniRenderer PRIVATE ${RENDERBACKEND_SRC})

add_subdirectory(./RTShaders)