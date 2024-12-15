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

PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
    PSInput result;

    // float4x4 mvpMat = mul(vpMat, modelMat);
    
    // result.position = mul(mvpMat, position);
    // result.position = position;
    result.position = float4(position.xyz, 1.0);
    result.position = mul(vpMat, mul(modelMat, result.position));
    result.color = color;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
