#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "HWRTRenderBackend.h"
#include "../Scene/SceneAssetLoader.h"
#include "../Scene/Level.h"
#include "../Scene/Mesh.h"
#include "../Utils/DX12Utils.h"
#include "RTShaders/Raytracing.hlsl.h"
#include "../UI/UIManager.h"
#include "../MiniRendererApp.h"
#include <algorithm>     // For std::size, typed std::max, etc.
#include <DirectXMath.h> // For XMMATRIX
#include <d3d12.h>

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

constexpr float quadVtx[] = {-1, 0, -1, -1, 0,  1, 1, 0, 1,
                             -1, 0, -1,  1, 0, -1, 1, 0, 1};
constexpr float cubeVtx[] = {-1, -1, -1, 1, -1, -1, -1, 1, -1, 1, 1, -1,
                             -1, -1,  1, 1, -1,  1, -1, 1,  1, 1, 1,  1};
constexpr short cubeIdx[] = {4, 6, 0, 2, 0, 6, 0, 1, 4, 5, 4, 1,
                             0, 2, 1, 3, 1, 2, 1, 3, 5, 7, 5, 3,
                             2, 6, 3, 7, 3, 6, 4, 5, 6, 7, 6, 5};

/*
static const wchar_t* c_hitGroupName         = L"MyHitGroup";
static const wchar_t* c_raygenShaderName     = L"MyRaygenShader";
static const wchar_t* c_closestHitShaderName = L"MyClosestHitShader";
static const wchar_t* c_missShaderName       = L"MyMissShader";
*/

void HWRTRenderBackend::Flush()
{
    static UINT64 value = 1;
    m_pMainCommandQueue->Signal(m_fence, value);
    m_fence->SetEventOnCompletion(value++, nullptr);
}

void HWRTRenderBackend::InitMeshes()
{
    auto makeAndCopy = [=](auto& data) {
        auto desc = BASIC_BUFFER_DESC;
        desc.Width = sizeof(data);
        ID3D12Resource* res;
        m_pD3dDevice->CreateCommittedResource(&UPLOAD_HEAP, D3D12_HEAP_FLAG_NONE,
            // &desc, D3D12_RESOURCE_STATE_COMMON,
            &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&res));

        void* ptr;
        res->Map(0, nullptr, &ptr);
        memcpy(ptr, data, sizeof(data));
        res->Unmap(0, nullptr);
        return res;
        };

    m_quadVB = makeAndCopy(quadVtx);
    m_cubeVB = makeAndCopy(cubeVtx);
    m_cubeIB = makeAndCopy(cubeIdx);
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

constexpr UINT NUM_INSTANCES = 3;

void HWRTRenderBackend::UpdateTransforms()
{
    using namespace DirectX;
    auto set = [=](int idx, XMMATRIX mx) {
        auto* ptr = reinterpret_cast<XMFLOAT3X4*>(&m_instanceData[idx].Transform);
        XMStoreFloat3x4(ptr, mx);
    };

    auto time = static_cast<float>(GetTickCount64()) / 1000;

    auto cube = XMMatrixRotationRollPitchYaw(time / 2, time / 3, time / 5);
    cube *= XMMatrixTranslation(-1.5, 2, 2);
    set(0, cube);

    auto mirror = XMMatrixRotationX(-1.8f);
    mirror *= XMMatrixRotationY(XMScalarSinEst(time) / 8 + 1);
    mirror *= XMMatrixTranslation(2, 2, 2);
    set(1, mirror);

    auto floor = XMMatrixScaling(5, 5, 5);
    floor *= XMMatrixTranslation(0, 0, 2);
    set(2, floor);
}

void HWRTRenderBackend::InitScene()
{
    auto instancesDesc = BASIC_BUFFER_DESC;
    instancesDesc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * NUM_INSTANCES;
    m_pD3dDevice->CreateCommittedResource(&UPLOAD_HEAP, D3D12_HEAP_FLAG_NONE,
                                    // &instancesDesc, D3D12_RESOURCE_STATE_COMMON,
                                    &instancesDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                    nullptr, IID_PPV_ARGS(&m_instances));
    m_instances->Map(0, nullptr, reinterpret_cast<void**>(&m_instanceData));

    for (UINT i = 0; i < NUM_INSTANCES; ++i)
        m_instanceData[i] = {
            .InstanceID = i,
            .InstanceMask = 1,
            .AccelerationStructure = (i ? m_quadBlas : m_cubeBlas)->GetGPUVirtualAddress(),
        };

    UpdateTransforms();

}

