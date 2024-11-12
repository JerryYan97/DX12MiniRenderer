#pragma once
#include <string>
#include "Object.h"

namespace YAML
{
    class Node;
}

class StaticMesh : public Object
{
public:
    StaticMesh();
    ~StaticMesh() {}

    static Object* Deseralize(const YAML::Node& i_node);

private:
    float m_position[3];
    float m_rotation[3];
    float m_scale[3];

    std::string m_assetPath;
    bool        m_loadedInRAM;
    bool        m_loadedInVRAM;
};