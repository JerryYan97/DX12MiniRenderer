#include "MiniRendererApp.h"
#include "UI/UIManager.h"
#include "Utils/AssetManager.h"
#include "Scene/Level.h"
#include "Scene/Camera.h"
#include "TimePerfManager/TimePerfManager.h"
#include "RenderBackend/HWRTRenderBackend.h"
#include "RenderBackend/ForwardRenderBackend.h"
#include "Utils/StrPathUtils.h"
#include "Utils/DX12Utils.h"
#include "Utils/MathUtils.h"
#include "yaml-cpp/yaml.h"
#include <d3dcompiler.h>
#include <dxgidebug.h>
#include <filesystem>
#include <chrono>
#pragma comment(lib, "dxguid.lib")

namespace fs = std::filesystem;

bool DX12MiniRenderer::show_demo_window = true;
bool DX12MiniRenderer::show_another_window = true;
bool DX12MiniRenderer::clear_color = true;
DX12MiniRenderer* DX12MiniRenderer::m_pThis = nullptr;
ID3D12Device* g_pD3dDevice = nullptr;
UIManager* g_pUIManager = nullptr;
AssetManager* g_pAssetManager = nullptr;
TimePerfManager* g_pTimePerfManager = nullptr;
HEventManager* g_pEventManager = nullptr;

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
    int fps = 0.f;
    float cpuTime = 0.f;
    float gpuTime = 0.f;
    uint32_t displayWidth = 100;
    uint32_t displayHeight = 100;
    uint32_t renderWidth = 100;
    uint32_t renderHeight = 100;

    if (g_pUIManager)
    {
        g_pUIManager->GetSwapchainRenderTargetSize(displayWidth, displayHeight);
    }

    if (DX12MiniRenderer::m_pThis != nullptr && DX12MiniRenderer::m_pThis->m_pRendererBackend)
    {
        DX12MiniRenderer::m_pThis->m_pRendererBackend->GetMainRenderTargetSize(renderWidth, renderHeight);
    }

    if (TimePerfManager::GetInstance())
    {
        fps = TimePerfManager::GetInstance()->GetFPS();
        cpuTime = TimePerfManager::GetInstance()->GetAverageCPUFrameTime();
        gpuTime = TimePerfManager::GetInstance()->GetAverageGPUFrameTime();
    }

    {
        ImGui::Begin("Debug Menu", &show_another_window, ImGuiWindowFlags_AlwaysAutoResize);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("FPS: %d, CPU time: %.1f ms, GPU time: %.1f ms", fps, cpuTime, gpuTime);
        ImGui::Text("Display res: %d x %d, Render Res: %d x %d", displayWidth, displayHeight, renderWidth, renderHeight);
        if (ImGui::Button("Center Camera"))
        {
            HEventManager* pEventManager = HEventManager::HEventManagerInstance();
            HEventArguments args;
            float levelCenter[3] = {};
            DX12MiniRenderer::m_pThis->m_pLevel->GetLevelCenter(levelCenter);

            args[crc32("centerX")] = levelCenter[0];
            args[crc32("centerY")] = levelCenter[1];
            args[crc32("centerZ")] = levelCenter[2];

            float bbxMin[3] = {};
            float bbxMax[3] = {};
            DX12MiniRenderer::m_pThis->m_pLevel->GetBoundingBox(bbxMin, bbxMax);
            args[crc32("bbxMinX")] = bbxMin[0];
            args[crc32("bbxMinY")] = bbxMin[1];
            args[crc32("bbxMinZ")] = bbxMin[2];
            args[crc32("bbxMaxX")] = bbxMax[0];
            args[crc32("bbxMaxY")] = bbxMax[1];
            args[crc32("bbxMaxZ")] = bbxMax[2];

            HEvent centerCameraEvent(args, "CenterCamera");
            pEventManager->SendEvent(centerCameraEvent);
        }

        ImGui::SameLine();

        if (ImGui::Button("Camera Anim"))
        {
            DX12MiniRenderer::m_pThis->m_bCamAnim = !DX12MiniRenderer::m_pThis->m_bCamAnim;
        }
        
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

void DX12MiniRenderer::Init(std::string sceneYaml)
{
    InitDevice();
    InitTempRendererInfarstructure();
    m_pUIManager = new UIManager(m_pD3dDevice, &m_eventManager);
    m_pUIManager->Init(m_pD3dCommandQueue);
    m_pUIManager->SetCustomImGUIFunc(GenerateImGUIStates);
    g_pUIManager = m_pUIManager;

    m_pTimePerfManager = new TimePerfManager();
    g_pTimePerfManager = m_pTimePerfManager;
    m_pTimePerfManager->Init(m_pD3dDevice, m_pD3dCommandQueue);

    m_pAssetManager = new AssetManager();
    g_pAssetManager = m_pAssetManager;

    // Tmp Load Test Triangle Level
    m_pLevel = new Level();
    m_sceneAssetLoader.LoadAsLevel(sceneYaml, m_pLevel);

    if (m_pLevel->m_rendererBackendType == RendererBackendType::PathTracing)
    {
        m_pRendererBackend = new HWRTRenderBackend();
    }
    else
    {
        m_pRendererBackend = new ForwardRenderer();
    }

    g_pEventManager = &m_eventManager;
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

    if (m_pLevel->HasEnvMap()) { InitEnvMapPipeline(); }
}

void DX12MiniRenderer::InitEnvMapDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvCbvHeapDesc = {};
    {
        srvCbvHeapDesc.NumDescriptors = 2;
        srvCbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvCbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    }
    ThrowIfFailed(m_pD3dDevice->CreateDescriptorHeap(&srvCbvHeapDesc, IID_PPV_ARGS(&m_pEnvMapSRVCBVHeap)));

    D3D12_CPU_DESCRIPTOR_HANDLE descriptorHeapHandle = m_pEnvMapSRVCBVHeap->GetCPUDescriptorHandleForHeapStart();

    // Buffer hookup
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    {
        cbvDesc.BufferLocation = m_pEnvMapCnstBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = ALIGN_UP_256(sizeof(EnvMapCnstBuffer));
    }
    m_pD3dDevice->CreateConstantBufferView(&cbvDesc, descriptorHeapHandle);

    // Images hookup
    descriptorHeapHandle.ptr += m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_pLevel->AttachEnvMapGPUResource(m_pD3dDevice, descriptorHeapHandle);
}

