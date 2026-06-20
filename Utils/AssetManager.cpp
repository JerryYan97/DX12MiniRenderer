#include "AssetManager.h"
#include "DX12Utils.h"
#include "../Scene/Mesh.h"
#include "../Scene/Level.h"
#include <unordered_set>
#include <cassert>
#include "../ThirdParty/TinyGltf/stb_image.h"
#include "../ThirdParty/TinyGltf/tiny_gltf.h"

extern ID3D12Device5* g_pD3dDevice;
AssetManager* AssetManager::m_pThis = nullptr;

void AssetManager::Deinit()
{
    for (const auto& itr : m_geoAssets)
    {
        for (const auto& geoItr : itr.second)
        {
            if (geoItr->m_gpuVertBuffer) { geoItr->m_gpuVertBuffer->Release(); }
            if (geoItr->m_gpuIndexBuffer) { geoItr->m_gpuIndexBuffer->Release(); }
            if (geoItr->m_blas) { geoItr->m_blas->Release(); }

            delete geoItr;
        }
    }

    for (const auto& itr : m_textureAssets)
    {
        for(const auto& texItr : itr.second)
        {
            if (texItr->imgInfo.texDescHeap) { texItr->imgInfo.texDescHeap->Release(); }
            if (texItr->imgInfo.gpuResource) { texItr->imgInfo.gpuResource->Release(); }
            delete texItr;
        }
    }
}

void AssetManager::StoreModelAssets(const std::string& assetPath, const std::vector<Primitive>& iPrimitives)
{
    for(int i = 0; i < iPrimitives.size(); i++)
    {
        SendGeoAssetToGpu(iPrimitives[i].geometry);
        m_geoAssets[assetPath].push_back(iPrimitives[i].geometry);

        if (iPrimitives[i].material.m_baseColorTex != nullptr)
        {
            SendTextureAssetToGpu(iPrimitives[i].material.m_baseColorTex);
            m_textureAssets[assetPath].push_back(iPrimitives[i].material.m_baseColorTex);
        }

        if (iPrimitives[i].material.m_metallicRoughnessTex != nullptr)
        {
            SendTextureAssetToGpu(iPrimitives[i].material.m_metallicRoughnessTex);
            m_textureAssets[assetPath].push_back(iPrimitives[i].material.m_metallicRoughnessTex);
        }

        if (iPrimitives[i].material.m_normalTex != nullptr)
        {
            SendTextureAssetToGpu(iPrimitives[i].material.m_normalTex);
            m_textureAssets[assetPath].push_back(iPrimitives[i].material.m_normalTex);
        }

        if (iPrimitives[i].material.m_occlusionTex != nullptr)
        {
            SendTextureAssetToGpu(iPrimitives[i].material.m_occlusionTex);
            m_textureAssets[assetPath].push_back(iPrimitives[i].material.m_occlusionTex);
        }

        if (iPrimitives[i].material.m_emissiveTex != nullptr)
        {
            SendTextureAssetToGpu(iPrimitives[i].material.m_emissiveTex);
            m_textureAssets[assetPath].push_back(iPrimitives[i].material.m_emissiveTex);
        }
    }
}

void AssetManager::StoreTextureAsset(const std::string& assetPath, TextureAsset* iTexAsset)
{
    SendTextureAssetToGpu(iTexAsset);
    m_textureAssets[assetPath].push_back(iTexAsset);
}

