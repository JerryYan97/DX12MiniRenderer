#include "Lights.h"
#include "yaml-cpp/yaml.h"

Object* AmbientLight::Deseralize(const YAML::Node& i_node)
{
    std::pair<std::string, YAML::Node> ambientLightNode = i_node.as<std::pair<std::string, YAML::Node>>();
    std::string name = ambientLightNode.first;
    
    const YAML::Node& node = ambientLightNode.second;

    std::vector<float> radiance = node["Radiance"].as<std::vector<float>>();

    AmbientLight* pAmbientLight = new AmbientLight(radiance.data());
    pAmbientLight->m_objectName = name;

    return pAmbientLight;
}