#pragma once
#include <vector>
#include <string>
#include "Object.h"
#include "../RenderBackend/RendererBackend.h"

class StaticMesh;
class Camera;
class Light;

namespace YAML
{
    class Node;
}

typedef Object* (*PFN_CustomSerlizeObject)(const std::string& objName, const YAML::Node& i_node);

enum BackgroundType : uint32_t
{
    BLACK,
    DEFAULT_SKY,
    CONST_COLOR
};

class Level
{
public:
    Level();
    ~Level();

    void LoadObject(const std::string& objName, const YAML::Node& i_node, PFN_CustomSerlizeObject i_func);

    void RetriveStaticMeshes(std::vector<StaticMesh*>& o_staticMeshes);
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

    std::string m_sceneName;
    float m_backgroundColor[3];

    RendererBackendType m_rendererBackendType;
    BackgroundType m_backgroundType;

private:
    std::vector<Object*> m_objects;
    float m_levelCenter[3] = {};

    float m_bbxMin[3] = {};
    float m_bbxMax[3] = {};
};