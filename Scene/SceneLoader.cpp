#include "SceneLoader.h"
#include "Level.h"
#include "Mesh.h"
#include "Lights.h"
#include "Camera.h"
#include "yaml-cpp/yaml.h"

SceneLoader::SceneLoader()
{
}

SceneLoader::~SceneLoader()
{
}

void SceneLoader::LoadAsLevel(const std::string& fileNamePath, Level* o_pLevel)
{
    // Load the scene file into the level
    YAML::Node config = YAML::LoadFile(fileNamePath.c_str());
    std::string sceneType = config["SceneType"].as<std::string>();

    YAML::Node sceneGraph = config["SceneGraph"];

    m_currentScenePath = fileNamePath;

    for (uint32_t i = 0; i < sceneGraph.size(); i++)
    {
        const YAML::Node node = sceneGraph[i];
        std::string type = node["Type"].as<std::string>();
        if (type.compare("StaticMesh"))
        {
            o_pLevel->LoadObject(node, StaticMesh::Deseralize);
        }
        else if (type.compare("AmbientLight"))
        {
            o_pLevel->LoadObject(node, AmbientLight::Deseralize);
        }
        else if (type.compare("Camera"))
        {
            o_pLevel->LoadObject(node, Camera::Deseralize);
        }
    }
}