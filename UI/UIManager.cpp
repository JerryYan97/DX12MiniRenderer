#include "UIManager.h"
#include "../EventSystem/EventManager.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "../Utils/crc32.h"

UIManager* UIManager::m_pThis = nullptr;

UIManager::UIManager(ID3D12Device*  i_pD3dDevice,
                     HEventManager* i_pEventManager)
    : m_pD3dDevice(i_pD3dDevice),
      m_pEventManager(i_pEventManager),
      m_pCustomImGUIGenFunc(nullptr)
{
    m_pThis = this;
    m_windowTitle = L"DX12MiniRenderer Default Scene";
}

UIManager::~UIManager()
{
    m_pThis = nullptr;
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            UIManager::m_pThis->WindowResize(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

void UIManager::WindowResize(LPARAM lParam)
{
    m_windowWidth = (UINT)LOWORD(lParam);
    m_windowHeight = (UINT)HIWORD(lParam);

    // Inform other systems that we are waiting for the GPU to be idle before resizing the swapchain.
    HEventArguments args;
    HEvent WaitGpuIdleEvent(args, "WaitGpuIdle");
    m_pEventManager->SendEvent(WaitGpuIdleEvent);

    HEventArguments resizeArgs;
    resizeArgs[crc32("Width")] = (UINT)LOWORD(lParam);
    resizeArgs[crc32("Height")] = (UINT)HIWORD(lParam);

    HEvent ResizeEvent(resizeArgs, "ResizeSwapchain");
    m_pEventManager->SendEvent(ResizeEvent);

    CleanupSwapchainRenderTargets();
    HRESULT result = m_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
    assert(SUCCEEDED(result) && "Failed to resize swapchain.");
    CreateSwapchainRenderTargets();
}

void UIManager::Init(ID3D12CommandQueue* iCmdQueue)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    m_wc = { sizeof(m_wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"DX12MiniRenderer", nullptr };
    ::RegisterClassExW(&m_wc);
    m_hWnd = ::CreateWindowW(m_wc.lpszClassName, m_windowTitle.c_str(), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, m_wc.hInstance, nullptr);
    m_windowWidth = 1280;
    m_windowHeight = 800;

    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC1 sd;
    {
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = NUM_BACK_BUFFERS;
        sd.Width = 0;
        sd.Height = 0;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.Stereo = FALSE;
    }

    {
        IDXGIFactory4* dxgiFactory = nullptr;
        IDXGISwapChain1* swapChain1 = nullptr;
        CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
        // The command queue for swapchain back buffer rendering must be same as the command queue used to create the swapchain.
        dxgiFactory->CreateSwapChainForHwnd(iCmdQueue, m_hWnd, &sd, nullptr, nullptr, &swapChain1);
        swapChain1->QueryInterface(IID_PPV_ARGS(&m_pSwapChain));
        swapChain1->Release();
        dxgiFactory->Release();
        m_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
        m_hSwapChainWaitableObject = m_pSwapChain->GetFrameLatencyWaitableObject();
    }

    // Create Swap chain Render Target View Descriptor Heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = NUM_BACK_BUFFERS;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;
        m_pD3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pD3dSwapchainRtvDescHeap));

        SIZE_T rtvDescriptorSize = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pD3dSwapchainRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            m_mainRenderTargetDescriptors.push_back(rtvHandle);
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    // Create ImGUI Textures Descriptor Heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_pD3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pD3dImGUISrvDescHeap));
    }

    CreateSwapchainRenderTargets();

    // Initialize ImGui and Show Window
    // Show the window
    ::ShowWindow(m_hWnd, SW_SHOWDEFAULT);
    ::UpdateWindow(m_hWnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_hWnd);
    ImGui_ImplDX12_Init(m_pD3dDevice, NUM_BACK_BUFFERS,
        DXGI_FORMAT_R8G8B8A8_UNORM, m_pD3dImGUISrvDescHeap,
        m_pD3dImGUISrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        m_pD3dImGUISrvDescHeap->GetGPUDescriptorHandleForHeapStart());
}

void UIManager::FrameStart()
{
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void UIManager::RecordDrawData(ID3D12GraphicsCommandList* iCmdList)
{
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), iCmdList);
}

void UIManager::Tick(float deltaTime)
{
    FrameStart();

    // UIManager can serve to execute custom ImGUI generation functions.
    // It will be called until the served renderer component reset it to nullptr.
    if (m_pCustomImGUIGenFunc)
    {
        m_pCustomImGUIGenFunc();
    }

    // Rendering
    ImGui::Render();

    // Wait for Swapchain to be writable.
    WaitForSingleObject(m_hSwapChainWaitableObject, INFINITE);
}

void UIManager::CreateSwapchainRenderTargets()
{
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        ID3D12Resource* pBackBuffer = nullptr;
        m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));

        //
        std::wstring bufferName = L"Swapchain Buffer " + std::to_wstring(i);
        pBackBuffer->SetName(bufferName.c_str());
        //

        m_pD3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, m_mainRenderTargetDescriptors[i]);
        m_mainRenderTargetResources.push_back(pBackBuffer); // Note that we need to set the vector size to zero when we destroy Swapchain render targets.
    }
}

bool UIManager::ContinueRunning()
{
    bool done = false;
    // Poll and handle messages (inputs, window resize, etc.)
    // See the WndProc() function below for our to dispatch events to the Win32 backend.
    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
            done = true;
    }

    return !done;
}

void UIManager::CleanupSwapchainRenderTargets()
{
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        if (m_mainRenderTargetResources[i])
        { 
            m_mainRenderTargetResources[i]->Release();
            m_mainRenderTargetResources[i] = nullptr;
        }
    }
    m_mainRenderTargetResources.clear();
}

void UIManager::Finalize()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    ::DestroyWindow(m_hWnd);
    ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
    CleanupSwapchainRenderTargets();
    if (m_pSwapChain)
    { 
        m_pSwapChain->SetFullscreenState(false, nullptr);
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }
    if (m_hSwapChainWaitableObject != nullptr) { CloseHandle(m_hSwapChainWaitableObject); }
    if (m_pD3dSwapchainRtvDescHeap) { m_pD3dSwapchainRtvDescHeap->Release(); m_pD3dSwapchainRtvDescHeap = nullptr; }
    if (m_pD3dImGUISrvDescHeap) { m_pD3dImGUISrvDescHeap->Release(); m_pD3dImGUISrvDescHeap = nullptr; }
}