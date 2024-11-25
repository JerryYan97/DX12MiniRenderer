#pragma once
#include <string>
#include <unordered_map>

class Level;
class Camera;
class StaticMesh;

typedef void(*PFN_SerializeAndCreate)(const std::string& i_fileNamePath);

class Serializer
{
public:
    Serializer() {}
    ~Serializer() {}

    static void RegisterObject(const std::string& identifier, PFN_SerializeAndCreate pFunc) { m_serializeCreateJmpTbl[identifier] = pFunc; }

private:
    static std::unordered_map<std::string, PFN_SerializeAndCreate> m_serializeCreateJmpTbl;
};

class SceneAssetLoader
{
public:
    SceneAssetLoader();
    ~SceneAssetLoader();

    // Load a scene file into a new level
    void LoadAsLevel(const std::string& fileNamePath, Level* o_pLevel);

    // Load a scene file into the input level
    void LoadInLevel(Level* i_pLevel, Level* o_pSubLevel) {}

    static void LoadStaticMesh(const std::string& fileNamePath, StaticMesh* pStaticMesh);

private:
    static void LoadGltf(const std::string& fileNamePath, StaticMesh* pStaticMesh);

    static SceneAssetLoader* m_pThis;
    std::string m_currentScenePath;
};