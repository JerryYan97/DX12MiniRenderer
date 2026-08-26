#include "EnvironmentMap.h"
#include "../Utils/AssetManager.h"

const std::string EnvironmentMap::ENV_MAP_TEX_ASSET_NAME = "EnvironmentMapTexture";

void EnvironmentMap::InitEnvironmentMap(TextureAsset* pEnvMapTextureAsset, TextureAsset* pDiffIrradianceTextureAsset, TextureAsset* pEnvBrdfTextureAsset, TextureAsset* pPrefilterEnvMapTextureAsset)
{
    m_pBackgroundTexAsset = pEnvMapTextureAsset;
    m_pDiffIrradianceTexAsset = pDiffIrradianceTextureAsset;
    m_pEnvBrdfTexAsset = pEnvBrdfTextureAsset;
    m_pPrefilteredEnvTexAsset = pPrefilterEnvMapTextureAsset;
    m_isLoaded = true;
}

void EnvironmentMap::AttachEnvMapGPUResource(ID3D12Device5* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE envMapBkGrdDescriptorHeapHandle)
{
    if (m_isLoaded && m_pBackgroundTexAsset != nullptr)
    {
        // For now, we only attach the background cubemap to the GPU resource. We can also attach the other IBL related textures in the future if needed.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = 1;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        pDevice->CreateShaderResourceView(m_pBackgroundTexAsset->imgInfo.gpuResource, &srvDesc, envMapBkGrdDescriptorHeapHandle);
    }
}

void EnvironmentMap::AttachEnvMapIBLGPUResource(ID3D12Device5* pDevice,
                                                D3D12_CPU_DESCRIPTOR_HANDLE diffIrradianceDescHeapHandle,
                                                D3D12_CPU_DESCRIPTOR_HANDLE envBrdfDescHeapHandle,
                                                D3D12_CPU_DESCRIPTOR_HANDLE prefilterEnvMapDescHeapHandle)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC diffIrradanceSrvDesc{};
    {
        diffIrradanceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        diffIrradanceSrvDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
        diffIrradanceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        diffIrradanceSrvDesc.TextureCube.MipLevels = 1;
        diffIrradanceSrvDesc.TextureCube.MostDetailedMip = 0;
        diffIrradanceSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    }
    pDevice->CreateShaderResourceView(m_pDiffIrradianceTexAsset->imgInfo.gpuResource, &diffIrradanceSrvDesc, diffIrradianceDescHeapHandle);

    D3D12_SHADER_RESOURCE_VIEW_DESC envBrdfSrvDesc{};
    {
        envBrdfSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        envBrdfSrvDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
        envBrdfSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        envBrdfSrvDesc.Texture2D.MipLevels = 1;
        envBrdfSrvDesc.Texture2D.MostDetailedMip = 0;
        envBrdfSrvDesc.Texture2D.PlaneSlice = 0;
        envBrdfSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    }
    pDevice->CreateShaderResourceView(m_pEnvBrdfTexAsset->imgInfo.gpuResource, &envBrdfSrvDesc, envBrdfDescHeapHandle);

    D3D12_SHADER_RESOURCE_VIEW_DESC prefilterEnvMapSrvDesc{};
    {
        prefilterEnvMapSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        prefilterEnvMapSrvDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
        prefilterEnvMapSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        prefilterEnvMapSrvDesc.TextureCube.MipLevels = m_pPrefilteredEnvTexAsset->imgInfo.mipLevelCnt;
        prefilterEnvMapSrvDesc.TextureCube.MostDetailedMip = 0;
        prefilterEnvMapSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    }
    pDevice->CreateShaderResourceView(m_pPrefilteredEnvTexAsset->imgInfo.gpuResource, &prefilterEnvMapSrvDesc, prefilterEnvMapDescHeapHandle);
}