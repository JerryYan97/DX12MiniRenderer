#pragma once
#include <d3d12.h>
#include "../EventSystem/EventManager.h"
#include "../Utils/crc32.h"

class UIManager;
class HEventManager;
class SceneAssetLoader;
class Level;
class FrameContext;

enum class RendererBackendType
{
    Forward,
    PathTracing
};

struct RenderTargetInfo
{
    ID3D12Resource* pResource = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
};

struct RendererBackendInitStruct
{
    ID3D12Device5*  pD3dDevice = nullptr;
    ID3D12Debug*   pDx12Debug = nullptr;
    UIManager*     pUIManager = nullptr;
    HEventManager* pEventManager = nullptr;
    SceneAssetLoader*   pSceneAssetLoader = nullptr;
    Level*         pLevel = nullptr;
    ID3D12CommandQueue* pMainCmdQueue = nullptr;
    FrameContext*  pInitFrameContext = nullptr;
    ID3D12GraphicsCommandList4* pCommandList = nullptr;
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

    virtual void RenderTick(ID3D12GraphicsCommandList* pCommandList, RenderTargetInfo rtInfo) = 0;
    static void OnResizeCallback(HEventArguments args)
    {
        uint32_t width = std::any_cast<uint32_t>(args[crc32("Width")]);
        uint32_t height = std::any_cast<uint32_t>(args[crc32("Height")]);
        m_pInstance->CustomResize(width, height);
    }

    RendererBackendType GetType() { return m_type; }

    virtual void Resize(uint32_t width, uint32_t height) {}

protected:
    virtual void CustomResize(uint32_t width, uint32_t height) {}
    virtual void CustomInit() {}
    virtual void CustomDeinit() {}

    ID3D12Device5* m_pD3dDevice = nullptr;
    ID3D12Debug*   m_pDx12Debug = nullptr;
    ID3D12CommandQueue* m_pMainCommandQueue = nullptr;
    UIManager*     m_pUIManager = nullptr;
    HEventManager* m_pEventManager = nullptr;
    SceneAssetLoader*   m_pSceneAssetLoader = nullptr;
    Level*         m_pLevel = nullptr;
    FrameContext*  m_pInitFrameContext = nullptr;
    ID3D12GraphicsCommandList4* m_pCommandList = nullptr;

private:
    RendererBackendType m_type;
    static RendererBackend* m_pInstance;
};