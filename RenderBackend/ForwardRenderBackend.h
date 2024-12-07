#pragma once
#include "RendererBackend.h"

class ForwardRenderer : public RendererBackend
{
public:
    ForwardRenderer();
    ~ForwardRenderer();

    virtual void RenderTick(ID3D12GraphicsCommandList* pCommandList, D3D12_CPU_DESCRIPTOR_HANDLE* pRT = nullptr) override;

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
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
};