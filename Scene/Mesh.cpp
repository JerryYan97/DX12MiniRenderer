#include "Mesh.h"
#include "yaml-cpp/yaml.h"
#include "SceneAssetLoader.h"
#include "../Utils/crc32.h"
#include "../Utils/MathUtils.h"
#include "../Utils/DX12Utils.h"
#include <cassert>
#include <array>

extern ID3D12Device5* g_pD3dDevice;

Mesh::~Mesh()
{
    for (const auto& prim : m_primitives)
    {
        if (prim.primMaterialCnstBuffer != nullptr) { prim.primMaterialCnstBuffer->Release(); }
        if (prim.primMaterialCbvDescHeap != nullptr) { prim.primMaterialCbvDescHeap->Release(); }
    }
}

void Mesh::OverrideAsConstMaterial(const ConstMaterialData& cnstMatData)
{
    for(int i = 0; i < m_primitives.size(); ++i)
    {
        Primitive& prim = m_primitives[i];
        prim.material.Init(cnstMatData);
    }
}

MeshObject::MeshObject()
    : m_pMeshObjCnstBuffer(nullptr),
      m_pMeshObjCbvDescHeap(nullptr)
{
    memset(m_position, 0, sizeof(float) * 3);
    memset(m_scale, 0, sizeof(float) * 3);
    memset(m_rotation, 0, sizeof(float) * 3);
    memset(m_modelMat, 0, sizeof(float) * 16);

    m_objectType = "MeshObject";
    m_objectTypeHash = crc32(m_objectType.c_str());
}

MeshObject::~MeshObject()
{
    if (m_pMeshObjCnstBuffer)
    {
        m_pMeshObjCnstBuffer->Release();
    }

    if (m_pMeshObjCbvDescHeap)
    {
        m_pMeshObjCbvDescHeap->Release();
    }
}

Object* MeshObject::Deseralize(const std::string& objName, const YAML::Node& i_node)
{
    std::string name = objName;

    std::vector<float> pos = i_node["Position"].as<std::vector<float>>();
    std::vector<float> scale = i_node["Scale"].as<std::vector<float>>();
    std::vector<float> rotation = i_node["Rotation"].as<std::vector<float>>();
    std::string assetPath = i_node["AssetPath"].as<std::string>();

    MeshObject* mesh = new MeshObject();
    mesh->m_objectName = name;
    memcpy(mesh->m_position, pos.data(), sizeof(float) * 3);
    memcpy(mesh->m_scale, scale.data(), sizeof(float) * 3);
    memcpy(mesh->m_rotation, rotation.data(), sizeof(float) * 3);
    
    mesh->m_mesh = AssetLoader::LoadAsOneMesh(assetPath);

    bool bNotDefineMaterial = !i_node["Material"].IsDefined();
    if (bNotDefineMaterial == false)
    {
        std::string materialType = i_node["Material"]["Type"].as<std::string>();
        if (crc32(materialType.c_str()) == crc32("ConstMaterial"))
        {
            ConstMaterialData cnstMatData = Material::DefaultConstMaterialData();

            if (i_node["Material"]["Emissive"].IsDefined())
            {
                cnstMatData.isEmissive = true;
                std::vector<float> cnstEmissive = i_node["Material"]["Emissive"].as<std::vector<float>>();
                memcpy(cnstMatData.emissive, cnstEmissive.data(), sizeof(float) * 3);
            }

            if (i_node["Material"]["IsDielectrics"].IsDefined())
            {
                cnstMatData.isDielectric = i_node["Material"]["IsDielectrics"].as<bool>();
            }

            if (i_node["Material"]["IsDoubleFace"].IsDefined())
            {
                cnstMatData.isDoubleFace = i_node["Material"]["IsDoubleFace"].as<bool>();
            }

            std::vector<float> cnstAlbedo = i_node["Material"]["Albedo"].as<std::vector<float>>();
            cnstMatData.metallic = i_node["Material"]["Metallic"].as<float>();
            cnstMatData.roughness = i_node["Material"]["Roughness"].as<float>();
            memcpy(cnstMatData.albedo, cnstAlbedo.data(), sizeof(float) * 3);

            mesh->m_mesh.OverrideAsConstMaterial(cnstMatData);
        }
        else
        {
            assert(false, "Currently only support constant material.");
        }
    }

    assert((mesh->m_scale[0] == mesh->m_scale[1]) &&
           (mesh->m_scale[1] == mesh->m_scale[2]), "Assume scale are equal.");

    GenModelMat(mesh->m_position,
                mesh->m_rotation[2], mesh->m_rotation[0], mesh->m_rotation[1],
                mesh->m_scale, mesh->m_modelMat);

    mesh->GenAndInitRuntimeGpuBufferRsrc();
    // mesh->SendModelMatrixToGpuBuffer();

    return mesh;
}