void HWRTRenderBackend::InitTopLevel()
{
    UINT64 updateScratchSize;
    m_tlas = MakeTLAS(m_instances, NUM_INSTANCES, &updateScratchSize);

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
            .Transform3x4 = 0,

            .IndexFormat = indexBuffer ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_UNKNOWN,
            .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
            .IndexCount = indices,
            .VertexCount = vertexFloats / 3,
            .IndexBuffer = indexBuffer ? indexBuffer->GetGPUVirtualAddress() : 0,
            .VertexBuffer = {.StartAddress = vertexBuffer->GetGPUVirtualAddress(),
                             .StrideInBytes = sizeof(float) * 3}}};

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
    m_quadBlas = MakeBLAS(m_quadVB, std::size(quadVtx));
    m_cubeBlas = MakeBLAS(m_cubeVB, std::size(cubeVtx), m_cubeIB, std::size(cubeIdx));
}

void HWRTRenderBackend::InitRootSignature()
{

    D3D12_DESCRIPTOR_RANGE uavRange = {
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
        .NumDescriptors = 1,
    };
    D3D12_ROOT_PARAMETER params[] = {
        {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
         .DescriptorTable = {.NumDescriptorRanges = 1,
                             .pDescriptorRanges = &uavRange}},

        {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
         .Descriptor = {.ShaderRegister = 0, .RegisterSpace = 0}}};

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

void HWRTRenderBackend::CustomInit()
{
    m_pD3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));

    D3D12_DESCRIPTOR_HEAP_DESC uavHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 1,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
    m_pD3dDevice->CreateDescriptorHeap(&uavHeapDesc, IID_PPV_ARGS(&m_uavHeap));

    uint32_t winWidth, winHeight;
    m_pUIManager->GetWindowSize(winWidth, winHeight);
    CustomResize(winWidth, winHeight);

    InitMeshes();
    InitBottomLevel();
    InitScene();
    InitTopLevel();
    InitRootSignature();
    /*
    m_rayGenCB.viewport = { -1.0f, -1.0f, 1.0f, 1.0f };
    ThrowIfFailed(m_pD3dDevice->QueryInterface(IID_PPV_ARGS(&m_dxrDevice)), L"Couldn't get DirectX Raytracing interface for the device.\n");

    uint32_t winWidth, winHeight;
    m_pUIManager->GetWindowSize(winWidth, winHeight);
    UpdateForSizeChange(winWidth, winHeight);

    InitRootSignatures();
    InitPipelineStates();
    InitDescriptorHeaps();
    BuildGeometry();
    BuildAccelerationStructures();
    BuildShaderTables();
    BuildRaytracingOutput();
    */
}

void HWRTRenderBackend::CustomDeinit()
{
    if(m_uavHeap) { m_uavHeap->Release(); m_uavHeap = nullptr; }
    if(m_renderTarget) { m_renderTarget->Release(); m_renderTarget = nullptr; }
    if(m_quadVB) { m_quadVB->Release(); m_quadVB = nullptr; }
    if(m_cubeVB) { m_cubeVB->Release(); m_cubeVB = nullptr; }
    if(m_cubeIB) { m_cubeIB->Release(); m_cubeIB = nullptr; }
    if(m_fence) { m_fence->Release(); m_fence = nullptr; }
    if(m_instances) { m_instances->Release(); m_instances = nullptr; }
    if(m_quadBlas) { m_quadBlas->Release(); m_quadBlas = nullptr; }
    if(m_cubeBlas) { m_cubeBlas->Release(); m_cubeBlas = nullptr; }
    if(m_tlas) { m_tlas->Release(); m_tlas = nullptr; }
    if(m_tlasUpdateScratch) { m_tlasUpdateScratch->Release(); m_tlasUpdateScratch = nullptr; }
    if(m_rootSignature) { m_rootSignature->Release(); m_rootSignature = nullptr; }
    /*
    m_raytracingGlobalRootSignature->Release();
    m_raytracingGlobalRootSignature = nullptr;
    m_raytracingLocalRootSignature->Release();
    m_raytracingLocalRootSignature = nullptr;
    m_rtPipelineStateObject->Release();
    m_rtPipelineStateObject = nullptr;
    m_descriptorHeap->Release();
    m_descriptorHeap = nullptr;

    m_blas->Release();
    m_blas = nullptr;
    m_tlas->Release();
    m_tlas = nullptr;

    m_raytracingOutput->Release();
    m_raytracingOutput = nullptr;

    m_rayGenShaderTable->Release();
    m_rayGenShaderTable = nullptr;
    m_missShaderTable->Release();
    m_missShaderTable = nullptr;
    m_hitGroupShaderTable->Release();
    m_hitGroupShaderTable = nullptr;
    */
}

