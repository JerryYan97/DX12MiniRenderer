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
    HEventManager* pEventManager = HEventManager::HEventManagerInstance();
    pEventManager->RegisterListener("MoveForward", MoveForward);
    pEventManager->RegisterListener("MoveBackward", MoveBackward);
    pEventManager->RegisterListener("MoveRight", MoveRight);
    pEventManager->RegisterListener("MoveLeft", MoveLeft);
    pEventManager->RegisterListener("MoveUp", MoveUp);
    pEventManager->RegisterListener("MoveDown", MoveDown);
}

void Camera::MoveForward(HEventArguments args)
{
    if (m_pActiveCamera)
    {
        float fVals = std::any_cast<float>(args[crc32("delta")]);

        float delta[3] = {
            m_pActiveCamera->m_view[0] * fVals,
            m_pActiveCamera->m_view[1] * fVals,
            m_pActiveCamera->m_view[2] * fVals
        };
        VecAdd(m_pActiveCamera->m_pos, delta, 3, m_pActiveCamera->m_pos);
    }
}

void Camera::MoveBackward(HEventArguments args)
{
    args[crc32("delta")] = -1.f * std::any_cast<float>(args[crc32("delta")]);
    MoveForward(args);
}

void Camera::MoveRight(HEventArguments args)
{
    if (m_pActiveCamera)
    {
        float right[3] = {};
        CrossProductVec3(m_pActiveCamera->m_view, m_pActiveCamera->m_up, right);
        NormalizeVec(right, 3);

        float fVals = std::any_cast<float>(args[crc32("delta")]);
        ScalarMul(-fVals, right, 3);
        VecAdd(m_pActiveCamera->m_pos, right, 3, m_pActiveCamera->m_pos);
    }
}

void Camera::MoveLeft(HEventArguments args)
{
    args[crc32("delta")] = -1.f * std::any_cast<float>(args[crc32("delta")]);
    MoveRight(args);
}

void Camera::MoveUp(HEventArguments args)
{
    if (m_pActiveCamera)
    {
        float upDelta[3] = {};
        memcpy(upDelta, m_pActiveCamera->m_up, 3 * sizeof(float));
        NormalizeVec(upDelta, 3);

        float fVals = std::any_cast<float>(args[crc32("delta")]);
        ScalarMul(fVals, upDelta, 3);
        VecAdd(m_pActiveCamera->m_pos, upDelta, 3, m_pActiveCamera->m_pos);
    }
}

void Camera::MoveDown(HEventArguments args)
{
    args[crc32("delta")] = -1.f * std::any_cast<float>(args[crc32("delta")]);
    MoveUp(args); // Assuming fVals[0] corresponds to downward movement delta
}

void Camera::RotateCamera(HEventArguments args)
{
    if (m_pActiveCamera)
    {
        // Implement camera rotation logic here based on input.fVals
        // This is a placeholder for actual rotation logic
        float deltaTime = std::any_cast<float>(args[crc32("delta")]);
        const float rotationSpeed = 0.1f; // Adjust rotation speed as needed
        float angle = rotationSpeed * deltaTime;

        float camToViewPt[3] = { m_pActiveCamera->m_view[0], m_pActiveCamera->m_view[1], m_pActiveCamera->m_view[2] };
        ScalarMul(m_pActiveCamera->m_viewDist, camToViewPt, 3);
        float viewPt[3] = {};
        VecAdd(m_pActiveCamera->m_pos, camToViewPt, 3, viewPt);

        float rotMatY[9] = {};
        float newView[3] = {};

        GenRotationMatY(angle, rotMatY);
        MatMulVec(rotMatY, m_pActiveCamera->m_view, 3, newView);
        NormalizeVec(newView, 3);
        memcpy(m_pActiveCamera->m_view, newView, 3 * sizeof(float));

        float newCamPos[3] = {};
        ScalarMul(-m_pActiveCamera->m_viewDist, newView, 3);
        VecAdd(viewPt, newView, 3, newCamPos);
        memcpy(m_pActiveCamera->m_pos, newCamPos, 3 * sizeof(float));
    }
}
