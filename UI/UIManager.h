#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <string>
#include <vector>

class HEventManager;

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
    void Init();

    bool ContinueRunning();
    void Tick(float deltaTime);

    /*
    * Free all resources.
    */
    void Finalize();

    friend LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Member Variables
    const static unsigned int NUM_BACK_BUFFERS = 3;

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
    HANDLE                                   m_hSwapChainWaitableObject = nullptr;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_mainRenderTargetDescriptors;
    std::vector<ID3D12Resource*>             m_mainRenderTargetResources;

    // Other members.
    std::wstring m_windowTitle;
    HWND         m_hWnd;

    // Self Reference
    static UIManager* m_pInstance;
};