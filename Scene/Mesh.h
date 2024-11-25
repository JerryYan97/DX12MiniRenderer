#pragma once
#include <string>
#include <vector>
#include "Object.h"

namespace YAML
{
    class Node;
}

struct ImgInfo
{
    uint32_t             pixWidth;
    uint32_t             pixHeight;
    uint32_t             componentCnt;
    std::vector<uint8_t> dataVec;
    uint32_t             componentType;
};

class MeshPrimitive
{
public:
    MeshPrimitive() {}
    ~MeshPrimitive() {}

    // float m_position[3];
    std::vector<float>    m_vertData;

    std::vector<float>    m_posData;
    std::vector<float>    m_normalData;
    std::vector<float>    m_tangentData;
    std::vector<float>    m_texCoordData;

    std::vector<uint16_t> m_idxDataUint16;

    ImgInfo m_baseColorTex;         // TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE (5121), 4 components.
    ImgInfo m_metallicRoughnessTex; // R32G32_SFLOAT
    ImgInfo m_normalTex;            // R32G32B32_SFLOAT
    ImgInfo m_occlusionTex;         // R32_SFLOAT
    ImgInfo m_emissiveTex;          // Currently don't support.

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
};

class StaticMesh : public Object
{
public:
    StaticMesh();
    ~StaticMesh() {}

    static Object* Deseralize(const std::string& objName, const YAML::Node& i_node);

    std::string m_assetPath;

    std::vector<MeshPrimitive> m_meshPrimitives;

private:
    float m_position[3];
    float m_rotation[3];
    float m_scale[3];

    bool        m_loadedInRAM;
    bool        m_loadedInVRAM;
};