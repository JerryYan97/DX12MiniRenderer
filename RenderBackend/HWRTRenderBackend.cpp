#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "HWRTRenderBackend.h"
#include "../Scene/SceneAssetLoader.h"
#include "../Utils/AssetManager.h"
#include "../Scene/Level.h"
#include "../Scene/Mesh.h"
#include "../Scene/Camera.h"
#include "../Utils/DX12Utils.h"
#include "../Utils/MathUtils.h"
#include "RTShaders/CustomRTShader.fxh"
#include "../UI/UIManager.h"
#include "../MiniRendererApp.h"
#include <algorithm>     // For std::size, typed std::max, etc.
#include <DirectXMath.h> // For XMMATRIX
#include <d3d12.h>
#include <iostream>
#include <cstdlib> // for rand() and srand()
#include <ctime>   // for time()
#include <chrono>

extern AssetManager* g_pAssetManager;

constexpr DXGI_SAMPLE_DESC NO_AA = {.Count = 1, .Quality = 0};
constexpr D3D12_HEAP_PROPERTIES UPLOAD_HEAP = {.Type = D3D12_HEAP_TYPE_UPLOAD};
constexpr D3D12_HEAP_PROPERTIES DEFAULT_HEAP = {.Type = D3D12_HEAP_TYPE_DEFAULT};
constexpr D3D12_RESOURCE_DESC BASIC_BUFFER_DESC = {
    .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Width = 0, // Will be changed in copies
    .Height = 1,
    .DepthOrArraySize = 1,
    .MipLevels = 1,
    .SampleDesc = NO_AA,
    .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR};

void HWRTRenderBackend::Flush()
{
    static UINT64 value = 1;
    m_pMainCommandQueue->Signal(m_fence, value);
    m_fence->SetEventOnCompletion(value++, nullptr);
}

ID3D12Resource* HWRTRenderBackend::MakeAccelerationStructure(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs, UINT64* updateScratchSize)
{
    auto makeBuffer = [=](UINT64 size, auto initialState) {
        auto desc = BASIC_BUFFER_DESC;
        desc.Width = size;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        ID3D12Resource* buffer;
        m_pD3dDevice->CreateCommittedResource(&DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE,
                                              &desc, initialState, nullptr,
                                              IID_PPV_ARGS(&buffer));
        return buffer;
    };

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo;
    m_pD3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs,
                                                           &prebuildInfo);
    if (updateScratchSize)
        *updateScratchSize = prebuildInfo.UpdateScratchDataSizeInBytes;

    auto* scratch = makeBuffer(prebuildInfo.ScratchDataSizeInBytes,
                               D3D12_RESOURCE_STATE_COMMON);
    auto* as = makeBuffer(prebuildInfo.ResultDataMaxSizeInBytes,
                          D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {
        .DestAccelerationStructureData = as->GetGPUVirtualAddress(),
        .Inputs = inputs,
        .ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress()};

    m_pInitFrameContext->CommandAllocator->Reset();
    m_pCommandList->Reset(m_pInitFrameContext->CommandAllocator, nullptr);
    m_pCommandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    m_pCommandList->Close();
    m_pMainCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList**>(&m_pCommandList));

    Flush();
    scratch->Release();
    return as;
}

