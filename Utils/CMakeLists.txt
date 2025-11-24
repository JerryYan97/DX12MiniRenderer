set(UTILS_SRC
    crc32.h
    GltfUtils.cpp
    GltfUtils.h
    StrPathUtils.cpp
    StrPathUtils.h
    MathUtils.cpp
    MathUtils.h
    DX12Utils.cpp
    DX12Utils.h
    DescriptorManager.cpp
    DescriptorManager.h
    AssetManager.h
    AssetManager.cpp)

target_sources(DX12MiniRenderer PRIVATE ${UTILS_SRC})