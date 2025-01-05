#pragma once

#include "RendererBackend.h"
#include "RTShaders\RaytracingHlslCompat.h"


class HWRTRenderBackend : public RendererBackend
{
    public:
        HWRTRenderBackend() : 
            RendererBackend(RendererBackendType::PathTracing),
            m_uavHeap(nullptr),
            m_renderTarget(nullptr)
            /*,
            m_raytracingGlobalRootSignature(nullptr),
            m_raytracingLocalRootSignature(nullptr),
            m_rtPipelineStateObject(nullptr),
            m_dxrDevice(nullptr),
            m_descriptorHeap(nullptr),
            m_rayGenCB(),
            m_blas(nullptr),
            m_tlas(nullptr),
            m_raytracingOutput(nullptr),
            m_rayGenShaderTable(nullptr),
            m_missShaderTable(nullptr),
            m_hitGroupShaderTable(nullptr)*/
        {}
        
        ~HWRTRenderBackend() {}

        virtual void RenderTick(ID3D12GraphicsCommandList* pCommandList, RenderTargetInfo rtInfo) override;

        // ID3D12Resource* GetRaytracingOutput() { return m_raytracingOutput; }

    protected:
        virtual void CustomResize(uint32_t width, uint32_t height) override;
        virtual void CustomInit() override;
        virtual void CustomDeinit() override;
    private:
        /*
        // Various Init Functions
        void InitRootSignatures(); // Global and Local Root Signatures
        void InitPipelineStates(); // PSOs
        void InitDescriptorHeaps(); // Descriptor Heaps
        void BuildGeometry(); // Build Scene Geometry
        void BuildAccelerationStructures(); // Build Scene Acceleration Structures
        void BuildShaderTables(); // Build Shader Tables
        void BuildRaytracingOutput(); // Build Raytracing Output

        void UpdateForSizeChange(UINT width, UINT height);

        // Root signatures
        ID3D12RootSignature* m_raytracingGlobalRootSignature;
        ID3D12RootSignature* m_raytracingLocalRootSignature;

        ID3D12Resource* m_blas;
        ID3D12Resource* m_tlas;

        ID3D12Resource*             m_raytracingOutput;
        D3D12_GPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVGpuDescriptor;
        UINT                        m_raytracingOutputResourceUAVDescriptorHeapIndex;

        ID3D12StateObject*    m_rtPipelineStateObject;
        ID3D12DescriptorHeap* m_descriptorHeap;

        RayGenConstantBuffer m_rayGenCB;

        ID3D12Device5* m_dxrDevice;

        // Shader Table
        ID3D12Resource* m_rayGenShaderTable;
        ID3D12Resource* m_missShaderTable;
        ID3D12Resource* m_hitGroupShaderTable;

        ID3D12Resource* m_indexBuffer;
        ID3D12Resource* m_vertexBuffer;

        typedef UINT16 Index;
        struct Vertex { float v1, v2, v3; };
        */

        ID3D12DescriptorHeap* m_uavHeap;
        ID3D12Resource* m_renderTarget;
};