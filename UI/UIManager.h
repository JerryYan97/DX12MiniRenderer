#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <string>
#include <vector>
#include "imgui.h"

class HEventManager;

typedef void(*ImGUIGenFuncPtr) ();

// User Input Handling.
// ImGUI Render Commands Generation. (GUI Data Generation and State Control)
class UIManager
{
public:
    UIManager(ID3D12Device* i_pD3dDevice, HEventManager* i_pEventManager);
    ~UIManager();

    /*
    * Create UIManager.
    */
    void Init(ID3D12CommandQueue* iCmdQueue);

    bool ContinueRunning();

    // Record UI data (ImGUI Command List).
    // Wait for the Swapchain to be writable.
    void Tick(float deltaTime);

    // Present with vsync
    void Present() { m_pSwapChain->Present(1, 0); }

    void SetCustomImGUIFunc(ImGUIGenFuncPtr i_pCustomImGUIFunc) { m_pCustomImGUIGenFunc = i_pCustomImGUIFunc; }
    void CleanupCustomImGUIFunc() { m_pCustomImGUIGenFunc = nullptr; }

    void RecordDrawData(ID3D12GraphicsCommandList* iCmdList);

    UINT GetCurrentBackBufferIndex() { return m_pSwapChain->GetCurrentBackBufferIndex(); }
    ID3D12Resource* GetCurrentMainRTResource() { return m_mainRenderTargetResources[GetCurrentBackBufferIndex()]; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentMainRTDescriptor() { return m_mainRenderTargetDescriptors[GetCurrentBackBufferIndex()]; }
    ID3D12DescriptorHeap* GetImGUISrvDescHeap() { return m_pD3dImGUISrvDescHeap; }

    ID3D12Resource* GetCurrentMainDSVResource() { return m_mainDepthStencilResources[GetCurrentBackBufferIndex()]; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentMainDSVDescriptor() { return m_mainDepthStencilDescriptors[GetCurrentBackBufferIndex()]; }
    
    D3D12_RESOURCE_DESC GetCurrentRTResourceDesc() { return m_mainRenderTargetResources[GetCurrentBackBufferIndex()]->GetDesc(); }

    /*
    * Free all resources.
    */
    void Finalize();

    friend LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void GetWindowSize(uint32_t& oWidth, uint32_t& oHeight) { oWidth = m_windowWidth; oHeight = m_windowHeight; }
    void GetSwapchainRenderTargetSize(uint32_t& oWidth, uint32_t& oHeight)
    {
        D3D12_RESOURCE_DESC rtDesc = m_mainRenderTargetResources[0]->GetDesc();
        oWidth = static_cast<uint32_t>(rtDesc.Width);
        oHeight = rtDesc.Height;
    }

    // Member Variables
    const static unsigned int NUM_BACK_BUFFERS = 2;

private:
    void WindowResize(LPARAM lParam);
    void CreateSwapchainRenderTargets();
    void CleanupSwapchainRenderTargets();
    void FrameStart();

    // Reference to GPU Rsrcs managed by other classes.
    ID3D12Device*  const m_pD3dDevice;
    HEventManager* const m_pEventManager;

    // GPU/OS Rsrcs managed by this class.
    WNDCLASSEXW                              m_wc;
    IDXGISwapChain3*                         m_pSwapChain = nullptr;
    ID3D12DescriptorHeap*                    m_pD3dSwapchainRtvDescHeap = nullptr;
    ID3D12DescriptorHeap*                    m_pD3dImGUISrvDescHeap = nullptr;
    ID3D12DescriptorHeap*                    m_pD3dSwapchainDsvDescHeap = nullptr;
    HANDLE                                   m_hSwapChainWaitableObject = nullptr;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_mainRenderTargetDescriptors;
    std::vector<ID3D12Resource*>             m_mainRenderTargetResources;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_mainDepthStencilDescriptors;
    std::vector<ID3D12Resource*>             m_mainDepthStencilResources;

    // Other members.
    std::wstring    m_windowTitle;
    HWND            m_hWnd;
    ImGUIGenFuncPtr m_pCustomImGUIGenFunc;

    // Self Reference
    static UIManager* m_pThis;

    uint32_t m_windowWidth;
    uint32_t m_windowHeight;
};