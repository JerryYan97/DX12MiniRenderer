#include "RendererBackend.h"

void RendererBackend::Init(RendererBackendInitStruct initStruct)
{
    m_pD3dDevice = initStruct.pD3dDevice;
    m_pDx12Debug = initStruct.pDx12Debug;
    m_pUIManager = initStruct.pUIManager;
    m_pEventManager = initStruct.pEventManager;
    m_pSceneAssetLoader = initStruct.pSceneAssetLoader;
}