#include "Level.h"
#include "Mesh.h"
#include "Camera.h"
#include "Lights.h"
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

void Level::RetriveMeshObjects(std::vector<MeshObject*>& o_meshObjects)
{
    for (Object* pObj : m_objects)
    {
        if (pObj->GetObjectTypeHash() == crc32("MeshObject"))
        {
            MeshObject* pMeshObject = dynamic_cast<MeshObject*>(pObj);
            o_meshObjects.push_back(pMeshObject);
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

void Level::RetriveLights(std::vector<Light*>& o_lights)
{
    for (Object* pObj : m_objects)
    {
        if (pObj->GetObjectTypeHash() == crc32("AmbientLight") || pObj->GetObjectTypeHash() == crc32("PointLight") || pObj->GetObjectTypeHash() == crc32("ImageBasedLight"))
        {
            Light* pLight = dynamic_cast<Light*>(pObj);
            o_lights.push_back(pLight);
        }
    }
}

void Level::AttachEnvMapGPUResource(ID3D12Device5* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE envMapBkGrdDescriptorHeapHandle)
{
    m_envMap.AttachEnvMapGPUResource(pDevice, envMapBkGrdDescriptorHeapHandle);
}

void Level::Tick(float DeltaTime)
{
    for (Object* pObj : m_objects)
    {
        pObj->Tick(DeltaTime);
    }
}