#include "Mesh.h"
#include "yaml-cpp/yaml.h"
#include "SceneAssetLoader.h"
#include "../Utils/crc32.h"
#include "../Utils/MathUtils.h"
#include "../Utils/DX12Utils.h"
#include <cassert>
#include <array>

extern ID3D12Device* g_pD3dDevice;

StaticMesh::StaticMesh()
    : m_loadedInRAM(false),
      m_loadedInVRAM(false),
      m_staticMeshConstantBuffer(nullptr),
      m_staticMeshCbvDescHeap(nullptr),
      m_isCnstMaterial(false),
      m_isCnstEmissiveMaterial(false),
      m_isDoubleFace(true),
      m_isDielectric(false)
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

    if (m_staticMeshCnstMaterialBuffer)
    {
        m_staticMeshCnstMaterialBuffer->Release();
    }

    if (m_staticMeshCnstMaterialCbvDescHeap)
    {
        m_staticMeshCnstMaterialCbvDescHeap->Release();
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

    bool bNotDefineMaterial = !i_node["Material"].IsDefined();
    if (bNotDefineMaterial)
    {
        mesh->m_isCnstMaterial = true;
        mesh->m_cnstAlbedo[0] = 1.f; mesh->m_cnstAlbedo[1] = 1.f; mesh->m_cnstAlbedo[2] = 1.f;
        mesh->m_cnstMetallic = 0.f; mesh->m_cnstRoughness = 1.f;
    }
    else
    {
        std::string materialType = i_node["Material"]["Type"].as<std::string>();
        if (crc32(materialType.c_str()) == crc32("ConstMaterial"))
        {
            mesh->m_isCnstMaterial = true;
            if (i_node["Material"]["Emissive"].IsDefined())
            {
                mesh->m_isCnstEmissiveMaterial = true;
                std::vector<float> cnstEmissive = i_node["Material"]["Emissive"].as<std::vector<float>>();
                memcpy(mesh->m_cnstEmissive, cnstEmissive.data(), sizeof(float) * 3);
            }

            if (i_node["Material"]["IsDielectrics"].IsDefined())
            {
                mesh->m_isDielectric = i_node["Material"]["IsDielectrics"].as<bool>();
            }

            if (i_node["Material"]["IsDoubleFace"].IsDefined())
            {
                mesh->m_isDoubleFace = i_node["Material"]["IsDoubleFace"].as<bool>();
            }

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

    // Create and populate the static mesh constant material buffer.
    ThrowIfFailed(g_pD3dDevice->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferRsrcDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_staticMeshCnstMaterialBuffer)));

    ThrowIfFailed(g_pD3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_staticMeshCnstMaterialCbvDescHeap)));

    {
        cbvDesc.BufferLocation = m_staticMeshCnstMaterialBuffer->GetGPUVirtualAddress();
    }
    g_pD3dDevice->CreateConstantBufferView(&cbvDesc, m_staticMeshCnstMaterialCbvDescHeap->GetCPUDescriptorHandleForHeapStart());

    struct ConstantMaterial
    {
        float baseColorFactor[4];
        float metallicRoughness[4];
    };

    ConstantMaterial cnstMaterialData;
    cnstMaterialData.baseColorFactor[0] = m_cnstAlbedo[0];
    cnstMaterialData.baseColorFactor[1] = m_cnstAlbedo[1];
    cnstMaterialData.baseColorFactor[2] = m_cnstAlbedo[2];
    cnstMaterialData.metallicRoughness[0] = m_cnstMetallic;
    cnstMaterialData.metallicRoughness[1] = m_cnstRoughness;

    void* pConstBufferBegin;
    D3D12_RANGE readRange{ 0, 0 };
    ThrowIfFailed(m_staticMeshCnstMaterialBuffer->Map(0, &readRange, &pConstBufferBegin));
    memcpy(pConstBufferBegin, &cnstMaterialData, sizeof(ConstantMaterial));
    m_staticMeshCnstMaterialBuffer->Unmap(0, nullptr);
}