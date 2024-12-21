//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#pragma pack_matrix(row_major)

struct VSInput
{
    float4 position : POSITION;
    float4 normal : NORMAL0;
    float4 tangent : TANGENT0;
    float2 texcoord : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

cbuffer VsUboBuffer : register(b0)
{
    float4x4 modelMat;
    float4x4 vpMat;
};

PSInput VSMain(VSInput input)
{
    PSInput result;

    // float4x4 mvpMat = mul(vpMat, modelMat);
    
    // result.position = mul(mvpMat, position);
    // result.position = position;
    result.position = float4(input.position.xyz, 1.0);
    result.position = mul(vpMat, mul(modelMat, result.position));
    result.color = float4(1.0, 1.0, 0.0, 1.0);

    return result;
}

cbuffer PsSceneBuffer : register(b1)
{
    float3 lightPositions[4];
    float4 cameraPos; // one padding float
    float4 ambientLight; // one padding float
}

cbuffer PsMaterialBuffer : register(b2)
{
    float4 constAlbedo;
    float4 metalicRoughness;
    uint materialMask; // 0 -- Constant, 1 -- Texture. No or all.
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
