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
    if (m_isLoaded)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = m_pBackgroundTexAsset->imgInfo.texDescHeap->GetCPUDescriptorHandleForHeapStart();
        pDevice->CopyDescriptorsSimple(1, envMapBkGrdDescriptorHeapHandle, srcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
}

void EnvironmentMap::AttachEnvMapIBLGPUResource(ID3D12Device5* pDevice,
                                                D3D12_CPU_DESCRIPTOR_HANDLE diffIrradianceDescHeapHandle,
                                                D3D12_CPU_DESCRIPTOR_HANDLE envBrdfDescHeapHandle,
                                                D3D12_CPU_DESCRIPTOR_HANDLE prefilterEnvMapDescHeapHandle)
{
    if (m_isLoaded)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE diffIrradanceSrcHandle = m_pDiffIrradianceTexAsset->imgInfo.texDescHeap->GetCPUDescriptorHandleForHeapStart();
        pDevice->CopyDescriptorsSimple(1, diffIrradianceDescHeapHandle, diffIrradanceSrcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE envBrdfSrcHandle = m_pEnvBrdfTexAsset->imgInfo.texDescHeap->GetCPUDescriptorHandleForHeapStart();
        pDevice->CopyDescriptorsSimple(1, envBrdfDescHeapHandle, envBrdfSrcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE prefilterEnvMapSrcHandle = m_pPrefilteredEnvTexAsset->imgInfo.texDescHeap->GetCPUDescriptorHandleForHeapStart();
        pDevice->CopyDescriptorsSimple(1, prefilterEnvMapDescHeapHandle, prefilterEnvMapSrcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
}