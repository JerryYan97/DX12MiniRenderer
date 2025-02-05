#include "MiniRendererApp.h"
#include "UI/UIManager.h"
#include "Utils/AssetManager.h"
#include "Scene/Level.h"
#include "RenderBackend/HWRTRenderBackend.h"
#include "RenderBackend/ForwardRenderBackend.h"
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")

bool DX12MiniRenderer::show_demo_window = true;
bool DX12MiniRenderer::show_another_window = true;
bool DX12MiniRenderer::clear_color = true;
DX12MiniRenderer* DX12MiniRenderer::m_pThis = nullptr;
ID3D12Device* g_pD3dDevice = nullptr;
UIManager* g_pUIManager = nullptr;
AssetManager* g_pAssetManager = nullptr;

DX12MiniRenderer::DX12MiniRenderer()
    : m_pD3dDevice(nullptr),
      m_pUIManager(nullptr),
      m_pAssetManager(nullptr)
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
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_1;
    D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&m_pD3dDevice));

    if (m_pDx12Debug != nullptr)
    {
        ID3D12InfoQueue* pInfoQueue = nullptr;
        m_pD3dDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        // pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_INFO, true);
        // pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_MESSAGE, true);
        pInfoQueue->Release();
        m_pDx12Debug->Release();
    }

    g_pD3dDevice = m_pD3dDevice;
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

void DX12MiniRenderer::WaitGpuIdle(HEventArguments args)
{
    TempRendererWaitGpuIdle();
}

