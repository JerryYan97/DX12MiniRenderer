#include "ForwardRenderBackend.h"
#include "../Utils/DX12Utils.h"
#include "../UI/UIManager.h"
#include "../Scene/Mesh.h"
#include "../Scene/Camera.h"
#include "../Scene/Level.h"
#include "../Scene/Lights.h"
#include <d3dcompiler.h>

ForwardRenderer::ForwardRenderer() :
    RendererBackend(RendererBackendType::Forward),
    m_pRootSignature(nullptr),
    m_pPipelineState(nullptr),
    m_viewport(),
    m_scissorRect(),
    m_pVsSceneBuffer(nullptr),
    m_pPsSceneBuffer(nullptr),
    m_pSceneCbvHeap(nullptr),
    m_pVsSceneBufferBegin(nullptr),
    m_pPsSceneBufferBegin(nullptr)
    // m_shaderVisibleCbvHeap(nullptr)
{
}

ForwardRenderer::~ForwardRenderer()
{
}

void ForwardRenderer::CreateRootSignature()
{
    D3D12_DESCRIPTOR_RANGE vsCbvRange = {};
    {
        vsCbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        vsCbvRange.NumDescriptors = 2;
        vsCbvRange.BaseShaderRegister = 0;
        vsCbvRange.RegisterSpace = 0;
        vsCbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }

    D3D12_DESCRIPTOR_RANGE psCbvRange = {};
    {
        psCbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        psCbvRange.NumDescriptors = 2;
        psCbvRange.BaseShaderRegister = 2;
        psCbvRange.RegisterSpace = 0;
        psCbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }

    D3D12_ROOT_PARAMETER rootParameters[2] = {};
    {
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
        rootParameters[0].DescriptorTable.pDescriptorRanges = &vsCbvRange;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParameters[1].DescriptorTable.pDescriptorRanges = &psCbvRange;
        rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    {
        rootSignatureDesc.NumParameters = 2;
        rootSignatureDesc.pParameters = rootParameters;
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

    // ThrowIfFailed(D3DCompileFromFile(L"C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\RenderBackend\\ForwardRendererShaders\\shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertShader, nullptr));
    // ThrowIfFailed(D3DCompileFromFile(L"C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\RenderBackend\\ForwardRendererShaders\\shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
    ThrowIfFailed(D3DCompileFromFile(L"C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\RenderBackend\\ForwardRendererShaders\\PBRShaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertShader, nullptr));
    ThrowIfFailed(D3DCompileFromFile(L"C:\\JiaruiYan\\Projects\\DX12MiniRenderer\\RenderBackend\\ForwardRendererShaders\\PBRShaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

    // Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    /*
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    */

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    {
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
        rasterizerDesc.FrontCounterClockwise = FALSE;
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

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    {
        depthStencilDesc.DepthEnable = TRUE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        depthStencilDesc.StencilEnable = FALSE;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = m_pRootSignature;
    psoDesc.VS = D3D12_SHADER_BYTECODE{vertShader->GetBufferPointer(), vertShader->GetBufferSize()};
    psoDesc.PS = D3D12_SHADER_BYTECODE{pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState = depthStencilDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(m_pD3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState)));

    vertShader->Release();
    pixelShader->Release();
}

void ForwardRenderer::CreateMeshRenderGpuResources()
{
    // Padding to 256 bytes for constant buffer
    float vsConstantBuffer[64] = {};

    D3D12_HEAP_PROPERTIES heapProperties{};
    {
        heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;
    }

    D3D12_RESOURCE_DESC bufferRsrcDesc{};
    {
        bufferRsrcDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferRsrcDesc.Alignment = 0;
        bufferRsrcDesc.Width = sizeof(vsConstantBuffer);
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
            IID_PPV_ARGS(&m_pVsSceneBuffer)));

    ThrowIfFailed(m_pD3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferRsrcDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_pPsSceneBuffer)));

    // Describe and create a constant buffer view (CBV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 2;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(m_pD3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_pSceneCbvHeap)));

    // Describe and create a constant buffer view.
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_pVsSceneBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = sizeof(vsConstantBuffer);
    m_pD3dDevice->CreateConstantBufferView(&cbvDesc, m_pSceneCbvHeap->GetCPUDescriptorHandleForHeapStart());

    const uint32_t cbvDescHandleOffset = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    cbvDesc.BufferLocation = m_pPsSceneBuffer->GetGPUVirtualAddress();
    D3D12_CPU_DESCRIPTOR_HANDLE psCbvHandle = m_pSceneCbvHeap->GetCPUDescriptorHandleForHeapStart();
    psCbvHandle.ptr += cbvDescHandleOffset;
    m_pD3dDevice->CreateConstantBufferView(&cbvDesc, psCbvHandle);
}

void ForwardRenderer::UpdatePerFrameGpuResources()
{
    float vsConstantBuffer[64] = {};
    Camera* pCamera = nullptr;
    m_pLevel->RetriveActiveCamera(&pCamera);
    pCamera->CameraUpdate();

    memcpy(vsConstantBuffer, pCamera->m_vpMat, sizeof(float) * 16);

    // Map and initialize the constant buffer. We don't unmap this until the
    // app closes. Keeping things mapped for the lifetime of the resource is okay.
    D3D12_RANGE readRange{ 0, 0 };        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_pVsSceneBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pVsSceneBufferBegin)));
    memcpy(m_pVsSceneBufferBegin, vsConstantBuffer, sizeof(vsConstantBuffer));
    m_pVsSceneBuffer->Unmap(0, nullptr);


    // Collect scene environment data to PS scene constant buffer
    float psConstantBuffer[64] = {};

    std::vector<Light*> sceneLights;
    m_pLevel->RetriveLights(sceneLights);
    assert(sceneLights.size() == 4, "Now we assume 4 point lights.");
    for (uint32_t i = 0; i < sceneLights.size(); i++)
    {
        assert(sceneLights[i]->GetObjectTypeHash() == crc32("PointLight"));
        PointLight* pPtLight = dynamic_cast<PointLight*>(sceneLights[i]);
        memcpy(psConstantBuffer + i * 4, pPtLight->position, sizeof(float) * 3);
    }

    memcpy(psConstantBuffer + 16, pCamera->m_pos, sizeof(float) * 3);
    // Current No Ambient Light.

    ThrowIfFailed(m_pPsSceneBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pPsSceneBufferBegin)));
    memcpy(m_pPsSceneBufferBegin, psConstantBuffer, sizeof(psConstantBuffer));
    m_pPsSceneBuffer->Unmap(0, nullptr);
}

