#pragma once

#include <d3d12.h>
#include "EventSystem/EventManager.h"

class UIManager;

class DX12MiniRenderer
{
public:
    DX12MiniRenderer();
    ~DX12MiniRenderer();

    /*
    * Create UIManager.
    * Create DX12 Device.
    */
    void Init();

    /*
    * The main loop of the application.
    */
    void Run();

    /*
    * Free all resources.
    */
    void Finalize();

private:
    void InitDevice();
    static void WaitGpuIdle();

    ID3D12Device*  m_pD3dDevice = nullptr;
    ID3D12Debug*   m_pDx12Debug = nullptr;
    UIManager*     m_pUIManager = nullptr;
    HEventManager  m_eventManager;

    DX12MiniRenderer* m_pThis = nullptr;
};