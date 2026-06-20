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
    pCamera->m_aspect = (float)winWidth / (float)winHeight;

    m_pActiveCamera = pCamera;

    return pCamera;
}

void Camera::CameraUpdate()
{
    uint32_t winWidth, winHeight;
    g_pUIManager->GetWindowSize(winWidth, winHeight);
    m_aspect = (float)winWidth / (float)winHeight;

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
    pEventManager->RegisterListener("RotateCamera", RotateCamera);
    pEventManager->RegisterListener("ZoomCamera", ZoomCamera);
    pEventManager->RegisterListener("CenterCamera", CenterCamera);
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

void Camera::ZoomCamera(HEventArguments args) // It's different from moving forward/backward. It changes the view distance to the look-at point.
{
    if (m_pActiveCamera && (m_pActiveCamera->m_viewDist > 0))
    {
        float camToViewPt[3] = { m_pActiveCamera->m_view[0], m_pActiveCamera->m_view[1], m_pActiveCamera->m_view[2] };
        ScalarMul(m_pActiveCamera->m_viewDist, camToViewPt, 3);
        float viewPt[3] = {};
        VecAdd(m_pActiveCamera->m_pos, camToViewPt, 3, viewPt);

        float delta = std::any_cast<float>(args[crc32("delta")]);
        const float zoomSpeed = 0.5f;
        m_pActiveCamera->m_viewDist += zoomSpeed * delta;

        float newCamPos[3] = {};
        float viewPtToCam[3] = { -m_pActiveCamera->m_view[0], -m_pActiveCamera->m_view[1], -m_pActiveCamera->m_view[2] };
        ScalarMul(m_pActiveCamera->m_viewDist, viewPtToCam, 3);
        VecAdd(viewPt, viewPtToCam, 3, newCamPos);
        memcpy(m_pActiveCamera->m_pos, newCamPos, 3 * sizeof(float));
    }
}

void Camera::CenterCamera(HEventArguments args)
{
    if (m_pActiveCamera)
    {
        float centerX = std::any_cast<float>(args[crc32("centerX")]);
        float centerY = std::any_cast<float>(args[crc32("centerY")]);
        float centerZ = std::any_cast<float>(args[crc32("centerZ")]);
        float center[3] = { centerX, centerY, centerZ };

        float camAdjustPos[3] = { m_pActiveCamera->m_view[0], m_pActiveCamera->m_view[1], m_pActiveCamera->m_view[2] };
        ScalarMul(-1.f * m_pActiveCamera->m_viewDist, camAdjustPos, 3);
        VecAdd(center, camAdjustPos, 3, m_pActiveCamera->m_pos);

        float bbxMinX = std::any_cast<float>(args[crc32("bbxMinX")]);
        float bbxMinY = std::any_cast<float>(args[crc32("bbxMinY")]);
        float bbxMinZ = std::any_cast<float>(args[crc32("bbxMinZ")]);
        float bbxMaxX = std::any_cast<float>(args[crc32("bbxMaxX")]);
        float bbxMaxY = std::any_cast<float>(args[crc32("bbxMaxY")]);
        float bbxMaxZ = std::any_cast<float>(args[crc32("bbxMaxZ")]);

        float levelBBXMin[4] = {bbxMinX, bbxMinY, bbxMinZ, 1.f};
        float levelBBXMax[4] = {bbxMaxX, bbxMaxY, bbxMaxZ, 1.f};
        float viewMat[16] = {};
        float projMat[16] = {};
        float vpMat[16] = {};
        // float camNewPos[3] = {};
        // memcpy(camNewPos, m_pActiveCamera->m_pos, 3 * sizeof(float));

        bool bIsWidthGreater = true;
        
        // TODO: Need to experiment the 8 points screen space occupation calculation.
        auto pfnClipLevelBBXOccRatio = [&]() -> float {
            float clipSpaceBBXMin[3] = {  FLT_MAX,  FLT_MAX,  FLT_MAX};
            float clipSpaceBBXMax[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX};

            GenPerspectiveProjMat(m_pActiveCamera->m_near,
                                  m_pActiveCamera->m_far,
                                  m_pActiveCamera->m_fov,
                                  m_pActiveCamera->m_aspect,
                                  projMat);

            GenViewMat(m_pActiveCamera->m_view, m_pActiveCamera->m_pos, m_pActiveCamera->m_up, viewMat);
            MatMulMat(projMat, viewMat, vpMat, 4);

            // 8 points clip space bounding box calculation.
            float levelBBXPoints[8][4] = {
                {levelBBXMin[0], levelBBXMin[1], levelBBXMin[2], 1.f},
                {levelBBXMin[0], levelBBXMin[1], levelBBXMax[2], 1.f},
                {levelBBXMin[0], levelBBXMax[1], levelBBXMin[2], 1.f},
                {levelBBXMin[0], levelBBXMax[1], levelBBXMax[2], 1.f},
                {levelBBXMax[0], levelBBXMin[1], levelBBXMin[2], 1.f},
                {levelBBXMax[0], levelBBXMin[1], levelBBXMax[2], 1.f},
                {levelBBXMax[0], levelBBXMax[1], levelBBXMin[2], 1.f},
                {levelBBXMax[0], levelBBXMax[1], levelBBXMax[2], 1.f}
            };

            for (int i = 0; i < 8; i++)
            {
                MatMulVec(vpMat, &levelBBXPoints[i][0], 4, &levelBBXPoints[i][0]);
                levelBBXPoints[i][0] /= levelBBXPoints[i][3];
                levelBBXPoints[i][1] /= levelBBXPoints[i][3];
                levelBBXPoints[i][2] /= levelBBXPoints[i][3];

                clipSpaceBBXMin[0] = min(clipSpaceBBXMin[0], levelBBXPoints[i][0]);
                clipSpaceBBXMin[1] = min(clipSpaceBBXMin[1], levelBBXPoints[i][1]);
                clipSpaceBBXMin[2] = min(clipSpaceBBXMin[2], levelBBXPoints[i][2]);

                clipSpaceBBXMax[0] = max(clipSpaceBBXMax[0], levelBBXPoints[i][0]);
                clipSpaceBBXMax[1] = max(clipSpaceBBXMax[1], levelBBXPoints[i][1]);
                clipSpaceBBXMax[2] = max(clipSpaceBBXMax[2], levelBBXPoints[i][2]);
            }
            //

            printf("min: <%f, %f, %f>. max: <%f, %f, %f>\n", clipSpaceBBXMin[0], clipSpaceBBXMin[1], clipSpaceBBXMin[2],
                                                             clipSpaceBBXMax[0], clipSpaceBBXMax[1], clipSpaceBBXMax[2]);
            

            float levelBBXWidth = abs(clipSpaceBBXMax[0] - clipSpaceBBXMin[0]);
            float levelBBXHeight = abs(clipSpaceBBXMax[1] - clipSpaceBBXMin[1]);
            if (levelBBXWidth > levelBBXHeight)
            {
                bIsWidthGreater = true;
            }
            else
            {
                bIsWidthGreater = false;
            }

            // Adjust view distance.
            float occupyRatio = bIsWidthGreater ? levelBBXWidth / 2.f : levelBBXHeight / 2.f;
            return occupyRatio;
        };
        
        float stepping = 0.3f;
        float occupyRatio = pfnClipLevelBBXOccRatio();
        while (occupyRatio < 0.3f)
        {
            HEventArguments args;
            args[crc32("delta")] = -stepping;
            ZoomCamera(args);
            occupyRatio = pfnClipLevelBBXOccRatio();

            printf("Occupy Radio: %f\n", occupyRatio);
        }
    }
}

void Camera::Tick(float DeltaTime)
{
    CameraUpdate();
}