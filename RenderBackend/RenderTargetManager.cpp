#include "RenderTargetManager.h"
#include "../UI/UIManager.h"
#include "../Utils/DX12Utils.h"
#include <cstring>
#include <string>

RenderTargetManager::RenderTargetManager(ID3D12Device5* pDevice)
    : m_pD3dDevice(pDevice)
{
}

RenderTargetManager::~RenderTargetManager()
{
    ReleaseSceneRenderTargets();

    if (m_pSceneColorRtvHeap) { m_pSceneColorRtvHeap->Release(); m_pSceneColorRtvHeap = nullptr; }
    if (m_pSceneColorSrvHeap) { m_pSceneColorSrvHeap->Release(); m_pSceneColorSrvHeap = nullptr; }
    if (m_pSceneDepthDsvHeap) { m_pSceneDepthDsvHeap->Release(); m_pSceneDepthDsvHeap = nullptr; }
}

void RenderTargetManager::Initialize(uint32_t width, uint32_t height, const float clearColor[4])
{
    CreateSceneRenderTargets(width, height, clearColor);
}

void RenderTargetManager::Recreate(uint32_t width, uint32_t height, const float clearColor[4])
{
    if (m_sceneColorTargetDesc.Width == width && m_sceneColorTargetDesc.Height == height)
    {
        return;
    }

    CreateSceneRenderTargets(width, height, clearColor);
}

void RenderTargetManager::ReleaseSceneRenderTargets()
{
    for (ID3D12Resource*& pResource : m_sceneColorTargets)
    {
        if (pResource) { pResource->Release(); pResource = nullptr; }
    }
    m_sceneColorTargets.clear();

    for (ID3D12Resource*& pResource : m_sceneDepthTargets)
    {
        if (pResource) { pResource->Release(); pResource = nullptr; }
    }
    m_sceneDepthTargets.clear();

    m_sceneColorTargetDesc = {};
}

void RenderTargetManager::CreateSceneRenderTargets(uint32_t width, uint32_t height, const float clearColor[4])
{
    ReleaseSceneRenderTargets();

    if (m_pSceneColorRtvHeap) { m_pSceneColorRtvHeap->Release(); m_pSceneColorRtvHeap = nullptr; }
    if (m_pSceneColorSrvHeap) { m_pSceneColorSrvHeap->Release(); m_pSceneColorSrvHeap = nullptr; }
    if (m_pSceneDepthDsvHeap) { m_pSceneDepthDsvHeap->Release(); m_pSceneDepthDsvHeap = nullptr; }

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = UIManager::NUM_BACK_BUFFERS;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ThrowIfFailed(m_pD3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_pSceneColorRtvHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = UIManager::NUM_BACK_BUFFERS;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_pD3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_pSceneColorSrvHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = UIManager::NUM_BACK_BUFFERS;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ThrowIfFailed(m_pD3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_pSceneDepthDsvHeap)));

    m_sceneColorTargetDesc = {};
    m_sceneColorTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    m_sceneColorTargetDesc.Width = width;
    m_sceneColorTargetDesc.Height = height;
    m_sceneColorTargetDesc.DepthOrArraySize = 1;
    m_sceneColorTargetDesc.MipLevels = 1;
    m_sceneColorTargetDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    m_sceneColorTargetDesc.SampleDesc.Count = 1;
    m_sceneColorTargetDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    m_sceneColorTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_RESOURCE_DESC depthTargetDesc = m_sceneColorTargetDesc;
    depthTargetDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE colorClearValue = {};
    colorClearValue.Format = m_sceneColorTargetDesc.Format;
    memcpy(colorClearValue.Color, clearColor, sizeof(colorClearValue.Color));

    D3D12_CLEAR_VALUE depthClearValue = {};
    depthClearValue.Format = depthTargetDesc.Format;
    depthClearValue.DepthStencil.Depth = 1.0f;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_sceneColorTargetDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    const UINT rtvDescriptorSize = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const UINT srvDescriptorSize = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const UINT dsvDescriptorSize = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pSceneColorRtvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_pSceneColorSrvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pSceneDepthDsvHeap->GetCPUDescriptorHandleForHeapStart();

    m_sceneColorTargets.resize(UIManager::NUM_BACK_BUFFERS, nullptr);
    m_sceneDepthTargets.resize(UIManager::NUM_BACK_BUFFERS, nullptr);
    for (UINT frameIndex = 0; frameIndex < UIManager::NUM_BACK_BUFFERS; ++frameIndex)
    {
        ThrowIfFailed(m_pD3dDevice->CreateCommittedResource(
            &DEFAULT_HEAP,
            D3D12_HEAP_FLAG_NONE,
            &m_sceneColorTargetDesc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            &colorClearValue,
            IID_PPV_ARGS(&m_sceneColorTargets[frameIndex])));
        ThrowIfFailed(m_pD3dDevice->CreateCommittedResource(
            &DEFAULT_HEAP,
            D3D12_HEAP_FLAG_NONE,
            &depthTargetDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthClearValue,
            IID_PPV_ARGS(&m_sceneDepthTargets[frameIndex])));

        std::wstring colorName = L"Scene Color " + std::to_wstring(frameIndex);
        std::wstring depthName = L"Scene Depth " + std::to_wstring(frameIndex);
        m_sceneColorTargets[frameIndex]->SetName(colorName.c_str());
        m_sceneDepthTargets[frameIndex]->SetName(depthName.c_str());

        m_pD3dDevice->CreateRenderTargetView(m_sceneColorTargets[frameIndex], nullptr, rtvHandle);
        m_pD3dDevice->CreateShaderResourceView(m_sceneColorTargets[frameIndex], &srvDesc, srvHandle);
        m_pD3dDevice->CreateDepthStencilView(m_sceneDepthTargets[frameIndex], nullptr, dsvHandle);
        rtvHandle.ptr += rtvDescriptorSize;
        srvHandle.ptr += srvDescriptorSize;
        dsvHandle.ptr += dsvDescriptorSize;
    }
}

RenderTargetInfo RenderTargetManager::GetSceneRenderTarget(UINT frameIndex) const
{
    const UINT rtvDescriptorSize = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const UINT srvDescriptorSize = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const UINT dsvDescriptorSize = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pSceneColorRtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += rtvDescriptorSize * frameIndex;
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = m_pSceneColorSrvHeap->GetGPUDescriptorHandleForHeapStart();
    srvHandle.ptr += srvDescriptorSize * frameIndex;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pSceneDepthDsvHeap->GetCPUDescriptorHandleForHeapStart();
    dsvHandle.ptr += dsvDescriptorSize * frameIndex;

    return {
        m_sceneColorTargets[frameIndex],
        rtvHandle,
        m_sceneColorTargetDesc,
        m_sceneDepthTargets[frameIndex],
        dsvHandle,
        srvHandle};
}

void RenderTargetManager::GetSceneRenderTargetSize(uint32_t& oWidth, uint32_t& oHeight) const
{
    oWidth = static_cast<uint32_t>(m_sceneColorTargetDesc.Width);
    oHeight = m_sceneColorTargetDesc.Height;
}
