#include "DX12Utils.h"
#include <cassert>

class RAIIGPUQueueAndAllocator
{
public:
    RAIIGPUQueueAndAllocator(ID3D12Device* pDevice)
    {
        {
            D3D12_COMMAND_QUEUE_DESC desc = {};
            desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            desc.NodeMask = 1;
            pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_pTempCmdQueue));
            m_pTempCmdQueue->SetName(L"Temp Command Queue");
        }

        HRESULT hr = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pTempCmdAllocator));
        ThrowIfFailed(hr);
        if (!m_pTempCmdAllocator) {
            // Handle error appropriately, e.g., throw or return
            throw std::runtime_error("Failed to create command allocator.");
        }
        m_pTempCmdAllocator->SetName(L"Upload Command Allocator");

        hr = pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pTempCmdAllocator, nullptr, IID_PPV_ARGS(&m_pTempCmdList));
        ThrowIfFailed(hr);
        m_pTempCmdList->SetName(L"Upload Command List");
    }

    ~RAIIGPUQueueAndAllocator()
    {
        if (m_pTempCmdList) m_pTempCmdList->Release();
        if (m_pTempCmdAllocator) m_pTempCmdAllocator->Release();
        if (m_pTempCmdQueue) m_pTempCmdQueue->Release();
    }

    ID3D12CommandQueue*        m_pTempCmdQueue = nullptr;
    ID3D12CommandAllocator*    m_pTempCmdAllocator = nullptr;
    ID3D12GraphicsCommandList* m_pTempCmdList = nullptr;
};

static void WaitGpuIdle(ID3D12Device* pDevice, ID3D12CommandQueue* pQueue)
{
    ID3D12Fence* tmpCmdQueuefence = nullptr;
    pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&tmpCmdQueuefence));

    pQueue->Signal(tmpCmdQueuefence, 1);

    // If hEvent is a null handle, then this API will not return until the specified fence value(s) have been reached.
    tmpCmdQueuefence->SetEventOnCompletion(1, nullptr);

    tmpCmdQueuefence->Release();
}

void SendDataToUploadBuffer(ID3D12Resource* pUploadBuffer, void* pSrcData, uint32_t dataSizeBytes, uint32_t dstOffsetBytes)
{
    void* pConstBufferBegin;
    D3D12_RANGE readRange{ dstOffsetBytes, 0 };
    ThrowIfFailed(pUploadBuffer->Map(0, &readRange, &pConstBufferBegin));
    memcpy(pConstBufferBegin, pSrcData, dataSizeBytes);
    pUploadBuffer->Unmap(0, nullptr);
}

void SendDataToGPUBuffer(ID3D12Device* pDevice, ID3D12Resource* pDstBuffer, void* pSrcData, uint32_t dataSizeBytes)
{
    RAIIGPUQueueAndAllocator raiiQueueAndAllocator(pDevice);

    ID3D12Resource* pUploadBuffer = CreateUploadBufferAndInit(pDevice, dataSizeBytes, pSrcData);

    raiiQueueAndAllocator.m_pTempCmdList->CopyBufferRegion(pDstBuffer, 0, pUploadBuffer, 0, dataSizeBytes);
    raiiQueueAndAllocator.m_pTempCmdList->Close();
    raiiQueueAndAllocator.m_pTempCmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&raiiQueueAndAllocator.m_pTempCmdList);

    WaitGpuIdle(pDevice, raiiQueueAndAllocator.m_pTempCmdQueue);

    pUploadBuffer->Release();
}

void CopyADescriptor(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE dstHandle, uint32_t dstId, D3D12_CPU_DESCRIPTOR_HANDLE srcHandle, uint32_t srcId, D3D12_DESCRIPTOR_HEAP_TYPE heapType)
{
    UINT descriptorSize = pDevice->GetDescriptorHandleIncrementSize(heapType);
    D3D12_CPU_DESCRIPTOR_HANDLE dstHandleWithOffset = { dstHandle.ptr + dstId * descriptorSize };
    D3D12_CPU_DESCRIPTOR_HANDLE srcHandleWithOffset = { srcHandle.ptr + srcId * descriptorSize };
    pDevice->CopyDescriptorsSimple(1, dstHandleWithOffset, srcHandleWithOffset, heapType);
}

ID3D12Resource* CreateGPUBuffer(ID3D12Device* pDevice, uint32_t sizeBytes, D3D12_RESOURCE_STATES initialResourceState)
{
    sizeBytes = max(GPU_BUFFER_SIZE_ALIGNMENT, sizeBytes);

    ID3D12Resource* pBuffer;
    D3D12_HEAP_PROPERTIES heapProperties{};
    {
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;
    }
    D3D12_RESOURCE_DESC bufferRsrcDesc{};
    {
        bufferRsrcDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferRsrcDesc.Alignment = 0;
        bufferRsrcDesc.Width = sizeBytes;
        bufferRsrcDesc.Height = 1;
        bufferRsrcDesc.DepthOrArraySize = 1;
        bufferRsrcDesc.MipLevels = 1;
        bufferRsrcDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferRsrcDesc.SampleDesc.Count = 1;
        bufferRsrcDesc.SampleDesc.Quality = 0;
        bufferRsrcDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferRsrcDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    }
    ThrowIfFailed(pDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferRsrcDesc,
            initialResourceState,
            nullptr,
            IID_PPV_ARGS(&pBuffer)));
    return pBuffer;
}