void AssetManager::SendGeoAssetToGpu(GeometryAsset* pGeoAsset)
{
    if (pGeoAsset == nullptr)
    {
        return;
    }

    if (pGeoAsset->m_gpuVertBuffer != nullptr && pGeoAsset->m_gpuIndexBuffer != nullptr)
    {
        return;
    }

    const uint32_t idxBufferSizeByte = pGeoAsset->m_idxType ? sizeof(uint32_t) * pGeoAsset->m_idxDataUint32.size() :
                                                              sizeof(uint16_t) * pGeoAsset->m_idxDataUint16.size();

    const uint32_t vertCnt = pGeoAsset->m_posData.size() / 3;
    const uint32_t vertSizeFloat = VERT_SIZE_FLOAT; // Position(3) + Normal(3) + Tangent(4) + TexCoord(2).
    const uint32_t vertSizeByte = sizeof(float) * vertSizeFloat;
    const uint32_t vertexBufferSize = vertCnt * vertSizeByte;
    pGeoAsset->m_vertData.resize(vertCnt * vertSizeFloat);

    for (uint32_t i = 0; i < vertCnt; i++)
    {
        memcpy(&pGeoAsset->m_vertData[i * vertSizeFloat], &pGeoAsset->m_posData[i * 3], sizeof(float) * 3);
        memcpy(&pGeoAsset->m_vertData[i * vertSizeFloat + 3], &pGeoAsset->m_normalData[i * 3], sizeof(float) * 3);
        memcpy(&pGeoAsset->m_vertData[i * vertSizeFloat + 6], &pGeoAsset->m_tangentData[i * 4], sizeof(float) * 4);
        memcpy(&pGeoAsset->m_vertData[i * vertSizeFloat + 10], &pGeoAsset->m_texCoordData[i * 2], sizeof(float) * 2);
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
            IID_PPV_ARGS(&pGeoAsset->m_gpuVertBuffer)));

    ThrowIfFailed(g_pD3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &idxBufferRsrcDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&pGeoAsset->m_gpuIndexBuffer)
    ));

    // Copy the triangle data to the vertex buffer.
    void* pVertexDataBegin;
    D3D12_RANGE readRange{ 0, 0 };        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(pGeoAsset->m_gpuVertBuffer->Map(0, &readRange, &pVertexDataBegin));
    memcpy(pVertexDataBegin, pGeoAsset->m_vertData.data(), vertexBufferSize);
    pGeoAsset->m_gpuVertBuffer->Unmap(0, nullptr);

    // Initialize the vertex buffer view.
    pGeoAsset->m_vertexBufferView.BufferLocation = pGeoAsset->m_gpuVertBuffer->GetGPUVirtualAddress();
    pGeoAsset->m_vertexBufferView.StrideInBytes = vertSizeByte;
    pGeoAsset->m_vertexBufferView.SizeInBytes = vertexBufferSize;

    // Copy the model idx data to the idx buffer.
    void* pIdxDataBegin;
    ThrowIfFailed(pGeoAsset->m_gpuIndexBuffer->Map(0, &readRange, &pIdxDataBegin));
    if (pGeoAsset->m_idxType)
    {
        memcpy(pIdxDataBegin, pGeoAsset->m_idxDataUint32.data(), idxBufferSizeByte);
    }
    else
    {
        memcpy(pIdxDataBegin, pGeoAsset->m_idxDataUint16.data(), idxBufferSizeByte);
    }
    pGeoAsset->m_gpuIndexBuffer->Unmap(0, nullptr);

    // Initialize the index buffer view.
    pGeoAsset->m_idxBufferView.BufferLocation = pGeoAsset->m_gpuIndexBuffer->GetGPUVirtualAddress();
    pGeoAsset->m_idxBufferView.Format = pGeoAsset->m_idxType ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    pGeoAsset->m_idxBufferView.SizeInBytes = idxBufferSizeByte;

    if (m_pLevel != nullptr &&
        m_pLevel->m_rendererBackendType == RendererBackendType::PathTracing &&
        pGeoAsset->m_blas == nullptr)
    {
        pGeoAsset->m_blas = MakeBLAS(pGeoAsset);
    }
}

