#include "Camera.h"
#include "yaml-cpp/yaml.h"
#include "../UI/UIManager.h"
#include "../Utils/MathUtils.h"
#include "../Utils/crc32.h"

extern UIManager* g_pUIManager;
Camera* Camera::m_pActiveCamera = nullptr;

Camera::Camera(
    float* pPos,
    float* pView,
    float* pUp,
    float  fov,
    float nearPlane,
    float farPane)
        : m_active(true),
        m_far(farPane),
        m_near(nearPlane),
        m_aspect(1.0f)
{
    memcpy(m_pos, pPos, 3 * sizeof(float));
    memcpy(m_view, pView, 3 * sizeof(float));
    memcpy(m_up, pUp, 3 * sizeof(float));
    memset(m_projMat, 0, 16 * sizeof(float));
    memset(m_viewMat, 0, 16 * sizeof(float));
    memset(m_vpMat, 0, 16 * sizeof(float));
    m_fov = fov;

    m_objectType = "Camera";
    m_objectTypeHash = crc32(m_objectType.c_str());
}

Object* Camera::Deseralize(const std::string& objName, const YAML::Node& i_node)
{
    std::vector<float> pos = i_node["Position"].as<std::vector<float>>();
    std::vector<float> view = i_node["View"].as<std::vector<float>>();
    std::vector<float> up = i_node["Up"].as<std::vector<float>>();
    float fov = i_node["fov"].as<float>();
    float farPlane = i_node["far"].as<float>();
    float nearPlane = i_node["near"].as<float>();
    bool isActive = i_node["isActive"].as<bool>();

    Camera* pCamera = new Camera(pos.data(),
                                 view.data(),
                                 up.data(),
                                 fov, nearPlane, farPlane);

    pCamera->m_active = isActive;
    pCamera->m_objectName = objName;

    uint32_t winWidth, winHeight;
    g_pUIManager->GetWindowSize(winWidth, winHeight);
    pCamera->m_aspect = (float)winHeight / (float)winWidth;

    m_pActiveCamera = pCamera;

    return pCamera;
}

void Camera::CameraUpdate()
{
    uint32_t winWidth, winHeight;
    g_pUIManager->GetWindowSize(winWidth, winHeight);
    m_aspect = (float)winHeight / (float)winWidth;

    GenPerspectiveProjMat(m_near, m_far, m_fov, m_aspect, m_projMat);
    GenViewMat(m_view, m_pos, m_up, m_viewMat);

    MatMulMat(m_projMat, m_viewMat, m_vpMat, 4);
}

void Camera::BindKeyboardMouseInput(InputHandler* pInputHandler)
{
    pInputHandler->BindKeyboardMouseHandler(KM_INPUT_KEY_W, MoveForward);
    pInputHandler->BindKeyboardMouseHandler(KM_INPUT_KEY_S, MoveBackward);
    pInputHandler->BindKeyboardMouseHandler(KM_INPUT_KEY_D, MoveRight);
    pInputHandler->BindKeyboardMouseHandler(KM_INPUT_KEY_A, MoveLeft);
    pInputHandler->BindKeyboardMouseHandler(KM_INPUT_KEY_E, MoveUp);
    pInputHandler->BindKeyboardMouseHandler(KM_INPUT_KEY_Q, MoveDown);
}

void Camera::MoveForward(StBindingInput input)
{
    if (m_pActiveCamera)
    {
        m_pActiveCamera->m_pos[0] += m_pActiveCamera->m_view[0] * input.fVals[0]; // Assuming fVals[0] corresponds to forward movement delta
    }
}

void Camera::MoveBackward(StBindingInput input)
{
    MoveForward(StBindingInput{-input.fVals[0], 0, 0, 0}); // Assuming fVals[0] corresponds to backward movement delta
}

void Camera::MoveRight(StBindingInput input)
{
    if (m_pActiveCamera)
    {
        m_pActiveCamera->m_pos[0] += m_pActiveCamera->m_view[1] * input.fVals[0]; // Assuming fVals[0] corresponds to right movement delta
    }
}

void Camera::MoveLeft(StBindingInput input)
{
    MoveRight(StBindingInput{-input.fVals[0], 0, 0, 0}); // Assuming fVals[0] corresponds to left movement delta
}

void Camera::MoveUp(StBindingInput input)
{
    if (m_pActiveCamera)
    {
        m_pActiveCamera->m_pos[1] += m_pActiveCamera->m_view[2] * input.fVals[0]; // Assuming fVals[0] corresponds to upward movement delta
    }
}

void Camera::MoveDown(StBindingInput input)
{
    MoveUp(StBindingInput{-input.fVals[0], 0, 0, 0}); // Assuming fVals[0] corresponds to downward movement delta
}
