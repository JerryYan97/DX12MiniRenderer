#pragma once
#include "RendererBackend.h"

class ForwardRenderer : public RendererBackend
{
public:
    ForwardRenderer();
    ~ForwardRenderer();

    virtual void RenderTick(ID3D12GraphicsCommandList* pCommandList) override;

protected:
    virtual void CustomInit();
    virtual void CustomDeinit();

private:
    void CreateRootSignature();
    void CreatePipelineStateObject();
    void CreateVertexBuffer();

    ID3D12RootSignature* m_pRootSignature;
};