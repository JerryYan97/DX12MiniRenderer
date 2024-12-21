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
    AmbientLight(float* i_pRadiance)
    { 
        memcpy(radiance, i_pRadiance, sizeof(radiance));
        m_objectType = "AmbientLight";
    }
    ~AmbientLight() {}

    static Object* Deseralize(const std::string& objName, const YAML::Node& i_node);

private:
    float radiance[3];
};

class PointLight : public Light
{
public:
    PointLight(float* i_pos, float* i_rad)
    {
        memcpy(position, i_pos, sizeof(position));
        memcpy(radiance, i_rad, sizeof(radiance));
        m_objectType = "PointLight";
    }
    ~PointLight() {}

    static Object* Deseralize(const std::string& objName, const YAML::Node& i_node);

    float position[3];
    float radiance[3];
private:
};