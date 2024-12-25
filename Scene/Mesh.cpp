#include "Mesh.h"
#include "yaml-cpp/yaml.h"
#include "SceneAssetLoader.h"
#include "../Utils/crc32.h"
#include "../Utils/MathUtils.h"
#include "../Utils/DX12Utils.h"
#include <cassert>
#include <array>

extern ID3D12Device* g_pD3dDevice;

/*

MeshPrimitive::MeshPrimitive()
    : m_gpuVertBuffer(nullptr),
      m_gpuIndexBuffer(nullptr),
      m_gpuConstantMaterialBuffer(nullptr),
      m_isConstantMaterial(false),
      m_constantMaterial(),
      m_idxBufferView(),
      m_vertexBufferView()
{
}

MeshPrimitive::~MeshPrimitive()
{
    for (auto& itr : m_gpuRsrcMap)
    {
        itr.second->Release();
    }
}

void MeshPrimitive::CreateVertIdxBuffer()
{
    const uint32_t idxBufferSizeByte = m_idxType ? sizeof(uint32_t) * m_idxDataUint32.size() :
                                                   sizeof(uint16_t) * m_idxDataUint16.size();
    const uint32_t vertCnt = m_posData.size() / 3;
    const uint32_t vertSizeFloat = (3 + 3 + 4 + 2); // Position(3) + Normal(3) + Tangent(4) + TexCoord(2).
    const uint32_t vertSizeByte = sizeof(float) * vertSizeFloat;
    const uint32_t vertexBufferSize = vertCnt * vertSizeByte;
    m_vertData.resize(vertCnt * vertSizeFloat);

    for (uint32_t i = 0; i < vertCnt; i++)
    {
        memcpy(&m_vertData[i * vertSizeFloat], &m_posData[i * 3], sizeof(float) * 3);
        memcpy(&m_vertData[i * vertSizeFloat + 3], &m_normalData[i * 3], sizeof(float) * 3);
        memcpy(&m_vertData[i * vertSizeFloat + 6], &m_tangentData[i * 4], sizeof(float) * 4);
        memcpy(&m_vertData[i * vertSizeFloat + 10], &m_texCoordData[i * 2], sizeof(float) * 2);
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
            IID_PPV_ARGS(&m_gpuVertBuffer)));

    ThrowIfFailed(g_pD3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &idxBufferRsrcDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_gpuIndexBuffer)
    ));

    m_gpuRsrcMap["VertexGpuBuffer"] = m_gpuVertBuffer;
    m_gpuRsrcMap["IndexGpuBuffer"]  = m_gpuIndexBuffer;

    // Copy the triangle data to the vertex buffer.
    void* pVertexDataBegin;
    D3D12_RANGE readRange{ 0, 0 };        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_gpuVertBuffer->Map(0, &readRange, &pVertexDataBegin));
    memcpy(pVertexDataBegin, m_vertData.data(), vertexBufferSize);
    m_gpuVertBuffer->Unmap(0, nullptr);

    // Initialize the vertex buffer view.
    m_vertexBufferView.BufferLocation = m_gpuVertBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = vertSizeByte;
    m_vertexBufferView.SizeInBytes = vertexBufferSize;

    // Copy the model idx data to the idx buffer.
    void* pIdxDataBegin;
    ThrowIfFailed(m_gpuIndexBuffer->Map(0, &readRange, &pIdxDataBegin));
    if (m_idxType)
    {
        memcpy(pIdxDataBegin, m_idxDataUint32.data(), idxBufferSizeByte);
    }
    else
    {
        memcpy(pIdxDataBegin, m_idxDataUint16.data(), idxBufferSizeByte);
    }
    m_gpuIndexBuffer->Unmap(0, nullptr);

    // Initialize the index buffer view.
    m_idxBufferView.BufferLocation = m_gpuIndexBuffer->GetGPUVirtualAddress();
    m_idxBufferView.Format = m_idxType ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    m_idxBufferView.SizeInBytes = idxBufferSizeByte;
}
*/


StaticMesh::StaticMesh()
    : m_loadedInRAM(false),
      m_loadedInVRAM(false),
      m_staticMeshConstantBuffer(nullptr),
      m_staticMeshCbvDescHeap(nullptr)
{
    memset(m_position, 0, sizeof(float) * 3);
    memset(m_scale, 0, sizeof(float) * 3);
    memset(m_rotation, 0, sizeof(float) * 3);
    memset(m_modelMat, 0, sizeof(float) * 16);

    m_objectType = "StaticMesh";
    m_objectTypeHash = crc32(m_objectType.c_str());
}

