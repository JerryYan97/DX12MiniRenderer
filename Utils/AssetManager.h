#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <d3d12.h>

/*
* Make sure heavy data is only stored one time and managed by the AssetManager.
*/

class StaticMesh;

const uint32_t ALBEDO_MASK            = 1;
const uint32_t NORMAL_MASK            = 2;
const uint32_t ROUGHNESS_METALIC_MASK = 4;
const uint32_t AO_MASK                = 8;
const uint32_t EMISSIVE_MASK          = 16;

enum class TexWrapMode
{
    REPEAT,
    MIRRORED_REPEAT,
    CLAMP_TO_EDGE,
    CLAMP_TO_BORDER
};

struct ImgInfo
{
    uint32_t             pixWidth;
    uint32_t             pixHeight;
    uint32_t             componentCnt;
    std::vector<uint8_t> dataVec;
    uint32_t             componentType;
    ID3D12Resource*      gpuResource;
    bool                 isSentToGpu;
    uint32_t             srvHeapIdx;
    TexWrapMode          wrapModeVertical;
    TexWrapMode          wrapModeHorizontal;
};

struct TextureAsset
{
    ImgInfo imgInfo;
};

struct EnvMapAsset
{

};

struct PrimitiveAsset
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

    ID3D12DescriptorHeap* m_pTexturesSrvHeap;

    ImgInfo m_baseColorTex;         // TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE (5121), 4 components.
    ImgInfo m_metallicRoughnessTex; // R32G32_SFLOAT
    ImgInfo m_normalTex;            // R32G32B32_SFLOAT
    ImgInfo m_occlusionTex;         // R32_SFLOAT
    ImgInfo m_emissiveTex;          // Currently don't support.

    uint32_t              m_materialMask;
    ID3D12Resource*       m_materialMaskBuffer;
    ID3D12DescriptorHeap* m_pMaterialMaskCbvHeap;

    uint32_t TextureCnt() const
    {
        uint32_t texCnt = 0;
        if(m_baseColorTex.pixWidth > 1) { texCnt++; }
        if(m_metallicRoughnessTex.pixWidth > 1) { texCnt++; }
        if(m_normalTex.pixWidth > 1) { texCnt++; }
        if(m_occlusionTex.pixWidth > 1) { texCnt++; }
        if(m_emissiveTex.pixWidth > 1) { texCnt++; }
        return texCnt;
    }

    void GenMaterialMask()
    {
        m_materialMask = 0;
        if(m_baseColorTex.pixWidth > 1) { m_materialMask |= ALBEDO_MASK; }
        if(m_normalTex.pixWidth > 1) { m_materialMask |= NORMAL_MASK; }
        if(m_metallicRoughnessTex.pixWidth > 1) { m_materialMask |= ROUGHNESS_METALIC_MASK; }
        if(m_occlusionTex.pixWidth > 1) { m_materialMask |= AO_MASK; }
        if(m_emissiveTex.pixWidth > 1) { m_materialMask |= EMISSIVE_MASK; }
    }
};

class AssetManager
{
public:
    AssetManager() {}
    ~AssetManager() {}

    void Deinit();

    void SaveModelPrimAssetAndCreateGpuRsrc(const std::string& modelName, PrimitiveAsset* pPrimitiveAsset);
    void LoadAssets();
    void UnloadAssets();

    bool IsStaticMeshAssetLoaded(const std::string& modelName) const
    {
        return m_primitiveAssets.find(modelName) != m_primitiveAssets.end();
    }

    void LoadStaticMeshAssets(const std::string& modelName, StaticMesh* pStaticMesh);

private:
    void CreateVertIdxBuffer(PrimitiveAsset* pPrimAsset);
    void GenMaterialTexBuffer(PrimitiveAsset* pPrimAsset);
    void GenPrimAssetMaterialBuffer(PrimitiveAsset* pPrimAsset);

    std::unordered_map<std::string, std::vector<PrimitiveAsset*>> m_primitiveAssets;
};