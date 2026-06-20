#pragma once
#include <cstdint>
#include <string>
#include "Object.h"
#include "../UI/InputHandler.h"
#include "../EventSystem/EventManager.h"

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
    MAX_CAMERA_MOVEMENT
};

enum CAMERA_MODE
{
    USER_CONTROLLED,
    ANIMATION
};

class Camera : public Object
{
public:
    Camera(float* pPos, float* pView, float* pUp, float  fov, float near, float far);

    void SetCameraMode(CAMERA_MODE mode) { m_cameraMode = mode; }

    static void BindKeyboardMouseInput(InputHandler* pInputHandler);

    static Object* Deseralize(const std::string& objName, const YAML::Node& i_node);

    static void MoveForward(HEventArguments args);
    static void MoveBackward(HEventArguments args);
    static void MoveRight(HEventArguments args);
    static void MoveLeft(HEventArguments args);
    static void MoveUp(HEventArguments args);
    static void MoveDown(HEventArguments args);
    static void RotateCamera(HEventArguments args);
    static void ZoomCamera(HEventArguments args); // It's different from moving forward/backward. It changes the view distance to the look-at point.
    static void CenterCamera(HEventArguments args);

    virtual void Tick(float DeltaTime) override;

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

    float m_viewDist = 100.f; // By combining with m_pos and m_view, we get the current 'look-at' point.

    bool  m_active;

private:
    void CameraUpdate(); // Update projection matrix according to current window size and camera position every frame.

    static Camera* m_pActiveCamera;
    CAMERA_MODE m_cameraMode = CAMERA_MODE::USER_CONTROLLED;
};