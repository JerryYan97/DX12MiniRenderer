#pragma once
#include <vector>
#include <d3d12.h>

enum class TexWrapMode
{
    REPEAT,
    MIRRORED_REPEAT,
    CLAMP_TO_EDGE,
    CLAMP_TO_BORDER
};

struct ImgInfo
{
    uint32_t              pixWidth;
    uint32_t              pixHeight;
    std::vector<uint8_t>  dataVec;
    DXGI_FORMAT           textureFormat;
    ID3D12Resource*       gpuResource;
    bool                  isSentToGpu;
    ID3D12DescriptorHeap* texDescHeap; // The descriptor heap that only contains the SRV of this texture. This descriptor will be copied out to the big descriptor heap for rendering at each render loop. It's entirely same as the ImgInfo. No special interpretation for the original image.
    TexWrapMode           wrapModeVertical;
    TexWrapMode           wrapModeHorizontal;
    uint32_t              arrayLayerCnt = 1; // For cubemap, it should be 6. For 2D texture, it should be 1.
    uint32_t              mipLevelCnt = 1;
};

struct TextureAsset
{
    ImgInfo imgInfo;
};

struct GeometryAsset
{
    std::vector<float> m_vertData;
    std::vector<float> m_posData;
    std::vector<float> m_normalData;
    std::vector<float> m_tangentData;
    std::vector<float> m_texCoordData;
    bool                  m_idxType; // 0: uint16_t, 1: uint32_t
    uint32_t              m_idxCnt;
    std::vector<uint16_t> m_idxDataUint16;
    std::vector<uint32_t> m_idxDataUint32;

    ID3D12Resource*          m_gpuVertBuffer;
    ID3D12Resource*          m_gpuIndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW  m_idxBufferView;
    ID3D12Resource*          m_blas;
    bool                     m_useHWRT;

    GeometryAsset()
    {
        m_vertData.clear();
        m_posData.clear();
        m_normalData.clear();
        m_tangentData.clear();
        m_texCoordData.clear();

        m_idxType = false;
        m_idxCnt = 0;
        m_idxDataUint16.clear();
        m_idxDataUint32.clear();

        m_gpuVertBuffer = nullptr;
        m_gpuIndexBuffer = nullptr;
        m_vertexBufferView = {};
        m_idxBufferView = {};
        m_blas = nullptr;
        m_useHWRT = false;
    }
};