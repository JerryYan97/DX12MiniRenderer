#pragma once

#include <d3d12.h>
#include <vector>
#include "RendererBackend.h"

class PostProcessing
{
public:
    explicit PostProcessing(ID3D12Device5* pDevice);
    ~PostProcessing();

    void RenderToneMapping(
        ID3D12GraphicsCommandList4* pCommandList,
        const RenderTargetInfo& sceneTarget,
        ID3D12DescriptorHeap* pSceneColorSrvHeap,
        const RenderTargetInfo& outputTarget);

private:
    void CreateRootSignature();
    void CreatePipelineStateObject();

    ID3D12Device5* m_pD3dDevice = nullptr;
    ID3D12RootSignature* m_pRootSignature = nullptr;
    ID3D12PipelineState* m_pPipelineState = nullptr;
};
