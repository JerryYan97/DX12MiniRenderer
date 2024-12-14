#include "Mesh.h"
#include "yaml-cpp/yaml.h"
#include "SceneAssetLoader.h"
#include "../Utils/crc32.h"
#include "../Utils/MathUtils.h"
#include "../Utils/DX12Utils.h"
#include <cassert>
#include <array>

extern ID3D12Device* g_pD3dDevice;

MeshPrimitive::MeshPrimitive()
    : m_gpuVertBuffer(nullptr),
      m_gpuIndexBuffer(nullptr),
      m_gpuConstantMaterialBuffer(nullptr),
      m_isConstantMaterial(false),
      m_constantMaterial()
{
}

MeshPrimitive::~MeshPrimitive()
{
    for (auto& itr : m_gpuRsrcMap)
    {
        itr.second->Release();
    }
}

StaticMesh::StaticMesh()
    : m_loadedInRAM(false),
      m_loadedInVRAM(false),
      m_pVsConstBuffer(nullptr),
      m_cbvDescHeap(nullptr)
{
    memset(m_position, 0, sizeof(float) * 3);
    memset(m_scale, 0, sizeof(float) * 3);
    memset(m_rotation, 0, sizeof(float) * 3);

    m_objectType = "StaticMesh";
    m_objectTypeHash = crc32(m_objectType.c_str());
}

StaticMesh::~StaticMesh()
{
    if (m_pVsConstBuffer)
    {
        m_pVsConstBuffer->Release();
        m_pVsConstBuffer = nullptr;
    }

    if (m_cbvDescHeap)
    {
        m_cbvDescHeap->Release();
        m_cbvDescHeap = nullptr;
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

    assert((mesh->m_scale[0] == mesh->m_scale[1]) &&
           (mesh->m_scale[1] == mesh->m_scale[2]), "Assume scale are equal.");

    SceneAssetLoader::LoadStaticMesh(assetPath, mesh);

    mesh->GenModelMatrix(g_pD3dDevice);

    return mesh;
}

void StaticMesh::GenModelMatrix(ID3D12Device* pDevice)
{
    // Padding to 256 bytes for constant buffer
    float vsConstantBuffer[64] = {};
    float o_modelMat[16] = {};

    float tmpVPMat[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    GenModelMat(m_position, m_rotation[2], m_rotation[0], m_rotation[1], m_scale, o_modelMat);

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
        bufferRsrcDesc.Width = sizeof(vsConstantBuffer);
        bufferRsrcDesc.Height = 1;
        bufferRsrcDesc.DepthOrArraySize = 1;
        bufferRsrcDesc.MipLevels = 1;
        bufferRsrcDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferRsrcDesc.SampleDesc.Count = 1;
        bufferRsrcDesc.SampleDesc.Quality = 0;
        bufferRsrcDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferRsrcDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    }

    ThrowIfFailed(pDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferRsrcDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_pVsConstBuffer)));

    // Describe and create a constant buffer view (CBV) descriptor heap.
    // Flags indicate that this descriptor heap can be bound to the pipeline 
    // and that descriptors contained in it can be referenced by a root table.
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(pDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvDescHeap)));

    // Describe and create a constant buffer view.
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_pVsConstBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = sizeof(vsConstantBuffer);
    pDevice->CreateConstantBufferView(&cbvDesc, m_cbvDescHeap->GetCPUDescriptorHandleForHeapStart());

    memcpy(vsConstantBuffer, o_modelMat, sizeof(float) * 16);
    memcpy(vsConstantBuffer + 16, tmpVPMat, sizeof(float) * 16);

    // Map and initialize the constant buffer. We don't unmap this until the
    // app closes. Keeping things mapped for the lifetime of the resource is okay.
    D3D12_RANGE readRange{ 0, 0 };        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_pVsConstBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pVsConstBufferBegin)));
    memcpy(m_pVsConstBufferBegin, vsConstantBuffer, sizeof(vsConstantBuffer));
    m_pVsConstBuffer->Unmap(0, nullptr);
}