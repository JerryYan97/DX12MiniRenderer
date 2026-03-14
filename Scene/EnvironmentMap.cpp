#include "EnvironmentMap.h"
#include "../Utils/AssetManager.h"

const std::string EnvironmentMap::ENV_MAP_TEX_ASSET_NAME = "EnvironmentMapTexture";

void EnvironmentMap::LoadEnvironmentMap(const std::string& filepath)
{
    m_pEnvMapAsset = AssetManager::GetInstance()->LoadEnvMapAsset(filepath);
    m_isLoaded = true;
}

void EnvironmentMap::AttachEnvMapGPUResource(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE envMapBkGrdDescriptorHeapHandle)
{
    if (m_isLoaded && m_pEnvMapAsset != nullptr)
    {
        // For now, we only attach the background cubemap to the GPU resource. We can also attach the other IBL related textures in the future if needed.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = 1;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        pDevice->CreateShaderResourceView(m_pEnvMapAsset->backGroundCubemap.gpuResource, &srvDesc, envMapBkGrdDescriptorHeapHandle);
    }
}