#pragma once

#include <d3d12.h>
#include <vector>
#include "RendererBackend.h"

class PostProcessing
{
public:
    explicit PostProcessing(ID3D12Device5* pDevice);
    ~PostProcessing();

    void InitializeSceneRenderTargets(const D3D12_RESOURCE_DESC& outputDesc, const float clearColor[4]);
    RenderTargetInfo GetSceneRenderTarget(UINT frameIndex, const D3D12_RESOURCE_DESC& outputDesc);
    void RenderToneMapping(ID3D12GraphicsCommandList4* pCommandList, UINT frameIndex, const RenderTargetInfo& outputTarget);

private:
    void CreateRootSignature();
    void CreatePipelineStateObject();
    void CreateSceneRenderTargets(const D3D12_RESOURCE_DESC& outputDesc);
    void ReleaseSceneRenderTargets();

    ID3D12Device5* m_pD3dDevice = nullptr;
    ID3D12RootSignature* m_pRootSignature = nullptr;
    ID3D12PipelineState* m_pPipelineState = nullptr;
    ID3D12DescriptorHeap* m_pSceneRtvHeap = nullptr;
    ID3D12DescriptorHeap* m_pSceneSrvHeap = nullptr;
    std::vector<ID3D12Resource*> m_sceneRenderTargets;
    D3D12_RESOURCE_DESC m_sceneRenderTargetDesc = {};
    float m_clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
};