/*
void HWRTRenderBackend::InitRootSignatures() // Global and Local Root Signatures
{
    // Global Root Signature
    {
        D3D12_DESCRIPTOR_RANGE uavDescriptorRange{};
        {
            uavDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            uavDescriptorRange.NumDescriptors = 1;
            uavDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        }

        // The maximum size of a root signature is 64 DWORDs.
        // Descriptor tables cost 1 DWORD each.
        // Root constants cost 1 DWORD each, since they are 32-bit values.
        // Root descriptors (64-bit GPU virtual addresses) cost 2 DWORDs each.
        // The root signature is prime real estate, and there are strict limits and costs to consider.
        // 0: OutputViewSlot
        // 1: AccelerationStructureSlot
        D3D12_ROOT_PARAMETER rootParameters[2]{};
        {
            rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
            rootParameters[0].DescriptorTable.pDescriptorRanges = &uavDescriptorRange;

            rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
            rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            rootParameters[1].Descriptor.ShaderRegister = 0;
            rootParameters[1].Descriptor.RegisterSpace = 0;
        }

        D3D12_ROOT_SIGNATURE_DESC globalRootSignatureDesc{};
        {
            globalRootSignatureDesc.NumParameters = _countof(rootParameters);
            globalRootSignatureDesc.pParameters = rootParameters;
            globalRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        }

        ID3DBlob* pBlob = nullptr;
        ID3DBlob* pError = nullptr;
        ThrowIfFailed(D3D12SerializeRootSignature(&globalRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pBlob, &pError), pError ? static_cast<wchar_t*>(pError->GetBufferPointer()) : nullptr);
        ThrowIfFailed(m_pD3dDevice->CreateRootSignature(1, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), IID_PPV_ARGS(&m_raytracingGlobalRootSignature)));
        SetName(m_raytracingGlobalRootSignature, L"Global Root Signature");

        pBlob->Release();
        if (pError) { pError->Release(); }
    }
    
    // Local Root Signature
    {
        D3D12_ROOT_PARAMETER viewportConstantParameter{};
        viewportConstantParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        viewportConstantParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        viewportConstantParameter.Constants.Num32BitValues = SizeOfInUint32(m_rayGenCB); // float4 viewport

        D3D12_ROOT_SIGNATURE_DESC localRootSignatureDesc{};
        {
            localRootSignatureDesc.NumParameters = 1;
            localRootSignatureDesc.pParameters = &viewportConstantParameter;
            localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
        }

        ID3DBlob* pBlob = nullptr;
        ID3DBlob* pError = nullptr;
        ThrowIfFailed(D3D12SerializeRootSignature(&localRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pBlob, &pError), pError ? static_cast<wchar_t*>(pError->GetBufferPointer()) : nullptr);
        ThrowIfFailed(m_pD3dDevice->CreateRootSignature(1, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), IID_PPV_ARGS(&m_raytracingLocalRootSignature)));
        SetName(m_raytracingLocalRootSignature, L"Local Root Signature");

        pBlob->Release();
        if (pError) { pError->Release(); }
    }
}
*/
/*
// NOTE that the local/global root signatures need to be the pointer of the pointer -- In the DX12 example, it uses `GetAddressOf` to get the pointer of the pointer.
void HWRTRenderBackend::InitPipelineStates() // PSOs
{
    //  CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };
    D3D12_STATE_OBJECT_DESC raytracingPipelineStateObjectDesc{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

    D3D12_STATE_SUBOBJECT subobjects[7]{};

    // Setup Shader Libraries
    D3D12_DXIL_LIBRARY_DESC libDesc{};
    // D3D12_STATE_SUBOBJECT libSubobject{D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &libDesc};
    subobjects[0].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    subobjects[0].pDesc = &libDesc;

    // std::vector<unsigned char> raytracingShadersByteCode;
    std::string rtShaderObjectPath = std::string(SOURCE_PATH) + "/RenderBackend/RTShaders/RaytracingShader.o";
    // SceneAssetLoader::LoadShaderObject(rtShaderObjectPath, raytracingShadersByteCode);

    D3D12_SHADER_BYTECODE libdxil{ g_pRaytracing, ARRAYSIZE(g_pRaytracing)};

    D3D12_EXPORT_DESC rayGenShaderExport{c_raygenShaderName, nullptr, D3D12_EXPORT_FLAG_NONE};
    D3D12_EXPORT_DESC closestHitShaderExport{c_closestHitShaderName, nullptr, D3D12_EXPORT_FLAG_NONE};
    D3D12_EXPORT_DESC missShaderExport{c_missShaderName, nullptr, D3D12_EXPORT_FLAG_NONE};
    D3D12_EXPORT_DESC exports[3]{rayGenShaderExport, closestHitShaderExport, missShaderExport};

    libDesc.DXILLibrary = libdxil;
    libDesc.NumExports = 3;
    libDesc.pExports = exports;

    // Setup Hit Groups
    D3D12_HIT_GROUP_DESC hitGroupDesc{c_hitGroupName, D3D12_HIT_GROUP_TYPE_TRIANGLES, nullptr, c_closestHitShaderName, nullptr};
    // D3D12_STATE_SUBOBJECT hitGroupSubobject{D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroupDesc};
    subobjects[1].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    subobjects[1].pDesc = &hitGroupDesc;

    // Setup Shader Config
    UINT payloadSize = 4 * sizeof(float);   // float4 color
    UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig{payloadSize, attributeSize};
    // D3D12_STATE_SUBOBJECT shaderConfigSubobject{D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderConfig};
    subobjects[2].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    subobjects[2].pDesc = &shaderConfig;

    // Local root signature and shader association state subobjects
    // D3D12_STATE_SUBOBJECT localRootSignatureSubobject{D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, m_raytracingLocalRootSignature};
    subobjects[3].Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
    subobjects[3].pDesc = &m_raytracingLocalRootSignature;

    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION localRootSignatureAssociation{&subobjects[3], 1, &c_raygenShaderName};
    // D3D12_STATE_SUBOBJECT localRootSignatureAssociationSubobject{D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &localRootSignatureAssociation};
    subobjects[4].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    subobjects[4].pDesc = &localRootSignatureAssociation;

    // Global root signature
    // D3D12_STATE_SUBOBJECT globalRootSignatureSubobject{D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, m_raytracingGlobalRootSignature};
    subobjects[5].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    subobjects[5].pDesc = &m_raytracingGlobalRootSignature;

    // Ray tracing pipeline config
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfigDesc{1}; // Max recursion depth
    // D3D12_STATE_SUBOBJECT pipelineConfigSubobject{D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipelineConfigDesc};
    subobjects[6].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    subobjects[6].pDesc = &pipelineConfigDesc;
    */
    // Create the state object
    /*
    D3D12_STATE_SUBOBJECT subobjects[7]{libSubobject, hitGroupSubobject, shaderConfigSubobject, localRootSignatureSubobject, 
                                        localRootSignatureAssociationSubobject, globalRootSignatureSubobject, pipelineConfigSubobject};
    */  
