#include "Mesh.h"
#include "yaml-cpp/yaml.h"
#include "SceneAssetLoader.h"
#include "../Utils/crc32.h"

MeshPrimitive::~MeshPrimitive()
{
    for (auto& itr : m_gpuRsrcMap)
    {
        itr.second->Release();
    }
}

StaticMesh::StaticMesh()
    : m_loadedInRAM(false),
      m_loadedInVRAM(false)
{
    memset(m_position, 0, sizeof(float) * 3);
    memset(m_scale, 0, sizeof(float) * 3);
    memset(m_rotation, 0, sizeof(float) * 3);

    m_objectType = "StaticMesh";
    m_objectTypeHash = crc32(m_objectType.c_str());
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

    SceneAssetLoader::LoadStaticMesh(assetPath, mesh);

    return mesh;
}