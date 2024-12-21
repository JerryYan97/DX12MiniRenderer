#pragma once
#include <vector>
#include "Object.h"

class StaticMesh;
class Camera;
class Light;

namespace YAML
{
    class Node;
}

typedef Object* (*PFN_CustomSerlizeObject)(const std::string& objName, const YAML::Node& i_node);

class Level
{
public:
    Level();
    ~Level();

    void LoadObject(const std::string& objName, const YAML::Node& i_node, PFN_CustomSerlizeObject i_func);

    void RetriveStaticMeshes(std::vector<StaticMesh*>& o_staticMeshes);
    void RetriveActiveCamera(Camera** o_camera);
    void RetriveLights(std::vector<Light*>& o_lights);

private:
    std::vector<Object*> m_objects;
};