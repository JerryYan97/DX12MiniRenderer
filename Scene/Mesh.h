#pragma once
#include "../Utils/Asset.h"
#include <string>
#include <vector>
#include <unordered_map>
#include "Object.h"
#include "../RenderBackend/Material.h"

namespace YAML
{
    class Node;
}

// Both Primitive and Mesh are composer. Only GeometryAsset and TextureAsset are the actual storage data, which are managed by the AssetManager.
//
// Primitive is a composer of references to geometry assets and material. It's the smallest renderable unit in the scene.
// When a Primitive is stored in the Mesh stored in AssetLoader, it doesn't have constant buffer and descriptor heap for the material. (Maybe this is not a good design and I should separate 'Primitive' as 'PrimitiveStorageData' and 'PrimitiveRuntimeData')
// When a Primitive is stored in the MeshObject, it has its own constant buffer and descriptor heap for the material. Their lifetime is managed by the MeshObject.
struct Primitive
{
    Material              material;
    GeometryAsset*        geometry = nullptr;
    ID3D12Resource*       primMaterialCnstBuffer = nullptr;
    ID3D12DescriptorHeap* primMaterialCbvDescHeap = nullptr;
};

// Mesh is a wrapper of the pointers to <geometry+materials -- Primitive> + anim data.
// Since it only stores the pointers to the geo-material data, we can also easily change the material of it.
//
// A Mesh can be used as two cases:
// (1): It can be used as the data source of a MeshObject. (Note that different MeshObjects won't share a 'Mesh')
// (2): It is a storage of the combination data from an asset file like GLTF/OBJ/OpenUSD, which represents an original model without any material/animation override in the AssetLoader.
class Mesh
{
public:
    Mesh() {}
    ~Mesh();

    //
    void InitAsAssetStorage(const std::string& assetPath, const std::vector<Primitive>& primitives);

    //
    void InitAsObjectDataSource(const Mesh& otherMesh);

    std::vector<float> GetMeshCenter() const
    {
        std::vector<float> res = {m_meshCenter[0], m_meshCenter[1], m_meshCenter[2]};
        return res;
    }

    std::vector<float> GetMeshBBX() const
    {
        std::vector<float> res = {
            m_meshBBXMin[0], m_meshBBXMin[1], m_meshBBXMin[2],
            m_meshBBXMax[0], m_meshBBXMax[1], m_meshBBXMax[2]};
        return res;
    }

    std::vector<Primitive> GetPrimitives() const { return m_primitives; }

    void OverrideAsConstMaterial(const ConstMaterialData& cnstMatData);

    void GenAndInitRuntimeGpuBufferRsrcForPrimitives();

private:
    std::string m_assetPath;

    std::vector<Primitive> m_primitives;
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC*> m_primMappedInsts; // Maybe useful when we want to render dynamic scene in the future.

    float m_meshCenter[3] = {}; // In the model space.
    float m_meshBBXMin[3] = {};
    float m_meshBBXMax[3] = {};

    bool m_isMeshObjectDataSource = false; // Or, it's managed by the AssetLoader.
};

// MeshObject is an instance of a Mesh in the SceneGraph. It can have position, rotation and scale to transform the Mesh.
class MeshObject : public Object
{
public:
    MeshObject();
    ~MeshObject();

    static Object* Deseralize(const std::string& objName, const YAML::Node& i_node);
    static std::vector<Object*> DeseralizeFromSubLevel(const std::string& objCommonName, const YAML::Node& i_node);

    // void SendModelMatrixToGpuBuffer();

    std::vector<float> GetMeshCenter() const { return m_mesh.GetMeshCenter(); }
    std::vector<float> GetMeshBBX() const { return m_mesh.GetMeshBBX(); }
    std::vector<Primitive> GetMeshPrimitives() const { return m_mesh.GetPrimitives(); }
    float* GetModelMat() { return m_modelMat; }

    void Init(const Mesh& mesh, const std::string& name, const float position[3], const float rotation[3], const float scale[3]);

    ID3D12DescriptorHeap* GetMeshObjCbvDescHeap() const { return m_pMeshObjCbvDescHeap; }

private:
    // The GPU buffer of a MeshObject is used to store per-object data like model matrix/material settings.
    void GenAndInitRuntimeGpuBufferRsrc();

    ID3D12Resource*       m_pMeshObjCnstBuffer = nullptr;
    ID3D12DescriptorHeap* m_pMeshObjCbvDescHeap = nullptr;

    float m_modelMat[16];

    Mesh m_mesh;
    float m_position[3];
    float m_rotation[3];
    float m_scale[3];
};