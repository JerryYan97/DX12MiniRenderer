#include "DX12Utils.h"
#include <cassert>

static void WaitGpuIdle(ID3D12Device* pDevice, ID3D12CommandQueue* pQueue)
{
    ID3D12Fence* tmpCmdQueuefence = nullptr;
    pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&tmpCmdQueuefence));

    pQueue->Signal(tmpCmdQueuefence, 1);

    // If hEvent is a null handle, then this API will not return until the specified fence value(s) have been reached.
    tmpCmdQueuefence->SetEventOnCompletion(1, nullptr);

    tmpCmdQueuefence->Release();
}

// TODO: Send data to upload buffer and then copy to the destination buffer.
void SendDataToBuffer(ID3D12Device* pDevice, ID3D12Resource* pDstBuffer, void* pSrcData, uint32_t dataSizeBytes)
{

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

// Assume the input texture is COPY DEST and the texture is PIXEL SHADER RESOURCE after copying.
void SendDataToTexture2D(ID3D12Device* pDevice, ID3D12Resource* pDstTexture, void* pSrcData, uint32_t dataSizeBytes)
{
    ID3D12CommandQueue* pUploadCmdQueue;
    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&pUploadCmdQueue));
        pUploadCmdQueue->SetName(L"Upload Command Queue");
    }

    ID3D12CommandAllocator* pUploadCmdAllocator;
    pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pUploadCmdAllocator));
    pUploadCmdAllocator->SetName(L"Upload Command Allocator");
    
    ID3D12GraphicsCommandList* pUploadCmdList;
    pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pUploadCmdAllocator, nullptr, IID_PPV_ARGS(&pUploadCmdList));
    pUploadCmdList->SetName(L"Upload Command List");

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
    
    pUploadCmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

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
    pUploadCmdList->ResourceBarrier(1, &barrier);
    pUploadCmdList->Close();

    pUploadCmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&pUploadCmdList);

    // pUploadCmdList->Close();
    WaitGpuIdle(pDevice, pUploadCmdQueue);
    
    // pUploadCmdList->Reset(pUploadCmdAllocator, nullptr);

    pUploadBuffer->Release();
    pUploadCmdList->Release();
    
    pUploadCmdAllocator->Release();
    pUploadCmdQueue->Release();

}