void ForwardRenderer::CustomInit()
{
    CreateRootSignature();
    CreatePipelineStateObject();
    CreateMeshRenderGpuResources();

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

    D3D12_CPU_DESCRIPTOR_HANDLE frameDSVDescriptor = m_pUIManager->GetCurrentMainDSVDescriptor();

    // Padding to 256 bytes for constant buffer
    float vsConstantBuffer[64] = {};

    UpdatePerFrameGpuResources();

    std::vector<StaticMesh*> staticMeshes;
    m_pLevel->RetriveStaticMeshes(staticMeshes);

    // Pre-Render
    // Free previous frame resources
    for (const auto& itr : m_inflightShaderVisibleCbvHeaps)
    {
        itr->Release();
    }
    m_inflightShaderVisibleCbvHeaps.clear();

    // Render Logic
    for (uint32_t mshIdx = 0; mshIdx < staticMeshes.size(); mshIdx++)
    {
        for (uint32_t primIdx = 0; primIdx < staticMeshes[mshIdx]->m_meshPrimitives.size(); primIdx++)
        {
            // Create in-flight shader visible CBV heap and properly copy the CBV descriptor to it.
            // Shader visible heap.
            D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
            cbvHeapDesc.NumDescriptors = 4;
            cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

            ID3D12DescriptorHeap* pInflightShaderVisibleCbvHeap = nullptr;
            ThrowIfFailed(m_pD3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&pInflightShaderVisibleCbvHeap)));
            m_inflightShaderVisibleCbvHeaps.push_back(pInflightShaderVisibleCbvHeap);

            D3D12_CPU_DESCRIPTOR_HANDLE shaderCbvDescHeapCpuHandle = pInflightShaderVisibleCbvHeap->GetCPUDescriptorHandleForHeapStart();
            const uint32_t cbvDescHandleOffset = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            D3D12_CPU_DESCRIPTOR_HANDLE vsObjCbvHandle = shaderCbvDescHeapCpuHandle;
            D3D12_CPU_DESCRIPTOR_HANDLE meshModelMatCbvHandle = staticMeshes[mshIdx]->m_staticMeshCbvDescHeap->GetCPUDescriptorHandleForHeapStart();
            m_pD3dDevice->CopyDescriptorsSimple(1, vsObjCbvHandle, meshModelMatCbvHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            D3D12_CPU_DESCRIPTOR_HANDLE vsSceneCbvHandle = shaderCbvDescHeapCpuHandle;
            vsSceneCbvHandle.ptr += cbvDescHandleOffset;
            D3D12_CPU_DESCRIPTOR_HANDLE vsSceneVpMatCbvHandle = m_pSceneCbvHeap->GetCPUDescriptorHandleForHeapStart();
            m_pD3dDevice->CopyDescriptorsSimple(1, vsSceneCbvHandle, vsSceneVpMatCbvHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            
            D3D12_CPU_DESCRIPTOR_HANDLE psObjCnstMaterialCbvHandle = shaderCbvDescHeapCpuHandle;
            psObjCnstMaterialCbvHandle.ptr += cbvDescHandleOffset * 2;
            D3D12_CPU_DESCRIPTOR_HANDLE psCnstMaterialCbvHandle = staticMeshes[mshIdx]->m_staticMeshCnstMaterialCbvDescHeap->GetCPUDescriptorHandleForHeapStart();
            m_pD3dDevice->CopyDescriptorsSimple(1, psObjCnstMaterialCbvHandle, psCnstMaterialCbvHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            D3D12_CPU_DESCRIPTOR_HANDLE psSceneCbvHandle = shaderCbvDescHeapCpuHandle;
            psSceneCbvHandle.ptr += cbvDescHandleOffset * 3;
            D3D12_CPU_DESCRIPTOR_HANDLE psSceneSrcCbvHandle = m_pSceneCbvHeap->GetCPUDescriptorHandleForHeapStart();
            psSceneSrcCbvHandle.ptr += cbvDescHandleOffset;
            m_pD3dDevice->CopyDescriptorsSimple(1, psSceneCbvHandle, psSceneSrcCbvHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            const uint32_t idxCnt = staticMeshes[mshIdx]->m_meshPrimitives[primIdx].GetIdxCount();
            const uint32_t cbvDescHeapHandleOffset = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            D3D12_GPU_DESCRIPTOR_HANDLE psCbvDescHeapGpuHandle = pInflightShaderVisibleCbvHeap->GetGPUDescriptorHandleForHeapStart();
            psCbvDescHeapGpuHandle.ptr += cbvDescHeapHandleOffset * 2;

            // ID3D12DescriptorHeap* ppHeaps[] = { m_cbvDescHeap };
            ID3D12DescriptorHeap* ppHeaps[] = { pInflightShaderVisibleCbvHeap };
            pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

            pCommandList->SetPipelineState(m_pPipelineState);
            pCommandList->SetGraphicsRootSignature(m_pRootSignature);
            pCommandList->RSSetViewports(1, &m_viewport);
            pCommandList->RSSetScissorRects(1, &m_scissorRect);
            pCommandList->OMSetRenderTargets(1, &rtInfo.rtvHandle, FALSE, &frameDSVDescriptor);
            pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            pCommandList->IASetIndexBuffer(&staticMeshes[mshIdx]->m_meshPrimitives[primIdx].m_idxBufferView);
            pCommandList->IASetVertexBuffers(0, 1, &staticMeshes[mshIdx]->m_meshPrimitives[primIdx].m_vertexBufferView);
            // pCommandList->SetGraphicsRootDescriptorTable(0, m_cbvDescHeap->GetGPUDescriptorHandleForHeapStart());
            pCommandList->SetGraphicsRootDescriptorTable(0, pInflightShaderVisibleCbvHeap->GetGPUDescriptorHandleForHeapStart());
            pCommandList->SetGraphicsRootDescriptorTable(1, psCbvDescHeapGpuHandle);
            pCommandList->DrawIndexedInstanced(idxCnt, 1, 0, 0, 0);
        }
    }

    // Post-Render


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

    if (m_pVsSceneBuffer)
    {
        m_pVsSceneBuffer->Release();
        m_pVsSceneBuffer = nullptr;
    }

    if (m_pPsSceneBuffer)
    {
        m_pPsSceneBuffer->Release();
        m_pPsSceneBuffer = nullptr;
    }

    if (m_pSceneCbvHeap)
    {
        m_pSceneCbvHeap->Release();
        m_pSceneCbvHeap = nullptr;
    }
    
    /*
    if (m_shaderVisibleCbvHeap)
    {
        m_shaderVisibleCbvHeap->Release();
        m_shaderVisibleCbvHeap = nullptr;
    }
    */

    for (const auto& itr : m_inflightShaderVisibleCbvHeaps)
    {
        itr->Release();
    }
}