void AssetManager::SendTextureAssetToGpu(TextureAsset* pTexAsset)
{
    if (pTexAsset == nullptr || pTexAsset->imgInfo.dataVec.empty())
    {
        return;
    }

    const bool isCubemap = (pTexAsset->imgInfo.arrayLayerCnt == 6);

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = static_cast<UINT16>(pTexAsset->imgInfo.mipLevelCnt);
    textureDesc.DepthOrArraySize = static_cast<UINT16>(pTexAsset->imgInfo.arrayLayerCnt);
    textureDesc.Format = pTexAsset->imgInfo.textureFormat;
    textureDesc.Width = pTexAsset->imgInfo.pixWidth;
    textureDesc.Height = pTexAsset->imgInfo.pixHeight;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

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
        IID_PPV_ARGS(&pTexAsset->imgInfo.gpuResource)));

    pTexAsset->imgInfo.gpuResource->SetName(isCubemap ? L"AssetCubeTex" : L"AssetTex");

    if (isCubemap)
    {
        const uint32_t sliceCnt = 6;
        const uint32_t mipLevelCnt = pTexAsset->imgInfo.mipLevelCnt;
        const uint32_t bytesPerSlice = static_cast<uint32_t>(pTexAsset->imgInfo.dataVec.size() / sliceCnt);

        for (uint32_t sliceIdx = 0; sliceIdx < sliceCnt; ++sliceIdx)
        {
            void* pSliceData = pTexAsset->imgInfo.dataVec.data() + static_cast<size_t>(sliceIdx) * bytesPerSlice;
            SendDataToCubemapSlice(g_pD3dDevice,
                                   pTexAsset->imgInfo.gpuResource,
                                   pSliceData,
                                   bytesPerSlice,
                                   sliceIdx,
                                   0);
        }

        ChangeResourceState(g_pD3dDevice,
                            pTexAsset->imgInfo.gpuResource,
                            D3D12_RESOURCE_STATE_COPY_DEST,
                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    else
    {
        SendDataToTexture2D(g_pD3dDevice,
                            pTexAsset->imgInfo.gpuResource,
                            pTexAsset->imgInfo.dataVec.data(),
                            pTexAsset->imgInfo.dataVec.size());
    }

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    {
        srvHeapDesc.NumDescriptors = 1;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        srvHeapDesc.NodeMask = 0;
    }
    ThrowIfFailed(g_pD3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&pTexAsset->imgInfo.texDescHeap)));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    {
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;

        if (isCubemap)
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MipLevels = textureDesc.MipLevels;
            srvDesc.TextureCube.MostDetailedMip = 0;
            srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        }
        else
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.PlaneSlice = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        }
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE descHeapPtr = pTexAsset->imgInfo.texDescHeap->GetCPUDescriptorHandleForHeapStart();
    g_pD3dDevice->CreateShaderResourceView(pTexAsset->imgInfo.gpuResource, &srvDesc, descHeapPtr);

    pTexAsset->imgInfo.isSentToGpu = true;
}

ID3D12Resource* AssetManager::MakeBLAS(GeometryAsset* pGeoAsset)
{
    if (pGeoAsset == nullptr || pGeoAsset->m_gpuVertBuffer == nullptr)
    {
        return nullptr;
    }

    const DXGI_FORMAT indexFormat = (pGeoAsset->m_gpuIndexBuffer == nullptr) ? DXGI_FORMAT_UNKNOWN :
                                    (pGeoAsset->m_idxType ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT);

    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    geometryDesc.Triangles.Transform3x4 = 0;
    geometryDesc.Triangles.IndexFormat = indexFormat;
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.IndexCount = pGeoAsset->m_gpuIndexBuffer ? pGeoAsset->m_idxCnt : 0;
    geometryDesc.Triangles.VertexCount = static_cast<UINT>(pGeoAsset->m_vertData.size() / VERT_SIZE_FLOAT);
    geometryDesc.Triangles.IndexBuffer = pGeoAsset->m_gpuIndexBuffer ? pGeoAsset->m_gpuIndexBuffer->GetGPUVirtualAddress() : 0;
    geometryDesc.Triangles.VertexBuffer.StartAddress = pGeoAsset->m_gpuVertBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(float) * VERT_SIZE_FLOAT;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    inputs.NumDescs = 1;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.pGeometryDescs = &geometryDesc;

    return MakeAccelerationStructure(g_pD3dDevice, inputs, nullptr);
}

/*
void AssetManager::SaveModelPrimAssetAndCreateGpuRsrc(const std::string& name, PrimitiveAsset* pPrimitiveAsset)
{
    

    

    

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
        sceneIdxBuffer.insert(sceneIdxBuffer.end, prim->m_idxDataUint16.begin(), prim->m_idxDataUint16.end());
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
*/