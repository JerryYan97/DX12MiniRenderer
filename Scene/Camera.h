#pragma once
#include <cstdint>
#include <string>
#include "Object.h"
#include "../UI/InputHandler.h"

class InputHandler;

namespace YAML
{
    class Node;
}

enum CAMERA_MOVEMENT
{
    NONE,
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN,
    ROTATE, // Counter-clockwise. Right Hande. Around Top-Down Axis. Self-Rotate.
    CUSTOM_ANIM,
    MAX_CAMERA_MOVEMENT
};

class Camera : public Object
{
public:
    Camera(float* pPos, float* pView, float* pUp, float  fov, float near, float far);

    void CameraUpdate(); // Update projection matrix according to current window size and camera position every frame.

    static void BindKeyboardMouseInput(InputHandler* pInputHandler);

    static Object* Deseralize(const std::string& objName, const YAML::Node& i_node);

    static void MoveForward(StBindingInput input);
    static void MoveBackward(StBindingInput input);
    static void MoveRight(StBindingInput input);
    static void MoveLeft(StBindingInput input);
    static void MoveUp(StBindingInput input);
    static void MoveDown(StBindingInput input);

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

private:
    static Camera* m_pActiveCamera;
};