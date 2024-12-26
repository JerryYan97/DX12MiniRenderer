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

// By default, no occlusion, no emissive, standard geometry normal.
/*
struct ConstantMaterial
{
    float baseColorFactor[4];
    float metallicRoughness[2];
};
*/
/*
class MeshPrimitive
{
public:
    MeshPrimitive();
    ~MeshPrimitive();

    void CreateVertIdxBuffer(); // Create vertex buffer ram data and vertex and index buffer on GPU.

    uint32_t GetIdxCount() const
    {
        return m_idxType ? m_idxDataUint32.size() : m_idxDataUint16.size();
    }

    // float m_position[3];
    std::vector<float>    m_vertData;

    std::vector<float>    m_posData;
    std::vector<float>    m_normalData;
    std::vector<float>    m_tangentData;
    std::vector<float>    m_texCoordData;

    bool                  m_idxType; // 0: uint16_t, 1: uint32_t
    std::vector<uint16_t> m_idxDataUint16;
    std::vector<uint32_t> m_idxDataUint32;

    ConstantMaterial m_constantMaterial;

    ImgInfo m_baseColorTex;         // TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE (5121), 4 components.
    ImgInfo m_metallicRoughnessTex; // R32G32_SFLOAT
    ImgInfo m_normalTex;            // R32G32B32_SFLOAT
    ImgInfo m_occlusionTex;         // R32_SFLOAT
    ImgInfo m_emissiveTex;          // Currently don't support.

    std::unordered_map<std::string, ID3D12Resource*> m_gpuRsrcMap;

    ID3D12Resource*       m_gpuVertBuffer;
    ID3D12Resource*       m_gpuIndexBuffer;
    ID3D12Resource*       m_gpuConstantMaterialBuffer;

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW  m_idxBufferView;
    */
    /*
    void InitGpuRsrc(VkDevice device, VmaAllocator* pAllocator, VkCommandBuffer cmdBuffer, VkQueue queue);
    void FinializeGpuRsrc(VkDevice device, VmaAllocator* pAllocator);

    VkBuffer* GetVertBuffer() { return &m_vertBuffer.buffer; }
    VkBuffer  GetIndexBuffer() { return m_indexBuffer.buffer; }

    VkDescriptorImageInfo* GetBaseColorImgDescInfo() { return &m_baseColorGpuImg.imageDescInfo; }
    VkDescriptorImageInfo* GetMetallicRoughnessImgDescInfo() { return &m_metallicRoughnessGpuImg.imageDescInfo; }
    VkDescriptorImageInfo* GetNormalImgDescInfo() { return &m_normalGpuImg.imageDescInfo; }
    VkDescriptorImageInfo* GetOcclusionImgDescInfo() { return &m_occlusionGpuImg.imageDescInfo; }
    VkDescriptorImageInfo* GetEmissiveImgDescInfo() { return &m_emissiveGpuImg.imageDescInfo; }

protected:
    GpuBuffer m_vertBuffer;
    GpuBuffer m_indexBuffer;

    GpuImg m_baseColorGpuImg;
    GpuImg m_metallicRoughnessGpuImg;
    GpuImg m_normalGpuImg;
    GpuImg m_occlusionGpuImg;
    GpuImg m_emissiveGpuImg;
    */
// };

class StaticMesh : public Object
{
public:
    StaticMesh();
    ~StaticMesh();

    static Object* Deseralize(const std::string& objName, const YAML::Node& i_node);
    void SendModelMatrixToGpuBuffer();

    std::string m_assetPath;

    // std::vector<MeshPrimitive> m_meshPrimitives;

    std::vector<PrimitiveAsset*> m_primitiveAssets;

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

    bool m_loadedInRAM;
    bool m_loadedInVRAM;
};