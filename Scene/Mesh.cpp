#include "Mesh.h"
#include "yaml-cpp/yaml.h"

StaticMesh::StaticMesh()
    : m_loadedInRAM(false),
      m_loadedInVRAM(false)
{
    memset(m_position, 0, sizeof(float) * 3);
    memset(m_scale, 0, sizeof(float) * 3);
    memset(m_rotation, 0, sizeof(float) * 3);

    m_objectType = "StaticMesh";
}

Object* StaticMesh::Deseralize(const YAML::Node& i_node)
{
    std::pair<std::string, YAML::Node> mshNode = i_node.as<std::pair<std::string, YAML::Node>>();
    std::string name = mshNode.first;
    const YAML::Node& node = mshNode.second;

    std::vector<float> pos = node["Position"].as<std::vector<float>>();
    std::vector<float> scale = node["Scale"].as<std::vector<float>>();
    std::vector<float> rotation = node["Rotation"].as<std::vector<float>>();
    std::string assetPath = node["AssetPath"].as<std::string>();

    StaticMesh* mesh = new StaticMesh();
    mesh->m_objectName = name;
    memcpy(mesh->m_position, pos.data(), sizeof(float) * 3);
    memcpy(mesh->m_scale, scale.data(), sizeof(float) * 3);
    memcpy(mesh->m_rotation, rotation.data(), sizeof(float) * 3);

    return mesh;
}