#pragma once
#include "../Utils/AssetManager.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <d3d12.h>
#include "Object.h"

namespace YAML
{
    class Node;
}

// StaticMesh is actually a StaticMesh Instance that can share the same mesh data with other StaticMesh Instances.
class StaticMesh : public Object
{
public:
    StaticMesh();
    ~StaticMesh();

    static Object* Deseralize(const std::string& objName, const YAML::Node& i_node);
    void SendModelMatrixToGpuBuffer();

    bool IsCnstEmissiveMaterial() const { return m_isCnstEmissiveMaterial; }
    std::vector<float> GetCnstEmissive() const
    {
        std::vector<float> res = {m_cnstEmissive[0], m_cnstEmissive[1], m_cnstEmissive[2]};
        return res;
    }

    std::vector<float> GetCnstAlbedo() const
    {
        std::vector<float> res = {m_cnstAlbedo[0], m_cnstAlbedo[1], m_cnstAlbedo[2]};
        return res;
    }

    std::vector<float> GetCnstMetallicRoughness() const
    {
        std::vector<float> res = {m_cnstMetallic, m_cnstRoughness};
        return res;
    }

    std::string m_assetPath;

    // std::vector<MeshPrimitive> m_meshPrimitives;

    std::vector<PrimitiveAsset*> m_primitiveAssets;
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC*> m_primMappedInsts; // Maybe useful when we want to render dynamic scene in the future.

    float m_modelMat[16];

    ID3D12Resource* m_staticMeshConstantBuffer;
    ID3D12DescriptorHeap* m_staticMeshCbvDescHeap;

    ID3D12Resource* m_staticMeshCnstMaterialBuffer;
    ID3D12DescriptorHeap* m_staticMeshCnstMaterialCbvDescHeap;

private:
    void GenAndInitGpuBufferRsrc();

    float m_position[3];
    float m_rotation[3];
    float m_scale[3];

    bool  m_isCnstMaterial;
    float m_cnstAlbedo[3];
    float m_cnstMetallic;
    float m_cnstRoughness;

    bool m_isCnstEmissiveMaterial;
    float m_cnstEmissive[3];

    bool m_loadedInRAM;
    bool m_loadedInVRAM;
};