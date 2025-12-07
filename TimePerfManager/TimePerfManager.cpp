#include "TimePerfManager.h"

TimePerfManager* TimePerfManager::m_pThis = nullptr;

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

void TimePerfManager::AddGPUTimeStamp() {

}

void TimePerfManager::GPUTimeStampStart() {
    // m_gpuTimeStampStart = …;
}

void TimePerfManager::GPUTimeStampEnd() {
    // m_gpuTimeStampEnd = …;
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