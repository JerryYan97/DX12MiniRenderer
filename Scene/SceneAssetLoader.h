#pragma once
#include <string>
#include <unordered_map>
#include "EnvironmentMap.h"
#include "Mesh.h"

class Level;
class Camera;
class Material;
struct GeometryAsset;
struct ConstMaterialData;

typedef void(*PFN_SerializeAndCreate)(const std::string& i_fileNamePath);

namespace tinygltf
{
    struct Primitive;
    class  Model;
}

class Serializer
{
public:
    Serializer() {}
    ~Serializer() {}

    static void RegisterObject(const std::string& identifier, PFN_SerializeAndCreate pFunc) { m_serializeCreateJmpTbl[identifier] = pFunc; }

private:
    static std::unordered_map<std::string, PFN_SerializeAndCreate> m_serializeCreateJmpTbl;
};

// SceneLoader loads in yaml scene files and deserializes objects specified in files into Level objects.
class SceneLoader
{
public:
    SceneLoader();
    ~SceneLoader();

    // Load a scene file into a new level. So far we only load from a custom yaml. May also extend it to support gltf/openusd in the future.
    void LoadAsLevel(const std::string& fileNamePath, Level* o_pLevel);

    // Load a scene file into the input level
    void LoadInLevel(Level* i_pLevel, Level* o_pSubLevel) {}

    std::string GetCurrentScenePath() const { return m_currentScenePath; }

private:
    std::string m_currentScenePath;
    static SceneLoader* m_pThis;
};

// AssetLoader is called by level objects' deserialization function to load the relevant assets (e.g., mesh and textures) into the AssetManager.
// It wraps logic to avoid duplicated assets loading and also hides the details of how the assets are loaded from lower level like the AssetManager (e.g., using tinygltf to load gltf files).
//
// It also deals with different asset data format and call the renderer's internal API to create geometry assets or texture assets. So far it's gltf and custom env map.
class AssetLoader
{
public:
    AssetLoader();
    ~AssetLoader();

    void Init(std::string currentScenePath) { m_currentScenePath = currentScenePath; }

    // The path is relative to the scene folder since the caller doesn't know the absolute path of the scene folder. The AssetLoader will resolve the absolute path and load the asset into the AssetManager.
    static Mesh LoadAsOneMesh(const std::string& fileNamePath);
    static EnvironmentMap LoadAsEnvMap(const std::string& fileNamePath);

private:
    static Mesh LoadTinyGltfOneModelAsOneMesh(const std::string& fileNamePath); // This func assumes the gltf is not loaded before.
    static GeometryAsset* LoadOneGltfPrimGeometryAsset(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const int meshIdx, const int primIdx);
    static Material LoadOneGltfPrimMaterial(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const int meshIdx, const int primIdx);
    // static void LoadTextureAsset();

    bool IsAssetLoaded(const std::string& assetName) const
    {
        return m_AssetsMeshes.find(assetName) != m_AssetsMeshes.end();
    }

    static AssetLoader* m_pThis;
    std::string m_currentScenePath;

    // Assets meshes assets life time is managed by the AssetManager, so we do nothing in the destructor.
    std::unordered_map<std::string, Mesh> m_AssetsMeshes;
};