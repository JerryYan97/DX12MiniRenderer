#include "Lights.h"
#include "yaml-cpp/yaml.h"

Object* AmbientLight::Deseralize(const std::string& objName, const YAML::Node& i_node)
{
    std::vector<float> radiance = i_node["Radiance"].as<std::vector<float>>();

    AmbientLight* pAmbientLight = new AmbientLight(radiance.data());
    pAmbientLight->m_objectName = objName;

    return pAmbientLight;
}