std::vector<Object*> MeshObject::DeseralizeFromSubLevel(const std::string& objCommonName, const YAML::Node& i_node)
{
    std::vector<Object*> meshObjects;

    std::string assetPath = i_node["AssetPath"].as<std::string>();
    std::vector<Mesh> loadedMeshes = AssetLoader::LoadSubLevelAsMultipleMeshes(assetPath, meshObjects);
    /*
    meshObjects.resize(loadedMeshes.size());
    for (int i = 0; i < loadedMeshes.size(); ++i)
    {
        std::string name = objCommonName + "_" + std::to_string(i);
        MeshObject* mesh = new MeshObject();
        mesh->m_objectName = name;
        mesh->m_mesh = loadedMeshes[i];
        
        GenModelMat(mesh->m_position,
                    mesh->m_rotation[2], mesh->m_rotation[0], mesh->m_rotation[1],
                    mesh->m_scale, mesh->m_modelMat);

        mesh->GenAndInitRuntimeGpuBufferRsrc();
    }
    */
    return meshObjects;
}

/*
void MeshObject::SendModelMatrixToGpuBuffer()
{
    void* pConstBufferBegin;
    D3D12_RANGE readRange{ 0, 0 };
    ThrowIfFailed(m_staticMeshConstantBuffer->Map(0, &readRange, &pConstBufferBegin));
    memcpy(pConstBufferBegin, m_modelMat, sizeof(float) * 16);
    m_staticMeshConstantBuffer->Unmap(0, nullptr);
}
*/

void MeshObject::GenAndInitRuntimeGpuBufferRsrc()
{
    // NOTE: Constant buffer needs to be padded to 256 bytes.
    constexpr uint32_t CnstBufferSize = sizeof(float) * 64;
    AllocateUploadBuffer(g_pD3dDevice, CnstBufferSize, &m_pMeshObjCnstBuffer);

    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(g_pD3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_pMeshObjCbvDescHeap)));

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
    {
        cbvDesc.BufferLocation = m_pMeshObjCnstBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = CnstBufferSize;
    }
    g_pD3dDevice->CreateConstantBufferView(&cbvDesc, m_pMeshObjCbvDescHeap->GetCPUDescriptorHandleForHeapStart());

    m_mesh.GenAndInitRuntimeGpuBufferRsrcForPrimitives();

    void* pMeshObjConstBufferBegin;
    D3D12_RANGE readRange{ 0, 0 };
    ThrowIfFailed(m_pMeshObjCnstBuffer->Map(0, &readRange, &pMeshObjConstBufferBegin));
    memcpy(pMeshObjConstBufferBegin, m_modelMat, sizeof(m_modelMat));
    m_pMeshObjCnstBuffer->Unmap(0, nullptr);

    // Create and populate the static mesh constant material buffer.
    /*
    ThrowIfFailed(g_pD3dDevice->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferRsrcDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_pMeshObjCnstBuffer)));

    ThrowIfFailed(g_pD3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_pMeshObjCnstMaterialCbvDescHeap)));

    {
        cbvDesc.BufferLocation = m_pMeshObjCnstBuffer->GetGPUVirtualAddress();
    }
    g_pD3dDevice->CreateConstantBufferView(&cbvDesc, m_pMeshObjCnstMaterialCbvDescHeap->GetCPUDescriptorHandleForHeapStart());

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
    */
}