/*
    raytracingPipelineStateObjectDesc.NumSubobjects = 7;
    raytracingPipelineStateObjectDesc.pSubobjects = subobjects;

#if _DEBUG
    PrintStateObjectDesc(&raytracingPipelineStateObjectDesc);
#endif

    ThrowIfFailed(m_dxrDevice->CreateStateObject(&raytracingPipelineStateObjectDesc, IID_PPV_ARGS(&m_rtPipelineStateObject)), L"Couldn't create DirectX Raytracing state object.\n");



}
*/
/*
void HWRTRenderBackend::InitDescriptorHeaps() // Descriptor Heaps
{
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    {
        descriptorHeapDesc.NumDescriptors = 1; 
        descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        descriptorHeapDesc.NodeMask = 0;
    }
    m_pD3dDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap));
    NAME_D3D12_OBJECT(m_descriptorHeap);
}

void HWRTRenderBackend::BuildGeometry() // Build Scene Geometry
{
    // std::vector<StaticMesh*> staticMeshes;
    // m_pLevel->RetriveStaticMeshes(staticMeshes);
    Index indices[] =
    {
        0, 1, 2
    };

    float depthValue = 1.0;
    float offset = 0.7f;
    Vertex vertices[] =
    {
        // The sample raytraces in screen space coordinates.
        // Since DirectX screen space coordinates are right handed (i.e. Y axis points down).
        // Define the vertices in counter clockwise order ~ clockwise in left handed.
        { 0, -offset, depthValue },
        { -offset, offset, depthValue },
        { offset, offset, depthValue }
    };
    
    AllocateUploadBuffer(m_pD3dDevice, vertices, sizeof(vertices), &m_vertexBuffer);
    AllocateUploadBuffer(m_pD3dDevice, indices, sizeof(indices), &m_indexBuffer);
    // Loop through all primitives and create gpu resources map
    */
    /*
    for (StaticMesh* staticMesh : staticMeshes)
    {
        for (int i = 0; i < staticMesh->m_meshPrimitives.size(); ++i)
        {
            MeshPrimitive* meshPrimitive = &staticMesh->m_meshPrimitives[i];
            meshPrimitive->m_gpuRsrcMap["Position Buffer"] = nullptr;
            AllocateUploadBuffer(m_pD3dDevice,
                                 meshPrimitive->m_posData.data(),
                                 meshPrimitive->m_posData.size() * sizeof(float),
                                 &meshPrimitive->m_gpuRsrcMap["Position Buffer"],
                                 L"Position Buffer");

            meshPrimitive->m_gpuRsrcMap["Index Buffer"] = nullptr;
            AllocateUploadBuffer(m_pD3dDevice,
                                 meshPrimitive->m_idxDataUint16.data(),
                                 meshPrimitive->m_idxDataUint16.size() * sizeof(uint16_t),
                                 &meshPrimitive->m_gpuRsrcMap["Index Buffer"],
                                 L"Index Buffer");
        }
    }
    */