StaticMesh::~StaticMesh()
{
    if (m_staticMeshConstantBuffer)
    {
        m_staticMeshConstantBuffer->Release();
    }

    if (m_staticMeshCbvDescHeap)
    {
        m_staticMeshCbvDescHeap->Release();
    }

    for (const auto& primItr : m_primitiveAssets)
    {
        bool bReleaseSrvHeap = false;
        if (primItr->m_baseColorTex.isSentToGpu)
        {
            primItr->m_baseColorTex.gpuResource->Release();
            bReleaseSrvHeap = true;
        }

        if (primItr->m_metallicRoughnessTex.isSentToGpu)
        {
            primItr->m_metallicRoughnessTex.gpuResource->Release();
            bReleaseSrvHeap = true;
        }

        if (primItr->m_normalTex.isSentToGpu)
        {
            primItr->m_normalTex.gpuResource->Release();
            bReleaseSrvHeap = true;
        }

        if (primItr->m_occlusionTex.isSentToGpu)
        {
            primItr->m_occlusionTex.gpuResource->Release();
            bReleaseSrvHeap = true;
        }

        if (primItr->m_staticMeshCnstMaterialBuffer)
        {
            primItr->m_staticMeshCnstMaterialBuffer->Release();
        }

        if (primItr->m_staticMeshCnstMaterialCbvDescHeap)
        {
            primItr->m_staticMeshCnstMaterialCbvDescHeap->Release();
        }

        if (bReleaseSrvHeap)
        {
            primItr->m_pTexturesSrvHeap->Release();
        }
    }
}

Object* StaticMesh::Deseralize(const std::string& objName, const YAML::Node& i_node)
{
    std::string name = objName;

    std::vector<float> pos = i_node["Position"].as<std::vector<float>>();
    std::vector<float> scale = i_node["Scale"].as<std::vector<float>>();
    std::vector<float> rotation = i_node["Rotation"].as<std::vector<float>>();
    std::string assetPath = i_node["AssetPath"].as<std::string>();

    StaticMesh* mesh = new StaticMesh();
    mesh->m_objectName = name;
    memcpy(mesh->m_position, pos.data(), sizeof(float) * 3);
    memcpy(mesh->m_scale, scale.data(), sizeof(float) * 3);
    memcpy(mesh->m_rotation, rotation.data(), sizeof(float) * 3);

    bool bNotDefineMaterial = i_node["Material"].IsNull();
    if (bNotDefineMaterial)
    {
        mesh->m_isCnstMaterial = true;
        mesh->m_cnstAlbedo[0] = 1.f; mesh->m_cnstAlbedo[1] = 1.f; mesh->m_cnstAlbedo[0] = 1.f;
        mesh->m_cnstMetallic = 0.f; mesh->m_cnstRoughness = 1.f;
    }
    else
    {
        std::string materialType = i_node["Material"]["Type"].as<std::string>();
        if (crc32(materialType.c_str()) == crc32("ConstMaterial"))
        {
            mesh->m_isCnstMaterial = true;
            std::vector<float> cnstAlbedo = i_node["Material"]["Albedo"].as<std::vector<float>>();
            mesh->m_cnstMetallic = i_node["Material"]["Metallic"].as<float>();
            mesh->m_cnstRoughness = i_node["Material"]["Roughness"].as<float>();
            memcpy(mesh->m_cnstAlbedo, cnstAlbedo.data(), sizeof(float) * 3);
        }
        else
        {
            assert(false, "Currently only support constant material.");
        }
    }

    assert((mesh->m_scale[0] == mesh->m_scale[1]) &&
           (mesh->m_scale[1] == mesh->m_scale[2]), "Assume scale are equal.");

    SceneAssetLoader::LoadStaticMesh(assetPath, mesh);

    GenModelMat(mesh->m_position,
                mesh->m_rotation[2], mesh->m_rotation[0], mesh->m_rotation[1],
                mesh->m_scale, mesh->m_modelMat);

    mesh->GenAndInitGpuBufferRsrc();
    mesh->SendModelMatrixToGpuBuffer();
    mesh->GenAndInitTextures();

    return mesh;
}

void StaticMesh::SendModelMatrixToGpuBuffer()
{
    void* pConstBufferBegin;
    D3D12_RANGE readRange{ 0, 0 };
    ThrowIfFailed(m_staticMeshConstantBuffer->Map(0, &readRange, &pConstBufferBegin));
    memcpy(pConstBufferBegin, m_modelMat, sizeof(float) * 16);
    m_staticMeshConstantBuffer->Unmap(0, nullptr);
}

void StaticMesh::GenAndInitGpuBufferRsrc()
{
    constexpr uint32_t CnstBufferSize = sizeof(float) * 64;

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

    ThrowIfFailed(g_pD3dDevice->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferRsrcDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_staticMeshConstantBuffer)));

    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(g_pD3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_staticMeshCbvDescHeap)));

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
    {
        cbvDesc.BufferLocation = m_staticMeshConstantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = CnstBufferSize;
    }

    g_pD3dDevice->CreateConstantBufferView(&cbvDesc, m_staticMeshCbvDescHeap->GetCPUDescriptorHandleForHeapStart());
}

