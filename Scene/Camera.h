#pragma once
#include <cstdint>
#include <string>
#include "Object.h"

namespace YAML
{
    class Node;
}

class Camera : public Object
{
public:
    Camera(float* pPos, float* pView, float* pUp, float  fov, float near, float far);

    void CameraUpdate(); // Update projection matrix according to current window size and camera position every frame.

    static Object* Deseralize(const std::string& objName, const YAML::Node& i_node);

    float m_projMat[16];
    float m_viewMat[16];
    float m_vpMat[16];

    float m_pos[3];
    float m_view[3];
    float m_up[3];
    float m_fov; // Vertical fov.
    float m_aspect; // Width / Height;
    float m_far;  // Far and near are positive and m_far > m_near > 0.
    float m_near;
    bool  m_active;
};