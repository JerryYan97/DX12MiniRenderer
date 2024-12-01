#pragma once
#include <d3d12.h>
#include "../EventSystem/EventManager.h"
#include "../Utils/crc32.h"

class UIManager;
class HEventManager;
class SceneAssetLoader;
class Level;

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
    Level*         pLevel = nullptr;
    uint32_t      startWidth = 0;
    uint32_t      startHeight = 0;
};

class RendererBackend
{
public:
    RendererBackend(RendererBackendType type)
    {
        m_type = type;
        m_pInstance = this;
    }
    ~RendererBackend(){}

    void Init(RendererBackendInitStruct initStruct);
    void Deinit();

    virtual void RenderTick(ID3D12GraphicsCommandList* pCommandList) = 0;
    static void OnResizeCallback(HEventArguments args)
    {
        m_pInstance->m_windowWidth = std::any_cast<uint32_t>(args[crc32("Width")]);
        m_pInstance->m_windowHeight = std::any_cast<uint32_t>(args[crc32("Height")]);
        m_pInstance->CustomResize();
    }

    RendererBackendType GetType() { return m_type; }

protected:
    virtual void CustomResize() {}
    virtual void CustomInit() {}
    virtual void CustomDeinit() {}

    ID3D12Device*  m_pD3dDevice = nullptr;
    ID3D12Debug*   m_pDx12Debug = nullptr;
    UIManager*     m_pUIManager = nullptr;
    HEventManager* m_pEventManager = nullptr;
    SceneAssetLoader*   m_pSceneAssetLoader = nullptr;
    Level*         m_pLevel = nullptr;
    
    uint32_t         m_windowWidth;
    uint32_t         m_windowHeight;

private:
    RendererBackendType m_type;
    static RendererBackend* m_pInstance;
};