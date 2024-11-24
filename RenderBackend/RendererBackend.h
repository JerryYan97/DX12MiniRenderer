#pragma once
#include <d3d12.h>

class UIManager;
class HEventManager;
class SceneAssetLoader;

enum class RendererBackendType
{
    Forward,
    PathTracing
};

struct RendererBackendInitStruct
{
    ID3D12Device*  pD3dDevice = nullptr;
    ID3D12Debug*   pDx12Debug = nullptr;
    UIManager*     pUIManager = nullptr;
    HEventManager* pEventManager = nullptr;
    SceneAssetLoader*   pSceneAssetLoader = nullptr;
};

class RendererBackend
{
public:
    RendererBackend(RendererBackendType type)
    {
        m_type = type;
    }
    ~RendererBackend(){}

    void Init(RendererBackendInitStruct initStruct);

protected:
    ID3D12Device*  m_pD3dDevice = nullptr;
    ID3D12Debug*   m_pDx12Debug = nullptr;
    UIManager*     m_pUIManager = nullptr;
    HEventManager* m_pEventManager = nullptr;
    SceneAssetLoader*   m_pSceneAssetLoader = nullptr;

private:
    RendererBackendType m_type;
};