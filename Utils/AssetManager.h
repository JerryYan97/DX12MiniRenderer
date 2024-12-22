#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <d3d12.h>

/*
* Make sure heavy data is only stored one time and managed by the AssetManager.
*/

struct ImgInfo
{
    uint32_t             pixWidth;
    uint32_t             pixHeight;
    uint32_t             componentCnt;
    std::vector<uint8_t> dataVec;
    uint32_t             componentType;
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

    ImgInfo m_baseColorTex;         // TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE (5121), 4 components.
    ImgInfo m_metallicRoughnessTex; // R32G32_SFLOAT
    ImgInfo m_normalTex;            // R32G32B32_SFLOAT
    ImgInfo m_occlusionTex;         // R32_SFLOAT
    ImgInfo m_emissiveTex;          // Currently don't support.
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

private:
    void CreateVertIdxBuffer(PrimitiveAsset* pPrimAsset);

    std::unordered_map<std::string, std::vector<PrimitiveAsset*>> m_primitiveAssets;
};