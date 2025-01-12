#pragma once
#include "RendererBackend.h"

class ForwardRenderer : public RendererBackend
{
public:
    ForwardRenderer();
    ~ForwardRenderer();

    virtual void RenderTick(ID3D12GraphicsCommandList4* pCommandList, RenderTargetInfo rtInfo) override;

protected:
    virtual void CustomInit();
    virtual void CustomDeinit();

private:
    void CreateMeshRenderGpuResources();
    void CreateRootSignature();
    void CreatePipelineStateObject();

    void UpdatePerFrameGpuResources();

    ID3D12RootSignature* m_pRootSignature;
    ID3D12PipelineState* m_pPipelineState;
    
    ID3D12Resource*       m_pVsSceneBuffer;
    ID3D12Resource*       m_pPsSceneBuffer;
    UINT8*                m_pVsSceneBufferBegin;
    UINT8*                m_pPsSceneBufferBegin;
    ID3D12DescriptorHeap* m_pSceneCbvHeap;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT     m_scissorRect;

    // ID3D12DescriptorHeap* m_shaderVisibleCbvHeap;

    std::vector<ID3D12DescriptorHeap*> m_inflightShaderVisibleCbvHeaps;
};