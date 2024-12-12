#include "ForwardRenderBackend.h"
#include "../Utils/DX12Utils.h"
#include "../UI/UIManager.h"
#include "../Scene/Mesh.h"
#include "../Scene/Level.h"
#include <d3dcompiler.h>

ForwardRenderer::ForwardRenderer() :
    RendererBackend(RendererBackendType::Forward),
    m_pRootSignature(nullptr),
    m_pPipelineState(nullptr),
    m_vertexBuffer(nullptr),
    m_vertexBufferView(),
    m_viewport(),
    m_scissorRect()
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
{
    ID3DBlob* vertShader;
    ID3DBlob* pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

    ThrowIfFailed(D3DCompileFromFile(L"C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\RenderBackend\\ForwardRendererShaders\\shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertShader, nullptr));
    ThrowIfFailed(D3DCompileFromFile(L"C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\RenderBackend\\ForwardRendererShaders\\shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

    // Define the vertex input layout.
    /*
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    */
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    {
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
        rasterizerDesc.FrontCounterClockwise = TRUE;
        rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizerDesc.DepthClipEnable = TRUE;
        rasterizerDesc.MultisampleEnable = FALSE;
        rasterizerDesc.AntialiasedLineEnable = FALSE;
        rasterizerDesc.ForcedSampleCount = 0;
        rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    }
    
    D3D12_BLEND_DESC blendDesc = {};
    {
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
        {
            FALSE,FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
        }
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = m_pRootSignature;
    psoDesc.VS = D3D12_SHADER_BYTECODE{vertShader->GetBufferPointer(), vertShader->GetBufferSize()};
    psoDesc.PS = D3D12_SHADER_BYTECODE{pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(m_pD3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState)));

    vertShader->Release();
    pixelShader->Release();
}

void ForwardRenderer::CreateVertexBuffer()
{
    std::vector<StaticMesh*> staticMeshes;
    m_pLevel->RetriveStaticMeshes(staticMeshes);
    std::vector<float>& posData = staticMeshes[0]->m_meshPrimitives[0].m_posData;

    struct Vertex
    {
        float position[3];
        float color[4];
    };

    // Define the geometry for a triangle.
    /**/
    Vertex triangleVertices[] =
    {
        { { 0.0f, 0.25f * 1.6, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { { 0.25f, -0.25f * 1.6, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { -0.25f, -0.25f * 1.6, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
    };
    
    /*
    Vertex triangleVertices[] =
    {
        { { posData[0], posData[1], posData[2]}, {1.0f, 0.0f, 0.0f, 1.0f}},
        { { posData[3], posData[4], posData[5] }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { posData[6], posData[7], posData[8] }, { 0.0f, 0.0f, 1.0f, 1.0f } }
    };
    */
    
    const UINT vertexBufferSize = sizeof(triangleVertices);

    D3D12_HEAP_PROPERTIES heapProperties{};
    {
        heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;
    }

    D3D12_RESOURCE_DESC bufferRsrcDesc{};
    {
        bufferRsrcDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferRsrcDesc.Alignment = 0;
        bufferRsrcDesc.Width = vertexBufferSize;
        bufferRsrcDesc.Height = 1;
        bufferRsrcDesc.DepthOrArraySize = 1;
        bufferRsrcDesc.MipLevels = 1;
        bufferRsrcDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferRsrcDesc.SampleDesc.Count = 1;
        bufferRsrcDesc.SampleDesc.Quality = 0;
        bufferRsrcDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferRsrcDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    }

    ThrowIfFailed(m_pD3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferRsrcDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

    // Copy the triangle data to the vertex buffer.
    void* pVertexDataBegin;
    D3D12_RANGE readRange{ 0, 0 };        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, &pVertexDataBegin));
    memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
    m_vertexBuffer->Unmap(0, nullptr);

    // Initialize the vertex buffer view.
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vertexBufferSize;

    GpuQueueWaitIdle(m_pD3dDevice, m_pMainCommandQueue);
}

void ForwardRenderer::CustomInit()
{
    CreateRootSignature();
    CreatePipelineStateObject();
    CreateVertexBuffer();

    uint32_t winWidth, winHeight;
    m_pUIManager->GetWindowSize(winWidth, winHeight);
    m_viewport = { 0.0f, 0.0f, static_cast<float>(winWidth), static_cast<float>(winHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
    m_scissorRect = { 0, 0, static_cast<LONG>(winWidth), static_cast<LONG>(winHeight) };
}

void ForwardRenderer::RenderTick(ID3D12GraphicsCommandList* pCommandList, RenderTargetInfo rtInfo)
{
    uint32_t winWidth, winHeight;
    m_pUIManager->GetWindowSize(winWidth, winHeight);
    m_viewport = { 0.0f, 0.0f, static_cast<float>(winWidth), static_cast<float>(winHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
    m_scissorRect = { 0, 0, static_cast<LONG>(winWidth), static_cast<LONG>(winHeight) };

    pCommandList->SetPipelineState(m_pPipelineState);
    pCommandList->SetGraphicsRootSignature(m_pRootSignature);
    pCommandList->RSSetViewports(1, &m_viewport);
    pCommandList->RSSetScissorRects(1, &m_scissorRect);
    pCommandList->OMSetRenderTargets(1, &rtInfo.rtvHandle, FALSE, nullptr);
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // pCommandList->IASetIndexBuffer();
    pCommandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    pCommandList->DrawInstanced(3, 1, 0, 0);

    /*
    * It looks like D3D auto sync RT: https://github.com/microsoft/DirectX-Graphics-Samples/issues/132#issuecomment-209052225
    *
    D3D12_RESOURCE_BARRIER barrier = {};
    {
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.UAV.pResource = rtInfo.pResource;
    }
    pCommandList->ResourceBarrier(1, &barrier);
    */
}

void ForwardRenderer::CustomDeinit()
{
    m_pRootSignature->Release();
    m_pRootSignature = nullptr;

    m_pPipelineState->Release();
    m_pPipelineState = nullptr;

    m_vertexBuffer->Release();
    m_vertexBuffer = nullptr;
}