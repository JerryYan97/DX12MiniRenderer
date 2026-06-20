#pragma once
#include <d3d12.h>
#include <stdexcept>
#include <debugapi.h>
#include <sstream>
#include <iomanip>

#define max(a,b) (((a) > (b)) ? (a) : (b))

enum DX12_GPU_CPU_ACCESS_ENUM {
    UPLOAD = 0, // CPU TO GPU
    READBACK = 1, // GPU TO CPU
    GPU_ONLY = 2 // Default
};

#define GPU_BUFFER_SIZE_ALIGNMENT 256

#define ALIGNED_GPU_BUFFER_SIZE(size) (((size) + GPU_BUFFER_SIZE_ALIGNMENT - 1) & ~(GPU_BUFFER_SIZE_ALIGNMENT - 1))
#define ALIGN_UP_256(size) (((size) + 255) & ~255)

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

// Assign a name to the object to aid with debugging.
#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
    pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
    WCHAR fullName[50];
    if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
    {
        pObject->SetName(fullName);
    }
}
#else
inline void SetName(ID3D12Object*, LPCWSTR)
{
}
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}
#endif

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)
#define NAME_D3D12_OBJECT(x) SetName(x, L#x)

class HrException : public std::runtime_error
{
    inline std::string HrToString(HRESULT hr)
    {
        char s_str[64] = {};
        sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
        return std::string(s_str);
    }
public:
    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
    HRESULT Error() const { return m_hr; }
private:
    const HRESULT m_hr;
};

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw HrException(hr);
    }
}

inline void ThrowIfFailed(HRESULT hr, const wchar_t* msg)
{
    if (FAILED(hr))
    {
        OutputDebugStringW(msg);
        throw HrException(hr);
    }
}

