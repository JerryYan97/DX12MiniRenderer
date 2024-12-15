#pragma once
#include "RendererBackend.h"

class ForwardRenderer : public RendererBackend
{
public:
    ForwardRenderer();
    ~ForwardRenderer();

    virtual void RenderTick(ID3D12GraphicsCommandList* pCommandList, RenderTargetInfo rtInfo) override;

protected:
    virtual void CustomInit();
    virtual void CustomDeinit();

private:
    void CreateMeshRenderGpuResources();
    void CreateRootSignature();
    void CreatePipelineStateObject();
    void CreateVertexBuffer();

    void UpdatePerFrameGpuResources();

    ID3D12RootSignature* m_pRootSignature;
    ID3D12PipelineState* m_pPipelineState;

    ID3D12Resource*          m_vertexBuffer;
    ID3D12Resource*          m_idxBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW  m_idxBufferView;
    
    ID3D12Resource*       m_pVsConstBuffer;
    UINT8*                m_pVsConstBufferBegin;
    ID3D12DescriptorHeap* m_cbvDescHeap;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT     m_scissorRect;
};