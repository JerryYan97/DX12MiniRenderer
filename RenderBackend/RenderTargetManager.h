#pragma once

#include <d3d12.h>
#include <vector>
#include "RendererBackend.h"

class RenderTargetManager
{
public:
    explicit RenderTargetManager(ID3D12Device5* pDevice);
    ~RenderTargetManager();

    void Initialize(uint32_t width, uint32_t height, const float clearColor[4]);
    void Recreate(uint32_t width, uint32_t height, const float clearColor[4]);

    RenderTargetInfo GetSceneRenderTarget(UINT frameIndex) const;
    ID3D12DescriptorHeap* GetSceneColorSrvHeap() const { return m_pSceneColorSrvHeap; }
    void GetSceneRenderTargetSize(uint32_t& oWidth, uint32_t& oHeight) const;

private:
    void CreateSceneRenderTargets(uint32_t width, uint32_t height, const float clearColor[4]);
    void ReleaseSceneRenderTargets();

    ID3D12Device5* m_pD3dDevice = nullptr;
    ID3D12DescriptorHeap* m_pSceneColorRtvHeap = nullptr;
    ID3D12DescriptorHeap* m_pSceneColorSrvHeap = nullptr;
    ID3D12DescriptorHeap* m_pSceneDepthDsvHeap = nullptr;
    std::vector<ID3D12Resource*> m_sceneColorTargets;
    std::vector<ID3D12Resource*> m_sceneDepthTargets;
    D3D12_RESOURCE_DESC m_sceneColorTargetDesc = {};
};