// Pretty-print a state object tree.
inline void PrintStateObjectDesc(const D3D12_STATE_OBJECT_DESC* desc)
{
    std::wstringstream wstr;
    wstr << L"\n";
    wstr << L"--------------------------------------------------------------------\n";
    wstr << L"| D3D12 State Object 0x" << static_cast<const void*>(desc) << L": ";
    if (desc->Type == D3D12_STATE_OBJECT_TYPE_COLLECTION) wstr << L"Collection\n";
    if (desc->Type == D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE) wstr << L"Raytracing Pipeline\n";

    auto ExportTree = [](UINT depth, UINT numExports, const D3D12_EXPORT_DESC* exports)
    {
        std::wostringstream woss;
        for (UINT i = 0; i < numExports; i++)
        {
            woss << L"|";
            if (depth > 0)
            {
                for (UINT j = 0; j < 2 * depth - 1; j++) woss << L" ";
            }
            woss << L" [" << i << L"]: ";
            if (exports[i].ExportToRename) woss << exports[i].ExportToRename << L" --> ";
            woss << exports[i].Name << L"\n";
        }
        return woss.str();
    };

    for (UINT i = 0; i < desc->NumSubobjects; i++)
    {
        wstr << L"| [" << i << L"]: ";
        switch (desc->pSubobjects[i].Type)
        {
        case D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE:
            wstr << L"Global Root Signature 0x" << desc->pSubobjects[i].pDesc << L"\n";
            break;
        case D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE:
            wstr << L"Local Root Signature 0x" << desc->pSubobjects[i].pDesc << L"\n";
            break;
        case D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK:
            wstr << L"Node Mask: 0x" << std::hex << std::setfill(L'0') << std::setw(8) << *static_cast<const UINT*>(desc->pSubobjects[i].pDesc) << std::setw(0) << std::dec << L"\n";
            break;
        case D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY:
        {
            wstr << L"DXIL Library 0x";
            auto lib = static_cast<const D3D12_DXIL_LIBRARY_DESC*>(desc->pSubobjects[i].pDesc);
            wstr << lib->DXILLibrary.pShaderBytecode << L", " << lib->DXILLibrary.BytecodeLength << L" bytes\n";
            wstr << ExportTree(1, lib->NumExports, lib->pExports);
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION:
        {
            wstr << L"Existing Library 0x";
            auto collection = static_cast<const D3D12_EXISTING_COLLECTION_DESC*>(desc->pSubobjects[i].pDesc);
            wstr << collection->pExistingCollection << L"\n";
            wstr << ExportTree(1, collection->NumExports, collection->pExports);
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
        {
            wstr << L"Subobject to Exports Association (Subobject [";
            auto association = static_cast<const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(desc->pSubobjects[i].pDesc);
            UINT index = static_cast<UINT>(association->pSubobjectToAssociate - desc->pSubobjects);
            wstr << index << L"])\n";
            for (UINT j = 0; j < association->NumExports; j++)
            {
                wstr << L"|  [" << j << L"]: " << association->pExports[j] << L"\n";
            }
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
        {
            wstr << L"DXIL Subobjects to Exports Association (";
            auto association = static_cast<const D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(desc->pSubobjects[i].pDesc);
            wstr << association->SubobjectToAssociate << L")\n";
            for (UINT j = 0; j < association->NumExports; j++)
            {
                wstr << L"|  [" << j << L"]: " << association->pExports[j] << L"\n";
            }
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG:
        {
            wstr << L"Raytracing Shader Config\n";
            auto config = static_cast<const D3D12_RAYTRACING_SHADER_CONFIG*>(desc->pSubobjects[i].pDesc);
            wstr << L"|  [0]: Max Payload Size: " << config->MaxPayloadSizeInBytes << L" bytes\n";
            wstr << L"|  [1]: Max Attribute Size: " << config->MaxAttributeSizeInBytes << L" bytes\n";
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG:
        {
            wstr << L"Raytracing Pipeline Config\n";
            auto config = static_cast<const D3D12_RAYTRACING_PIPELINE_CONFIG*>(desc->pSubobjects[i].pDesc);
            wstr << L"|  [0]: Max Recursion Depth: " << config->MaxTraceRecursionDepth << L"\n";
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP:
        {
            wstr << L"Hit Group (";
            auto hitGroup = static_cast<const D3D12_HIT_GROUP_DESC*>(desc->pSubobjects[i].pDesc);
            wstr << (hitGroup->HitGroupExport ? hitGroup->HitGroupExport : L"[none]") << L")\n";
            wstr << L"|  [0]: Any Hit Import: " << (hitGroup->AnyHitShaderImport ? hitGroup->AnyHitShaderImport : L"[none]") << L"\n";
            wstr << L"|  [1]: Closest Hit Import: " << (hitGroup->ClosestHitShaderImport ? hitGroup->ClosestHitShaderImport : L"[none]") << L"\n";
            wstr << L"|  [2]: Intersection Import: " << (hitGroup->IntersectionShaderImport ? hitGroup->IntersectionShaderImport : L"[none]") << L"\n";
            break;
        }
        }
        wstr << L"|--------------------------------------------------------------------\n";
    }
    wstr << L"\n";
    OutputDebugStringW(wstr.str().c_str());
}

inline void AllocateUploadBuffer(ID3D12Device5* pDevice, void *pData, UINT64 datasize, ID3D12Resource **ppResource, const wchar_t* resourceName = nullptr)
{
    // NOTE: Constant buffer needs to be padded to 256 bytes.
    constexpr UINT64 CnstBufferMinSize = sizeof(float) * 64;

    D3D12_HEAP_PROPERTIES uploadHeapProperties = D3D12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_UPLOAD,
                                                                        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                                                        D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

    DXGI_SAMPLE_DESC bufferSampleDesc = DXGI_SAMPLE_DESC{ 1, 0 };
    D3D12_RESOURCE_DESC bufferDesc = D3D12_RESOURCE_DESC{D3D12_RESOURCE_DIMENSION_BUFFER,
                                                         0, max(datasize, CnstBufferMinSize), 1, 1, 1,
                                                         DXGI_FORMAT_UNKNOWN, bufferSampleDesc,
                                                         D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE};
    
    ThrowIfFailed(pDevice->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(ppResource)));
    if (resourceName)
    {
        (*ppResource)->SetName(resourceName);
    }
    void *pMappedData;
    (*ppResource)->Map(0, nullptr, &pMappedData);
    memcpy(pMappedData, pData, datasize);
    (*ppResource)->Unmap(0, nullptr);
}

inline void AllocateUploadBuffer(ID3D12Device5* pDevice, UINT64 buffersize, ID3D12Resource **ppResource, const wchar_t* resourceName = nullptr)
{
    // NOTE: Constant buffer needs to be padded to 256 bytes.
    constexpr uint32_t CnstBufferMinSize = sizeof(float) * 64;

    D3D12_HEAP_PROPERTIES uploadHeapProperties = D3D12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_UPLOAD,
                                                                        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                                                        D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

    DXGI_SAMPLE_DESC bufferSampleDesc = DXGI_SAMPLE_DESC{ 1, 0 };
    D3D12_RESOURCE_DESC bufferDesc = D3D12_RESOURCE_DESC{D3D12_RESOURCE_DIMENSION_BUFFER,
                                                         0, max( CnstBufferMinSize, buffersize), 1, 1, 1,
                                                         DXGI_FORMAT_UNKNOWN, bufferSampleDesc,
                                                         D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE};
    
    ThrowIfFailed(pDevice->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(ppResource)));
    if (resourceName)
    {
        (*ppResource)->SetName(resourceName);
    }
}

inline void AllocateUAVBuffer(ID3D12Device5* pDevice, UINT64 bufferSize, ID3D12Resource **ppResource, D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON, const wchar_t* resourceName = nullptr)
{
    D3D12_HEAP_PROPERTIES defaultHeapProperties = D3D12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT,
                                                                         D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                                                         D3D12_MEMORY_POOL_UNKNOWN, 1, 1  };

    DXGI_SAMPLE_DESC bufferSampleDesc = DXGI_SAMPLE_DESC{ 1, 0 };
    D3D12_RESOURCE_DESC bufferDesc = D3D12_RESOURCE_DESC{D3D12_RESOURCE_DIMENSION_BUFFER,
                                                         0, bufferSize, 1, 1, 1,
                                                         DXGI_FORMAT_UNKNOWN, bufferSampleDesc,
                                                         D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};

    ThrowIfFailed(pDevice->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        initialResourceState,
        nullptr,
        IID_PPV_ARGS(ppResource)));
    if (resourceName)
    {
        (*ppResource)->SetName(resourceName);
    }
}

inline void ThrowIfFalse(bool value)
{
    ThrowIfFailed(value ? S_OK : E_FAIL);
}

inline UINT Align(UINT size, UINT alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

ID3D12Resource* CreateUploadBufferAndInit(ID3D12Device5* pDevice, uint32_t sizeBytes, void* pSrcData);
void SendDataToGPUBuffer(ID3D12Device5* pDevice, ID3D12Resource* pDstBuffer, void* pSrcData, uint32_t dataSizeBytes);
void SendDataToUploadBuffer(ID3D12Resource* pUploadBuffer, void* pSrcData, uint32_t dataSizeBytes, uint32_t dstOffsetBytes = 0);

inline void GpuQueueWaitIdle(ID3D12Device5* pDevice, ID3D12CommandQueue* pCmdQueue)
{
    ID3D12Fence* tmpCmdQueuefence = nullptr;
    pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&tmpCmdQueuefence));

    pCmdQueue->Signal(tmpCmdQueuefence, 1);

    // If hEvent is a null handle, then this API will not return until the specified fence value(s) have been reached.
    tmpCmdQueuefence->SetEventOnCompletion(1, nullptr);

    tmpCmdQueuefence->Release();
}

void SendDataToTexture2D(ID3D12Device5* pDevice, ID3D12Resource* pDstTexture, void* pSrcData, uint32_t dataSizeBytes);
void SendDataToCubemapSlice(ID3D12Device5* pDevice, ID3D12Resource* pDstTexture, void* pSrcData, uint32_t dataSizeBytes, uint32_t layerIndex, uint32_t mipIndex);
void ChangeResourceState(ID3D12Device5* pDevice, ID3D12Resource* pResource, D3D12_RESOURCE_STATES curState, D3D12_RESOURCE_STATES destState); // Block the thread until the resource is in the desired state.
ID3D12Resource* CreateUploadBuffer(ID3D12Device5* pDevice, uint32_t sizeBytes);
ID3D12Resource* CreateGPUBuffer(ID3D12Device5* pDevice, uint32_t sizeBytes, D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON);
void CopyADescriptor(ID3D12Device5* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE dstHandle, uint32_t dstId, D3D12_CPU_DESCRIPTOR_HANDLE srcHandle, uint32_t srcId, D3D12_DESCRIPTOR_HEAP_TYPE heapType);

D3D12_RESOURCE_BARRIER TransitionStateBarrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES curState, D3D12_RESOURCE_STATES destState);
D3D12_RESOURCE_BARRIER UAVBarrier(ID3D12Resource* pResource);
ID3D12Resource* AllocateGpuBuffer(ID3D12Device5* pDevice, uint32_t sizeBytes, DX12_GPU_CPU_ACCESS_ENUM accessType, D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON);
inline int SubresourceIdx(uint32_t mipLevel, uint32_t arrayLayer, uint32_t mipLevelsPerLayer) { return mipLevel + arrayLayer * mipLevelsPerLayer; }

inline D3D12_STATIC_SAMPLER_DESC StaticSampler(uint32_t regIdx, D3D12_TEXTURE_ADDRESS_MODE addressMode)
{
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    // sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = addressMode;
    sampler.AddressV = addressMode;
    sampler.AddressW = addressMode;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = regIdx;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    return sampler;
}

inline D3D12_STATIC_SAMPLER_DESC StaticBorderSampler(uint32_t regIdx)
{
    return StaticSampler(regIdx, D3D12_TEXTURE_ADDRESS_MODE_BORDER);
}

inline D3D12_STATIC_SAMPLER_DESC StaticWrapSampler(uint32_t regIdx)
{
    return StaticSampler(regIdx, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
}

ID3D12Resource* MakeAccelerationStructure(ID3D12Device5* pDevice, const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs, UINT64* updateScratchSize);

// Pipeline descriptions
// D3D12_GRAPHICS_PIPELINE_STATE_DESC CreateVsPsPipelineDesc();

//