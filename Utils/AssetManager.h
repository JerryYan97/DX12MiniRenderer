#pragma once
#include "Asset.h"
#include <unordered_map>
#include <string>

struct Primitive;

constexpr int VERT_SIZE_FLOAT = (3 + 3 + 4 + 2); // Position(3) + Normal(3) + Tangent(4) + TexCoord(2).

// EnvMap Asset can be looked as combination of multiple TextureAssets, but we want to keep them together for better management and usage.
/*
struct EnvMapAsset
{
    ImgInfo backGroundCubemap;
    ImgInfo diffuseIrradianceCubemap;
    ImgInfo envBRDF;
    ImgInfo prefilteredEnvMap;
};
*/


/*
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

    ID3D12Resource* m_blas;

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
*/

class AssetManager
{
public:
    AssetManager() { m_pThis = this; }
    ~AssetManager() { m_pThis = nullptr; }

    static AssetManager* GetInstance() { return m_pThis; }

    void Deinit();

    bool IsAssetLoaded(const std::string& modelName) const
    {
        return ( m_geoAssets.find(modelName) != m_geoAssets.end() ) || ( m_textureAssets.find(modelName) != m_textureAssets.end() );
    }

    // The upper level AssetLoader calls this func after it loads the model file and arrage the data to a 'Primitive' vector.
    // It will also send assets' resources to GPU according to the rendering backend.
    void StoreModelAssets(const std::string& assetPath, const std::vector<Primitive>& iPrimitives);
    void StoreTextureAsset(const std::string& assetPath, TextureAsset* iTexAsset);
    // EnvMapAsset* StoreEnvMapAsset(const std::string& filepath);

    void RetriveAllMeshAssetsNames(std::vector<std::string>& o_meshNames) const
    {
        for (const auto& pair : m_geoAssets)
        {
            o_meshNames.push_back(pair.first);
        }
    }

private:
    /*
    void CreateVertIdxBuffer(PrimitiveAsset* pPrimAsset);
    void GenMaterialTexBuffer(PrimitiveAsset* pPrimAsset);
    void GenPrimAssetMaterialBuffer(PrimitiveAsset* pPrimAsset);
    */

    // void LoadCubemapFromSingleFile(const std::string& filepath, ImgInfo& oImgInfo);

    void SendGeoAssetToGpu(GeometryAsset* pGeoAsset);
    void SendTextureAssetToGpu(TextureAsset* pTexAsset);

    // Key - Value pairs to avoid loading the same asset multiple times. The key is the asset's relative file/folder path. The value is the geo/tex data in/linked to the corresponding asset file.
    // The asset can be a normal .gltf file (Geo assets + Texture assets) or a custom env map folder (Only texture assets).
    //
    // The AssetManager doesn't care about how to interpret the geo/tex data. It simply stores them. The upper level class/instance will decide how to use them.
    std::unordered_map<std::string, std::vector<GeometryAsset*>> m_geoAssets;
    std::unordered_map<std::string, std::vector<TextureAsset*>>  m_textureAssets;

    static AssetManager* m_pThis;
};