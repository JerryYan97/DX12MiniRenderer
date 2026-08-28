#pragma once
#include <vector>
#include <string>
#include "Object.h"
#include "Mesh.h"
#include "../RenderBackend/RendererBackend.h"
#include "EnvironmentMap.h"

class Camera;
class Light;

namespace YAML
{
    class Node;
}

typedef Object* (*PFN_CustomSerlizeObject)(const std::string& objName, const YAML::Node& i_node);
typedef std::vector<Object*> (*PFN_CustomDeserializeMultipleObjects)(const std::string& objCommonName, const YAML::Node& i_node);

enum BackgroundType : uint32_t
{
    BLACK,
    DEFAULT_SKY,
    CONST_COLOR
};

// Level represents the scene to be rendered. It contains the scene graph of objects and scene-level info. E.g. environment map, bbx, level center, etc.
class Level
{
public:
    Level();
    ~Level();

    void Tick(float DeltaTime);

    void LoadObject(const std::string& objName, const YAML::Node& i_node, PFN_CustomSerlizeObject i_func);
    void LoadMultipleObjects(const std::string& objCommonName, const YAML::Node& i_node, PFN_CustomDeserializeMultipleObjects i_func);

    void RetriveMeshObjects(std::vector<MeshObject*>& o_meshObjects);
    void RetriveActiveCamera(Camera** o_camera);
    void RetriveLights(std::vector<Light*>& o_lights);

    void SetLevelCenter(float center[3]) { m_levelCenter[0] = center[0]; m_levelCenter[1] = center[1]; m_levelCenter[2] = center[2]; }
    void GetLevelCenter(float* outCenter) { outCenter[0] = m_levelCenter[0]; outCenter[1] = m_levelCenter[1]; outCenter[2] = m_levelCenter[2]; }

    void SetBoundingBox(float bbxMin[3], float bbxMax[3])
    {
        memcpy(m_bbxMin, bbxMin, sizeof(float) * 3);
        memcpy(m_bbxMax, bbxMax, sizeof(float) * 3);
    }

    void GetBoundingBox(float* outBbxMin, float* outBbxMax)
    {
        memcpy(outBbxMin, m_bbxMin, sizeof(float) * 3);
        memcpy(outBbxMax, m_bbxMax, sizeof(float) * 3);
    }

    void SetEnvMapAndIBL(EnvironmentMap envMap) { m_envMap = envMap; }
    void AttachEnvMapGPUResource(ID3D12Device5* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE envMapBkGrdDescriptorHeapHandle); // Multiple Heaps in Future.
    void AttachEnvMapIBLGPUResource(ID3D12Device5* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE diffIrradianceDescHeapHandle, D3D12_CPU_DESCRIPTOR_HANDLE envBrdfDescHeapHandle, D3D12_CPU_DESCRIPTOR_HANDLE prefilterEnvMapDescHeapHandle);

    std::string m_sceneName;
    float m_backgroundColor[3];

    RendererBackendType m_rendererBackendType;
    BackgroundType m_backgroundType;

    bool HasEnvMap() const { return m_envMap.IsLoaded(); }
private:
    std::vector<Object*> m_objects;
    EnvironmentMap m_envMap;
    float m_levelCenter[3] = {};

    float m_bbxMin[3] = {};
    float m_bbxMax[3] = {};
};