void DX12MiniRenderer::InitEnvMapRootSignature()
{
    D3D12_DESCRIPTOR_RANGE psCbvRange = {};
    {
        psCbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        psCbvRange.NumDescriptors = 1;
        psCbvRange.BaseShaderRegister = 0;
        psCbvRange.RegisterSpace = 0;
        psCbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }

    D3D12_DESCRIPTOR_RANGE psSrvRange = {};
    {
        psSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        psSrvRange.NumDescriptors = 1;
        psSrvRange.BaseShaderRegister = 0;
        psSrvRange.RegisterSpace = 0;
        psSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }

    D3D12_DESCRIPTOR_RANGE psRanges[] = { psCbvRange, psSrvRange };

    D3D12_ROOT_PARAMETER rootParameters = {};
    {
        rootParameters.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters.DescriptorTable.NumDescriptorRanges = 2;
        rootParameters.DescriptorTable.pDescriptorRanges = psRanges;
        rootParameters.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }

    D3D12_STATIC_SAMPLER_DESC staticSamplers[7] = { StaticWrapSampler(0), StaticWrapSampler(1), StaticWrapSampler(2), StaticWrapSampler(3), StaticWrapSampler(4), StaticWrapSampler(5), StaticWrapSampler(6) };

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    {
        rootSignatureDesc.NumParameters = 1;
        rootSignatureDesc.pParameters = &rootParameters;
        rootSignatureDesc.NumStaticSamplers = 7;
        rootSignatureDesc.pStaticSamplers = staticSamplers;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    }

    ID3DBlob* pSignature;
    ID3DBlob* pError;

    ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError));
    ThrowIfFailed(m_pD3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pEnvMapRootSignature)));
    m_pEnvMapRootSignature->SetName(L"EnvMapRootSignature");
    pSignature->Release();

    if (pError)
    {
        pError->Release();
    }
}