void Mesh::InitAsAssetStorage(const std::string& assetPath, const std::vector<Primitive>& primitives)
{
    m_assetPath = assetPath;
    m_primitives = primitives;

    // TODO: calculate the bounding box.
    // Calculating center point and bounding box.
    printf("Calculating center point and bounding box\n");
    m_meshBBXMin[0] = FLT_MAX;  m_meshBBXMin[1] = FLT_MAX;   m_meshBBXMin[2] = FLT_MAX;
    m_meshBBXMax[0] = -FLT_MAX; m_meshBBXMax[1] = -FLT_MAX; m_meshBBXMax[2] = -FLT_MAX;

    for (int primId = 0; primId < m_primitives.size(); primId++)
    {
        size_t    vertCnt = m_primitives[primId].geometry->m_posData.size() / 3;
        float     center[3] = { 0.f, 0.f, 0.f };
        for (size_t vId = 0; vId < vertCnt; vId++)
        {
            center[0] += m_primitives[primId].geometry->m_posData[vId * 3 + 0];
            center[1] += m_primitives[primId].geometry->m_posData[vId * 3 + 1];
            center[2] += m_primitives[primId].geometry->m_posData[vId * 3 + 2];

            m_meshBBXMin[0] = min(m_meshBBXMin[0], m_primitives[primId].geometry->m_posData[vId * 3 + 0]);
            m_meshBBXMin[1] = min(m_meshBBXMin[1], m_primitives[primId].geometry->m_posData[vId * 3 + 1]);
            m_meshBBXMin[2] = min(m_meshBBXMin[2], m_primitives[primId].geometry->m_posData[vId * 3 + 2]);

            m_meshBBXMax[0] = max(m_meshBBXMax[0], m_primitives[primId].geometry->m_posData[vId * 3 + 0]);
            m_meshBBXMax[1] = max(m_meshBBXMax[1], m_primitives[primId].geometry->m_posData[vId * 3 + 1]);
            m_meshBBXMax[2] = max(m_meshBBXMax[2], m_primitives[primId].geometry->m_posData[vId * 3 + 2]);
        }
        center[0] /= static_cast<float>(vertCnt);
        center[1] /= static_cast<float>(vertCnt);
        center[2] /= static_cast<float>(vertCnt);
        m_meshCenter[0] += center[0];
        m_meshCenter[1] += center[1];
        m_meshCenter[2] += center[2];
    }
    m_meshCenter[0] /= static_cast<float>(m_primitives.size());
    m_meshCenter[1] /= static_cast<float>(m_primitives.size());
    m_meshCenter[2] /= static_cast<float>(m_primitives.size());

    /*
    float meshCenterLocal[4] = { m_meshCenter[0], m_meshCenter[1], m_meshCenter[2], 1.f };
    float meshCenterWorld[4] = { 0.f, 0.f, 0.f, 0.f };
    MatMulVec(m_modelMat, meshCenterLocal, 4, meshCenterWorld);
    mesh->m_meshCenter[0] = meshCenterWorld[0]; mesh->m_meshCenter[1] = meshCenterWorld[1]; mesh->m_meshCenter[2] = meshCenterWorld[2];

    float meshBBXMinLocal[4] = { mesh->m_meshBBXMin[0], mesh->m_meshBBXMin[1], mesh->m_meshBBXMin[2], 1.f };
    float meshBBXMaxLocal[4] = { mesh->m_meshBBXMax[0], mesh->m_meshBBXMax[1], mesh->m_meshBBXMax[2], 1.f };
    MatMulVec(mesh->m_modelMat, meshBBXMinLocal, 4, meshBBXMinLocal);
    MatMulVec(mesh->m_modelMat, meshBBXMaxLocal, 4, meshBBXMaxLocal);
    mesh->m_meshBBXMin[0] = meshBBXMinLocal[0]; mesh->m_meshBBXMin[1] = meshBBXMinLocal[1]; mesh->m_meshBBXMin[2] = meshBBXMinLocal[2];
    mesh->m_meshBBXMax[0] = meshBBXMaxLocal[0]; mesh->m_meshBBXMax[1] = meshBBXMaxLocal[1]; mesh->m_meshBBXMax[2] = meshBBXMaxLocal[2];
    
    printf("Mesh center point: (%f, %f, %f); Bounding Box: (%f, %f, %f) to (%f, %f, %f)\n",
        mesh->m_meshCenter[0], mesh->m_meshCenter[1], mesh->m_meshCenter[2],
        mesh->m_meshBBXMin[0], mesh->m_meshBBXMin[1], mesh->m_meshBBXMin[2],
        mesh->m_meshBBXMax[0], mesh->m_meshBBXMax[1], mesh->m_meshBBXMax[2]);
    */
    //
}

