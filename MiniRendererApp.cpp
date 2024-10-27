#include "MiniRendererApp.h"
#include "UI/UIManager.h"
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")

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
    // Create device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_2;
    D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&m_pD3dDevice));

    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_pDx12Debug))))
    {
        m_pDx12Debug->EnableDebugLayer();
    }

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

void DX12MiniRenderer::WaitGpuIdle()
{

}

void DX12MiniRenderer::Init()
{
    InitDevice();
    m_pUIManager = new UIManager(m_pD3dDevice, &m_eventManager);
    m_pUIManager->Init();
    m_eventManager.RegisterListener("WaitGpuIdle", DX12MiniRenderer::WaitGpuIdle);
}

void DX12MiniRenderer::Run()
{
    while (m_pUIManager->ContinueRunning())
    {
        float tempDeltaTime = 0.0f;
        m_pUIManager->Tick(tempDeltaTime);
    }
}

void DX12MiniRenderer::Finalize()
{
    if (m_pUIManager) { m_pUIManager->Finalize(); delete m_pUIManager; m_pUIManager = nullptr; }
    if (m_pD3dDevice) { m_pD3dDevice->Release(); m_pD3dDevice = nullptr; }

    IDXGIDebug1* pDebug = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
    {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        pDebug->Release();
    }
}