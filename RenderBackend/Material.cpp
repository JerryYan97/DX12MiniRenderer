#include "Material.h"
#include "../ThirdParty/TinyGltf/tiny_gltf.h"

Material::Material() :
    m_baseColorTex(nullptr),
    m_metallicRoughnessTex(nullptr),
    m_normalTex(nullptr),
    m_occlusionTex(nullptr),
    m_emissiveTex(nullptr),
    m_materialMask(0),
    m_isInitialized(false),
    m_cnstMetallic(0.0f),
    m_cnstRoughness(1.0f),
    m_isDielectric(true),
    m_isDoubleFace(false),
    m_isEmissiveMaterial(false)
{
    m_cnstAlbedo[0] = 1.0f;
    m_cnstAlbedo[1] = 1.0f;
    m_cnstAlbedo[2] = 1.0f;

    m_cnstEmissive[0] = 0.0f;
    m_cnstEmissive[1] = 0.0f;
    m_cnstEmissive[2] = 0.0f;
}

Material::~Material()
{
}

void Material::GenMaterialMask()
{
    m_materialMask = 0;
    if(m_baseColorTex != nullptr) { m_materialMask |= ALBEDO_MASK; }
    if(m_normalTex != nullptr) { m_materialMask |= NORMAL_MASK; }
    if(m_metallicRoughnessTex != nullptr) { m_materialMask |= ROUGHNESS_METALIC_MASK; }
    if(m_occlusionTex != nullptr) { m_materialMask |= AO_MASK; }
    if(m_isEmissiveMaterial) { m_materialMask |= EMISSIVE_MASK; }

    m_materialMask |= m_isDielectric ? DIELECTRIC_MASK : 0;
    m_materialMask |= m_isDoubleFace ? DOUBLE_FACE_MASK : 0;
}

uint32_t Material::TextureCnt() const
{
    uint32_t texCnt = 0;
    if(m_baseColorTex != nullptr) { texCnt++; }
    if(m_metallicRoughnessTex != nullptr) { texCnt++; }
    if(m_normalTex != nullptr) { texCnt++; }
    if(m_occlusionTex != nullptr) { texCnt++; }
    if(m_emissiveTex != nullptr) { texCnt++; }
    return texCnt;
}

void Material::Init(const ConstMaterialData& fallbackData, TextureAsset* baseColorTex, TextureAsset* metallicRoughnessTex, TextureAsset* normalTex, TextureAsset* occlusionTex, TextureAsset* emissiveTex)
{
    m_baseColorTex = baseColorTex;
    m_metallicRoughnessTex = metallicRoughnessTex;
    m_normalTex = normalTex;
    m_occlusionTex = occlusionTex;
    m_emissiveTex = emissiveTex;

    m_cnstAlbedo[0] = fallbackData.albedo[0];
    m_cnstAlbedo[1] = fallbackData.albedo[1];
    m_cnstAlbedo[2] = fallbackData.albedo[2];
    m_cnstMetallic = fallbackData.metallic;
    m_cnstRoughness = fallbackData.roughness;
    m_isDielectric = fallbackData.isDielectric;
    m_isDoubleFace = fallbackData.isDoubleFace;

    m_isEmissiveMaterial = fallbackData.isEmissive || (m_emissiveTex != nullptr);
    m_cnstEmissive[0] = fallbackData.emissive[0];
    m_cnstEmissive[1] = fallbackData.emissive[1];
    m_cnstEmissive[2] = fallbackData.emissive[2];

    GenMaterialMask();
    
    m_isInitialized = true;
}

DXGI_FORMAT GLTFTextureFormatToRendererTextureFormat(int gltfComponentType, int gltfComponentCnt)
{
    switch (gltfComponentType)
    {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        switch (gltfComponentCnt)
        {
        case 1: return DXGI_FORMAT_R8_UNORM;
        case 2: return DXGI_FORMAT_R8G8_UNORM;
        case 4: return DXGI_FORMAT_R8G8B8A8_UNORM;
        default:
            assert(false && "Unsupported glTF unsigned byte texture component count.");
            return DXGI_FORMAT_UNKNOWN;
        }

    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        switch (gltfComponentCnt)
        {
        case 1: return DXGI_FORMAT_R16_UNORM;
        case 2: return DXGI_FORMAT_R16G16_UNORM;
        case 4: return DXGI_FORMAT_R16G16B16A16_UNORM;
        default:
            assert(false && "Unsupported glTF unsigned short texture component count.");
            return DXGI_FORMAT_UNKNOWN;
        }

    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        switch (gltfComponentCnt)
        {
        case 1: return DXGI_FORMAT_R32_FLOAT;
        case 2: return DXGI_FORMAT_R32G32_FLOAT;
        case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case 4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default:
            assert(false && "Unsupported glTF float texture component count.");
            return DXGI_FORMAT_UNKNOWN;
        }

    default:
        assert(false && "Unsupported glTF texture component type.");
        return DXGI_FORMAT_UNKNOWN;
    }
}