#include "AssetManager.h"
#include "DX12Utils.h"
#include "../Scene/Mesh.h"
#include <unordered_set>
#include <cassert>
#include "../ThirdParty/TinyGltf/stb_image.h"
#include "../ThirdParty/TinyGltf/tiny_gltf.h"

extern ID3D12Device* g_pD3dDevice;
AssetManager* AssetManager::m_pThis = nullptr;

void AssetManager::Deinit()
{
    for (const auto& itr : m_primitiveAssets)
    {
        for (const auto& primItr : itr.second)
        {
            if (primItr->m_gpuVertBuffer) { primItr->m_gpuVertBuffer->Release(); }
            if (primItr->m_gpuIndexBuffer) { primItr->m_gpuIndexBuffer->Release(); }
            if (primItr->m_pTexturesSrvHeap) { primItr->m_pTexturesSrvHeap->Release(); }
            if (primItr->m_baseColorTex.isSentToGpu) { primItr->m_baseColorTex.gpuResource->Release(); }
            if (primItr->m_metallicRoughnessTex.isSentToGpu) { primItr->m_metallicRoughnessTex.gpuResource->Release(); }
            if (primItr->m_normalTex.isSentToGpu) { primItr->m_normalTex.gpuResource->Release(); }
            if (primItr->m_occlusionTex.isSentToGpu) { primItr->m_occlusionTex.gpuResource->Release(); }
            if (primItr->m_emissiveTex.isSentToGpu) { primItr->m_emissiveTex.gpuResource->Release(); }
            if (primItr->m_materialMaskBuffer) { primItr->m_materialMaskBuffer->Release(); }
            if (primItr->m_pMaterialMaskCbvHeap) { primItr->m_pMaterialMaskCbvHeap->Release(); }
            if (primItr->m_blas) { primItr->m_blas->Release(); }

            delete primItr;
        }
    }

    for (const auto& itr : m_textureAssets)
    {
        itr.second->imgInfo.gpuResource->Release();
        delete itr.second;
    }

    if (m_envMapAsset != nullptr)
    {
        m_envMapAsset->backGroundCubemap.gpuResource->Release();
        m_envMapAsset->diffuseIrradianceCubemap.gpuResource->Release();
        m_envMapAsset->envBRDF.gpuResource->Release();
        m_envMapAsset->prefilteredEnvMap.gpuResource->Release();
        delete m_envMapAsset;
    }
}

void AssetManager::LoadAssets()
{
}

void AssetManager::UnloadAssets()
{
}

void AssetManager::LoadStaticMeshAssets(const std::string& modelName, StaticMesh* pStaticMesh)
{
    if (m_primitiveAssets.count(modelName) > 0)
    {
        pStaticMesh->m_primitiveAssets = m_primitiveAssets[modelName];
    }
    else
    {
        assert(false, "Cannot find the model.");
    }
}