void HWRTRenderBackend::InitScene()
{
    std::vector<float> sceneVertData;
    std::vector<uint16_t> sceneIdxData;
    std::vector<PrimitiveAsset*> scenePrimAssets = g_pAssetManager->GenSceneVertIdxBuffer(sceneVertData, sceneIdxData);

    auto GetPrimStartVertIdxStart = [&](PrimitiveAsset* tarPrim, uint32_t& vertStartFloat, uint32_t& idxStartInt) {
        uint32_t vertFloatCnt = 0;
        uint32_t idxIntCnt = 0;
        for (auto* primAsset : scenePrimAssets)
        {
            if (primAsset == tarPrim)
            {
                vertStartFloat = vertFloatCnt;
                idxStartInt = idxIntCnt;
                break;
            }
            vertFloatCnt += primAsset->m_vertData.size();
            idxIntCnt += primAsset->m_idxCnt;
        }
    };

    std::vector<StaticMesh*> staticMeshes;
    m_pLevel->RetriveStaticMeshes(staticMeshes);

    // Count the number of all primitives instances -- Different prims use referred by different static meshes.
    m_numInstances = 0;
    for (auto* staticMesh : staticMeshes)
    {
        m_numInstances += staticMesh->m_primitiveAssets.size();
    }

    auto instancesDesc = BASIC_BUFFER_DESC;
    instancesDesc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * m_numInstances;
    m_pD3dDevice->CreateCommittedResource(&UPLOAD_HEAP, D3D12_HEAP_FLAG_NONE,
                                    // &instancesDesc, D3D12_RESOURCE_STATE_COMMON,
                                    &instancesDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                    nullptr, IID_PPV_ARGS(&m_instances));

    D3D12_RAYTRACING_INSTANCE_DESC* pAllInstData = nullptr;
    m_instances->Map(0, nullptr, reinterpret_cast<void**>(&pAllInstData));

    struct InstanceInfo
    {
        uint32_t instUintInfo0[4];
        float instFloatInfo0[4];
        float instAlbedo[4];
        float instMetallicRoughness[4];
    };
    std::vector<InstanceInfo> instInfoVecData;

    UINT instIdx = 0;
    for (int sMeshIdx = 0; sMeshIdx < staticMeshes.size(); sMeshIdx++)
    {
        auto* pStaticMesh = staticMeshes[sMeshIdx];
        std::vector<float> staticMeshCnstAlbedo = pStaticMesh->GetCnstAlbedo();
        std::vector<float> staticMeshCnstMetallicRoughness = pStaticMesh->GetCnstMetallicRoughness();
        uint32_t meshMaterialMask = pStaticMesh->GetStaticMeshMaterialMask();

        for (int primIdx = 0; primIdx < staticMeshes[sMeshIdx]->m_primitiveAssets.size(); primIdx++, instIdx++)
        {
            auto* pPrimAsset = staticMeshes[sMeshIdx]->m_primitiveAssets[primIdx];
            auto* pInstDesc = &pAllInstData[instIdx];
            *pInstDesc = {
                .InstanceID = instIdx,
                .InstanceMask = 1,
                .AccelerationStructure = pPrimAsset->m_blas->GetGPUVirtualAddress(),
            };

            // Update transform
            memcpy(pInstDesc->Transform, staticMeshes[sMeshIdx]->m_modelMat, sizeof(float) * 12);

            // A prim asset is a blas, but there can be multiple instances refer to one blas and use different materials...
            uint32_t vertStartFloat, idxStartInt;
            GetPrimStartVertIdxStart(pPrimAsset, vertStartFloat, idxStartInt);

            float emissiveRadiance[3] = {0.f, 0.f, 0.f};
            if (pStaticMesh->IsCnstEmissiveMaterial())
            {
                std::vector<float> emissiveRadianceVec = pStaticMesh->GetCnstEmissive();
                memcpy(emissiveRadiance, emissiveRadianceVec.data(), sizeof(float) * 3);
            }

            InstanceInfo instInfo{
                .instUintInfo0 = {pPrimAsset->m_materialMask | meshMaterialMask, vertStartFloat, idxStartInt, 0},
                .instFloatInfo0 ={emissiveRadiance[0], emissiveRadiance[1], emissiveRadiance[2], 0.f},
                .instAlbedo = {staticMeshCnstAlbedo[0], staticMeshCnstAlbedo[1], staticMeshCnstAlbedo[2], 0.f},
                .instMetallicRoughness = {staticMeshCnstMetallicRoughness[0], staticMeshCnstMetallicRoughness[1], 0.f, 0.f}
            };
            instInfoVecData.push_back(instInfo);
        }
    }

    // Create and init the camera constant buffer
    auto cameraCnstDesc = BASIC_BUFFER_DESC;
    cameraCnstDesc.Width = sizeof(FrameConstBuffer);
    m_pD3dDevice->CreateCommittedResource(&UPLOAD_HEAP, D3D12_HEAP_FLAG_NONE,
                                          &cameraCnstDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                          nullptr, IID_PPV_ARGS(&m_frameCnstBuffer));
    m_frameCnstBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_frameCnstBufferMap));

    UpdateFrameConstBuffer();

    // Create and init instance info buffer
    auto instInfoDesc = BASIC_BUFFER_DESC;
    instInfoDesc.Width = sizeof(InstanceInfo) * instInfoVecData.size();
    m_pD3dDevice->CreateCommittedResource(&UPLOAD_HEAP, D3D12_HEAP_FLAG_NONE,
                                          &instInfoDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                          nullptr, IID_PPV_ARGS(&m_instInfoBuffer));
    void* instInfoBufferMap;
    m_instInfoBuffer->Map(0, nullptr, reinterpret_cast<void**>(&instInfoBufferMap));
    memcpy(instInfoBufferMap, instInfoVecData.data(), sizeof(InstanceInfo) * instInfoVecData.size());
    m_instInfoBuffer->Unmap(0, nullptr);

    // Create and init scene vert buffer
    auto sceneVertDesc = BASIC_BUFFER_DESC;
    sceneVertDesc.Width = sizeof(float) * sceneVertData.size();
    m_pD3dDevice->CreateCommittedResource(&UPLOAD_HEAP, D3D12_HEAP_FLAG_NONE,
                                          &sceneVertDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                          nullptr, IID_PPV_ARGS(&m_sceneVertBuffer));
    void* sceneVertBufferMap;
    m_sceneVertBuffer->Map(0, nullptr, &sceneVertBufferMap);
    memcpy(sceneVertBufferMap, sceneVertData.data(), sizeof(float) * sceneVertData.size());
    m_sceneVertBuffer->Unmap(0, nullptr);

    // Create and init scene idx buffer
    auto sceneIdxDesc = BASIC_BUFFER_DESC;
    sceneIdxDesc.Width = sizeof(uint16_t) * sceneIdxData.size();
    m_pD3dDevice->CreateCommittedResource(&UPLOAD_HEAP, D3D12_HEAP_FLAG_NONE,
                                          &sceneIdxDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                          nullptr, IID_PPV_ARGS(&m_sceneIdxBuffer));
    void* sceneIdxBufferMap;
    m_sceneIdxBuffer->Map(0, nullptr, &sceneIdxBufferMap);
    memcpy(sceneIdxBufferMap, sceneIdxData.data(), sizeof(uint16_t) * sceneIdxData.size());
    m_sceneIdxBuffer->Unmap(0, nullptr);
}

