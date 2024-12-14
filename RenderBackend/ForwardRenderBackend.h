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
    void CreateRootSignature();
    void CreatePipelineStateObject();
    void CreateVertexBuffer();

    ID3D12RootSignature* m_pRootSignature;
    ID3D12PipelineState* m_pPipelineState;

    ID3D12Resource*          m_vertexBuffer;
    ID3D12Resource*          m_idxBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW  m_idxBufferView;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT     m_scissorRect;
};