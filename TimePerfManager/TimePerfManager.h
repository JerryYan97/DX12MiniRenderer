#pragma once
#include <chrono>
#include <queue>
#include <d3d12.h>
// https://learn.microsoft.com/en-us/windows/win32/direct3d12/queries

class TimePerfManager
{
public:
    TimePerfManager();
    ~TimePerfManager();

    static TimePerfManager* GetInstance() { return m_pThis; }
    void Init(ID3D12Device* pDevice);
    void Finalize();

    void AddCPUTime(float deltaTimeSec); // In seconds. The delta time comes from the main loop, which drives the whole system. Thus, we don't need to query the time again.
    void GPUTimeStampStart(ID3D12GraphicsCommandList* pCmdList);
    void GPUTimeStampEnd(ID3D12GraphicsCommandList* pCmdList);
    void ResolveQuery();

    int GetFPS() const;
    float GetAverageCPUFrameTime() const; // In milliseconds.
    float GetAverageGPUFrameTime() const; // In milliseconds.

private:
    static TimePerfManager* m_pThis;
    ID3D12Device* m_pD3dDevice = nullptr;
    ID3D12QueryHeap* m_pQueryHeap = nullptr;

    std::queue<float> m_cpuFrameTimes; // In seconds.

    uint64_t m_gpuTimeStampStart = 0;
    uint64_t m_gpuTimeStampEnd = 0;

    uint32_t m_frameCounter = 0;
    float    m_accumulatedCpuTime = 0.0f;
};