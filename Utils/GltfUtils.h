#pragma once
#include <cstdint>
#include "../ThirdParty/TinyGltf/tiny_gltf.h"
// #include "tiny_gltf.h"

uint32_t GetAccessorDataBytes(const tinygltf::Accessor& accessor);

void ReadOutAccessorData(void*                              pDst,
                         const tinygltf::Accessor&          accessor,
                         std::vector<tinygltf::BufferView>& bufferViews,
                         std::vector<tinygltf::Buffer>&     buffers);
/*
namespace SharedLib
{
    uint32_t GetAccessorDataBytes(const tinygltf::Accessor& accessor);

    void ReadOutAccessorData(void*                              pDst,
                             const tinygltf::Accessor&          accessor,
                             std::vector<tinygltf::BufferView>& bufferViews,
                             std::vector<tinygltf::Buffer>&     buffers);

    void GetNodesModelMats(const tinygltf::Model& model, std::vector<float>& matsVec);

    int GetArmatureNodeIdx(const tinygltf::Model& model);
}
*/