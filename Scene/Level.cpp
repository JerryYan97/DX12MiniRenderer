#include "Level.h"
#include "Mesh.h"
#include "Camera.h"
#include "../Utils/crc32.h"

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

void Level::RetriveStaticMeshes(std::vector<StaticMesh*>& o_staticMeshes)
{
    for (Object* pObj : m_objects)
    {
        if (pObj->GetObjectTypeHash() == crc32("StaticMesh"))
        {
            StaticMesh* pStaticMesh = dynamic_cast<StaticMesh*>(pObj);
            o_staticMeshes.push_back(pStaticMesh);
        }
    }
}

void Level::RetriveActiveCamera(Camera** o_camera)
{
    for (Object* pObj : m_objects)
    {
        const unsigned int tempCheck = crc32("Camera");
        const unsigned int objHashCheck = pObj->GetObjectTypeHash();
        if (pObj->GetObjectTypeHash() == crc32("Camera"))
        {
            Camera* pCamera = dynamic_cast<Camera*>(pObj);
            if (pCamera->m_active)
            {
                *o_camera = pCamera;
                return;
            }
        }
    }
}