void AssetManager::SaveModelPrimAssetAndCreateGpuRsrc(const std::string& name, PrimitiveAsset* pPrimitiveAsset)
{
    const uint32_t idxBufferSizeByte = pPrimitiveAsset->m_idxType ?
                                                   sizeof(uint32_t) * pPrimitiveAsset->m_idxDataUint32.size() :
                                                   sizeof(uint16_t) * pPrimitiveAsset->m_idxDataUint16.size();
    const uint32_t vertCnt = pPrimitiveAsset->m_posData.size() / 3;
    const uint32_t vertSizeFloat = VERT_SIZE_FLOAT; // Position(3) + Normal(3) + Tangent(4) + TexCoord(2).
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

    // Generate the material textures and constant material buffer.
    GenMaterialTexBuffer(pPrimitiveAsset);

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

void AssetManager::GenMaterialTexBuffer(PrimitiveAsset* pPrimAsset)
{
    const uint32_t cbvSrvUavDescHandleOffset = g_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    // Describe and create a Texture2D.
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Width = 0;
    textureDesc.Height = 0;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    D3D12_HEAP_PROPERTIES heapProperties{};
    {
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE descHeapPtr{};

    if (pPrimAsset->TextureCnt() > 0)
    {
        uint32_t texCnt = pPrimAsset->TextureCnt();
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = texCnt;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(g_pD3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&pPrimAsset->m_pTexturesSrvHeap)));
        descHeapPtr = pPrimAsset->m_pTexturesSrvHeap->GetCPUDescriptorHandleForHeapStart();
    }

    uint32_t texHeapOffset = 0;
    if (pPrimAsset->m_baseColorTex.pixWidth > 1)
    {
        pPrimAsset->m_baseColorTex.isSentToGpu = true;
        textureDesc.Width = pPrimAsset->m_baseColorTex.pixWidth;
        textureDesc.Height = pPrimAsset->m_baseColorTex.pixHeight;

        ThrowIfFailed(g_pD3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&pPrimAsset->m_baseColorTex.gpuResource)));

        pPrimAsset->m_baseColorTex.gpuResource->SetName(L"BaseColorTexture");

        SendDataToTexture2D(g_pD3dDevice,
            pPrimAsset->m_baseColorTex.gpuResource,
            pPrimAsset->m_baseColorTex.dataVec.data(),
            pPrimAsset->m_baseColorTex.dataVec.size());

        g_pD3dDevice->CreateShaderResourceView(pPrimAsset->m_baseColorTex.gpuResource,
            &srvDesc,
            descHeapPtr);

        descHeapPtr.ptr += cbvSrvUavDescHandleOffset;
        pPrimAsset->m_baseColorTex.srvHeapIdx = texHeapOffset;
        texHeapOffset++;
    }

    if (pPrimAsset->m_metallicRoughnessTex.pixWidth > 1)
    {
        pPrimAsset->m_metallicRoughnessTex.isSentToGpu = true;
        textureDesc.Width = pPrimAsset->m_metallicRoughnessTex.pixWidth;
        textureDesc.Height = pPrimAsset->m_metallicRoughnessTex.pixHeight;

        ThrowIfFailed(g_pD3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&pPrimAsset->m_metallicRoughnessTex.gpuResource)));

        pPrimAsset->m_metallicRoughnessTex.gpuResource->SetName(L"MetallicRoughnessTexture");

        SendDataToTexture2D(g_pD3dDevice,
                            pPrimAsset->m_metallicRoughnessTex.gpuResource,
                            pPrimAsset->m_metallicRoughnessTex.dataVec.data(),
                            pPrimAsset->m_metallicRoughnessTex.dataVec.size());

        g_pD3dDevice->CreateShaderResourceView(pPrimAsset->m_metallicRoughnessTex.gpuResource,
                                               &srvDesc,
                                               descHeapPtr);

        descHeapPtr.ptr += cbvSrvUavDescHandleOffset;
        pPrimAsset->m_metallicRoughnessTex.srvHeapIdx = texHeapOffset;
        texHeapOffset++;
    }

    if (pPrimAsset->m_normalTex.pixWidth > 1)
    {
        pPrimAsset->m_normalTex.isSentToGpu = true;
        textureDesc.Width = pPrimAsset->m_normalTex.pixWidth;
        textureDesc.Height = pPrimAsset->m_normalTex.pixHeight;

        ThrowIfFailed(g_pD3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&pPrimAsset->m_normalTex.gpuResource)));

        pPrimAsset->m_normalTex.gpuResource->SetName(L"NormalTexture");

        SendDataToTexture2D(g_pD3dDevice,
                            pPrimAsset->m_normalTex.gpuResource,
                            pPrimAsset->m_normalTex.dataVec.data(),
                            pPrimAsset->m_normalTex.dataVec.size());

        g_pD3dDevice->CreateShaderResourceView(pPrimAsset->m_normalTex.gpuResource,
                                               &srvDesc,
                                               descHeapPtr);

        descHeapPtr.ptr += cbvSrvUavDescHandleOffset;
        pPrimAsset->m_normalTex.srvHeapIdx = texHeapOffset;
        texHeapOffset++;
    }

    if (pPrimAsset->m_occlusionTex.pixWidth > 1)
    {
        pPrimAsset->m_occlusionTex.isSentToGpu = true;
        textureDesc.Width = pPrimAsset->m_occlusionTex.pixWidth;
        textureDesc.Height = pPrimAsset->m_occlusionTex.pixHeight;

        ThrowIfFailed(g_pD3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&pPrimAsset->m_occlusionTex.gpuResource)));

        pPrimAsset->m_occlusionTex.gpuResource->SetName(L"OcclusionTexture");

        SendDataToTexture2D(g_pD3dDevice,
                            pPrimAsset->m_occlusionTex.gpuResource,
                            pPrimAsset->m_occlusionTex.dataVec.data(),
                            pPrimAsset->m_occlusionTex.dataVec.size());

        g_pD3dDevice->CreateShaderResourceView(pPrimAsset->m_occlusionTex.gpuResource,
                                               &srvDesc,
                                               descHeapPtr);

        descHeapPtr.ptr += cbvSrvUavDescHandleOffset;
        pPrimAsset->m_occlusionTex.srvHeapIdx = texHeapOffset;
        texHeapOffset++;
    }

    pPrimAsset->GenMaterialMask();
    GenPrimAssetMaterialBuffer(pPrimAsset);
}