void StaticMesh::GenAndInitTextures()
{
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

    for (uint32_t i = 0; i < m_primitiveAssets.size(); i++)
    {
        PrimitiveAsset* pPrimAsset = m_primitiveAssets[i];

        if (pPrimAsset->TextureCnt() > 0)
        {
            uint32_t texCnt = pPrimAsset->TextureCnt();
            D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
            srvHeapDesc.NumDescriptors = 1;
            srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            ThrowIfFailed(g_pD3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&pPrimAsset->m_pTexturesSrvHeap)));
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

            /**/
            SendDataToTexture2D(g_pD3dDevice,
                                pPrimAsset->m_baseColorTex.gpuResource,
                                pPrimAsset->m_baseColorTex.dataVec.data(),
                                pPrimAsset->m_baseColorTex.dataVec.size());

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = textureDesc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;

            g_pD3dDevice->CreateShaderResourceView(pPrimAsset->m_baseColorTex.gpuResource,
                                                   &srvDesc,
                                                   pPrimAsset->m_pTexturesSrvHeap->GetCPUDescriptorHandleForHeapStart());

            pPrimAsset->m_baseColorTex.srvHeapIdx = texHeapOffset;
            texHeapOffset++;
        }

        /*
        if (pPrimAsset->m_metallicRoughnessTex.pixWidth > 0)
        {
            SendDataToTexture2D(g_pD3dDevice, pPrimAsset->m_metallicRoughnessTex.m_gpuTexture,
                                               pPrimAsset->m_metallicRoughnessTex.m_imgData.data(),
                                               pPrimAsset->m_metallicRoughnessTex.m_imgData.size());
        }

        if (pPrimAsset->m_normalTex.m_imgData.size() > 0)
        {
            SendDataToTexture2D(g_pD3dDevice, pPrimAsset->m_normalTex.m_gpuTexture,
                                               pPrimAsset->m_normalTex.m_imgData.data(),
                                               pPrimAsset->m_normalTex.m_imgData.size());
        }

        if (pPrimAsset->m_occlusionTex.m_imgData.size() > 0)
        {
            SendDataToTexture2D(g_pD3dDevice, pPrimAsset->m_occlusionTex.m_gpuTexture,
                                               pPrimAsset->m_occlusionTex.m_imgData.data(),
                                               pPrimAsset->m_occlusionTex.m_imgData.size());
        }
        */

        pPrimAsset->GenMaterialMask();
        GenPrimAssetMaterialBuffer(pPrimAsset);
    }
}

void StaticMesh::GenPrimAssetMaterialBuffer(PrimitiveAsset* pPrimAsset)
{
    if (pPrimAsset->m_staticMeshCnstMaterialBuffer == nullptr &&
        pPrimAsset->m_staticMeshCnstMaterialCbvDescHeap == nullptr)
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
            IID_PPV_ARGS(&pPrimAsset->m_staticMeshCnstMaterialBuffer)));

        ThrowIfFailed(g_pD3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&pPrimAsset->m_staticMeshCnstMaterialCbvDescHeap)));

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        {
            cbvDesc.BufferLocation = pPrimAsset->m_staticMeshCnstMaterialBuffer->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = CnstBufferSize;
        }
        g_pD3dDevice->CreateConstantBufferView(&cbvDesc, pPrimAsset->m_staticMeshCnstMaterialCbvDescHeap->GetCPUDescriptorHandleForHeapStart());

        struct ConstantMaterial
        {
            float baseColorFactor[4];
            float metallicRoughness[4];
            uint32_t materialMask;
        };

        ConstantMaterial cnstMaterialData;
        cnstMaterialData.baseColorFactor[0] = m_cnstAlbedo[0];
        cnstMaterialData.baseColorFactor[1] = m_cnstAlbedo[1];
        cnstMaterialData.baseColorFactor[2] = m_cnstAlbedo[2];
        cnstMaterialData.metallicRoughness[0] = m_cnstMetallic;
        cnstMaterialData.metallicRoughness[1] = m_cnstRoughness;
        cnstMaterialData.materialMask = pPrimAsset->m_materialMask;

        void* pConstBufferBegin;
        D3D12_RANGE readRange{ 0, 0 };
        ThrowIfFailed(pPrimAsset->m_staticMeshCnstMaterialBuffer->Map(0, &readRange, &pConstBufferBegin));
        memcpy(pConstBufferBegin, &cnstMaterialData, sizeof(ConstantMaterial));
        pPrimAsset->m_staticMeshCnstMaterialBuffer->Unmap(0, nullptr);
    }
}