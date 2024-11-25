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

class StaticMesh : public Object
{
public:
    StaticMesh();
    ~StaticMesh() {}

    static Object* Deseralize(const std::string& objName, const YAML::Node& i_node);

    std::string m_assetPath;

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

private:
    float m_position[3];
    float m_rotation[3];
    float m_scale[3];

    bool        m_loadedInRAM;
    bool        m_loadedInVRAM;
};