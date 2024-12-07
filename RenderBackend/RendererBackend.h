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

struct RenderTargetInfo
{
    ID3D12Resource* pResource = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE* pRtvHandle;
};

struct RendererBackendInitStruct
{
    ID3D12Device*  pD3dDevice = nullptr;
    ID3D12Debug*   pDx12Debug = nullptr;
    UIManager*     pUIManager = nullptr;
    HEventManager* pEventManager = nullptr;
    SceneAssetLoader*   pSceneAssetLoader = nullptr;
    Level*         pLevel = nullptr;
    ID3D12CommandQueue* pMainCmdQueue = nullptr;
    uint32_t      startWidth = 0;
    uint32_t      startHeight = 0;
    RenderTargetInfo rtInfo;
};

class RendererBackend
{
public:
    RendererBackend(RendererBackendType type) :
        m_windowWidth(0),
        m_windowHeight(0)
    {
        m_type = type;
        m_pInstance = this;
    }
    ~RendererBackend(){}

    void Init(RendererBackendInitStruct initStruct);
    void Deinit();

    virtual void RenderTick(ID3D12GraphicsCommandList* pCommandList, D3D12_CPU_DESCRIPTOR_HANDLE* pRT = nullptr) = 0;
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
    ID3D12CommandQueue* m_pMainCommandQueue = nullptr;
    UIManager*     m_pUIManager = nullptr;
    HEventManager* m_pEventManager = nullptr;
    SceneAssetLoader*   m_pSceneAssetLoader = nullptr;
    Level*         m_pLevel = nullptr;
    
    uint32_t         m_windowWidth;
    uint32_t         m_windowHeight;

    RenderTargetInfo m_rtInfo;

private:
    RendererBackendType m_type;
    static RendererBackend* m_pInstance;
};