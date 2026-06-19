#pragma once
#include <string>
#include "../Utils/Asset.h"

class EnvironmentMap
{
public:
    EnvironmentMap() {}
    ~EnvironmentMap() {}

    static const std::string ENV_MAP_TEX_ASSET_NAME;

    void InitEnvironmentMap(TextureAsset* pEnvMapTextureAsset);
    bool IsLoaded() const { return m_isLoaded; }
    void AttachEnvMapGPUResource(ID3D12Device5* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE envMapBkGrdDescriptorHeapHandle); // Multiple Heaps in Future.

private:
    bool m_isLoaded = false;
    TextureAsset* m_pEnvMapTextureAsset = nullptr; // For now, we only support loading the background cubemap as the environment map. We can also add the other IBL related textures in the future if needed.
    // EnvMapAsset* m_pEnvMapAsset = nullptr;
};