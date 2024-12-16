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
    idxBufferRsrcDesc.Width = sizeof(uint16_t) * m_idxDataUint16.size();

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
    memcpy(pIdxDataBegin, m_idxDataUint16.data(), sizeof(uint16_t) * m_idxDataUint16.size());
    m_gpuIndexBuffer->Unmap(0, nullptr);

    // Initialize the index buffer view.
    m_idxBufferView.BufferLocation = m_gpuIndexBuffer->GetGPUVirtualAddress();
    m_idxBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_idxBufferView.SizeInBytes = sizeof(uint16_t) * m_idxDataUint16.size();
}

StaticMesh::StaticMesh()
    : m_loadedInRAM(false),
      m_loadedInVRAM(false)
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

    mesh->GenModelMatrix();

    return mesh;
}

void StaticMesh::GenModelMatrix()
{
    GenModelMat(m_position, m_rotation[2], m_rotation[0], m_rotation[1], m_scale, m_modelMat);
}