void HWRTRenderBackend::InitTopLevel()
{
    UINT64 updateScratchSize;
    m_tlas = MakeTLAS(m_instances, m_numInstances, &updateScratchSize);

    auto desc = BASIC_BUFFER_DESC;
    // WARP bug workaround: use 8 if the required size was reported as less
    desc.Width = std::max(updateScratchSize, 8ULL);
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    m_pD3dDevice->CreateCommittedResource(&DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE, &desc,
                                          D3D12_RESOURCE_STATE_COMMON, nullptr,
                                          IID_PPV_ARGS(&m_tlasUpdateScratch));
}

ID3D12Resource* HWRTRenderBackend::MakeBLAS(ID3D12Resource* vertexBuffer, UINT vertexFloats, ID3D12Resource* indexBuffer, UINT indices)
{
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {
        .Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
        .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,

        .Triangles = {
            .Transform3x4 = 0, // NOTE: We need model matrix for each mesh primitives.

            .IndexFormat = indexBuffer ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_UNKNOWN,
            .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
            .IndexCount = indices,
            .VertexCount = vertexFloats / VERT_SIZE_FLOAT,
            .IndexBuffer = indexBuffer ? indexBuffer->GetGPUVirtualAddress() : 0,
            .VertexBuffer = {.StartAddress = vertexBuffer->GetGPUVirtualAddress(),
                             .StrideInBytes = sizeof(float) * VERT_SIZE_FLOAT}}};

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
        .NumDescs = 1,
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .pGeometryDescs = &geometryDesc};

    return MakeAccelerationStructure(inputs);
}

ID3D12Resource* HWRTRenderBackend::MakeTLAS(ID3D12Resource* instances, UINT numInstances,
                         UINT64* updateScratchSize)
{
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE,
        .NumDescs = numInstances,
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .InstanceDescs = instances->GetGPUVirtualAddress()};

    return MakeAccelerationStructure(inputs, updateScratchSize);
}