// }
/*
void HWRTRenderBackend::BuildAccelerationStructures() // Build Scene Acceleration Structures
{
    // Create temp command list to build acceleration structures
    ID3D12CommandAllocator*     pCommandAllocator = nullptr;
    ID3D12Fence*                pFence            = nullptr;
    ID3D12GraphicsCommandList*  pD3dCommandList   = nullptr;
    ID3D12CommandQueue*         pCommandQueue     = nullptr;
    ID3D12GraphicsCommandList4* pDxrCommandList   = nullptr;

    if (m_tlas)
    {
        m_tlas->Release();
        m_tlas = nullptr;
    }

    if (m_blas)
    {
        m_blas->Release();
        m_blas = nullptr;
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        m_pD3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&pCommandQueue));
        pCommandQueue->SetName(L"Acc Structure Build Command Queue");
    }

    m_pD3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pCommandAllocator));
    m_pD3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence));
    m_pD3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCommandAllocator, nullptr, IID_PPV_ARGS(&pD3dCommandList));
    */
    // We only have one mesh primitive for now
    /*
    std::vector<StaticMesh*> staticMeshes;
    m_pLevel->RetriveStaticMeshes(staticMeshes);
    MeshPrimitive* meshPrimitive = &staticMeshes[0]->m_meshPrimitives[0];

    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    {
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Triangles.IndexBuffer = meshPrimitive->m_gpuRsrcMap["Index Buffer"]->GetGPUVirtualAddress();
        geometryDesc.Triangles.IndexCount = meshPrimitive->m_idxDataUint16.size();
        geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
        geometryDesc.Triangles.Transform3x4 = 0;
        geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometryDesc.Triangles.VertexCount = meshPrimitive->m_posData.size() / 3;
        geometryDesc.Triangles.VertexBuffer.StartAddress = meshPrimitive->m_gpuRsrcMap["Position Buffer"]->GetGPUVirtualAddress();
        geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(float) * 3;
        geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    }
    */
