#pragma once

#include "RendererBackend.h"
#include "RTShaders\RaytracingHlslCompat.h"


class HWRTRenderBackend : public RendererBackend
{
    public:
        HWRTRenderBackend() : 
            RendererBackend(RendererBackendType::PathTracing),
            m_uavHeap(nullptr),
            m_renderTarget(nullptr),
            m_fence(nullptr),
            m_instances(nullptr),
            m_tlas(nullptr),
            m_tlasUpdateScratch(nullptr),
            m_rootSignature(nullptr),
            m_pso(nullptr),
            m_shaderIDs(nullptr),
            m_numInstances(0),
            m_frameCnstBuffer(nullptr),
            m_frameCnstBufferMap(nullptr),
            m_sceneVertBuffer(nullptr),
            m_sceneIdxBuffer(nullptr),
            m_instInfoBuffer(nullptr),
            m_frameCount(0),
            m_renderTargetRadiance(nullptr)
        {}
        
        ~HWRTRenderBackend() {}

        virtual void RenderTick(ID3D12GraphicsCommandList4* pCommandList, RenderTargetInfo rtInfo) override;

    protected:
        virtual void CustomResize(uint32_t width, uint32_t height) override;
        virtual void CustomInit() override;
        virtual void CustomDeinit() override;
    private:
        ID3D12DescriptorHeap* m_uavHeap;
        ID3D12Resource* m_renderTarget; // With Gamma Correction.
        ID3D12Resource* m_renderTargetRadiance;

        ID3D12Fence* m_fence;

        ID3D12Resource* MakeAccelerationStructure(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs, UINT64* updateScratchSize = nullptr);
        ID3D12Resource* MakeBLAS(ID3D12Resource* vertexBuffer, UINT vertexFloats, ID3D12Resource* indexBuffer = nullptr, UINT indices = 0);
        ID3D12Resource* MakeTLAS(ID3D12Resource* instances, UINT numInstances, UINT64* updateScratchSize);

        void Flush();

        void InitScene();
        ID3D12Resource* m_instances;
        UINT m_numInstances;

        ID3D12Resource* m_instInfoBuffer;
        // For the scene vert/idx buffer, we will need to disable per-prim auto gpu buffer creation for the next refactorization.
        ID3D12Resource* m_sceneVertBuffer;
        ID3D12Resource* m_sceneIdxBuffer;

        void InitBottomLevel();

        void InitTopLevel();
        ID3D12Resource* m_tlas;
        ID3D12Resource* m_tlasUpdateScratch;

        void InitRootSignature();
        ID3D12RootSignature* m_rootSignature;

        void InitPipeline();
        ID3D12StateObject* m_pso;
        ID3D12Resource* m_shaderIDs;

        void UpdateScene(ID3D12GraphicsCommandList4* cmdList);
        void UpdateFrameConstBuffer();

        struct FrameConstBuffer
        {
            float    cameraPos[4];
            float    cameraDir[4];
            float    cameraUp[4];
            float    cameraRight[4];
            float    cameraInfo[4]; // x: fov, y: near, z: far.
            uint32_t frameUintInfo[4]; // x: frame count; y: random number.
        };
        ID3D12Resource* m_frameCnstBuffer;
        void*           m_frameCnstBufferMap;
        uint32_t        m_frameCount;
};