void DX12MiniRenderer::GenerateImGUIStates()
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    /*
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
    */
    // 3. Show another simple window.
    // if (show_another_window)
    {
        ImGui::Begin("Debug Menu", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        // ImGui::Text("Hello from another window!");
        // if (ImGui::Button("Close Me"))
            // show_another_window = false;
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
        // desc.NodeMask = 1;
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

    // m_pD3dDevice->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_frameContexts[0].CommandAllocator, nullptr, IID_PPV_ARGS(&m_pD3dCommandList));
    m_pD3dDevice->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_pD3dCommandList));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    // CreateCommandList1(...) creates a command list in the closed state.
    // m_pD3dCommandList->Close();
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
    g_pUIManager = m_pUIManager;

    m_pAssetManager = new AssetManager();
    g_pAssetManager = m_pAssetManager;

    // Tmp Load Test Triangle Level
    m_pLevel = new Level();
    m_sceneAssetLoader.LoadAsLevel("C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\Assets\\SampleScene\\GLTFs\\\DXRMilestoneScene\\data.yaml", m_pLevel);
    // m_sceneAssetLoader.LoadAsLevel("C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\Assets\\SampleScene\\GLTFs\\\DXRMilestoneScene\\DXRMilestone.yaml", m_pLevel);
    // m_sceneAssetLoader.LoadAsLevel("C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\Assets\\SampleScene\\GLTFs\\\CornellBoxMultiMaterials\\CornellboxMultiMaterial.yaml", m_pLevel);
    // m_sceneAssetLoader.LoadAsLevel("C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\Assets\\SampleScene\\GLTFs\\\CornellBox\\CornellBox.yaml", m_pLevel);
    // m_sceneAssetLoader.LoadAsLevel("C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\Assets\\SampleScene\\GLTFs\\\Fish\\Fish.yaml", m_pLevel);
    // m_sceneAssetLoader.LoadAsLevel("C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\Assets\\SampleScene\\GLTFs\\\Avocado\\Avocado.yaml", m_pLevel);
    // m_sceneAssetLoader.LoadAsLevel("C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\Assets\\SampleScene\\GLTFs\\\Duck\\Duck.yaml", m_pLevel);
    // m_sceneAssetLoader.LoadAsLevel("C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\Assets\\SampleScene\\GLTFs\\TexturedCube\\TexturedCube.yaml", m_pLevel);
    // m_sceneAssetLoader.LoadAsLevel("C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\Assets\\SampleScene\\GLTFs\\\PBRSpheresPtLights\\PBRSpherePtLights.yaml", m_pLevel);

    if (m_pLevel->m_rendererBackendType == RendererBackendType::PathTracing)
    {
        m_pRendererBackend = new HWRTRenderBackend();
    }
    else
    {
        m_pRendererBackend = new ForwardRenderer();
    }

    m_eventManager.RegisterListener("WaitGpuIdle", DX12MiniRenderer::WaitGpuIdle);

    uint32_t width = 0;
    uint32_t height = 0;
    m_pUIManager->GetWindowSize(width, height);

    /**/
    RendererBackendInitStruct initStruct;
    initStruct.pD3dDevice = m_pD3dDevice;
    initStruct.pMainCmdQueue = m_pD3dCommandQueue;
    initStruct.pDx12Debug = m_pDx12Debug;
    initStruct.pUIManager = m_pUIManager;
    initStruct.pEventManager = &m_eventManager;
    initStruct.pSceneAssetLoader = &m_sceneAssetLoader;
    initStruct.pLevel = m_pLevel;
    initStruct.pInitFrameContext = &m_frameContexts[0];
    initStruct.pCommandList = m_pD3dCommandList;
    m_pRendererBackend->Init(initStruct);
    
    m_eventManager.RegisterListener("ResizeSwapchain", RendererBackend::OnResizeCallback);
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

        ID3D12Resource* frameDSV = m_pUIManager->GetCurrentMainDSVResource();
        D3D12_CPU_DESCRIPTOR_HANDLE frameDSVDescriptor = m_pUIManager->GetCurrentMainDSVDescriptor();

        ID3D12DescriptorHeap* imGUIDescriptorHeap = m_pUIManager->GetImGUISrvDescHeap();
        frameCtx->CommandAllocator->Reset();

        /*
        D3D12_RESOURCE_BARRIER barriers[2] = {};
        {
            // Color Render Target
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barriers[0].Transition.pResource = frameCRT;
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

            // Depth Stencil Buffer
            barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barriers[1].Transition.pResource = m_pUIManager->GetCurrentMainDSVResource();
            barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_READ;
        }
        */
        D3D12_RESOURCE_BARRIER barrier = {};
        {
            // Color Render Target
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = frameCRT;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }

        m_pD3dCommandList->Reset(frameCtx->CommandAllocator, nullptr);
        m_pD3dCommandList->ResourceBarrier(1, &barrier);

        ImVec4 clear_color = ImVec4(m_pLevel->m_backgroundColor[0], 
                                    m_pLevel->m_backgroundColor[1],
                                    m_pLevel->m_backgroundColor[2], 1.00f);
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        m_pD3dCommandList->ClearRenderTargetView(frameCRTDescriptor, clear_color_with_alpha, 0, nullptr);
        m_pD3dCommandList->ClearDepthStencilView(frameDSVDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        // Render Scene
        RenderTargetInfo rtInfo{frameCRT, frameCRTDescriptor, m_pUIManager->GetCurrentRTResourceDesc()};
        m_pRendererBackend->RenderTick(m_pD3dCommandList, rtInfo);
        
        // Render Dear ImGui graphics
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

    HEventArguments dummyArgs;
    WaitGpuIdle(dummyArgs);
}

void DX12MiniRenderer::Finalize()
{
    delete m_pLevel;

    if (m_pUIManager) { m_pUIManager->Finalize(); delete m_pUIManager; m_pUIManager = nullptr; }
    if (m_pAssetManager) { m_pAssetManager->Deinit(); delete m_pAssetManager; m_pAssetManager = nullptr; }
    CleanupTempRendererInfarstructure();
    if (m_pD3dDevice) { m_pD3dDevice->Release(); m_pD3dDevice = nullptr; }
    if (m_pRendererBackend) { m_pRendererBackend->Deinit(); delete m_pRendererBackend; m_pRendererBackend = nullptr; }

    IDXGIDebug1* pDebug = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
    {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        pDebug->Release();
    }
}