void HWRTRenderBackend::InitBottomLevel()
{
    std::vector<std::string> staticMeshNames;
    g_pAssetManager->RetriveAllStaticMeshesNames(staticMeshNames);

    for (auto& modelName : staticMeshNames)
    {
        std::vector<PrimitiveAsset*> primAssets;
        g_pAssetManager->RetriveStaticMeshAssets(modelName, primAssets);
        for (auto primPtr : primAssets)
        {
            auto* vertexBuffer = primPtr->m_gpuVertBuffer;
            auto* indexBuffer = primPtr->m_gpuIndexBuffer;
            primPtr->m_blas = MakeBLAS(vertexBuffer, primPtr->m_vertData.size(), indexBuffer, primPtr->m_idxCnt);
        }
    }
}

void HWRTRenderBackend::InitRootSignature()
{
    D3D12_DESCRIPTOR_RANGE uavRange = {
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
        .NumDescriptors = 2,
    };
    D3D12_ROOT_PARAMETER params[] = {
        {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
         .DescriptorTable = {.NumDescriptorRanges = 1,
                             .pDescriptorRanges = &uavRange}},

        {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
         .Descriptor = {.ShaderRegister = 0, .RegisterSpace = 0}},
    
        {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
         .Descriptor = {.ShaderRegister = 0, .RegisterSpace = 0}},

        {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
         .Descriptor = {.ShaderRegister = 1, .RegisterSpace = 0}},

        {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
         .Descriptor = {.ShaderRegister = 2, .RegisterSpace = 0}},

        {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
         .Descriptor = {.ShaderRegister = 3, .RegisterSpace = 0}},

        {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
         .Descriptor = {.ShaderRegister = 4, .RegisterSpace = 0}},
    };

    D3D12_ROOT_SIGNATURE_DESC desc = {.NumParameters = std::size(params),
                                      .pParameters = params};

    ID3DBlob* blob;
    D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob,
                                nullptr);
    m_pD3dDevice->CreateRootSignature(0, blob->GetBufferPointer(),
                                blob->GetBufferSize(),
                                IID_PPV_ARGS(&m_rootSignature));
    blob->Release();
}

constexpr UINT64 NUM_SHADER_IDS = 3;
void HWRTRenderBackend::InitPipeline()
{

    D3D12_DXIL_LIBRARY_DESC lib = {
        .DXILLibrary = {.pShaderBytecode = CustomRTShader,
                        .BytecodeLength = std::size(CustomRTShader)}};

    D3D12_HIT_GROUP_DESC hitGroup = {.HitGroupExport = L"HitGroup",
                                     .Type = D3D12_HIT_GROUP_TYPE_TRIANGLES,
                                     .ClosestHitShaderImport = L"ClosestHit"};

    D3D12_RAYTRACING_SHADER_CONFIG shaderCfg = {
        .MaxPayloadSizeInBytes = 60,
        .MaxAttributeSizeInBytes = 8,
    };

    D3D12_GLOBAL_ROOT_SIGNATURE globalSig = {m_rootSignature};

    // NOTE! If the ray recursion is more than 1, the GPU will be disconnected/TDR.
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineCfg = {.MaxTraceRecursionDepth = 1};

    D3D12_STATE_SUBOBJECT subobjects[] = {
        {.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, .pDesc = &lib},
        {.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, .pDesc = &hitGroup},
        {.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, .pDesc = &shaderCfg},
        {.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, .pDesc = &globalSig},
        {.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, .pDesc = &pipelineCfg}};
    D3D12_STATE_OBJECT_DESC desc = {.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
                                    .NumSubobjects = std::size(subobjects),
                                    .pSubobjects = subobjects};
    m_pD3dDevice->CreateStateObject(&desc, IID_PPV_ARGS(&m_pso));

    auto idDesc = BASIC_BUFFER_DESC;
    idDesc.Width = NUM_SHADER_IDS * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
    m_pD3dDevice->CreateCommittedResource(&UPLOAD_HEAP, D3D12_HEAP_FLAG_NONE, &idDesc,
                                    // D3D12_RESOURCE_STATE_COMMON, nullptr,
                                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                    IID_PPV_ARGS(&m_shaderIDs));

    ID3D12StateObjectProperties* props;
    m_pso->QueryInterface(&props);

    void* data;
    auto writeId = [&](const wchar_t* name) {
        void* id = props->GetShaderIdentifier(name);
        memcpy(data, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        data = static_cast<char*>(data) +
               D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
    };

    m_shaderIDs->Map(0, nullptr, &data);
    writeId(L"RayGeneration");
    writeId(L"Miss");
    writeId(L"HitGroup");
    m_shaderIDs->Unmap(0, nullptr);

    props->Release();
}

void HWRTRenderBackend::CustomInit()
{
    m_pD3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));

    D3D12_DESCRIPTOR_HEAP_DESC uavHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 2,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
    m_pD3dDevice->CreateDescriptorHeap(&uavHeapDesc, IID_PPV_ARGS(&m_uavHeap));

    uint32_t winWidth, winHeight;
    m_pUIManager->GetWindowSize(winWidth, winHeight);
    CustomResize(winWidth, winHeight);

    InitBottomLevel();
    InitScene();
    InitTopLevel();
    InitRootSignature();
    InitPipeline();
}

