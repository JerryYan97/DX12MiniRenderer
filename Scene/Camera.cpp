#include "Camera.h"
#include "yaml-cpp/yaml.h"

Object* Camera::Deseralize(const std::string& objName, const YAML::Node& i_node)
{
    std::vector<float> pos = i_node["Position"].as<std::vector<float>>();
    std::vector<float> view = i_node["View"].as<std::vector<float>>();
    std::vector<float> up = i_node["Up"].as<std::vector<float>>();
    float fov = i_node["fov"].as<float>();
    float far = i_node["far"].as<float>();
    float near = i_node["near"].as<float>();
    bool isActive = i_node["isActive"].as<bool>();

    Camera* pCamera = new Camera(pos.data(),
                                 view.data(),
                                 up.data(),
                                 fov, near, far);

    pCamera->m_active = isActive;
    pCamera->m_objectName = objName;

    return pCamera;
}