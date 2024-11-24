#pragma once
#include <vector>
#include "Object.h"

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

private:
    std::vector<Object*> m_objects;
};