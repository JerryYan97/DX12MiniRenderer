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
    Camera(
        float* pPos,
        float* pView,
        float* pUp,
        float  fov,
        float near,
        float far)
        : m_active(true),
        m_far(far),
        m_near(near),
        m_aspect(1.0f)
    {
        memcpy(m_pos, pPos, 3 * sizeof(float));
        memcpy(m_view, pView, 3 * sizeof(float));
        memcpy(m_up, pUp, 3 * sizeof(float));
        m_fov = fov;

        m_objectType = "Camera";
    }

    void ScreenSizeUpdated(float width, float height)
    {
        m_aspect = width / height;
    }

    static Object* Deseralize(const YAML::Node& i_node);

    float m_pos[3];
    float m_view[3];
    float m_up[3];
    float m_fov;
    float m_aspect; // Width / Height;
    float m_far;  // Far and near are positive and m_far > m_near > 0.
    float m_near;
    bool  m_active;
};