void DX12MiniRenderer::InitEnvMapPSO()
{
    ID3DBlob* vertShader;
    ID3DBlob* pixelShader;

#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    // PSO
    std::string EnvMapShaderPathName(GetRootPath());
    EnvMapShaderPathName += "/RenderBackend/CommonShaders/EnvMapBackground.hlsl";
    std::wstring wideString(EnvMapShaderPathName.begin(), EnvMapShaderPathName.end());

    ID3DBlob* errorBlob = nullptr;
    ThrowIfFailed(D3DCreateBlob(250, &errorBlob));

    // ThrowIfFailed(D3DCompileFromFile(wideString.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertShader, &errorBlob));
    D3DCompileFromFile(wideString.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertShader, &errorBlob);
    // const char* errorMsg = (const char*)errorBlob->GetBufferPointer();
    ThrowIfFailed(D3DCompileFromFile(wideString.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errorBlob));

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    {
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
        rasterizerDesc.FrontCounterClockwise = FALSE;
        rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizerDesc.DepthClipEnable = TRUE;
        rasterizerDesc.MultisampleEnable = FALSE;
        rasterizerDesc.AntialiasedLineEnable = FALSE;
        rasterizerDesc.ForcedSampleCount = 0;
        rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    }
    
    D3D12_BLEND_DESC blendDesc = {};
    {
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
        {
            FALSE,FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
        }
    }

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    {
        depthStencilDesc.DepthEnable = FALSE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_NONE;
        depthStencilDesc.StencilEnable = FALSE;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { nullptr, 0 };
    psoDesc.pRootSignature = m_pEnvMapRootSignature;
    psoDesc.VS = D3D12_SHADER_BYTECODE{vertShader->GetBufferPointer(), vertShader->GetBufferSize()};
    psoDesc.PS = D3D12_SHADER_BYTECODE{pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState = depthStencilDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(m_pD3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pEnvMapPipelineState)));

    vertShader->Release();
    pixelShader->Release();
}

void DX12MiniRenderer::InitEnvMapCnstBuffer()
{
    m_pEnvMapCnstBuffer = CreateGPUBuffer(m_pD3dDevice, sizeof(EnvMapCnstBuffer));
}

void DX12MiniRenderer::InitEnvMapPipeline()
{
    InitEnvMapRootSignature();
    InitEnvMapPSO();
    InitEnvMapCnstBuffer();
    InitEnvMapDescriptorHeaps();
}

void DX12MiniRenderer::FinalizeEnvMapPipeline()
{
    if(m_pEnvMapRootSignature) {
        m_pEnvMapRootSignature->Release();
        m_pEnvMapRootSignature = nullptr;
    }
    if(m_pEnvMapSRVCBVHeap) {
        m_pEnvMapSRVCBVHeap->Release();
        m_pEnvMapSRVCBVHeap = nullptr;
    }
    if(m_pEnvMapPipelineState) {
        m_pEnvMapPipelineState->Release();
        m_pEnvMapPipelineState = nullptr;
    }
}

