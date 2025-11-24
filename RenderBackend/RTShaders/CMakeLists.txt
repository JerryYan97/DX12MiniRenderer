set(RT_SHADERS_SRC
    RaytracingHlslCompat.h
    Raytracing.hlsl.h
    shader.fxh
    CustomRTShader.fxh)

target_sources(DX12MiniRenderer PRIVATE ${RT_SHADERS_SRC})