#include "Level.h"

Level::Level()
{
}

Level::~Level()
{
    for (Object* pObj : m_objects)
    {
        delete pObj;
    }
}

void Level::LoadObject(const std::string& objName, const YAML::Node& i_node, PFN_CustomSerlizeObject i_func)
{
    m_objects.push_back(i_func(objName, i_node));
}