void Mesh::GenAndInitRuntimeGpuBufferRsrcForPrimitives()
{
    constexpr uint32_t CnstBufferSize = sizeof(float) * 64;

    for (auto& primitive : m_primitives)
    {
        // Create and initialize GPU resources for each primitive: 'PsMaterialBuffer'.
        // This includes creating constant buffers, descriptor heaps, etc.
        struct PsMaterialBuffer
        {
            float    baseColorFactor[4];
            float    metallicRoughness[4];
            uint32_t materialMask;
        } primPSMaterialConstBuffer {};

        std::vector<float> cnstAlbedo = primitive.material.GetCnstAlbedo();
        std::vector<float> cnstMetallicRoughness = primitive.material.GetCnstMetallicRoughness();
        primPSMaterialConstBuffer.baseColorFactor[0] = cnstAlbedo[0];
        primPSMaterialConstBuffer.baseColorFactor[1] = cnstAlbedo[1];
        primPSMaterialConstBuffer.baseColorFactor[2] = cnstAlbedo[2];
        primPSMaterialConstBuffer.metallicRoughness[0] = cnstMetallicRoughness[0];
        primPSMaterialConstBuffer.metallicRoughness[1] = cnstMetallicRoughness[1];
        primPSMaterialConstBuffer.materialMask = primitive.material.GetMaterialMask();

        AllocateUploadBuffer(g_pD3dDevice, &primPSMaterialConstBuffer, sizeof(PsMaterialBuffer), &primitive.primMaterialCnstBuffer);

        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.NumDescriptors = 1;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        ThrowIfFailed(g_pD3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&primitive.primMaterialCbvDescHeap)));

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        {
            cbvDesc.BufferLocation = primitive.primMaterialCnstBuffer->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = CnstBufferSize;
        }
        g_pD3dDevice->CreateConstantBufferView(&cbvDesc, primitive.primMaterialCbvDescHeap->GetCPUDescriptorHandleForHeapStart());
    }
}

void Mesh::InitAsObjectDataSource(const Mesh& otherMesh)
{
    *this = otherMesh;
    m_isMeshObjectDataSource = true;
}

void MeshObject::Init(const Mesh& mesh, const std::string& name, const float position[3], const float rotation[3], const float scale[3])
{
    memcpy(m_position, position, sizeof(m_position));
    memcpy(m_rotation, rotation, sizeof(m_rotation));
    memcpy(m_scale, scale, sizeof(m_scale));

    m_mesh.InitAsObjectDataSource(mesh);

    /*
    assert((m_scale[0] == m_scale[1]) &&
           (m_scale[1] == m_scale[2]) && "Assume scale are equal.");
    */

    GenModelMat(m_position,
                m_rotation[2], m_rotation[0], m_rotation[1],
                m_scale, m_modelMat);

    GenAndInitRuntimeGpuBufferRsrc();

    m_objectName = name;
}