ID3D12Resource* CreateUploadBuffer(ID3D12Device* pDevice, uint32_t sizeBytes)
{
    ID3D12Resource* pUploadBuffer;
    // NOTE: Constant buffer needs to be padded to 256 bytes.
    assert(sizeBytes % 256 == 0);
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
        bufferRsrcDesc.Width = sizeBytes;
        bufferRsrcDesc.Height = 1;
        bufferRsrcDesc.DepthOrArraySize = 1;
        bufferRsrcDesc.MipLevels = 1;
        bufferRsrcDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferRsrcDesc.SampleDesc.Count = 1;
        bufferRsrcDesc.SampleDesc.Quality = 0;
        bufferRsrcDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferRsrcDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    }
    ThrowIfFailed(pDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferRsrcDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&pUploadBuffer)));
    return pUploadBuffer;
}

void ChangeResourceState(ID3D12Device* pDevice, ID3D12Resource* pResource, D3D12_RESOURCE_STATES curState, D3D12_RESOURCE_STATES destState)
{
    RAIIGPUQueueAndAllocator raiiQueueAndAllocator(pDevice);

    // Transfer the texture format from the copy dst to shader rsrc.
    D3D12_RESOURCE_BARRIER barrier = {};
    {
        // Color Render Target
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = pResource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = curState;
        barrier.Transition.StateAfter  = destState;
    }
    raiiQueueAndAllocator.m_pTempCmdList->ResourceBarrier(1, &barrier);
    raiiQueueAndAllocator.m_pTempCmdList->Close();

    raiiQueueAndAllocator.m_pTempCmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&raiiQueueAndAllocator.m_pTempCmdList);

    WaitGpuIdle(pDevice, raiiQueueAndAllocator.m_pTempCmdQueue);
}

ID3D12Resource* CreateUploadBufferAndInit(ID3D12Device* pDevice, uint32_t sizeBytes, void* pSrcData)
{
    ID3D12Resource* pUploadBuffer;
    // NOTE: Constant buffer needs to be padded to 256 bytes.
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
        bufferRsrcDesc.Width = sizeBytes;
        bufferRsrcDesc.Height = 1;
        bufferRsrcDesc.DepthOrArraySize = 1;
        bufferRsrcDesc.MipLevels = 1;
        bufferRsrcDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferRsrcDesc.SampleDesc.Count = 1;
        bufferRsrcDesc.SampleDesc.Quality = 0;
        bufferRsrcDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferRsrcDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    }

    ThrowIfFailed(pDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferRsrcDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&pUploadBuffer)));

    void* pConstBufferBegin;
    D3D12_RANGE readRange{ 0, 0 };
    ThrowIfFailed(pUploadBuffer->Map(0, &readRange, &pConstBufferBegin));
    memcpy(pConstBufferBegin, pSrcData, sizeBytes);
    pUploadBuffer->Unmap(0, nullptr);

    return pUploadBuffer;
}

D3D12_RESOURCE_BARRIER TransitionStateBarrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES curState, D3D12_RESOURCE_STATES destState)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    {
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = pResource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = curState;
        barrier.Transition.StateAfter = destState;
    }
    return barrier;
}

D3D12_RESOURCE_BARRIER UAVBarrier(ID3D12Resource* pResource)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    {
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.UAV.pResource = pResource;
    }
    return barrier;
}

ID3D12Resource* AllocateGpuBuffer(ID3D12Device* pDevice, uint32_t sizeBytes, DX12_GPU_CPU_ACCESS_ENUM accessType, D3D12_RESOURCE_STATES initialResourceState)
{
    assert(accessType == GPU_ONLY || accessType == READBACK, "Please use CreateUploadBufferAndInit to create UPLOAD buffers.");
    if (accessType == READBACK) { initialResourceState = D3D12_RESOURCE_STATE_COPY_DEST; } // READBACK buffers must be in COPY DEST state.

    ID3D12Resource* pBuffer;
    D3D12_HEAP_PROPERTIES heapProperties{};
    {
        if (accessType == GPU_ONLY)
        {
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        }
        else if (accessType == READBACK)
        {
            heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
        }
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;
    }

    D3D12_RESOURCE_DESC bufferRsrcDesc{};
    {
        bufferRsrcDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferRsrcDesc.Alignment = 0;
        bufferRsrcDesc.Width = sizeBytes;
        bufferRsrcDesc.Height = 1;
        bufferRsrcDesc.DepthOrArraySize = 1;
        bufferRsrcDesc.MipLevels = 1;
        bufferRsrcDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferRsrcDesc.SampleDesc.Count = 1;
        bufferRsrcDesc.SampleDesc.Quality = 0;
        bufferRsrcDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferRsrcDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    }

    ThrowIfFailed(pDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferRsrcDesc,
            initialResourceState,
            nullptr,
        IID_PPV_ARGS(&pBuffer)));

    return pBuffer;
}

