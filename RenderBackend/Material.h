#pragma once
#include <cstdint>
#include <vector>
#include <dxgiformat.h>

struct TextureAsset;

const uint32_t ALBEDO_MASK            = 1;
const uint32_t NORMAL_MASK            = 2;
const uint32_t ROUGHNESS_METALIC_MASK = 4;
const uint32_t AO_MASK                = 8;
const uint32_t EMISSIVE_MASK          = 16;
const uint32_t DIELECTRIC_MASK        = 32;
const uint32_t DOUBLE_FACE_MASK       = 64;

struct ConstMaterialData
{
    float     albedo[3];
    float     emissive[3];
    float     metallic;
    float     roughness;
    bool      isDielectric;
    bool      isDoubleFace;
    bool      isEmissive;
};

DXGI_FORMAT GLTFTextureFormatToRendererTextureFormat(int gltfComponentType, int gltfComponentCnt);

// Material can be constant material or normal PBR material. A normal PBR material would refer a set of textures.
// A 'Constant Material' means it's textures pointers are all nullptr.
class Material
{
    friend class AssetManager; // Used for accessing texture assets.

public:
    Material();
    ~Material();

    uint32_t TextureCnt() const;

    static ConstMaterialData DefaultConstMaterialData()
    {
        ConstMaterialData defaultData = {};
        defaultData.albedo[0] = 1.f;
        defaultData.albedo[1] = 1.f;
        defaultData.albedo[2] = 1.f;
        defaultData.metallic = 0.f;
        defaultData.roughness = 1.f;
        defaultData.isDielectric = false; // Since metallic is 0, it should not be dielectric.
        defaultData.isDoubleFace = false;
        defaultData.isEmissive = false;
        return defaultData;
    }

    void Init(const ConstMaterialData& fallbackData,
              TextureAsset* baseColorTex = nullptr,
              TextureAsset* metallicRoughnessTex = nullptr,
              TextureAsset* normalTex = nullptr,
              TextureAsset* occlusionTex = nullptr,
              TextureAsset* emissiveTex = nullptr); // The fallbackData is used when the corresponding texture is not defined. For example, if the baseColorTex is nullptr, we will use defaultData.albedo as the base color of this material.

    bool IsEmissiveMaterial() const { return m_isEmissiveMaterial; }
    uint32_t GetMaterialMask() const { return m_materialMask; }

    TextureAsset* GetBaseColorTex() const { return m_baseColorTex; }
    TextureAsset* GetMetallicRoughnessTex() const { return m_metallicRoughnessTex; }
    TextureAsset* GetNormalTex() const { return m_normalTex; }
    TextureAsset* GetOcclusionTex() const { return m_occlusionTex; }
    TextureAsset* GetEmissiveTex() const { return m_emissiveTex; }

    std::vector<float> GetCnstEmissive() const
    {
        std::vector<float> res = {m_cnstEmissive[0], m_cnstEmissive[1], m_cnstEmissive[2]};
        return res;
    }

    std::vector<float> GetCnstAlbedo() const
    {
        std::vector<float> res = {m_cnstAlbedo[0], m_cnstAlbedo[1], m_cnstAlbedo[2]};
        return res;
    }

    std::vector<float> GetCnstMetallicRoughness() const
    {
        std::vector<float> res = {m_cnstMetallic, m_cnstRoughness};
        return res;
    }

private:
    TextureAsset* m_baseColorTex;         // TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE (5121), 4 components.
    TextureAsset* m_metallicRoughnessTex; // R32G32_SFLOAT
    TextureAsset* m_normalTex;            // R32G32B32_SFLOAT
    TextureAsset* m_occlusionTex;         // R32_SFLOAT
    TextureAsset* m_emissiveTex;          // Currently don't support.

    uint32_t m_materialMask;
    bool m_isInitialized = false;
    bool m_isEmissiveMaterial = false;

    float m_cnstAlbedo[3];
    float m_cnstMetallic;
    float m_cnstRoughness;
    bool  m_isDielectric;
    bool  m_isDoubleFace;

    float m_cnstEmissive[3];

    void GenMaterialMask();
};