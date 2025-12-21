#include "TimePerfManager.h"
#include "../Utils/DX12Utils.h"

TimePerfManager* TimePerfManager::m_pThis = nullptr;

static int QUERY_FRAME_ID = 0;

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
    // pCmdList->BeginQuery(m_pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, QUERY_FRAME_ID * 2);
    pCmdList->EndQuery(m_pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, QUERY_FRAME_ID * 2);
}

void TimePerfManager::GPUTimeStampEnd(ID3D12GraphicsCommandList* pCmdList) {
    pCmdList->EndQuery(m_pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, QUERY_FRAME_ID * 2 + 1);

    QUERY_FRAME_ID = QUERY_FRAME_ID == 0 ? 1 : 0;
}

void TimePerfManager::ResolveQuery()
{
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
    return 0.f;
}

void TimePerfManager::Init(ID3D12Device* pDevice)
{
    m_pD3dDevice = pDevice;
    D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
    queryHeapDesc.Count = 4;
    queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    ThrowIfFailed(m_pD3dDevice->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_pQueryHeap)));
}

void TimePerfManager::Finalize()
{
    if (m_pQueryHeap) {
        m_pQueryHeap->Release();
        m_pQueryHeap = nullptr;
    }
}