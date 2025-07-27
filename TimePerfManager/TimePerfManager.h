#pragma once

// https://learn.microsoft.com/en-us/windows/win32/direct3d12/queries

class TimePerfManager
{
public:
    TimePerfManager() {}
    ~TimePerfManager() {}

    static void Create();
    static void Destroy();
    static TimePerfManager* GetInstance();

    void NewFrameStart();
    void GPUTimeStampStart();
    void GPUTimeStampEnd();
};