void DX12MiniRenderer::Run()
{
    auto timeStamp = std::chrono::high_resolution_clock::now();

    while (m_pUIManager->ContinueRunning())
    {
        auto nowTimeStamp = std::chrono::high_resolution_clock::now();
        auto elapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(nowTimeStamp - timeStamp);
        timeStamp = nowTimeStamp;

        float deltaSec = float(elapsedSec.count()) / 1000.0f;
        m_pTimePerfManager->AddCPUTime(deltaSec);
        m_pUIManager->Tick(deltaSec);

        // Camera Animation
        if (m_bCamAnim)
        {
            HEventManager* pEventManager = HEventManager::HEventManagerInstance();
            HEventArguments args;
            args[crc32("delta")] = 0.1f;

            HEvent rotateCameraEvent(args, "RotateCamera");
            pEventManager->SendEvent(rotateCameraEvent);
        }
        // Camera Update
        Camera* pCamera = nullptr;
        m_pLevel->RetriveActiveCamera(&pCamera);
        pCamera->CameraUpdate();

        FrameContext* frameCtx = WaitForCurrentFrameResources();
        ID3D12Resource* frameCRT = m_pUIManager->GetCurrentMainRTResource();
        D3D12_CPU_DESCRIPTOR_HANDLE frameCRTDescriptor = m_pUIManager->GetCurrentMainRTDescriptor();

        ID3D12Resource* frameDSV = m_pUIManager->GetCurrentMainDSVResource();
        D3D12_CPU_DESCRIPTOR_HANDLE frameDSVDescriptor = m_pUIManager->GetCurrentMainDSVDescriptor();

        ID3D12DescriptorHeap* imGUIDescriptorHeap = m_pUIManager->GetImGUISrvDescHeap();
        frameCtx->CommandAllocator->Reset();

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

        m_pTimePerfManager->GPUTimeStampStart(m_pD3dCommandList);

        ImVec4 clear_color = ImVec4(m_pLevel->m_backgroundColor[0],
                                    m_pLevel->m_backgroundColor[1],
                                    m_pLevel->m_backgroundColor[2], 1.00f);
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        m_pD3dCommandList->ClearRenderTargetView(frameCRTDescriptor, clear_color_with_alpha, 0, nullptr);
        m_pD3dCommandList->ClearDepthStencilView(frameDSVDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        if (m_pLevel->HasEnvMap()) { RenderEnvMap(m_pD3dCommandList, frameCRTDescriptor); }

        // Render Scene
        RenderTargetInfo rtInfo{frameCRT, frameCRTDescriptor, m_pUIManager->GetCurrentRTResourceDesc()};
        m_pRendererBackend->RenderTick(m_pD3dCommandList, rtInfo);

        // Render Dear ImGui graphics
        m_pD3dCommandList->OMSetRenderTargets(1, &frameCRTDescriptor, FALSE, nullptr); // Bind the render target.
        m_pD3dCommandList->SetDescriptorHeaps(1, &imGUIDescriptorHeap);

        m_pUIManager->RecordDrawData(m_pD3dCommandList);
        
        m_pTimePerfManager->GPUTimeStampEnd(m_pD3dCommandList);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        m_pD3dCommandList->ResourceBarrier(1, &barrier);
        m_pD3dCommandList->Close();

        m_pD3dCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_pD3dCommandList);

        m_pD3dCommandQueue->Signal(frameCtx->Fence, 1);

        // If hEvent is a null handle, then this API will not return until the specified fence value(s) have been reached.
        frameCtx->Fence->SetEventOnCompletion(1, nullptr);

        m_pTimePerfManager->ReadBackGPUTimeStampResults();

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

    FinalizeEnvMapPipeline();

    if (m_pUIManager) { m_pUIManager->Finalize(); delete m_pUIManager; m_pUIManager = nullptr; }
    if (m_pAssetManager) { m_pAssetManager->Deinit(); delete m_pAssetManager; m_pAssetManager = nullptr; }
    if (m_pTimePerfManager) { m_pTimePerfManager->Finalize(); delete m_pTimePerfManager; m_pTimePerfManager = nullptr; }
    CleanupTempRendererInfarstructure();

    if (m_pRendererBackend) { m_pRendererBackend->Deinit(); delete m_pRendererBackend; m_pRendererBackend = nullptr; }

#if defined(REPORT_LIVE_DEVICE_OBJ)
    ID3D12DebugDevice* pDebugDevice;
    if (SUCCEEDED(m_pD3dDevice->QueryInterface(IID_PPV_ARGS(&pDebugDevice))))
    {
        pDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
    }
    pDebugDevice->Release(); // Release after the report
#endif

    if (m_pD3dDevice) { m_pD3dDevice->Release(); m_pD3dDevice = nullptr; }

    IDXGIDebug1* pDebug = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
    {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        pDebug->Release();
    }
}

void DX12MiniRenderer::RenderEnvMap(ID3D12GraphicsCommandList4* pCmdList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
{
    if (m_pLevel->HasEnvMap())
    {
        // Populating the rendering commands
        uint32_t winWidth, winHeight;
        m_pUIManager->GetWindowSize(winWidth, winHeight);
        D3D12_VIEWPORT viewport    = { 0.0f, 0.0f, static_cast<float>(winWidth), static_cast<float>(winHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
        D3D12_RECT     scissorRect = { 0, 0, static_cast<LONG>(winWidth), static_cast<LONG>(winHeight) };

        EnvMapCnstBuffer envMapCB{};
        Camera* pCamera = nullptr;
        m_pLevel->RetriveActiveCamera(&pCamera);

        float right[3] = {};
        CrossProductVec3(pCamera->m_view, pCamera->m_up, right);
        NormalizeVec(right, 3);

        memcpy(envMapCB.view, pCamera->m_view, 3 * sizeof(float));
        memcpy(envMapCB.right, right, 3 * sizeof(float));
        memcpy(envMapCB.camUpNear, pCamera->m_up, 3 * sizeof(float));
        envMapCB.camUpNear[3] = pCamera->m_near;
        float nearHeight = 2.f * tanf(pCamera->m_fov / 2.f) * pCamera->m_near;
        float nearWidth  = pCamera->m_aspect * nearHeight;
        envMapCB.camNearWidthHeight[0] = nearWidth;
        envMapCB.camNearWidthHeight[1] = nearHeight;
        envMapCB.vpWidthHeight[0] = viewport.Width;
        envMapCB.vpWidthHeight[1] = viewport.Height;
        SendDataToGPUBuffer(m_pD3dDevice, m_pEnvMapCnstBuffer, &envMapCB, sizeof(EnvMapCnstBuffer));

        pCmdList->SetDescriptorHeaps(1, &m_pEnvMapSRVCBVHeap);
        pCmdList->SetPipelineState(m_pEnvMapPipelineState);
        pCmdList->SetGraphicsRootSignature(m_pEnvMapRootSignature);
        pCmdList->RSSetViewports(1, &viewport);
        pCmdList->RSSetScissorRects(1, &scissorRect);
        pCmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->SetGraphicsRootDescriptorTable(0, m_pEnvMapSRVCBVHeap->GetGPUDescriptorHandleForHeapStart());
        pCmdList->DrawInstanced(6, 1, 0, 0);
    }
}

void InputInfoManager::GatherInfo()
{
    std::string scenePath = GetRootPath();
    scenePath += "/Assets/SampleScene/GLTFs";

    try {
        std::unordered_map<int, SceneInfo> sceneInfoMap; // CLI Scene Index to SceneInfo mapping
        int maxSceneIndex = -1;

        // Iterate over the entries in the directory
        for (const auto& entry : fs::directory_iterator(scenePath)) {
            SceneInfo sceneInfo;
            sceneInfo.presentStr = entry.path().filename().string();
            sceneInfo.sceneYmlFilePath = scenePath + "/" + entry.path().filename().string() + "/" + entry.path().filename().string() + ".yaml";

            if (fs::exists(sceneInfo.sceneYmlFilePath))
            {
                YAML::Node config = YAML::LoadFile(sceneInfo.sceneYmlFilePath.c_str());
                if (config["SceneId"].IsDefined())
                {
                    int cliSceneIndex = config["SceneId"].as<int>();
                    if (sceneInfoMap.find(cliSceneIndex) != sceneInfoMap.end())
                    {
                        std::cerr << "Warning: Duplicate SceneId " << cliSceneIndex << " found in " << sceneInfo.sceneYmlFilePath << ". Skipping this scene." << std::endl;
                    }
                    else
                    {
                        sceneInfoMap[cliSceneIndex] = sceneInfo; // Store in map for sorting later
                    }
                    if (cliSceneIndex > maxSceneIndex)
                    {
                        maxSceneIndex = cliSceneIndex;
                    }
                }
                else
                {
                    std::cerr << "Warning: SceneId not defined in " << sceneInfo.sceneYmlFilePath << ". Skipping this scene." << std::endl;
                }
            }
        }

        // Sort the scenes based on CLI Scene Index and populate the list
        m_sceneInfoList.resize(maxSceneIndex + 1);
        for (const auto& pair : sceneInfoMap)
        {
            m_sceneInfoList[pair.first] = pair.second;
        }

    } catch (const fs::filesystem_error& e) {
        // Handle potential errors, e.g., if the directory doesn't exist
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

std::string InputInfoManager::GetCurrentScenesBackendInfoStr()
{
    std::string res;
    for (int i = 0; i < m_sceneInfoList.size(); i++)
    {
        res += ("(" + std::to_string(i));
        res += "):";
        res += m_sceneInfoList[i].presentStr;
        res += "\n";
    }
    return res;
}

std::string InputInfoManager::GetBackendSceneFilePath(int idx)
{
    if ((idx > 0) && (idx < m_sceneInfoList.size()))
    {
        return m_sceneInfoList[idx].sceneYmlFilePath;
    }
    else
    {
        if (m_sceneInfoList.size() > 0)
        {
            return m_sceneInfoList[0].sceneYmlFilePath;
        }
        else
        {
            return "";
        }
    }
}