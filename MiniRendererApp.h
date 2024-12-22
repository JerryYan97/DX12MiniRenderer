#pragma once

#include <d3d12.h>
#include "EventSystem/EventManager.h"
#include "Scene/SceneAssetLoader.h"

class UIManager;
class Level;
class RendererBackend;
class AssetManager;

// It's possible to just use one fence like the ImGUI example but I prefer to use multiple fences for readability, which is more similar to Vulkan Fence.
// The custom rule for sync is that the fence value = 0 means the fence is not signaled and is waiting for something or is ready to be used.
// The fence value = 1 means the fence is signaled and is not waiting for something.
// Need to manually set the fence value to 0 after the fence is signaled and needs to wait for something.
struct FrameContext
{
    ID3D12CommandAllocator* CommandAllocator;
    ID3D12Fence*            Fence;
    HANDLE                  FenceEvent = nullptr;
};

class DX12MiniRenderer
{
public:
    DX12MiniRenderer();
    ~DX12MiniRenderer();

    /*
    * Create UIManager.
    * Create DX12 Device.
    */
    void Init();

    /*
    * The main loop of the application.
    */
    void Run();

    /*
    * Free all resources.
    */
    void Finalize();

private:
    void InitDevice();
    static void WaitGpuIdle(HEventArguments args);
    static void GenerateImGUIStates();

    ID3D12Device*    m_pD3dDevice = nullptr;
    ID3D12Debug*     m_pDx12Debug = nullptr;
    UIManager*       m_pUIManager = nullptr;
    HEventManager    m_eventManager;
    SceneAssetLoader m_sceneAssetLoader;
    AssetManager*    m_pAssetManager = nullptr;

    static DX12MiniRenderer* m_pThis;

    // Temp Renderer Infarstructure
    void InitTempRendererInfarstructure();
    void CleanupTempRendererInfarstructure();
    FrameContext* WaitForCurrentFrameResources();
    void static TempRendererWaitGpuIdle();

    ID3D12CommandQueue*        m_pD3dCommandQueue = nullptr;
    ID3D12GraphicsCommandList* m_pD3dCommandList = nullptr;
    // ID3D12Fence*               m_fence = nullptr;
    // HANDLE                     m_fenceEvent = nullptr;
    std::vector<FrameContext>  m_frameContexts;

    RendererBackend* m_pRendererBackend = nullptr;

    // Temp Root Level
    Level* m_pLevel = nullptr;

    // Temp UI Infarstructure
    static bool show_demo_window;
    static bool show_another_window;
    static bool clear_color;
};