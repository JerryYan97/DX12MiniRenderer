#include "Camera.h"
#include "yaml-cpp/yaml.h"

Object* Camera::Deseralize(const YAML::Node& i_node)
{
    std::pair<std::string, YAML::Node> cameraNode = i_node.as<std::pair<std::string, YAML::Node>>();
    std::string name = cameraNode.first;
    
    const YAML::Node& node = cameraNode.second;

    std::vector<float> pos = node["Position"].as<std::vector<float>>();
    std::vector<float> view = node["View"].as<std::vector<float>>();
    std::vector<float> up = node["Up"].as<std::vector<float>>();
    float fov = node["fov"].as<float>();
    float far = node["far"].as<float>();
    float near = node["near"].as<float>();
    bool isActive = node["isActive"].as<bool>();

    Camera* pCamera = new Camera(pos.data(),
                                 view.data(),
                                 up.data(),
                                 fov, near, far);

    pCamera->m_active = isActive;
    pCamera->m_objectName = name;

    return pCamera;
}