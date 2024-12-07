#include "RendererBackend.h"

RendererBackend* RendererBackend::m_pInstance = nullptr;

void RendererBackend::Init(RendererBackendInitStruct initStruct)
{
    m_pD3dDevice = initStruct.pD3dDevice;
    m_pDx12Debug = initStruct.pDx12Debug;
    m_pUIManager = initStruct.pUIManager;
    m_pEventManager = initStruct.pEventManager;
    m_pSceneAssetLoader = initStruct.pSceneAssetLoader;
    m_pLevel = initStruct.pLevel;
    m_windowWidth = initStruct.startWidth;
    m_windowHeight = initStruct.startHeight;
    m_pMainCommandQueue = initStruct.pMainCmdQueue;
    m_rtInfo = initStruct.rtInfo;

    CustomInit();
}

void RendererBackend::Deinit()
{
    CustomDeinit();
}