void HWRTRenderBackend::CustomDeinit()
{
    if (m_uavHeap) { m_uavHeap->Release(); m_uavHeap = nullptr; }
    if (m_renderTarget) { m_renderTarget->Release(); m_renderTarget = nullptr; }
    if (m_renderTargetRadiance) { m_renderTargetRadiance->Release(); m_renderTargetRadiance = nullptr; }
    if (m_fence) { m_fence->Release(); m_fence = nullptr; }
    if (m_instances) { m_instances->Release(); m_instances = nullptr; }
    if (m_tlas) { m_tlas->Release(); m_tlas = nullptr; }
    if (m_tlasUpdateScratch) { m_tlasUpdateScratch->Release(); m_tlasUpdateScratch = nullptr; }
    if (m_rootSignature) { m_rootSignature->Release(); m_rootSignature = nullptr; }
    if (m_pso) { m_pso->Release(); m_pso = nullptr; }
    if (m_shaderIDs) { m_shaderIDs->Release(); m_shaderIDs = nullptr; }
    if (m_frameCnstBuffer) { m_frameCnstBuffer->Release(); m_frameCnstBuffer = nullptr; }
    if (m_sceneVertBuffer) { m_sceneVertBuffer->Release(); m_sceneVertBuffer = nullptr; }
    if (m_sceneIdxBuffer) { m_sceneIdxBuffer->Release(); m_sceneIdxBuffer = nullptr; }
    if (m_instInfoBuffer) { m_instInfoBuffer->Release(); m_instInfoBuffer = nullptr; }
}

void HWRTRenderBackend::UpdateFrameConstBuffer()
{
    Camera* pCamera = nullptr;
    m_pLevel->RetriveActiveCamera(&pCamera);
    float right[3] = {};
    CrossProductVec3(pCamera->m_view, pCamera->m_up, right);
    NormalizeVec(right, 3);

    srand(time(0));
    // Generate a random number between 1 and 100
    uint32_t random_number = rand() % 100 + 1;

    auto now = std::chrono::high_resolution_clock::now();
    auto nanoseconds = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
    uint64_t currentTime = nanoseconds.time_since_epoch().count();
    uint32_t currentTimeLow = static_cast<uint32_t>(currentTime & 0xFFFFFFFF);

    FrameConstBuffer frameCnstBuffer
    {
        .cameraPos = {pCamera->m_pos[0], pCamera->m_pos[1], pCamera->m_pos[2], 0.f},
        .cameraDir = {pCamera->m_view[0], pCamera->m_view[1], pCamera->m_view[2], 0.f},
        .cameraUp = {pCamera->m_up[0], pCamera->m_up[1], pCamera->m_up[2], 0.f},
        .cameraRight = {right[0], right[1], right[2], 0.f},
        .cameraInfo = {pCamera->m_fov, pCamera->m_near, pCamera->m_far, 0.f},
        .frameUintInfo = {m_frameCount++, random_number, currentTimeLow, m_pLevel->m_backgroundType}
    };

    memcpy(m_frameCnstBufferMap, &frameCnstBuffer, sizeof(FrameConstBuffer));
}

