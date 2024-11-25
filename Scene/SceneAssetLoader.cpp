#include "SceneAssetLoader.h"
#include "Level.h"
#include "Mesh.h"
#include "Lights.h"
#include "Camera.h"
#include "yaml-cpp/yaml.h"
#include <iostream>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ThirdParty/TinyGltf/tiny_gltf.h"

SceneAssetLoader* SceneAssetLoader::m_pThis = nullptr;

SceneAssetLoader::SceneAssetLoader()
{
   m_pThis = this;
}

SceneAssetLoader::~SceneAssetLoader()
{
}

void SceneAssetLoader::LoadAsLevel(const std::string& fileNamePath, Level* o_pLevel)
{
    // Load the scene file into the level
    YAML::Node config = YAML::LoadFile(fileNamePath.c_str());
    std::string sceneType = config["SceneType"].as<std::string>();

    YAML::Node sceneGraph = config["SceneGraph"];

    m_currentScenePath = fileNamePath;

    for (const auto& itr : sceneGraph)
    {
        const std::string objName = itr.first.as<std::string>();
        const std::string type = itr.second["Type"].as<std::string>();
        if (type.compare("StaticMesh") == 0)
        {
            o_pLevel->LoadObject(objName, itr.second, StaticMesh::Deseralize);
        }
        else if (type.compare("AmbientLight") == 0)
        {
            o_pLevel->LoadObject(objName, itr.second, AmbientLight::Deseralize);
        }
        else if (type.compare("Camera") == 0)
        {
            o_pLevel->LoadObject(objName, itr.second, Camera::Deseralize);
        }
    }
}

void SceneAssetLoader::LoadStaticMesh(const std::string& fileNamePath, StaticMesh* pStaticMesh)
{
    //#TODO: Check the file extension and call the appropriate loader. E.g. OpenUSD
    LoadGltf(fileNamePath, pStaticMesh);
}

void SceneAssetLoader::LoadGltf(const std::string& fileNamePath, StaticMesh* pStaticMesh)
{
    const std::string fullGltfPathName = m_pThis->m_currentScenePath + fileNamePath;
    std::cout << "Loading gltf file: " << fullGltfPathName << std::endl;
}