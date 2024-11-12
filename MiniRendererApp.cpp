#include "MiniRendererApp.h"
#include "UI/UIManager.h"
#include "Scene/Level.h"
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")

bool DX12MiniRenderer::show_demo_window = true;
bool DX12MiniRenderer::show_another_window = true;
bool DX12MiniRenderer::clear_color = true;
DX12MiniRenderer* DX12MiniRenderer::m_pThis = nullptr;

DX12MiniRenderer::DX12MiniRenderer()
    : m_pD3dDevice(nullptr),
      m_pUIManager(nullptr)
{
    m_pThis = this;
}

DX12MiniRenderer::~DX12MiniRenderer()
{
}

void DX12MiniRenderer::InitDevice()
{
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_pDx12Debug))))
    {
        m_pDx12Debug->EnableDebugLayer();
    }

    // Create device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_0;
    D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&m_pD3dDevice));

    if (m_pDx12Debug != nullptr)
    {
        ID3D12InfoQueue* pInfoQueue = nullptr;
        m_pD3dDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        pInfoQueue->Release();
        m_pDx12Debug->Release();
    }
}

void DX12MiniRenderer::TempRendererWaitGpuIdle()
{
    ID3D12Fence* tmpCmdQueuefence = nullptr;
    m_pThis->m_pD3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&tmpCmdQueuefence));

    m_pThis->m_pD3dCommandQueue->Signal(tmpCmdQueuefence, 1);

    // If hEvent is a null handle, then this API will not return until the specified fence value(s) have been reached.
    tmpCmdQueuefence->SetEventOnCompletion(1, nullptr);

    tmpCmdQueuefence->Release();
}

void DX12MiniRenderer::WaitGpuIdle()
{
    TempRendererWaitGpuIdle();
}

void DX12MiniRenderer::GenerateImGUIStates()
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }

    // 3. Show another simple window.
    if (show_another_window)
    {
        ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            show_another_window = false;
        ImGui::End();
    }
}

void DX12MiniRenderer::InitTempRendererInfarstructure()
{
    m_frameContexts.resize(UIManager::NUM_BACK_BUFFERS);

    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        m_pD3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_pD3dCommandQueue));
        m_pD3dCommandQueue->SetName(L"Main Command Queue");
    }

    for (UINT i = 0; i < UIManager::NUM_BACK_BUFFERS; i++)
    {
        m_pD3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_frameContexts[i].CommandAllocator));

        m_pD3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_frameContexts[i].Fence));

        m_frameContexts[i].Fence->Signal(1);

        m_frameContexts[i].FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    m_pD3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_frameContexts[0].CommandAllocator, nullptr, IID_PPV_ARGS(&m_pD3dCommandList));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    m_pD3dCommandList->Close();
}

FrameContext* DX12MiniRenderer::WaitForCurrentFrameResources()
{
    FrameContext* frameCtx = &m_frameContexts[m_pUIManager->GetCurrentBackBufferIndex()];
    if (frameCtx->Fence->GetCompletedValue() == 0) // means no fence was signaled
    {
        frameCtx->Fence->SetEventOnCompletion(1, frameCtx->FenceEvent);
        WaitForSingleObject(frameCtx->FenceEvent, INFINITE);
    }
    else
    {
        frameCtx->Fence->Signal(0);
    }
    return frameCtx;
}

void DX12MiniRenderer::CleanupTempRendererInfarstructure()
{
    if (m_pD3dCommandQueue) { m_pD3dCommandQueue->Release(); m_pD3dCommandQueue = nullptr; }
    if (m_pD3dCommandList) { m_pD3dCommandList->Release(); m_pD3dCommandList = nullptr; }

    for (auto& itr : m_frameContexts)
    {
        if (itr.CommandAllocator) { itr.CommandAllocator->Release(); itr.CommandAllocator = nullptr; }
        if (itr.Fence) { itr.Fence->Release(); itr.Fence = nullptr; }
        if (itr.FenceEvent) { CloseHandle(itr.FenceEvent); itr.FenceEvent = nullptr; }
    }
}

void DX12MiniRenderer::Init()
{
    InitDevice();
    InitTempRendererInfarstructure();
    m_pUIManager = new UIManager(m_pD3dDevice, &m_eventManager);
    m_pUIManager->Init(m_pD3dCommandQueue);
    m_pUIManager->SetCustomImGUIFunc(GenerateImGUIStates);

    // Tmp Load Test Triangle Level
    m_pLevel = new Level();
    m_sceneLoader.LoadAsLevel("C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\Assets\\SampleScene\\GLTFs\\Triangle\\TriangleTest.yaml", m_pLevel);

    m_eventManager.RegisterListener("WaitGpuIdle", DX12MiniRenderer::WaitGpuIdle);
}

void DX12MiniRenderer::Run()
{
    while (m_pUIManager->ContinueRunning())
    {
        float tempDeltaTime = 0.0f;
        m_pUIManager->Tick(tempDeltaTime);

        // Temp Renderer
        FrameContext* frameCtx = WaitForCurrentFrameResources();
        ID3D12Resource* frameCRT = m_pUIManager->GetCurrentMainRTResource();
        D3D12_CPU_DESCRIPTOR_HANDLE frameCRTDescriptor = m_pUIManager->GetCurrentMainRTDescriptor();
        ID3D12DescriptorHeap* imGUIDescriptorHeap = m_pUIManager->GetImGUISrvDescHeap();
        frameCtx->CommandAllocator->Reset();

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = frameCRT;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        m_pD3dCommandList->Reset(frameCtx->CommandAllocator, nullptr);
        m_pD3dCommandList->ResourceBarrier(1, &barrier);

        // Render Dear ImGui graphics
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        m_pD3dCommandList->ClearRenderTargetView(frameCRTDescriptor, clear_color_with_alpha, 0, nullptr);
        m_pD3dCommandList->OMSetRenderTargets(1, &frameCRTDescriptor, FALSE, nullptr); // Bind the render target.
        m_pD3dCommandList->SetDescriptorHeaps(1, &imGUIDescriptorHeap);

        m_pUIManager->RecordDrawData(m_pD3dCommandList);
        
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        m_pD3dCommandList->ResourceBarrier(1, &barrier);
        m_pD3dCommandList->Close();

        m_pD3dCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_pD3dCommandList);

        m_pD3dCommandQueue->Signal(frameCtx->Fence, 1);

        // If hEvent is a null handle, then this API will not return until the specified fence value(s) have been reached.
        frameCtx->Fence->SetEventOnCompletion(1, nullptr);

        // It looks like the Present() put works on the command queue, which means we need to use the command queue signal to wait for GPU to finish the work.
        m_pUIManager->Present();
    }

    FrameContext* frameCtx = WaitForCurrentFrameResources();
    m_pD3dCommandQueue->Signal(frameCtx->Fence, 1);
    frameCtx->Fence->SetEventOnCompletion(1, nullptr);

    WaitGpuIdle();
}

void DX12MiniRenderer::Finalize()
{
    if (m_pUIManager) { m_pUIManager->Finalize(); delete m_pUIManager; m_pUIManager = nullptr; }
    CleanupTempRendererInfarstructure();
    if (m_pD3dDevice) { m_pD3dDevice->Release(); m_pD3dDevice = nullptr; }

    IDXGIDebug1* pDebug = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
    {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        pDebug->Release();
    }

    delete m_pLevel;
}