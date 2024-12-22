#include "AssetManager.h"
#include "DX12Utils.h"

extern ID3D12Device* g_pD3dDevice;

void AssetManager::Deinit()
{
    for (const auto& itr : m_primitiveAssets)
    {
        for (const auto& primItr : itr.second)
        {
            if (primItr->m_gpuVertBuffer) { primItr->m_gpuVertBuffer->Release(); }
            if (primItr->m_gpuIndexBuffer) { primItr->m_gpuIndexBuffer->Release(); }
            delete primItr;
        }
    }
}

void AssetManager::LoadAssets()
{
}

void AssetManager::UnloadAssets()
{
}

void AssetManager::SaveModelPrimAssetAndCreateGpuRsrc(const std::string& name, PrimitiveAsset* pPrimitiveAsset)
{
    const uint32_t idxBufferSizeByte = pPrimitiveAsset->m_idxType ?
                                                   sizeof(uint32_t) * pPrimitiveAsset->m_idxDataUint32.size() :
                                                   sizeof(uint16_t) * pPrimitiveAsset->m_idxDataUint16.size();
    const uint32_t vertCnt = pPrimitiveAsset->m_posData.size() / 3;
    const uint32_t vertSizeFloat = (3 + 3 + 4 + 2); // Position(3) + Normal(3) + Tangent(4) + TexCoord(2).
    const uint32_t vertSizeByte = sizeof(float) * vertSizeFloat;
    const uint32_t vertexBufferSize = vertCnt * vertSizeByte;
    pPrimitiveAsset->m_vertData.resize(vertCnt * vertSizeFloat);

    for (uint32_t i = 0; i < vertCnt; i++)
    {
        memcpy(&pPrimitiveAsset->m_vertData[i * vertSizeFloat], &pPrimitiveAsset->m_posData[i * 3], sizeof(float) * 3);
        memcpy(&pPrimitiveAsset->m_vertData[i * vertSizeFloat + 3], &pPrimitiveAsset->m_normalData[i * 3], sizeof(float) * 3);
        memcpy(&pPrimitiveAsset->m_vertData[i * vertSizeFloat + 6], &pPrimitiveAsset->m_tangentData[i * 4], sizeof(float) * 4);
        memcpy(&pPrimitiveAsset->m_vertData[i * vertSizeFloat + 10], &pPrimitiveAsset->m_texCoordData[i * 2], sizeof(float) * 2);
    }

    D3D12_HEAP_PROPERTIES heapProperties{};
    {
        heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;
    }

    D3D12_RESOURCE_DESC bufferRsrcDesc{};
    {
        bufferRsrcDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferRsrcDesc.Alignment = 0;
        bufferRsrcDesc.Width = vertexBufferSize;
        bufferRsrcDesc.Height = 1;
        bufferRsrcDesc.DepthOrArraySize = 1;
        bufferRsrcDesc.MipLevels = 1;
        bufferRsrcDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferRsrcDesc.SampleDesc.Count = 1;
        bufferRsrcDesc.SampleDesc.Quality = 0;
        bufferRsrcDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferRsrcDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    }

    D3D12_RESOURCE_DESC idxBufferRsrcDesc = bufferRsrcDesc;
    idxBufferRsrcDesc.Width = idxBufferSizeByte;

    ThrowIfFailed(g_pD3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferRsrcDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&pPrimitiveAsset->m_gpuVertBuffer)));

    ThrowIfFailed(g_pD3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &idxBufferRsrcDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&pPrimitiveAsset->m_gpuIndexBuffer)
    ));

    // Copy the triangle data to the vertex buffer.
    void* pVertexDataBegin;
    D3D12_RANGE readRange{ 0, 0 };        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(pPrimitiveAsset->m_gpuVertBuffer->Map(0, &readRange, &pVertexDataBegin));
    memcpy(pVertexDataBegin, pPrimitiveAsset->m_vertData.data(), vertexBufferSize);
    pPrimitiveAsset->m_gpuVertBuffer->Unmap(0, nullptr);

    // Initialize the vertex buffer view.
    pPrimitiveAsset->m_vertexBufferView.BufferLocation = pPrimitiveAsset->m_gpuVertBuffer->GetGPUVirtualAddress();
    pPrimitiveAsset->m_vertexBufferView.StrideInBytes = vertSizeByte;
    pPrimitiveAsset->m_vertexBufferView.SizeInBytes = vertexBufferSize;

    // Copy the model idx data to the idx buffer.
    void* pIdxDataBegin;
    ThrowIfFailed(pPrimitiveAsset->m_gpuIndexBuffer->Map(0, &readRange, &pIdxDataBegin));
    if (pPrimitiveAsset->m_idxType)
    {
        memcpy(pIdxDataBegin, pPrimitiveAsset->m_idxDataUint32.data(), idxBufferSizeByte);
    }
    else
    {
        memcpy(pIdxDataBegin, pPrimitiveAsset->m_idxDataUint16.data(), idxBufferSizeByte);
    }
    pPrimitiveAsset->m_gpuIndexBuffer->Unmap(0, nullptr);

    // Initialize the index buffer view.
    pPrimitiveAsset->m_idxBufferView.BufferLocation = pPrimitiveAsset->m_gpuIndexBuffer->GetGPUVirtualAddress();
    pPrimitiveAsset->m_idxBufferView.Format = pPrimitiveAsset->m_idxType ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    pPrimitiveAsset->m_idxBufferView.SizeInBytes = idxBufferSizeByte;

    if (m_primitiveAssets.count(name) > 0)
    {
        m_primitiveAssets[name].push_back(pPrimitiveAsset);
    }
    else
    {
        m_primitiveAssets[name] = { pPrimitiveAsset };
    }
}

void AssetManager::CreateVertIdxBuffer(PrimitiveAsset* pPrimAsset)
{

}