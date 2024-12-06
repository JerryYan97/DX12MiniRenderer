#include "ForwardRenderBackend.h"
#include "../Utils/DX12Utils.h"

ForwardRenderer::ForwardRenderer() :
    RendererBackend(RendererBackendType::Forward),
    m_pRootSignature(nullptr)
{
}

ForwardRenderer::~ForwardRenderer()
{
}

void ForwardRenderer::CreateRootSignature()
{
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    {
        rootSignatureDesc.NumParameters = 0;
        rootSignatureDesc.pParameters = nullptr;
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    }

    ID3DBlob* pSignature;
    ID3DBlob* pError;

    ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError));
    ThrowIfFailed(m_pD3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature)));
    pSignature->Release();

    if (pError)
    {
        pError->Release();
    }
}

void ForwardRenderer::CreatePipelineStateObject()
{}

void ForwardRenderer::CreateVertexBuffer()
{}

void ForwardRenderer::CustomInit()
{
    CreateRootSignature();
}

void ForwardRenderer::RenderTick(ID3D12GraphicsCommandList* pCommandList)
{

}

void ForwardRenderer::CustomDeinit()
{
    m_pRootSignature->Release();
    m_pRootSignature = nullptr;
}