void HWRTRenderBackend::UpdateScene(ID3D12GraphicsCommandList4* cmdList)
{
    UpdateFrameConstBuffer();
    /*
    UpdateTransforms();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {
        .DestAccelerationStructureData = m_tlas->GetGPUVirtualAddress(),
        .Inputs = {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE,
            .NumDescs = NUM_INSTANCES,
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .InstanceDescs = m_instances->GetGPUVirtualAddress()},
        .SourceAccelerationStructureData = m_tlas->GetGPUVirtualAddress(),
        .ScratchAccelerationStructureData = m_tlasUpdateScratch->GetGPUVirtualAddress(),
    };
    cmdList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                                      .UAV = {.pResource = m_tlas}};
    cmdList->ResourceBarrier(1, &barrier);
    */
}


void HWRTRenderBackend::RenderTick(ID3D12GraphicsCommandList4* pCommandList, RenderTargetInfo rtInfo)
{
    UpdateScene(pCommandList);

    pCommandList->SetPipelineState1(m_pso);
    pCommandList->SetComputeRootSignature(m_rootSignature);
    pCommandList->SetDescriptorHeaps(1, &m_uavHeap);
    auto uavTable = m_uavHeap->GetGPUDescriptorHandleForHeapStart();
    pCommandList->SetComputeRootDescriptorTable(0, uavTable); // ?u0 ?t0
    pCommandList->SetComputeRootShaderResourceView(1, m_tlas->GetGPUVirtualAddress());
    pCommandList->SetComputeRootConstantBufferView(2, m_frameCnstBuffer->GetGPUVirtualAddress());
    pCommandList->SetComputeRootShaderResourceView(3, m_instInfoBuffer->GetGPUVirtualAddress());
    pCommandList->SetComputeRootShaderResourceView(4, m_sceneVertBuffer->GetGPUVirtualAddress());
    pCommandList->SetComputeRootShaderResourceView(5, m_sceneIdxBuffer->GetGPUVirtualAddress());

    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {
        .RayGenerationShaderRecord = {
            .StartAddress = m_shaderIDs->GetGPUVirtualAddress(),
            .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES},
        .MissShaderTable = {
            .StartAddress = m_shaderIDs->GetGPUVirtualAddress() +
                            D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
            .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES},
        .HitGroupTable = {
            .StartAddress = m_shaderIDs->GetGPUVirtualAddress() +
                            2 * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
            .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES},
        .Width = static_cast<UINT>(rtInfo.rtDesc.Width),
        .Height = rtInfo.rtDesc.Height,
        .Depth = 1};
    pCommandList->DispatchRays(&dispatchDesc);

    auto barrier = [&](auto* resource, auto before, auto after) {
        D3D12_RESOURCE_BARRIER rb = {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Transition = {.pResource = resource,
                           .StateBefore = before,
                           .StateAfter = after},
        };
        pCommandList->ResourceBarrier(1, &rb);
    };

    barrier(m_renderTarget, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
    barrier(rtInfo.pResource, D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST);

    pCommandList->CopyResource(rtInfo.pResource, m_renderTarget);

    barrier(rtInfo.pResource, D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
    barrier(m_renderTarget, D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void HWRTRenderBackend::CustomResize(uint32_t width, uint32_t height)
{
    std::cout << "Resizing to " << width << "x" << height << std::endl;

    if (m_renderTarget) [[likely]]
        m_renderTarget->Release();

    D3D12_RESOURCE_DESC rtDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width = width,
        .Height = height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = NO_AA,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};

    m_pD3dDevice->CreateCommittedResource(&DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE, &rtDesc,
                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                    nullptr, IID_PPV_ARGS(&m_renderTarget));

    m_pD3dDevice->CreateCommittedResource(&DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE, &rtDesc,
                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                    nullptr, IID_PPV_ARGS(&m_renderTargetRadiance));

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D};

    const uint32_t uavDescHandleOffset = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE uavHeapHandle = m_uavHeap->GetCPUDescriptorHandleForHeapStart();

    m_pD3dDevice->CreateUnorderedAccessView(m_renderTarget, nullptr, &uavDesc, uavHeapHandle);
    uavHeapHandle.ptr += uavDescHandleOffset;
    m_pD3dDevice->CreateUnorderedAccessView(m_renderTargetRadiance, nullptr, &uavDesc, uavHeapHandle);
}