void AssetManager::GenPrimAssetMaterialBuffer(PrimitiveAsset* pPrimAsset)
{
    pPrimAsset->GenMaterialMask();
    constexpr uint32_t CnstBufferSize = sizeof(float) * 64;

    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    // NOTE: Constant buffer needs to be padded to 256 bytes.
    D3D12_HEAP_PROPERTIES heapProperties{};
    {
        heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;
    }

    D3D12_RESOURCE_DESC bufferRsrcDesc{};
    {
        bufferRsrcDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferRsrcDesc.Alignment = 0;
        bufferRsrcDesc.Width = CnstBufferSize;
        bufferRsrcDesc.Height = 1;
        bufferRsrcDesc.DepthOrArraySize = 1;
        bufferRsrcDesc.MipLevels = 1;
        bufferRsrcDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferRsrcDesc.SampleDesc.Count = 1;
        bufferRsrcDesc.SampleDesc.Quality = 0;
        bufferRsrcDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferRsrcDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    }

    // Create and populate the static mesh constant material buffer.
    ThrowIfFailed(g_pD3dDevice->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferRsrcDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&pPrimAsset->m_materialMaskBuffer)));

    ThrowIfFailed(g_pD3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&pPrimAsset->m_pMaterialMaskCbvHeap)));

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
    {
        cbvDesc.BufferLocation = pPrimAsset->m_materialMaskBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = CnstBufferSize;
    }
    g_pD3dDevice->CreateConstantBufferView(&cbvDesc, pPrimAsset->m_pMaterialMaskCbvHeap->GetCPUDescriptorHandleForHeapStart());

    void* pConstBufferBegin;
    D3D12_RANGE readRange{ 0, 0 };
    ThrowIfFailed(pPrimAsset->m_materialMaskBuffer->Map(0, &readRange, &pConstBufferBegin));
    memcpy(pConstBufferBegin, &pPrimAsset->m_materialMask, sizeof(uint32_t));
    pPrimAsset->m_materialMaskBuffer->Unmap(0, nullptr);
}

