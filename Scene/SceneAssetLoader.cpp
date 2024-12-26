#include "SceneAssetLoader.h"
#include "Level.h"
#include "Mesh.h"
#include "Lights.h"
#include "Camera.h"
#include "yaml-cpp/yaml.h"
#include "../Utils/MathUtils.h"
#include "../Utils/StrPathUtils.h"
#include "../Utils/GltfUtils.h"
#include <iostream>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ThirdParty/TinyGltf/tiny_gltf.h"

SceneAssetLoader* SceneAssetLoader::m_pThis = nullptr;

extern AssetManager* g_pAssetManager;

SceneAssetLoader::SceneAssetLoader()
{
   m_pThis = this;
}

SceneAssetLoader::~SceneAssetLoader()
{
}

void SceneAssetLoader::LoadAsLevel(const std::string& fileNamePath, Level* o_pLevel)
{
    // Load the scene file into the level
    YAML::Node config = YAML::LoadFile(fileNamePath.c_str());
    std::string sceneType = config["SceneType"].as<std::string>();

    YAML::Node sceneGraph = config["SceneGraph"];
    std::vector<float> bgColor = config["BackgroundColor"].as<std::vector<float>>();
    memcpy(o_pLevel->m_backgroundColor, bgColor.data(), sizeof(o_pLevel->m_backgroundColor));

    m_currentScenePath = fileNamePath;

    uint32_t ambientLightCnt = 0;

    for (const auto& itr : sceneGraph)
    {
        const std::string objName = itr.first.as<std::string>();
        const std::string type = itr.second["Type"].as<std::string>();
        if (type.compare("StaticMesh") == 0)
        {
            o_pLevel->LoadObject(objName, itr.second, StaticMesh::Deseralize);
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
}

void SceneAssetLoader::LoadStaticMesh(const std::string& fileNamePath, StaticMesh* pStaticMesh)
{
    //#TODO: Check the file extension and call the appropriate loader. E.g. OpenUSD
    //#TODO: We may want to use FastGltf instead of TinyGltf.

    if (g_pAssetManager->IsStaticMeshAssetLoaded(fileNamePath))
    {
        g_pAssetManager->LoadStaticMeshAssets(fileNamePath, pStaticMesh);
    }
    else
    {
        LoadTinyGltf(fileNamePath, pStaticMesh);
    }
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

void SceneAssetLoader::LoadTinyGltf(const std::string& fileNamePath, StaticMesh* pStaticMesh)
{
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
    // pStaticMesh->m_primitiveAssets.resize(mesh.primitives.size());

    for (uint32_t i = 0; i < mesh.primitives.size(); i++)
    {
        const auto& primitive = mesh.primitives[i];
        PrimitiveAsset* pPrimitiveAsset = new PrimitiveAsset();

        // Load pos
        int posIdx = mesh.primitives[i].attributes.at("POSITION");
        const auto& posAccessor = model.accessors[posIdx];

        assert(posAccessor.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT, "The pos accessor data type should be float.");
        assert(posAccessor.type == TINYGLTF_TYPE_VEC3, "The pos accessor type should be vec3.");

        const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
        // Assmue the data and element type of the position is float3
        pPrimitiveAsset->m_posData.resize(3 * posAccessor.count);
        ReadOutAccessorData(pPrimitiveAsset->m_posData.data(), posAccessor, model.bufferViews, model.buffers);

        // Load indices
        int indicesIdx = mesh.primitives[i].indices;
        const auto& idxAccessor = model.accessors[indicesIdx];

        assert(idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT ||
               idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT, "The idx accessor data type should be uint16/32.");
        assert(idxAccessor.type == TINYGLTF_TYPE_SCALAR, "The idx accessor type should be scalar.");

        if (idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT)
        {
            pPrimitiveAsset->m_idxType = false;
            pPrimitiveAsset->m_idxDataUint16.resize(idxAccessor.count);
            ReadOutAccessorData(pPrimitiveAsset->m_idxDataUint16.data(), idxAccessor, model.bufferViews, model.buffers);
        }
        else if (idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT)
        {
            pPrimitiveAsset->m_idxType = true;
            pPrimitiveAsset->m_idxDataUint32.resize(idxAccessor.count);
            ReadOutAccessorData(pPrimitiveAsset->m_idxDataUint32.data(), idxAccessor, model.bufferViews, model.buffers);
        }
        pPrimitiveAsset->m_idxCnt = idxAccessor.count;

        // Load normal
        int normalIdx = -1;
        if (mesh.primitives[i].attributes.count("NORMAL") > 0)
        {
            normalIdx = mesh.primitives[i].attributes.at("NORMAL");
            const auto& normalAccessor = model.accessors[normalIdx];

            assert(normalAccessor.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT, "The normal accessor data type should be float.");
            assert(normalAccessor.type == TINYGLTF_TYPE_VEC3, "The normal accessor type should be vec3.");

            pPrimitiveAsset->m_normalData.resize(3 * normalAccessor.count);
            ReadOutAccessorData(pPrimitiveAsset->m_normalData.data(), normalAccessor, model.bufferViews, model.buffers);
        }
        else
        {
            // If we don't have any normal geo data, then we will just apply the first triangle's normal to all the other
            // triangles/vertices.
            uint16_t idx0 = pPrimitiveAsset->m_idxDataUint16[0];
            float vertPos0[3] = { pPrimitiveAsset->m_posData[3 * idx0], pPrimitiveAsset->m_posData[3 * idx0 + 1], pPrimitiveAsset->m_posData[3 * idx0 + 2] };

            uint16_t idx1 = pPrimitiveAsset->m_idxDataUint16[1];
            float vertPos1[3] = { pPrimitiveAsset->m_posData[3 * idx1], pPrimitiveAsset->m_posData[3 * idx1 + 1], pPrimitiveAsset->m_posData[3 * idx1 + 2] };

            uint16_t idx2 = pPrimitiveAsset->m_idxDataUint16[2];
            float vertPos2[3] = { pPrimitiveAsset->m_posData[3 * idx2], pPrimitiveAsset->m_posData[3 * idx2 + 1], pPrimitiveAsset->m_posData[3 * idx2 + 2] };

            float v1[3] = { vertPos1[0] - vertPos0[0], vertPos1[1] - vertPos0[1], vertPos1[2] - vertPos0[2] };
            float v2[3] = { vertPos2[0] - vertPos0[0], vertPos2[1] - vertPos0[1], vertPos2[2] - vertPos0[2] };

            float autoGenNormal[3] = { 0.f };
            CrossProductVec3(v1, v2, autoGenNormal);
            NormalizeVec(autoGenNormal, 3);

            pPrimitiveAsset->m_normalData.resize(3 * posAccessor.count);
            for (uint32_t i = 0; i < posAccessor.count; i++)
            {
                uint32_t normalStartingIdx = i * 3;
                pPrimitiveAsset->m_normalData[normalStartingIdx] = autoGenNormal[0];
                pPrimitiveAsset->m_normalData[normalStartingIdx + 1] = autoGenNormal[1];
                pPrimitiveAsset->m_normalData[normalStartingIdx + 2] = autoGenNormal[2];
            }
        }

        // Load uv
        int uvIdx = -1;
        if (mesh.primitives[i].attributes.count("TEXCOORD_0") > 0)
        {
            uvIdx = mesh.primitives[i].attributes.at("TEXCOORD_0");
            const auto& uvAccessor = model.accessors[uvIdx];

            assert(uvAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "The uv accessor data type should be float.");
            assert(uvAccessor.type == TINYGLTF_TYPE_VEC2, "The uv accessor type should be vec2.");

            pPrimitiveAsset->m_texCoordData.resize(2 * uvAccessor.count);
            ReadOutAccessorData(pPrimitiveAsset->m_texCoordData.data(), uvAccessor, model.bufferViews, model.buffers);
        }
        else
        {
            // assert(false, "The loaded mesh doesn't have uv data.");
            pPrimitiveAsset->m_texCoordData = std::vector<float>(posAccessor.count * 2, 0.f);
        }

        // Load tangent
        int tangentIdx = -1;
        if (mesh.primitives[i].attributes.count("TANGENT"))
        {
            tangentIdx = mesh.primitives[i].attributes.at("TANGENT");
            const auto& tangentAccessor = model.accessors[tangentIdx];

            assert(tangentAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "The tangent accessor data type should be float.");
            assert(tangentAccessor.type == TINYGLTF_TYPE_VEC4, "The tangent accessor type should be vec4.");
            assert(tangentAccessor.count == posAccessor.count, "The tangent data count should be the same as the pos data count.");

            pPrimitiveAsset->m_tangentData.resize(4 * tangentAccessor.count);
            ReadOutAccessorData(pPrimitiveAsset->m_tangentData.data(), tangentAccessor, model.bufferViews, model.buffers);
        }
        else
        {
            // assert(false, "The loaded mesh doesn't have tangent data.");
            pPrimitiveAsset->m_tangentData = std::vector<float>(posAccessor.count * 4, 0.f);
        }

        // Load the base color texture or create a default pure color texture.
        // The baseColorFactor contains the red, green, blue, and alpha components of the main color of the material.
        int materialIdx = mesh.primitives[i].material;

        // All the textures are 4 components R8G8B8A8 textures.
        auto& pfnSetDefaultBaseColor = [](PrimitiveAsset& meshPrimitive) {
            meshPrimitive.m_baseColorTex.pixHeight = 1;
            meshPrimitive.m_baseColorTex.pixWidth = 1;
            meshPrimitive.m_baseColorTex.componentCnt = 4;
            meshPrimitive.m_baseColorTex.dataVec = std::vector<uint8_t>(4, 255);
            };

        auto& pfnSetDefaultMetallicRoughness = [](PrimitiveAsset& meshPrimitive) {
            float defaultMetallicRoughness[4] = { 0.f, 1.f, 0.f, 0.f };
            meshPrimitive.m_metallicRoughnessTex.pixHeight = 1;
            meshPrimitive.m_metallicRoughnessTex.pixWidth = 1;
            meshPrimitive.m_metallicRoughnessTex.componentCnt = 4;
            meshPrimitive.m_metallicRoughnessTex.dataVec = std::vector<uint8_t>(sizeof(defaultMetallicRoughness), 0);
            memcpy(meshPrimitive.m_metallicRoughnessTex.dataVec.data(), defaultMetallicRoughness, sizeof(defaultMetallicRoughness));
            };

        auto& pfnSetDefaultOcclusion = [](PrimitiveAsset& meshPrimitive) {
            float defaultOcclusion[4] = { 1.f, 0.f, 0.f, 0.f };
            meshPrimitive.m_occlusionTex.pixHeight = 1;
            meshPrimitive.m_occlusionTex.pixWidth = 1;
            meshPrimitive.m_occlusionTex.componentCnt = 4;
            meshPrimitive.m_occlusionTex.dataVec = std::vector<uint8_t>(sizeof(defaultOcclusion), 0);
            memcpy(meshPrimitive.m_occlusionTex.dataVec.data(), &defaultOcclusion, sizeof(defaultOcclusion));
            };

        auto& pfnSetDefaultNormal = [](PrimitiveAsset& meshPrimitive) {
            float defaultNormal[3] = { 0.f, 0.f, 1.f };
            meshPrimitive.m_normalTex.pixHeight = 1;
            meshPrimitive.m_normalTex.pixWidth = 1;
            meshPrimitive.m_normalTex.componentCnt = 3;
            meshPrimitive.m_normalTex.dataVec = std::vector<uint8_t>(sizeof(defaultNormal), 0);
            memcpy(meshPrimitive.m_normalTex.dataVec.data(), defaultNormal, sizeof(defaultNormal));
            };

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
                pfnSetDefaultBaseColor(*pPrimitiveAsset);
            }
            else
            {
                // A texture is defined by an image index, denoted by the source property and a sampler index (sampler).
                // Assmue that all textures are 8 bits per channel. They are all xxx / 255. They all have 4 components.
                const auto& baseColorTex = model.textures[baseColorTexIdx];
                int baseColorTexImgIdx = baseColorTex.source;

                // This model has a base color texture.
                const auto& baseColorImg = model.images[baseColorTexImgIdx];

                pPrimitiveAsset->m_baseColorTex.pixWidth = baseColorImg.width;
                pPrimitiveAsset->m_baseColorTex.pixHeight = baseColorImg.height;
                pPrimitiveAsset->m_baseColorTex.componentCnt = baseColorImg.component;
                pPrimitiveAsset->m_baseColorTex.dataVec = baseColorImg.image;
                if (baseColorTex.sampler >= 0)
                {
                    pPrimitiveAsset->m_baseColorTex.wrapModeHorizontal = GltfSamplerWrapToInternalWrapMode(model.samplers[baseColorTex.sampler].wrapS);
                    pPrimitiveAsset->m_baseColorTex.wrapModeVertical   = GltfSamplerWrapToInternalWrapMode(model.samplers[baseColorTex.sampler].wrapT);
                }
                else
                {
                    pPrimitiveAsset->m_baseColorTex.wrapModeHorizontal = TexWrapMode::REPEAT;
                    pPrimitiveAsset->m_baseColorTex.wrapModeVertical   = TexWrapMode::REPEAT;
                }
                
                assert(baseColorImg.component == 4, "All textures should have 4 components.");
                assert(baseColorImg.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, "All textures' each component should be a byte.");
            }

            // The textures for metalness and roughness properties are packed together in a single texture called metallicRoughnessTexture.Its green
            // channel contains roughness values and its blue channel contains metalness values.This texture MUST be encoded with linear transfer function
            // and MAY use more than 8 bits per channel.
            if (metallicRoughnessTexIdx == -1)
            {
                pfnSetDefaultMetallicRoughness(*pPrimitiveAsset);
            }
            else
            {
                const auto& metallicRoughnessTex = model.textures[metallicRoughnessTexIdx];
                int metallicRoughnessTexImgIdx = metallicRoughnessTex.source;

                const auto& metallicRoughnessImg = model.images[metallicRoughnessTexImgIdx];

                pPrimitiveAsset->m_metallicRoughnessTex.pixWidth = metallicRoughnessImg.width;
                pPrimitiveAsset->m_metallicRoughnessTex.pixHeight = metallicRoughnessImg.height;
                pPrimitiveAsset->m_metallicRoughnessTex.componentCnt = metallicRoughnessImg.component;
                pPrimitiveAsset->m_metallicRoughnessTex.dataVec = metallicRoughnessImg.image;
                if (metallicRoughnessTex.sampler >= 0)
                {
                    pPrimitiveAsset->m_metallicRoughnessTex.wrapModeHorizontal = GltfSamplerWrapToInternalWrapMode(model.samplers[metallicRoughnessTex.sampler].wrapS);
                    pPrimitiveAsset->m_metallicRoughnessTex.wrapModeVertical   = GltfSamplerWrapToInternalWrapMode(model.samplers[metallicRoughnessTex.sampler].wrapT);
                }
                else
                {
                    pPrimitiveAsset->m_metallicRoughnessTex.wrapModeHorizontal = TexWrapMode::REPEAT;
                    pPrimitiveAsset->m_metallicRoughnessTex.wrapModeVertical   = TexWrapMode::REPEAT;
                }

                assert(metallicRoughnessImg.component == 4, "All textures should have 4 components.");
                assert(metallicRoughnessImg.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, "All textures' each component should be a byte.");
            }

            if (normalTexIdx == -1)
            {
                pfnSetDefaultNormal(*pPrimitiveAsset);
            }
            else
            {
                const auto& normalTex = model.textures[normalTexIdx];
                int normalTexImgIdx = normalTex.source;

                const auto& normalImg = model.images[normalTexImgIdx];

                pPrimitiveAsset->m_normalTex.pixWidth = normalImg.width;
                pPrimitiveAsset->m_normalTex.pixHeight = normalImg.height;
                pPrimitiveAsset->m_normalTex.componentCnt = normalImg.component;
                pPrimitiveAsset->m_normalTex.dataVec = normalImg.image;
                if (normalTex.sampler >= 0)
                {
                    pPrimitiveAsset->m_normalTex.wrapModeHorizontal = GltfSamplerWrapToInternalWrapMode(model.samplers[normalTex.sampler].wrapS);
                    pPrimitiveAsset->m_normalTex.wrapModeVertical = GltfSamplerWrapToInternalWrapMode(model.samplers[normalTex.sampler].wrapT);
                }
                else
                {
                    pPrimitiveAsset->m_normalTex.wrapModeHorizontal = TexWrapMode::REPEAT;
                    pPrimitiveAsset->m_normalTex.wrapModeVertical = TexWrapMode::REPEAT;
                }

                assert(normalImg.component == 4, "All textures should have 4 components.");
                assert(normalImg.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, "All textures' each component should be a byte.");
            }

            // The occlusion texture; it indicates areas that receive less indirect lighting from ambient sources.
            // Direct lighting is not affected.The red channel of the texture encodes the occlusion value,
            // where 0.0 means fully - occluded area(no indirect lighting) and 1.0 means not occluded area(full indirect lighting).
            if (occlusionTexIdx == -1)
            {
                pfnSetDefaultOcclusion(*pPrimitiveAsset);
            }
            else
            {
                const auto& occlusionTex = model.textures[occlusionTexIdx];
                int occlusionTexImgIdx = occlusionTex.source;

                const auto& occlusionImg = model.images[occlusionTexImgIdx];

                pPrimitiveAsset->m_occlusionTex.pixWidth = occlusionImg.width;
                pPrimitiveAsset->m_occlusionTex.pixHeight = occlusionImg.height;
                pPrimitiveAsset->m_occlusionTex.componentCnt = occlusionImg.component;
                pPrimitiveAsset->m_occlusionTex.dataVec = occlusionImg.image;
                if (occlusionTex.sampler >= 0)
                {
                    pPrimitiveAsset->m_occlusionTex.wrapModeHorizontal = GltfSamplerWrapToInternalWrapMode(model.samplers[occlusionTex.sampler].wrapS);
                    pPrimitiveAsset->m_occlusionTex.wrapModeVertical = GltfSamplerWrapToInternalWrapMode(model.samplers[occlusionTex.sampler].wrapT);
                }
                else
                {
                    pPrimitiveAsset->m_occlusionTex.wrapModeHorizontal = TexWrapMode::REPEAT;
                    pPrimitiveAsset->m_occlusionTex.wrapModeVertical = TexWrapMode::REPEAT;
                }

                assert(occlusionImg.component == 4, "All textures should have 4 components.");
                assert(occlusionImg.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, "All textures' each component should be a byte.");
            }
        }
        else
        {
            // No material, then we will create a pure white model.
            pfnSetDefaultBaseColor(*pPrimitiveAsset);
            pfnSetDefaultMetallicRoughness(*pPrimitiveAsset);
            pfnSetDefaultOcclusion(*pPrimitiveAsset);
            pfnSetDefaultNormal(*pPrimitiveAsset);
        }

        // pPrimitiveAsset->CreateVertIdxBuffer();
        g_pAssetManager->SaveModelPrimAssetAndCreateGpuRsrc(fileNamePath, pPrimitiveAsset);
        pStaticMesh->m_primitiveAssets.push_back(pPrimitiveAsset);
    }
}

void SceneAssetLoader::LoadShaderObject(const std::string& fileNamePath, std::vector<unsigned char>& oShaderByteCode)
{
    std::ifstream inputShader(fileNamePath.c_str(), std::ios::binary | std::ios::in);
    std::vector<unsigned char> inputShaderStr(std::istreambuf_iterator<char>(inputShader), {});
    inputShader.close();
    oShaderByteCode = inputShaderStr;
}