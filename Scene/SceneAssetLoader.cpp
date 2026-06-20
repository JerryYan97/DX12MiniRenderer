#include "SceneAssetLoader.h"
#include "Level.h"
#include "Mesh.h"
#include "Lights.h"
#include "Camera.h"
#include "yaml-cpp/yaml.h"
#include "../Utils/MathUtils.h"
#include "../Utils/StrPathUtils.h"
#include "../Utils/GltfUtils.h"
#include "../Utils/AssetManager.h"
#include "../RenderBackend/Material.h"
#include <iostream>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ThirdParty/TinyGltf/tiny_gltf.h"

SceneLoader* SceneLoader::m_pThis = nullptr;
AssetLoader* AssetLoader::m_pThis = nullptr;

extern AssetManager* g_pAssetManager;

SceneLoader::SceneLoader()
{
    m_pThis = this;
}

SceneLoader::~SceneLoader()
{
}

AssetLoader::AssetLoader()
{
   m_pThis = this;
}

AssetLoader::~AssetLoader()
{
}

EnvironmentMap AssetLoader::LoadAsEnvMap(const std::string& fileNamePath)
{
    EnvironmentMap envMap = {};

    assert(fileNamePath.size() > 4);
    const std::string extLower = fileNamePath.substr(fileNamePath.size() - 4);
    assert(extLower == ".hdr" && "LoadAsEnvMap expects a .hdr file");

    int width = 0;
    int height = 0;
    int channels = 0;
    float* pData = stbi_loadf(fileNamePath.c_str(), &width, &height, &channels, 0);
    if (pData == nullptr)
    {
        return envMap;
    }

    TextureAsset* pEnvMapTexture = new TextureAsset();
    pEnvMapTexture->imgInfo.pixWidth = static_cast<uint32_t>(width);
    pEnvMapTexture->imgInfo.pixHeight = static_cast<uint32_t>(width);
    pEnvMapTexture->imgInfo.textureFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    pEnvMapTexture->imgInfo.gpuResource = nullptr;
    pEnvMapTexture->imgInfo.isSentToGpu = false;
    pEnvMapTexture->imgInfo.texDescHeap = nullptr;
    pEnvMapTexture->imgInfo.wrapModeHorizontal = TexWrapMode::CLAMP_TO_EDGE;
    pEnvMapTexture->imgInfo.wrapModeVertical = TexWrapMode::CLAMP_TO_EDGE;
    pEnvMapTexture->imgInfo.arrayLayerCnt = 6;
    pEnvMapTexture->imgInfo.mipLevelCnt = 1;

    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels);
    const size_t dataSizeByte = pixelCount * sizeof(float);
    pEnvMapTexture->imgInfo.dataVec.resize(dataSizeByte);
    memcpy(pEnvMapTexture->imgInfo.dataVec.data(), pData, dataSizeByte);

    stbi_image_free(pData);

    g_pAssetManager->StoreTextureAsset(fileNamePath, pEnvMapTexture);
    envMap.InitEnvironmentMap(pEnvMapTexture);

    return envMap;
}