/*
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    {
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Triangles.IndexBuffer = m_indexBuffer->GetGPUVirtualAddress();
        geometryDesc.Triangles.IndexCount = static_cast<UINT>(m_indexBuffer->GetDesc().Width) / sizeof(Index);
        geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
        geometryDesc.Triangles.Transform3x4 = 0;
        geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometryDesc.Triangles.VertexCount = static_cast<UINT>(m_vertexBuffer->GetDesc().Width) / sizeof(Vertex);
        geometryDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer->GetGPUVirtualAddress();
        geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
        geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
    {
        tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        tlasInputs.NumDescs = 1;
    }
    
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo = {};
    m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);
    ThrowIfFalse(tlasPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuildInfo = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = tlasInputs;
    {
        blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        blasInputs.pGeometryDescs = &geometryDesc;
    }
    m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasPrebuildInfo);
    ThrowIfFalse(blasPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    ID3D12Resource* scratchBuffer = nullptr;
    AllocateUAVBuffer(m_pD3dDevice,
                      max(tlasPrebuildInfo.ScratchDataSizeInBytes, blasPrebuildInfo.ScratchDataSizeInBytes),
                      &scratchBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

    // Allocate buffer for the tlas and blas
    {
        AllocateUAVBuffer(m_pD3dDevice,
                          blasPrebuildInfo.ResultDataMaxSizeInBytes,
                          &m_blas, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"BottomLevelAccelerationStructure");

        AllocateUAVBuffer(m_pD3dDevice,
                          tlasPrebuildInfo.ResultDataMaxSizeInBytes,
                          &m_tlas, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"TopLevelAccelerationStructure");
    }

    ID3D12Resource* pBlasInstDesc = nullptr;
    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
    {
        instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
        instanceDesc.InstanceMask = 1;
        instanceDesc.AccelerationStructure = m_blas->GetGPUVirtualAddress();
    }
    AllocateUploadBuffer(m_pD3dDevice, &instanceDesc, sizeof(instanceDesc), &pBlasInstDesc, L"InstanceDesc");

    // TLAS, BLAS build desc
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuildDesc = {};
    {
        blasBuildDesc.Inputs = blasInputs;
        blasBuildDesc.ScratchAccelerationStructureData = scratchBuffer->GetGPUVirtualAddress();
        blasBuildDesc.DestAccelerationStructureData = m_blas->GetGPUVirtualAddress();
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuildDesc = {};
    {
        tlasInputs.InstanceDescs = pBlasInstDesc->GetGPUVirtualAddress();
        tlasBuildDesc.Inputs = tlasInputs;
        tlasBuildDesc.DestAccelerationStructureData = m_tlas->GetGPUVirtualAddress();
        tlasBuildDesc.ScratchAccelerationStructureData = scratchBuffer->GetGPUVirtualAddress();
    }

    // Record command list
    ThrowIfFailed(pD3dCommandList->QueryInterface(IID_PPV_ARGS(&pDxrCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
    pDxrCommandList->BuildRaytracingAccelerationStructure(&blasBuildDesc, 0, nullptr);
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    {
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.UAV.pResource = m_blas;
    }
    pDxrCommandList->ResourceBarrier(1, &uavBarrier);
    pDxrCommandList->BuildRaytracingAccelerationStructure(&tlasBuildDesc, 0, nullptr);
    ThrowIfFailed(pDxrCommandList->Close(), L"Couldn't close the command list.\n");

    pCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&pDxrCommandList);

    // Wait for GPU Idle
    // If hEvent is a null handle, then this API will not return until the specified fence value(s) have been reached.
    pCommandQueue->Signal(pFence, 1);
    pFence->SetEventOnCompletion(1, nullptr);

    pD3dCommandList->Release();
    pFence->Release();
    pCommandAllocator->Release();

    scratchBuffer->Release();
    pBlasInstDesc->Release();
}

void HWRTRenderBackend::BuildShaderTables() // Build Shader Tables
{
    // Get shader identifiers
    UINT shaderIdentifierSize;
    void* rayGenShaderIdentifier;
    void* missShaderIdentifier;
    void* hitGroupShaderIdentifier;
    {
        ID3D12StateObjectProperties* pRtPipelineStateProperties = nullptr;
        m_rtPipelineStateObject->QueryInterface(IID_PPV_ARGS(&pRtPipelineStateProperties));
        rayGenShaderIdentifier = pRtPipelineStateProperties->GetShaderIdentifier(c_raygenShaderName);
        missShaderIdentifier = pRtPipelineStateProperties->GetShaderIdentifier(c_missShaderName);
        hitGroupShaderIdentifier = pRtPipelineStateProperties->GetShaderIdentifier(c_hitGroupName);
        shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    }

    // RayGen Shader Table
    {
        struct RayGenShaderTable
        {
            void* rayGenShaderID;
            RayGenConstantBuffer cb;
        };
        RayGenShaderTable rayGenShaderTable{rayGenShaderIdentifier, m_rayGenCB};
        uint64_t shaderRecordSize = Align(sizeof(RayGenShaderTable), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

        AllocateUploadBuffer(m_pD3dDevice,
                             shaderRecordSize,
                             &m_rayGenShaderTable,
                             L"RayGenShaderTable");

        void *pMappedData;
        m_rayGenShaderTable->Map(0, nullptr, &pMappedData);
        memcpy(pMappedData, &rayGenShaderTable, sizeof(RayGenShaderTable));
        m_rayGenShaderTable->Unmap(0, nullptr);
    }

    // Miss Shader Table
    {
        struct MissShaderTable
        {
            void* missShaderID;
        };
        MissShaderTable missShaderTable{missShaderIdentifier};
        uint64_t shaderRecordSize = Align(sizeof(MissShaderTable), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

        AllocateUploadBuffer(m_pD3dDevice,
                             shaderRecordSize,
                             &m_missShaderTable,
                             L"MissShaderTable");

        void *pMappedData;
        m_missShaderTable->Map(0, nullptr, &pMappedData);
        memcpy(pMappedData, &missShaderTable, sizeof(MissShaderTable));
        m_missShaderTable->Unmap(0, nullptr);
    }

    // Hit group shader table
    {
        struct HitGroupShaderTable
        {
            void* hitGroupShaderID;
        };
        HitGroupShaderTable hitGroupShaderTable{hitGroupShaderIdentifier};
        uint64_t shaderRecordSize = Align(sizeof(HitGroupShaderTable), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

        AllocateUploadBuffer(m_pD3dDevice,
                             shaderRecordSize,
                             &m_hitGroupShaderTable,
                             L"HitGroupShaderTable");

        void *pMappedData;
        m_hitGroupShaderTable->Map(0, nullptr, &pMappedData);
        memcpy(pMappedData, &hitGroupShaderTable, sizeof(HitGroupShaderTable));
        m_hitGroupShaderTable->Unmap(0, nullptr);
    }
}

void HWRTRenderBackend::BuildRaytracingOutput() // Build Raytracing Output
{
    uint32_t winWidth, winHeight;
    m_pUIManager->GetWindowSize(winWidth, winHeight);
    UpdateForSizeChange(winWidth, winHeight);

    D3D12_RESOURCE_DESC tex2DDesc = {};
    {
        tex2DDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        tex2DDesc.Alignment = 0;
        tex2DDesc.Width = winWidth;
        tex2DDesc.Height = winHeight;
        tex2DDesc.DepthOrArraySize = 1;
        tex2DDesc.MipLevels = 1;
        tex2DDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        tex2DDesc.SampleDesc.Count = 1;
        tex2DDesc.SampleDesc.Quality = 0;
        tex2DDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        tex2DDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    D3D12_HEAP_PROPERTIES defaultHeapProperties = {};
    {
        defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        defaultHeapProperties.CreationNodeMask = 1;
        defaultHeapProperties.VisibleNodeMask = 1;
    }

    ThrowIfFailed(m_pD3dDevice->CreateCommittedResource(
                                &defaultHeapProperties,
                                D3D12_HEAP_FLAG_NONE, 
                                &tex2DDesc,
                                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                nullptr, IID_PPV_ARGS(&m_raytracingOutput)));
    NAME_D3D12_OBJECT(m_raytracingOutput);

    D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    {
        uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;
        uavDesc.Texture2D.PlaneSlice = 0;
    }

    m_pD3dDevice->CreateUnorderedAccessView(m_raytracingOutput, nullptr, &uavDesc, uavDescriptorHandle);
    m_raytracingOutputResourceUAVGpuDescriptor = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
}
*/
void HWRTRenderBackend::RenderTick(ID3D12GraphicsCommandList* pCommandList, RenderTargetInfo rtInfo)
{
    /*
    BuildAccelerationStructures();

    ID3D12GraphicsCommandList4* pDxrCommandList = nullptr;
    ThrowIfFailed(pCommandList->QueryInterface(IID_PPV_ARGS(&pDxrCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");

    pCommandList->SetComputeRootSignature(m_raytracingGlobalRootSignature);
    pCommandList->SetDescriptorHeaps(1, &m_descriptorHeap);
    pCommandList->SetComputeRootDescriptorTable(0, m_raytracingOutputResourceUAVGpuDescriptor);
    pCommandList->SetComputeRootShaderResourceView(1, m_tlas->GetGPUVirtualAddress());
    pDxrCommandList->SetPipelineState1(m_rtPipelineStateObject);

    uint32_t winWidth, winHeight;
    m_pUIManager->GetWindowSize(winWidth, winHeight);
    UpdateForSizeChange(winWidth, winHeight);

    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    {
        dispatchDesc.HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
        dispatchDesc.HitGroupTable.SizeInBytes = m_hitGroupShaderTable->GetDesc().Width;
        dispatchDesc.HitGroupTable.StrideInBytes = dispatchDesc.HitGroupTable.SizeInBytes;
        dispatchDesc.MissShaderTable.StartAddress = m_missShaderTable->GetGPUVirtualAddress();
        dispatchDesc.MissShaderTable.SizeInBytes = m_missShaderTable->GetDesc().Width;
        dispatchDesc.MissShaderTable.StrideInBytes = dispatchDesc.MissShaderTable.SizeInBytes;
        dispatchDesc.RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
        dispatchDesc.RayGenerationShaderRecord.SizeInBytes = m_rayGenShaderTable->GetDesc().Width;
        dispatchDesc.Width = winWidth;
        dispatchDesc.Height = winHeight;
        dispatchDesc.Depth = 1;
    }
    pDxrCommandList->DispatchRays(&dispatchDesc);
    */
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

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D};
    m_pD3dDevice->CreateUnorderedAccessView(
        m_renderTarget, nullptr, &uavDesc,
        m_uavHeap->GetCPUDescriptorHandleForHeapStart());

    /*
    uint32_t winWidth, winHeight;
    m_pUIManager->GetWindowSize(winWidth, winHeight);
    UpdateForSizeChange(winWidth, winHeight);

    UpdateForSizeChange(winWidth, winHeight);
    */
}

/*
void HWRTRenderBackend::UpdateForSizeChange(UINT width, UINT height)
{
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    float border = 0.1f;
    if (width <= height)
    {
        m_rayGenCB.stencil =
        {
            -1 + border, -1 + border * aspectRatio,
            1.0f - border, 1 - border * aspectRatio
        };
    }
    else
    {
        m_rayGenCB.stencil =
        {
            -1 + border / aspectRatio, -1 + border,
             1 - border / aspectRatio, 1.0f - border
        };
    }
}
*/