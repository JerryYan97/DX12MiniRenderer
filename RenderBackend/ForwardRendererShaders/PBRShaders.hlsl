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

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    const float PI = 3.14159265359;

    float a = roughness*roughness + 0.001;
	
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
	
    // from tangent-space vector to world-space sample vector
    float3 up        = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent   = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
	
    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySchlickGGXDirectLight(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159265359 * denom * denom;
	
    return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float GeometrySmithDirectLight(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGXDirectLight(NdotV, roughness);
    float ggx1 = GeometrySchlickGGXDirectLight(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 FresnelSchlick(float lightCosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - lightCosTheta, 0.0, 1.0), 5.0);
}

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 uv       : TEXCOORD;
};

struct PSInput
{
    float4 pos      : SV_POSITION;
    float4 worldPos : POSITION0;
    float4 normal   : NORMAL0;
    float4 tangent  : TANGENT0;
    float2 uv       : TEXCOORD0;
};

cbuffer VsObjectBuffer : register(b0)
{
    float4x4 modelMat;
};

cbuffer VsSceneBuffer : register(b1)
{
    float4x4 vpMat;
};

PSInput VSMain(VSInput i_vertInput)
{
    PSInput result = (PSInput)0;

    float4x4 mvpMat = mul(vpMat, modelMat);
    
    result.pos      = mul(mvpMat, float4(i_vertInput.position, 1.0));
    result.normal   = mul(modelMat, float4(i_vertInput.normal, 0.0));
    result.worldPos = mul(modelMat, float4(i_vertInput.position, 1.0));
    result.tangent  = mul(modelMat, float4(i_vertInput.tangent.xyz, 0.0));
    result.uv       = i_vertInput.uv;
    
    return result;
}

static const uint ALBEDO_MASK            = 1;
static const uint NORMAL_MASK            = 2;
static const uint ROUGHNESS_METALIC_MASK = 4;
static const uint AO_MASK                = 8;

cbuffer PsMaterialBuffer : register(b2)
{
    float4 constAlbedo;
    float4 metalicRoughness;
    uint materialMask; // 0 -- Constant
}

cbuffer PsSceneBuffer : register(b3)
{
    float3 lightPositions[4];
    float3 lightRadiance[4];
    float4 cameraPos;    // one padding float
    float4 ambientLight; // one padding float
    uint4  extraIntData; // (0): Point Light Counts; (1): ;
}

Texture2D    i_baseColorTexture      : register(t0);
SamplerState i_baseColorSamplerState : register(s0);

Texture2D    i_normalTexture      : register(t1);
SamplerState i_normalSamplerState : register(s1);

// The textures for metalness and roughness properties are packed together in a single texture called
// metallicRoughnessTexture. Its green channel contains roughness values and its blue channel contains metalness
// values.
Texture2D    i_roughnessMetallicTexture      : register(t2);
SamplerState i_roughnessMetallicSamplerState : register(s2);

Texture2D    i_occlusionTexture      : register(t3);
SamplerState i_occlusionSamplerState : register(s3);


float4 PSMain(PSInput input) : SV_TARGET
{
	// Gold
    float3 texAlbedo = i_baseColorTexture.Sample(i_baseColorSamplerState, input.uv).xyz;
    float3 sphereRefAlbedo = constAlbedo.xyz; // F0
    float3 sphereDifAlbedo = constAlbedo.xyz;
    
    sphereDifAlbedo = texAlbedo;
    sphereRefAlbedo = texAlbedo;

    float3 wo = normalize(cameraPos.xyz - input.worldPos.xyz);
	
    float3 worldNormal = normalize(input.normal.xyz);

    float viewNormalCosTheta = max(dot(worldNormal, wo), 0.0);

    float metallic = metalicRoughness.x;
    float roughness = metalicRoughness.y;

    float3 Lo = float3(0.0, 0.0, 0.0); // Output light values to the view direction.
    uint lightCnt = extraIntData.x;
    for (int i = 0; i < lightCnt; i++)
    {        
        float3 lightColor = lightRadiance[i];
        float3 lightPos = lightPositions[i];
        float3 wi = normalize(lightPos - input.worldPos.xyz);
        float3 H = normalize(wi + wo);
        float distance = length(lightPos - input.worldPos.xyz);

        float attenuation = 1.0 / (distance * distance);
        float3 radiance = lightColor * attenuation;

        float lightNormalCosTheta = max(dot(worldNormal, wi), 0.0);

        float NDF = DistributionGGX(worldNormal, H, roughness);
        float G = GeometrySmithDirectLight(worldNormal, wo, wi, roughness);

        float3 F0 = float3(0.04, 0.04, 0.04);
        F0 = lerp(F0, sphereRefAlbedo, float3(metallic, metallic, metallic));
        float3 F = FresnelSchlick(max(dot(H, wo), 0.0), F0);

        float3 NFG = NDF * F * G;

        float denominator = 4.0 * viewNormalCosTheta * lightNormalCosTheta + 0.0001;

        float3 specular = NFG / denominator;

        float3 kD = float3(1.0, 1.0, 1.0) - F; // The amount of light goes into the material.
        kD *= (1.0 - metallic);

        Lo += (kD * (sphereDifAlbedo / 3.14159265359) + specular) * radiance * lightNormalCosTheta;
    }

    float3 ambient = ambientLight.xyz * sphereRefAlbedo;
    float3 color = ambient + Lo;
	
    // Gamma Correction
    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    
    return float4(color, 1.0);
}
