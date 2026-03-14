#pragma once
#include <string>
#include <d3d12.h>

struct EnvMapAsset;

class EnvironmentMap
{
public:
    EnvironmentMap() {}
    ~EnvironmentMap() {}

    static const std::string ENV_MAP_TEX_ASSET_NAME;
    
    void LoadEnvironmentMap(const std::string& filepath);
    bool IsLoaded() const { return m_isLoaded; }
    void AttachEnvMapGPUResource(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE envMapBkGrdDescriptorHeapHandle); // Multiple Heaps in Future.

private:
    bool m_isLoaded = false;
    EnvMapAsset* m_pEnvMapAsset = nullptr;
};