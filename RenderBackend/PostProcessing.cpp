#include "PostProcessing.h"
#include "../Utils/DX12Utils.h"
#include "../Utils/StrPathUtils.h"
#include <d3dcompiler.h>

PostProcessing::PostProcessing(ID3D12Device5* pDevice)
    : m_pD3dDevice(pDevice)
{
    CreateRootSignature();
    CreatePipelineStateObject();
}

PostProcessing::~PostProcessing()
{
    if (m_pPipelineState) { m_pPipelineState->Release(); m_pPipelineState = nullptr; }
    if (m_pRootSignature) { m_pRootSignature->Release(); m_pRootSignature = nullptr; }
}

void PostProcessing::CreateRootSignature()
{
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameter = {};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter.DescriptorTable.NumDescriptorRanges = 1;
    rootParameter.DescriptorTable.pDescriptorRanges = &srvRange;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 1;
    rootSignatureDesc.pParameters = &rootParameter;
    rootSignatureDesc.NumStaticSamplers = 1;
    D3D12_STATIC_SAMPLER_DESC sampler = StaticSampler(0, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    rootSignatureDesc.pStaticSamplers = &sampler;

    ID3DBlob* pSignature = nullptr;
    ID3DBlob* pError = nullptr;
    ThrowIfFailed(D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &pSignature,
        &pError));
    ThrowIfFailed(m_pD3dDevice->CreateRootSignature(
        0,
        pSignature->GetBufferPointer(),
        pSignature->GetBufferSize(),
        IID_PPV_ARGS(&m_pRootSignature)));

    if (pError) { pError->Release(); }
    pSignature->Release();
}

void PostProcessing::CreatePipelineStateObject()
{
    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    std::string shaderPath(GetRootPath());
    shaderPath += "/RenderBackend/CommonShaders/ToneMapping.hlsl";
    std::wstring wideShaderPath(shaderPath.begin(), shaderPath.end());

    ID3DBlob* pVertexShader = nullptr;
    ID3DBlob* pPixelShader = nullptr;
    ThrowIfFailed(D3DCompileFromFile(
        wideShaderPath.c_str(),
        nullptr,
        nullptr,
        "VSMain",
        "vs_5_0",
        compileFlags,
        0,
        &pVertexShader,
        nullptr));
    ThrowIfFailed(D3DCompileFromFile(
        wideShaderPath.c_str(),
        nullptr,
        nullptr,
        "PSMain",
        "ps_5_0",
        compileFlags,
        0,
        &pPixelShader,
        nullptr));

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;

    D3D12_BLEND_DESC blendDesc = {};
    const D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {
        FALSE, FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL};
    blendDesc.RenderTarget[0] = renderTargetBlendDesc;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    depthStencilDesc.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_pRootSignature;
    psoDesc.VS = { pVertexShader->GetBufferPointer(), pVertexShader->GetBufferSize() };
    psoDesc.PS = { pPixelShader->GetBufferPointer(), pPixelShader->GetBufferSize() };
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState = depthStencilDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(m_pD3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState)));

    pPixelShader->Release();
    pVertexShader->Release();
}

void PostProcessing::RenderToneMapping(
    ID3D12GraphicsCommandList4* pCommandList,
    const RenderTargetInfo& sceneTarget,
    ID3D12DescriptorHeap* pSceneColorSrvHeap,
    const RenderTargetInfo& outputTarget)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = sceneTarget.pResource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    pCommandList->ResourceBarrier(1, &barrier);

    D3D12_VIEWPORT viewport = {
        0.0f,
        0.0f,
        static_cast<float>(outputTarget.rtDesc.Width),
        static_cast<float>(outputTarget.rtDesc.Height),
        D3D12_MIN_DEPTH,
        D3D12_MAX_DEPTH};
    D3D12_RECT scissorRect = {
        0,
        0,
        static_cast<LONG>(outputTarget.rtDesc.Width),
        static_cast<LONG>(outputTarget.rtDesc.Height)};

    pCommandList->SetPipelineState(m_pPipelineState);
    pCommandList->SetGraphicsRootSignature(m_pRootSignature);
    pCommandList->SetDescriptorHeaps(1, &pSceneColorSrvHeap);
    pCommandList->RSSetViewports(1, &viewport);
    pCommandList->RSSetScissorRects(1, &scissorRect);
    pCommandList->OMSetRenderTargets(1, &outputTarget.rtvHandle, FALSE, nullptr);
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandList->SetGraphicsRootDescriptorTable(0, sceneTarget.srvHandle);
    pCommandList->DrawInstanced(3, 1, 0, 0);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    pCommandList->ResourceBarrier(1, &barrier);
}