std::vector<PrimitiveAsset*> AssetManager::GenSceneVertIdxBuffer(std::vector<float>& sceneVertBuffer, std::vector<uint16_t>& sceneIdxBuffer)
{
    std::unordered_set<PrimitiveAsset*> recordedPrims;
    std::vector<PrimitiveAsset*> prims;

    for (const auto itr : m_primitiveAssets)
    {
        for (const auto primItr : itr.second)
        {
            if (recordedPrims.count(primItr) == 0)
            {
                recordedPrims.insert(primItr);
                prims.push_back(primItr);
            }
        }
    }

    sceneVertBuffer.clear();
    sceneIdxBuffer.clear();

    for (auto prim : prims)
    {
        sceneVertBuffer.insert(sceneVertBuffer.end(), prim->m_vertData.begin(), prim->m_vertData.end());
        sceneIdxBuffer.insert(sceneIdxBuffer.end(), prim->m_idxDataUint16.begin(), prim->m_idxDataUint16.end());
    }

    return prims;
}

void AssetManager::LoadCubemapFromSingleFile(const std::string& filepath, ImgInfo& oImgInfo)
{
    // Load the cubemap texture from the file
    assert(filepath.size() > 4);
    std::string extLower = filepath.substr(filepath.size() - 4);
    assert(extLower == ".hdr" && "LoadCubemapFromSingleFile expects a .hdr file");

    int width, height, channels;
    float* data = stbi_loadf(filepath.c_str(), &width, &height, &channels, 0);
    if (data)
    {
        oImgInfo.pixWidth = static_cast<uint32_t>(width);
        oImgInfo.pixHeight = static_cast<uint32_t>(width);
        oImgInfo.componentCnt = static_cast<uint32_t>(channels);
        oImgInfo.arrayLayerCnt = 6;
        oImgInfo.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        oImgInfo.mipLevelCnt = 1;
        oImgInfo.wrapModeHorizontal = TexWrapMode::CLAMP_TO_EDGE;
        oImgInfo.wrapModeVertical = TexWrapMode::CLAMP_TO_EDGE;
        oImgInfo.dataVec.assign(data, data + (width * height * channels));

        D3D12_RESOURCE_DESC textureDesc = {};
        {
            textureDesc.MipLevels = 1;
            textureDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
            textureDesc.Width = width;
            textureDesc.Height = width;
            textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            textureDesc.DepthOrArraySize = 6;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        }

        D3D12_HEAP_PROPERTIES heapProperties{};
        {
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
            heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heapProperties.CreationNodeMask = 1;
            heapProperties.VisibleNodeMask = 1;
        }

        ThrowIfFailed(g_pD3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&oImgInfo.gpuResource)));

        for (int sliceIdx = 0; sliceIdx < 6; sliceIdx++)
        {
            float* sliceData = data + sliceIdx * (width * width * channels);
            uint32_t sliceSizeByte = width * width * channels * sizeof(float);
            SendDataToCubemapSlice(g_pD3dDevice, oImgInfo.gpuResource, sliceData, sliceSizeByte, sliceIdx, 0);
        }

        ChangeResourceState(g_pD3dDevice, oImgInfo.gpuResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        delete[] data;
    }
}

EnvMapAsset* AssetManager::LoadEnvMapAsset(const std::string& filepath)
{
    if (m_envMapAsset != nullptr)
    {
        // Release existing resources
        m_envMapAsset->backGroundCubemap.gpuResource->Release();
        m_envMapAsset->diffuseIrradianceCubemap.gpuResource->Release();
        m_envMapAsset->envBRDF.gpuResource->Release();
        m_envMapAsset->prefilteredEnvMap.gpuResource->Release();
        delete m_envMapAsset;
    }

    m_envMapAsset = new EnvMapAsset();

    // Load the new environment map asset from the file
    // Load the background cubemap.
    std::string backgroundCubemapPath = filepath + "/background_cubemap.hdr";
    LoadCubemapFromSingleFile(backgroundCubemapPath, m_envMapAsset->backGroundCubemap);

    // Load the diffuse irradiance cubemap.

    // Load the environment BRDF.

    // Load the prefiltered environment map.

    return m_envMapAsset;
}