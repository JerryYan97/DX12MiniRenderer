#pragma once
#include "Object.h"
#include <cstring>

namespace YAML
{
    class Node;
}

class Light : public Object
{
public:
    Light() {}
    ~Light() {}
};

class AmbientLight : public Light
{
public:
    AmbientLight(float* i_pRadiance) { memcpy(radiance, i_pRadiance, sizeof(radiance)); m_objectType = "AmbientLight"; }
    ~AmbientLight() {}

    static Object* Deseralize(const std::string& objName, const YAML::Node& i_node);

private:
    float radiance[3];
};