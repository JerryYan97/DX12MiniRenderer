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
            // m_instanceData(nullptr),
            m_tlas(nullptr),
            m_tlasUpdateScratch(nullptr),
            m_rootSignature(nullptr),
            m_pso(nullptr),
            m_shaderIDs(nullptr),
            m_numInstances(0),
            m_cameraCnstBuffer(nullptr),
            m_cameraCnstBufferMap(nullptr),
            m_cnstMaterialsBuffer(nullptr),
            m_materialMaskBuffer(nullptr)
        {}
        
        ~HWRTRenderBackend() {}

        virtual void RenderTick(ID3D12GraphicsCommandList4* pCommandList, RenderTargetInfo rtInfo) override;

        // ID3D12Resource* GetRaytracingOutput() { return m_raytracingOutput; }

    protected:
        virtual void CustomResize(uint32_t width, uint32_t height) override;
        virtual void CustomInit() override;
        virtual void CustomDeinit() override;
    private:
        ID3D12DescriptorHeap* m_uavHeap;
        ID3D12Resource* m_renderTarget;

        ID3D12Fence* m_fence;

        ID3D12Resource* MakeAccelerationStructure(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs, UINT64* updateScratchSize = nullptr);
        ID3D12Resource* MakeBLAS(ID3D12Resource* vertexBuffer, UINT vertexFloats, ID3D12Resource* indexBuffer = nullptr, UINT indices = 0);
        ID3D12Resource* MakeTLAS(ID3D12Resource* instances, UINT numInstances, UINT64* updateScratchSize);

        void Flush();

        void InitScene();
        ID3D12Resource* m_instances;
        UINT m_numInstances;

        ID3D12Resource* m_cnstMaterialsBuffer;
        ID3D12Resource* m_materialMaskBuffer;

        // D3D12_RAYTRACING_INSTANCE_DESC* m_instanceData;

        void InitBottomLevel();
        // std::vector<ID3D12Resource*> m_blases;
        // ID3D12Resource* m_quadBlas;
        // ID3D12Resource* m_cubeBlas;
        




        // void UpdateTransforms();

        void InitTopLevel();
        ID3D12Resource* m_tlas;
        ID3D12Resource* m_tlasUpdateScratch;

        void InitRootSignature();
        ID3D12RootSignature* m_rootSignature;

        void InitPipeline();
        ID3D12StateObject* m_pso;
        ID3D12Resource* m_shaderIDs;

        void UpdateScene(ID3D12GraphicsCommandList4* cmdList);
        void UpdateCamera();
        ID3D12Resource* m_cameraCnstBuffer;
        void*           m_cameraCnstBufferMap;
};