void SceneLoader::LoadAsLevel(const std::string& fileNamePath, Level* o_pLevel)
{
    // Load the scene file into the level
    YAML::Node config = YAML::LoadFile(fileNamePath.c_str());
    std::string sceneType = "";
    if (config["SceneType"].IsDefined())
    {
        sceneType = config["SceneType"].as<std::string>();
    }

    std::string rendererType = "";
    if(config["Renderer"].IsDefined())
    {
        rendererType = config["Renderer"].as<std::string>();
    }

    o_pLevel->m_backgroundType = BackgroundType::BLACK;

    if (rendererType.compare("PathTracer") == 0)
    {
        o_pLevel->m_rendererBackendType = RendererBackendType::PathTracing;
    }
    else
    {
        o_pLevel->m_rendererBackendType = RendererBackendType::Forward;
    }

    if (config["PathTracing Settings"].IsDefined())
    {
        YAML::Node pathTracingSettings = config["PathTracing Settings"];
        if (pathTracingSettings["Background"].IsDefined())
        {
            std::string backgroundType = pathTracingSettings["Background"]["Type"].as<std::string>();
            if (backgroundType.compare("DefaultInCodeSkybox") == 0)
            {
                o_pLevel->m_backgroundType = BackgroundType::DEFAULT_SKY;
            }
        }
    }

    // Environment Map and IBL
    if (config["EnvironmentMap"].IsDefined())
    {
        YAML::Node envMapNode = config["EnvironmentMap"];
        std::string iblPkgName = envMapNode.as<std::string>();
        std::string sceneDir = GetFileDir(fileNamePath);
        sceneDir += "\\";
        sceneDir += iblPkgName;

        EnvironmentMap envMap = AssetLoader::LoadAsEnvMap(sceneDir);
        o_pLevel->SetEnvMapAndIBL(envMap);
    }
    //

    YAML::Node sceneGraph = config["SceneGraph"];

    std::vector<float> bgColor = std::vector<float>(3, 0.1f);
    if (config["BackgroundColor"].IsSequence())
    {
        bgColor = config["BackgroundColor"].as<std::vector<float>>();
    }

    memcpy(o_pLevel->m_backgroundColor, bgColor.data(), sizeof(o_pLevel->m_backgroundColor));

    // m_currentScenePath = GetFileDir(fileNamePath);
    m_currentScenePath = fileNamePath;

    uint32_t ambientLightCnt = 0;

    for (const auto& itr : sceneGraph)
    {
        const std::string objName = itr.first.as<std::string>();
        const std::string type = itr.second["Type"].as<std::string>();
        if (type.compare("MeshObject") == 0)
        {
            o_pLevel->LoadObject(objName, itr.second, MeshObject::Deseralize);
        }
        else if (type.compare("AmbientLight") == 0)
        {
            assert(ambientLightCnt <= 1, "Ambient Light Count shouldn't be larger than 1.");
            ambientLightCnt++;
            o_pLevel->LoadObject(objName, itr.second, AmbientLight::Deseralize);
        }
        else if (type.compare("PointLight") == 0)
        {
            o_pLevel->LoadObject(objName, itr.second, PointLight::Deseralize);
        }
        else if (type.compare("Camera") == 0)
        {
            o_pLevel->LoadObject(objName, itr.second, Camera::Deseralize);
        }
    }

    // Calculate the meshes center and bounding box of the level
    std::vector<MeshObject*> meshObjects;
    float levelCenter[3] = { 0.f, 0.f, 0.f };
    float bbxMin[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
    float bbxMax[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    o_pLevel->RetriveMeshObjects(meshObjects);
    for(int i = 0; i < meshObjects.size(); i++)
    {
        std::vector<float> meshCenter = meshObjects[i]->GetMeshCenter();
        levelCenter[0] += meshCenter[0];
        levelCenter[1] += meshCenter[1];
        levelCenter[2] += meshCenter[2];

        std::vector<float> meshBBXMinMax = meshObjects[i]->GetMeshBBX();
        bbxMin[0] = min(bbxMin[0], meshBBXMinMax[0]);
        bbxMin[1] = min(bbxMin[1], meshBBXMinMax[1]);
        bbxMin[2] = min(bbxMin[2], meshBBXMinMax[2]);

        bbxMax[0] = max(bbxMax[0], meshBBXMinMax[3]);
        bbxMax[1] = max(bbxMax[1], meshBBXMinMax[4]);
        bbxMax[2] = max(bbxMax[2], meshBBXMinMax[5]);
    }
    levelCenter[0] = levelCenter[0] / meshObjects.size();
    levelCenter[1] = levelCenter[1] / meshObjects.size();
    levelCenter[2] = levelCenter[2] / meshObjects.size();

    o_pLevel->SetLevelCenter(levelCenter);
    o_pLevel->SetBoundingBox(bbxMin, bbxMax);
}

Mesh AssetLoader::LoadAsOneMesh(const std::string& fileNamePath)
{
    //#TODO: Check the file extension and call the appropriate loader. E.g. OpenUSD
    //#TODO: We may want to use FastGltf instead of TinyGltf.

    Mesh mesh = {};

    if (m_pThis->IsAssetLoaded(fileNamePath))
    {
        mesh = m_pThis->m_AssetsMeshes[fileNamePath];
    }
    else
    {
        mesh = LoadTinyGltfOneModelAsOneMesh(fileNamePath);
        m_pThis->m_AssetsMeshes[fileNamePath] = mesh;
    }

    g_pAssetManager->StoreModelAssets(fileNamePath, mesh.GetPrimitives());

    return mesh;
}

TexWrapMode GltfSamplerWrapToInternalWrapMode(int wrapMode)
{
    switch (wrapMode)
    {
    case TINYGLTF_TEXTURE_WRAP_REPEAT:
        return TexWrapMode::REPEAT;
    case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
        return TexWrapMode::CLAMP_TO_EDGE;
    case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
        return TexWrapMode::MIRRORED_REPEAT;
    default:
        return TexWrapMode::REPEAT;
    }
}

// This func assumes the gltf file only contains one 'mesh'. This 'mesh' will be laoded as a 'Mesh' in the engine.
Mesh AssetLoader::LoadTinyGltfOneModelAsOneMesh(const std::string& fileNamePath)
{
    Mesh LoadedMesh = {};

    std::string absPath = GetFileDir(m_pThis->m_currentScenePath);
    const std::string fullGltfPathName = absPath + "\\" + fileNamePath;
    std::cout << "Loading gltf file: " << fullGltfPathName << std::endl;

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, fullGltfPathName);

    if (!warn.empty()) {
        printf("Warn: %s\n", warn.c_str());
    }

    if (!err.empty()) {
        printf("Err: %s\n", err.c_str());
    }

    if (!ret) {
        printf("Failed to parse glTF\n");
        exit(1);
    }

    // NOTE: TinyGltf loader has already loaded the binary buffer data and the images data.
    const auto& binaryBuffer = model.buffers[0].data;
    const unsigned char* pBufferData = binaryBuffer.data();

    // NOTE: (1): TinyGltf loader has already loaded the binary buffer data and the images data.
    //       (2): The gltf may has multiple buffers. The buffer idx should come from the buffer view.
    //       (3): Be aware of the byte stride: https://github.com/KhronosGroup/glTF-Tutorials/blob/main/gltfTutorial/gltfTutorial_005_BuffersBufferViewsAccessors.md#data-interleaving
    //       (4): Be aware of the base color factor: https://github.com/KhronosGroup/glTF-Tutorials/blob/main/gltfTutorial/gltfTutorial_011_SimpleMaterial.md#material-definition
    // This example only supports gltf that only has one mesh and one skin.
    assert(model.meshes.size() == 1, "This SharedLib Gltf Loader currently only supports one mesh.");
    assert(model.skins.size() == 0, "This SharedLib Gltf Loader currently doesn't support the skinning."); // TODO: Support skinning and animation.
    assert(model.scenes.size() == 1, "This SharedLib Gltf Loader currently only supports one scene.");
    assert(model.scenes[0].nodes.size() == 1, "This SharedLib Gltf Loader currently only supports one node in the scene.");
    
    // Load mesh and relevant info
    // Any node MAY contain one mesh, defined in its mesh property. The mesh MAY be skinned using information provided in a referenced skin object.
    // TODO: We should support multiple meshes in the future.
    const auto& mesh = model.meshes[0];
    
    std::vector<Primitive> primitives;

    // TODO: This loading design is not good since we can load same geometry/texutre shared by multiple models/meshes in the gltf.
    //       A better design would be loading all geometry/texture data from a gltf file first and assumble their references to 'Mesh' or 'Primitive' later.
    for (uint32_t i = 0; i < mesh.primitives.size(); i++)
    {
        const auto& primitive = mesh.primitives[i];
        Primitive meshPrim = {};
        meshPrim.geometry = LoadOneGltfPrimGeometryAsset(primitive, model, 0, i);
        meshPrim.material = LoadOneGltfPrimMaterial(primitive, model, 0, i);

        primitives.push_back(meshPrim);
    }

    LoadedMesh.InitAsAssetStorage(fileNamePath, primitives);

    return LoadedMesh;
}

GeometryAsset* AssetLoader::LoadOneGltfPrimGeometryAsset(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const int meshIdx, const int primIdx)
{
    const auto& mesh = model.meshes[meshIdx];

    // Empty Geo Asset
    GeometryAsset* pGeoAsset = new GeometryAsset();

    // Load pos
    int posIdx = primitive.attributes.at("POSITION");
    const auto& posAccessor = model.accessors[posIdx];

    assert(posAccessor.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT, "The pos accessor data type should be float.");
    assert(posAccessor.type == TINYGLTF_TYPE_VEC3, "The pos accessor type should be vec3.");

    const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
    // Assmue the data and element type of the position is float3
    pGeoAsset->m_posData.resize(3 * posAccessor.count);
    ReadOutAccessorData(pGeoAsset->m_posData.data(), posAccessor, model.bufferViews, model.buffers);

    // Load indices
    int indicesIdx = mesh.primitives[primIdx].indices;
    const auto& idxAccessor = model.accessors[indicesIdx];

    assert(idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT ||
        idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT, "The idx accessor data type should be uint16/32.");
    assert(idxAccessor.type == TINYGLTF_TYPE_SCALAR, "The idx accessor type should be scalar.");

    if (idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT)
    {
        pGeoAsset->m_idxType = false;
        pGeoAsset->m_idxDataUint16.resize(idxAccessor.count);
        ReadOutAccessorData(pGeoAsset->m_idxDataUint16.data(), idxAccessor, model.bufferViews, model.buffers);
    }
    else if (idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT)
    {
        pGeoAsset->m_idxType = true;
        pGeoAsset->m_idxDataUint32.resize(idxAccessor.count);
        ReadOutAccessorData(pGeoAsset->m_idxDataUint32.data(), idxAccessor, model.bufferViews, model.buffers);
    }
    pGeoAsset->m_idxCnt = idxAccessor.count;

    // Load normal
    int normalIdx = -1;
    if (mesh.primitives[primIdx].attributes.count("NORMAL") > 0)
    {
        normalIdx = mesh.primitives[primIdx].attributes.at("NORMAL");
        const auto& normalAccessor = model.accessors[normalIdx];

        assert(normalAccessor.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT, "The normal accessor data type should be float.");
        assert(normalAccessor.type == TINYGLTF_TYPE_VEC3, "The normal accessor type should be vec3.");

        pGeoAsset->m_normalData.resize(3 * normalAccessor.count);
        ReadOutAccessorData(pGeoAsset->m_normalData.data(), normalAccessor, model.bufferViews, model.buffers);
    }
    else
    {
        // If we don't have any normal geo data, then we will just apply the first triangle's normal to all the other
        // triangles/vertices.
        uint16_t idx0 = pGeoAsset->m_idxDataUint16[0];
        float vertPos0[3] = { pGeoAsset->m_posData[3 * idx0], pGeoAsset->m_posData[3 * idx0 + 1], pGeoAsset->m_posData[3 * idx0 + 2] };

        uint16_t idx1 = pGeoAsset->m_idxDataUint16[1];
        float vertPos1[3] = { pGeoAsset->m_posData[3 * idx1], pGeoAsset->m_posData[3 * idx1 + 1], pGeoAsset->m_posData[3 * idx1 + 2] };

        uint16_t idx2 = pGeoAsset->m_idxDataUint16[2];
        float vertPos2[3] = { pGeoAsset->m_posData[3 * idx2], pGeoAsset->m_posData[3 * idx2 + 1], pGeoAsset->m_posData[3 * idx2 + 2] };

        float v1[3] = { vertPos1[0] - vertPos0[0], vertPos1[1] - vertPos0[1], vertPos1[2] - vertPos0[2] };
        float v2[3] = { vertPos2[0] - vertPos0[0], vertPos2[1] - vertPos0[1], vertPos2[2] - vertPos0[2] };

        float autoGenNormal[3] = { 0.f };
        CrossProductVec3(v1, v2, autoGenNormal);
        NormalizeVec(autoGenNormal, 3);

        pGeoAsset->m_normalData.resize(3 * posAccessor.count);
        for (uint32_t i = 0; i < posAccessor.count; i++)
        {
            uint32_t normalStartingIdx = i * 3;
            pGeoAsset->m_normalData[normalStartingIdx] = autoGenNormal[0];
            pGeoAsset->m_normalData[normalStartingIdx + 1] = autoGenNormal[1];
            pGeoAsset->m_normalData[normalStartingIdx + 2] = autoGenNormal[2];
        }
    }

    // Load uv
    int uvIdx = -1;
    if (mesh.primitives[primIdx].attributes.count("TEXCOORD_0") > 0)
    {
        uvIdx = mesh.primitives[primIdx].attributes.at("TEXCOORD_0");
        const auto& uvAccessor = model.accessors[uvIdx];

        assert(uvAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "The uv accessor data type should be float.");
        assert(uvAccessor.type == TINYGLTF_TYPE_VEC2, "The uv accessor type should be vec2.");

        pGeoAsset->m_texCoordData.resize(2 * uvAccessor.count);
        ReadOutAccessorData(pGeoAsset->m_texCoordData.data(), uvAccessor, model.bufferViews, model.buffers);
    }
    else
    {
        // assert(false, "The loaded mesh doesn't have uv data.");
        pGeoAsset->m_texCoordData = std::vector<float>(posAccessor.count * 2, 0.f);
    }

    // Load tangent
    int tangentIdx = -1;
    if (mesh.primitives[primIdx].attributes.count("TANGENT"))
    {
        tangentIdx = mesh.primitives[primIdx].attributes.at("TANGENT");
        const auto& tangentAccessor = model.accessors[tangentIdx];

        assert(tangentAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "The tangent accessor data type should be float.");
        assert(tangentAccessor.type == TINYGLTF_TYPE_VEC4, "The tangent accessor type should be vec4.");
        assert(tangentAccessor.count == posAccessor.count, "The tangent data count should be the same as the pos data count.");

        pGeoAsset->m_tangentData.resize(4 * tangentAccessor.count);
        ReadOutAccessorData(pGeoAsset->m_tangentData.data(), tangentAccessor, model.bufferViews, model.buffers);
    }
    else
    {
        // assert(false, "The loaded mesh doesn't have tangent data.");
        pGeoAsset->m_tangentData = std::vector<float>(posAccessor.count * 4, 0.f);
    }

    return pGeoAsset;
}

Material AssetLoader::LoadOneGltfPrimMaterial(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const int meshIdx, const int primIdx)
{
    Material material = {};
    const auto& mesh = model.meshes[meshIdx];

    // Load the base color texture or set the pure color.
    // The baseColorFactor contains the red, green, blue, and alpha components of the main color of the material.
    int materialIdx = mesh.primitives[primIdx].material;

    ConstMaterialData fallbackConstMaterialData = Material::DefaultConstMaterialData();
    TextureAsset* pBaseColorTex = nullptr;         // TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE (5121), 4 components.
    TextureAsset* pMetallicRoughnessTex = nullptr; // R32G32_SFLOAT
    TextureAsset* pNormalTex = nullptr;            // R32G32B32_SFLOAT
    TextureAsset* pOcclusionTex = nullptr;         // R32_SFLOAT
    TextureAsset* pEmissiveTex = nullptr;          // Currently don't support.

    if (materialIdx != -1)
    {
        const auto& material = model.materials[materialIdx];
        // A texture binding is defined by an index of a texture object and an optional index of texture coordinates.
        // Its green channel contains roughness values and its blue channel contains metalness values.
        int baseColorTexIdx = material.pbrMetallicRoughness.baseColorTexture.index;
        int metallicRoughnessTexIdx = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
        int occlusionTexIdx = material.occlusionTexture.index;
        int normalTexIdx = material.normalTexture.index;
        // material.emissiveTexture -- Let forget emissive. The renderer doesn't support emissive textures.

        if (baseColorTexIdx == -1)
        {
            fallbackConstMaterialData.albedo[0] = material.pbrMetallicRoughness.baseColorFactor[0];
            fallbackConstMaterialData.albedo[1] = material.pbrMetallicRoughness.baseColorFactor[1];
            fallbackConstMaterialData.albedo[2] = material.pbrMetallicRoughness.baseColorFactor[2];
        }
        else
        {
            pBaseColorTex = new TextureAsset();

            const auto& baseColorTex = model.textures[baseColorTexIdx];
            int baseColorTexImgIdx = baseColorTex.source;

            // This model has a base color texture.
            const auto& baseColorImg = model.images[baseColorTexImgIdx];

            pBaseColorTex->imgInfo.pixWidth = baseColorImg.width;
            pBaseColorTex->imgInfo.pixHeight = baseColorImg.height;
            pBaseColorTex->imgInfo.dataVec = baseColorImg.image;
            pBaseColorTex->imgInfo.textureFormat = GLTFTextureFormatToRendererTextureFormat(baseColorImg.pixel_type, baseColorImg.component);
            pBaseColorTex->imgInfo.gpuResource = nullptr;
            pBaseColorTex->imgInfo.isSentToGpu = false;
            pBaseColorTex->imgInfo.texDescHeap = nullptr;
            pBaseColorTex->imgInfo.arrayLayerCnt = 1;
            pBaseColorTex->imgInfo.mipLevelCnt = 1;
            if (baseColorTex.sampler >= 0)
            {
                pBaseColorTex->imgInfo.wrapModeHorizontal = GltfSamplerWrapToInternalWrapMode(model.samplers[baseColorTex.sampler].wrapS);
                pBaseColorTex->imgInfo.wrapModeVertical = GltfSamplerWrapToInternalWrapMode(model.samplers[baseColorTex.sampler].wrapT);
            }
            else
            {
                pBaseColorTex->imgInfo.wrapModeHorizontal = TexWrapMode::REPEAT;
                pBaseColorTex->imgInfo.wrapModeVertical = TexWrapMode::REPEAT;
            }

            assert(baseColorImg.component == 4, "All textures should have 4 components.");
            assert(baseColorImg.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, "All textures' each component should be a byte.");
        }

        // The textures for metalness and roughness properties are packed together in a single texture called metallicRoughnessTexture.Its green
        // channel contains roughness values and its blue channel contains metalness values.This texture MUST be encoded with linear transfer function
        // and MAY use more than 8 bits per channel.
        if (metallicRoughnessTexIdx == -1)
        {
            fallbackConstMaterialData.metallic = material.pbrMetallicRoughness.metallicFactor;
            fallbackConstMaterialData.roughness = material.pbrMetallicRoughness.roughnessFactor;
        }
        else
        {
            pMetallicRoughnessTex = new TextureAsset();

            const auto& metallicRoughnessTex = model.textures[metallicRoughnessTexIdx];
            int metallicRoughnessTexImgIdx = metallicRoughnessTex.source;
            const auto& metallicRoughnessImg = model.images[metallicRoughnessTexImgIdx];

            pMetallicRoughnessTex->imgInfo.pixWidth = metallicRoughnessImg.width;
            pMetallicRoughnessTex->imgInfo.pixHeight = metallicRoughnessImg.height;
            pMetallicRoughnessTex->imgInfo.dataVec = metallicRoughnessImg.image;
            pMetallicRoughnessTex->imgInfo.textureFormat = GLTFTextureFormatToRendererTextureFormat(metallicRoughnessImg.pixel_type, metallicRoughnessImg.component);
            pMetallicRoughnessTex->imgInfo.gpuResource = nullptr;
            pMetallicRoughnessTex->imgInfo.isSentToGpu = false;
            pMetallicRoughnessTex->imgInfo.texDescHeap = nullptr;
            pMetallicRoughnessTex->imgInfo.arrayLayerCnt = 1;
            pMetallicRoughnessTex->imgInfo.mipLevelCnt = 1;
            if (metallicRoughnessTex.sampler >= 0)
            {
                pMetallicRoughnessTex->imgInfo.wrapModeHorizontal = GltfSamplerWrapToInternalWrapMode(model.samplers[metallicRoughnessTex.sampler].wrapS);
                pMetallicRoughnessTex->imgInfo.wrapModeVertical = GltfSamplerWrapToInternalWrapMode(model.samplers[metallicRoughnessTex.sampler].wrapT);
            }
            else
            {
                pMetallicRoughnessTex->imgInfo.wrapModeHorizontal = TexWrapMode::REPEAT;
                pMetallicRoughnessTex->imgInfo.wrapModeVertical = TexWrapMode::REPEAT;
            }

            assert(metallicRoughnessImg.component == 4, "All textures should have 4 components.");
            assert(metallicRoughnessImg.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, "All textures' each component should be a byte.");
        }

        // No fallback for normal texture. If we don't have normal texture, then we will just use the normal data from the geometry asset.
        if (normalTexIdx != -1)
        {
            pNormalTex = new TextureAsset();

            const auto& normalTex = model.textures[normalTexIdx];
            int normalTexImgIdx = normalTex.source;
            const auto& normalImg = model.images[normalTexImgIdx];

            pNormalTex->imgInfo.pixWidth = normalImg.width;
            pNormalTex->imgInfo.pixHeight = normalImg.height;
            pNormalTex->imgInfo.dataVec = normalImg.image;
            pNormalTex->imgInfo.textureFormat = GLTFTextureFormatToRendererTextureFormat(normalImg.pixel_type, normalImg.component);
            pNormalTex->imgInfo.gpuResource = nullptr;
            pNormalTex->imgInfo.isSentToGpu = false;
            pNormalTex->imgInfo.texDescHeap = nullptr;
            pNormalTex->imgInfo.arrayLayerCnt = 1;
            pNormalTex->imgInfo.mipLevelCnt = 1;
            if (normalTex.sampler >= 0)
            {
                pNormalTex->imgInfo.wrapModeHorizontal = GltfSamplerWrapToInternalWrapMode(model.samplers[normalTex.sampler].wrapS);
                pNormalTex->imgInfo.wrapModeVertical = GltfSamplerWrapToInternalWrapMode(model.samplers[normalTex.sampler].wrapT);
            }
            else
            {
                pNormalTex->imgInfo.wrapModeHorizontal = TexWrapMode::REPEAT;
                pNormalTex->imgInfo.wrapModeVertical = TexWrapMode::REPEAT;
            }

            assert(normalImg.component == 4, "All textures should have 4 components.");
            assert(normalImg.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, "All textures' each component should be a byte.");
        }

        // The occlusion texture; it indicates areas that receive less indirect lighting from ambient sources.
        // Direct lighting is not affected.The red channel of the texture encodes the occlusion value,
        // where 0.0 means fully - occluded area(no indirect lighting) and 1.0 means not occluded area(full indirect lighting).
        if (occlusionTexIdx != -1)
        {
            pOcclusionTex = new TextureAsset();

            const auto& occlusionTex = model.textures[occlusionTexIdx];
            int occlusionTexImgIdx = occlusionTex.source;
            const auto& occlusionImg = model.images[occlusionTexImgIdx];

            pOcclusionTex->imgInfo.pixWidth = occlusionImg.width;
            pOcclusionTex->imgInfo.pixHeight = occlusionImg.height;
            pOcclusionTex->imgInfo.dataVec = occlusionImg.image;
            pOcclusionTex->imgInfo.textureFormat = GLTFTextureFormatToRendererTextureFormat(occlusionImg.pixel_type, occlusionImg.component);
            pOcclusionTex->imgInfo.gpuResource = nullptr;
            pOcclusionTex->imgInfo.isSentToGpu = false;
            pOcclusionTex->imgInfo.texDescHeap = nullptr;
            pOcclusionTex->imgInfo.arrayLayerCnt = 1;
            pOcclusionTex->imgInfo.mipLevelCnt = 1;
            if (occlusionTex.sampler >= 0)
            {
                pOcclusionTex->imgInfo.wrapModeHorizontal = GltfSamplerWrapToInternalWrapMode(model.samplers[occlusionTex.sampler].wrapS);
                pOcclusionTex->imgInfo.wrapModeVertical = GltfSamplerWrapToInternalWrapMode(model.samplers[occlusionTex.sampler].wrapT);
            }
            else
            {
                pOcclusionTex->imgInfo.wrapModeHorizontal = TexWrapMode::REPEAT;
                pOcclusionTex->imgInfo.wrapModeVertical = TexWrapMode::REPEAT;
            }

            assert(occlusionImg.component == 4, "All textures should have 4 components.");
            assert(occlusionImg.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, "All textures' each component should be a byte.");
        }
    }

    material.Init(fallbackConstMaterialData, pBaseColorTex, pMetallicRoughnessTex, pNormalTex, pOcclusionTex, pEmissiveTex);

    return material;
}