// Assume the input texture is COPY DEST and the texture is PIXEL SHADER RESOURCE after copying.
void SendDataToTexture2D(ID3D12Device* pDevice, ID3D12Resource* pDstTexture, void* pSrcData, uint32_t dataSizeBytes)
{
    RAIIGPUQueueAndAllocator raiiQueueAndAllocator(pDevice);

    const auto Desc = pDstTexture->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    UINT64 rowSizesInBytes;
    UINT numRows;
    UINT64 requiredSize;
    pDevice->GetCopyableFootprints(&Desc, 0, 1, 0, &layout, &numRows, &rowSizesInBytes, &requiredSize);
    assert(requiredSize == dataSizeBytes, "Assume that the size of the texture is the same as the size of the data.");

    ID3D12Resource* pUploadBuffer = CreateUploadBufferAndInit(pDevice, dataSizeBytes, pSrcData);
    pUploadBuffer->SetName(L"Upload Buffer");

    // Copy the data from the upload buffer to the texture.
    
    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    {
        dstLocation.pResource = pDstTexture;
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.SubresourceIndex = 0;
    }
    
    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    {
        srcLocation.pResource = pUploadBuffer;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint = layout;
    }
    
    raiiQueueAndAllocator.m_pTempCmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

    // Transfer the texture format from the copy dst to shader rsrc.
    D3D12_RESOURCE_BARRIER barrier = {};
    {
        // Color Render Target
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = pDstTexture;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
    raiiQueueAndAllocator.m_pTempCmdList->ResourceBarrier(1, &barrier);
    raiiQueueAndAllocator.m_pTempCmdList->Close();

    raiiQueueAndAllocator.m_pTempCmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&raiiQueueAndAllocator.m_pTempCmdList);

    WaitGpuIdle(pDevice, raiiQueueAndAllocator.m_pTempCmdQueue);

    pUploadBuffer->Release();
}

void SendDataToCubemapSlice(ID3D12Device* pDevice, ID3D12Resource* pDstTexture, void* pSrcData, uint32_t dataSizeBytes, uint32_t layerIndex, uint32_t mipIndex)
{
    RAIIGPUQueueAndAllocator raiiQueueAndAllocator(pDevice);

    const D3D12_RESOURCE_DESC DestDesc = pDstTexture->GetDesc();
    // For the hardware to understand how to treat a section of a buffer resource as a multi-dimensional texture.
    D3D12_RESOURCE_DESC SrcDesc = {};
    {
        SrcDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        SrcDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        SrcDesc.Width = DestDesc.Width >> mipIndex;
        SrcDesc.Height = DestDesc.Height >> mipIndex;
        SrcDesc.DepthOrArraySize = 1;
        SrcDesc.MipLevels = 1;
        SrcDesc.Format = DestDesc.Format;
        SrcDesc.SampleDesc.Count = 1;
        SrcDesc.SampleDesc.Quality = 0;
        SrcDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        SrcDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    UINT64 rowSizesInBytes;
    UINT numRows;
    UINT64 requiredSize;
    pDevice->GetCopyableFootprints(&SrcDesc, 0, 1, 0, &layout, &numRows, &rowSizesInBytes, &requiredSize);

    ID3D12Resource* pUploadBuffer = CreateUploadBufferAndInit(pDevice, dataSizeBytes, pSrcData);
    pUploadBuffer->SetName(L"Upload Buffer");

    // Copy the data from the upload buffer to the cubemap texture slice.
    
    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    {
        dstLocation.pResource = pDstTexture;
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.SubresourceIndex = SubresourceIdx(mipIndex, layerIndex, DestDesc.MipLevels);
    }
    
    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    {
        srcLocation.pResource = pUploadBuffer;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint = layout;
    }

    raiiQueueAndAllocator.m_pTempCmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

    // NOTE: We don't transfer the state of the resource here because for a slice, we may want to keep it's state as COPY DEST and then transition the whole texture to shader resource after copying all the slices.

    raiiQueueAndAllocator.m_pTempCmdList->Close();

    raiiQueueAndAllocator.m_pTempCmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&raiiQueueAndAllocator.m_pTempCmdList);

    WaitGpuIdle(pDevice, raiiQueueAndAllocator.m_pTempCmdQueue);

    pUploadBuffer->Release();
}