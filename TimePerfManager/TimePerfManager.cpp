#include "TimePerfManager.h"
#include "../Utils/DX12Utils.h"

TimePerfManager* TimePerfManager::m_pThis = nullptr;

// static int QUERY_FRAME_ID = 0;
static const int GPU_TIME_FRAME_COUNT = 5;

TimePerfManager::TimePerfManager()
{
    m_pThis = this;
}

TimePerfManager::~TimePerfManager()
{
}

void TimePerfManager::AddCPUTime(float deltaTimeSec) {
    m_frameCounter++;
    m_accumulatedCpuTime += deltaTimeSec;
    m_cpuFrameTimes.push(deltaTimeSec);
    while (m_accumulatedCpuTime > 1.f) {
        float frontFrameTime = m_cpuFrameTimes.front();
        m_cpuFrameTimes.pop();
        m_accumulatedCpuTime -= frontFrameTime;
        m_frameCounter--;
    }
}

void TimePerfManager::GPUTimeStampStart(ID3D12GraphicsCommandList* pCmdList) {
    // pCmdList->BeginQuery(m_pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, QUERY_FRAME_ID * 2); Timestamp doesn't have BeginQuery.
    pCmdList->EndQuery(m_pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0);
}

void TimePerfManager::GPUTimeStampEnd(ID3D12GraphicsCommandList* pCmdList) {
    // Insert last timestamp and copy the result to readback buffer.
    pCmdList->EndQuery(m_pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 1);

    D3D12_RESOURCE_BARRIER copySrcToDstBarrier = TransitionStateBarrier(
        m_pQueryResultBuffer,
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_COPY_DEST
    );
    pCmdList->ResourceBarrier(1, &copySrcToDstBarrier);

    pCmdList->ResolveQueryData(
        m_pQueryHeap,
        D3D12_QUERY_TYPE_TIMESTAMP,
        0,
        2,
        m_pQueryResultBuffer,
        0
    );

    D3D12_RESOURCE_BARRIER copyDstToSrcBarrier = TransitionStateBarrier(
        m_pQueryResultBuffer,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_COPY_SOURCE
    );
    pCmdList->ResourceBarrier(1, &copyDstToSrcBarrier);

    pCmdList->CopyBufferRegion(
        m_pQueryReadbackBuffer,
        0,
        m_pQueryResultBuffer,
        0,
        sizeof(uint64_t) * 2
    );
    // QUERY_FRAME_ID = QUERY_FRAME_ID == 0 ? 1 : 0;
}

int TimePerfManager::GetFPS() const
{
    float frameCnt = static_cast<float>(m_frameCounter);
    float fps = frameCnt / m_accumulatedCpuTime;
    return static_cast<int>(fps);
}

float TimePerfManager::GetAverageCPUFrameTime() const // In milliseconds.
{
    float frameCnt = static_cast<float>(m_frameCounter);
    return (m_accumulatedCpuTime / frameCnt) * 1000.0f;
}

float TimePerfManager::GetAverageGPUFrameTime() const // In milliseconds.
{
    // Implement GPU frame time calculation here.
    float avgGpuTime = 0.f;
    int size = static_cast<int>(m_gpuFrameTimes.size());
    if (m_gpuFrameTimes.size() > 0) {
        UINT64 avgGpuTicks = m_totalGPUTicks / static_cast<UINT64>(m_gpuFrameTimes.size());
        avgGpuTime = static_cast<float>(avgGpuTicks * 1000) / static_cast<float>(m_gpuTimeStampFrequency);
    }
    // printf("GPU Frame Count: %d, Avg GPU Time: %.3f ms\n", size, avgGpuTime);
    return avgGpuTime;
}

void TimePerfManager::Init(ID3D12Device* pDevice, ID3D12CommandQueue* pGfxQueue)
{
    m_pD3dDevice = pDevice;
    D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
    queryHeapDesc.Count = 4;
    queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    ThrowIfFailed(m_pD3dDevice->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_pQueryHeap)));

    pGfxQueue->GetTimestampFrequency(&m_gpuTimeStampFrequency);

    // Create result and readback buffers
    m_pQueryResultBuffer = AllocateGpuBuffer(
        m_pD3dDevice,
        sizeof(uint64_t) * 2,
        GPU_ONLY,
        D3D12_RESOURCE_STATE_COPY_SOURCE
    );
    m_pQueryResultBuffer->SetName(L"GPU TimeStamp Result Buffer");

    m_pQueryReadbackBuffer = AllocateGpuBuffer(
        m_pD3dDevice,
        sizeof(uint64_t) * 2,
        READBACK
    );
    m_pQueryReadbackBuffer->SetName(L"GPU TimeStamp Readback Buffer");
}

void TimePerfManager::ReadBackGPUTimeStampResults() // Call after GPU finishes the work.
{
    UINT64 GPUTimeStamps[2];
    void *pMappedData;
    m_pQueryReadbackBuffer->Map(0, nullptr, &pMappedData);
    memcpy(GPUTimeStamps, pMappedData, 2 * sizeof(UINT64));
    m_pQueryReadbackBuffer->Unmap(0, nullptr);

    UINT64 gpuTimeInerval = GPUTimeStamps[1] - GPUTimeStamps[0];
    if (m_gpuFrameTimes.size() < GPU_TIME_FRAME_COUNT) {
        m_gpuFrameTimes.push(gpuTimeInerval);
        m_totalGPUTicks += gpuTimeInerval;
    }
    else {
        UINT64 frontFrameTicks = m_gpuFrameTimes.front();
        m_gpuFrameTimes.pop();
        m_totalGPUTicks -= frontFrameTicks;
        m_gpuFrameTimes.push(gpuTimeInerval);
        m_totalGPUTicks += gpuTimeInerval;
    }

    // Debug
    // float gpuTime = static_cast<float>(gpuTimeInerval * 1000) / static_cast<float>(m_gpuTimeStampFrequency);
    // printf("GPU Time: %.3f ms\n", gpuTime);
}

void TimePerfManager::Finalize()
{
    if (m_pQueryHeap) {
        m_pQueryHeap->Release();
        m_pQueryResultBuffer->Release();
        m_pQueryReadbackBuffer->Release();
        m_pQueryHeap = nullptr;
    }
}