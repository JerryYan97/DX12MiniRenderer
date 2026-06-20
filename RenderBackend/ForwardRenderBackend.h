#pragma once
#include "RendererBackend.h"

class MeshObject;
struct Primitive;

class ForwardRenderer : public RendererBackend
{
public:
    ForwardRenderer();
    ~ForwardRenderer();

    virtual void RenderTick(ID3D12GraphicsCommandList4* pCommandList, RenderTargetInfo rtInfo) override;
    virtual void GetMainRenderTargetSize(uint32_t& oWidth, uint32_t& oHeight) override { oWidth = 100; oHeight = 100; } // TODO: Placeholder

protected:
    virtual void CustomInit();
    virtual void CustomDeinit();

private:
    void CreateMeshRenderGpuResources();
    void CreateRootSignature();
    void CreatePipelineStateObject();

    void UpdatePerFrameGpuResources();

    struct DescriptorHeapData {
        ID3D12DescriptorHeap*       pPrimRenderDescriptorHeap;
        D3D12_GPU_DESCRIPTOR_HANDLE gfxRootDescriptorTableHandleInHeap[2]; // [0] is the starting descriptor for VS and [1] is the starting descriptor for PS.
    };

    DescriptorHeapData GenerateOnFlightDescriptorHeapFromPrimitive(const MeshObject& meshObj, const Primitive& iPrim);

    ID3D12RootSignature* m_pRootSignature;
    ID3D12PipelineState* m_pPipelineState;
    
    ID3D12Resource*       m_pVsSceneBuffer;
    ID3D12Resource*       m_pPsSceneBuffer;
    UINT8*                m_pVsSceneBufferBegin;
    UINT8*                m_pPsSceneBufferBegin;
    ID3D12DescriptorHeap* m_pSceneCbvHeap;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT     m_scissorRect;

    // ID3D12DescriptorHeap* m_shaderVisibleCbvHeap;

    std::vector<ID3D12DescriptorHeap*> m_inflightShaderVisibleCbvHeaps;
};