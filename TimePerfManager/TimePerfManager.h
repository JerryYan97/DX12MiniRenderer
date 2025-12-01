#pragma once
#include <chrono>
// https://learn.microsoft.com/en-us/windows/win32/direct3d12/queries

class TimePerfManager
{
public:
    TimePerfManager() {}
    ~TimePerfManager() {}

    static void Create() {}
    static void Destroy() {}
    static TimePerfManager* GetInstance() { return m_pThis; }

    void NewFrameStart() {}
    void GPUTimeStampStart() {}
    void GPUTimeStampEnd() {}

private:
    static TimePerfManager* m_pThis;
    std::chrono::high_resolution_clock::time_point m_cpuFrameStartTime;
    std::chrono::high_resolution_clock::time_point m_cpuFrameEndTime;
    uint64_t m_gpuTimeStampStart = 0;
    uint64_t m_gpuTimeStampEnd = 0;
};