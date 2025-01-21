struct Payload
{
    float3 color;
    bool allowReflection;
    bool missed;
};

cbuffer CameraConstantBuffer : register(b0)
{
    float4 cameraPos;
    float4 cameraDir;
    float4 cameraUp;
    float4 cameraRight;
    float4 cameraInfo; // x: fov, y: near, z: far.
};

struct ConstantMaterialData
{
    float4 albedo;
    float4 metallicRoughness;
};

static const float3 skyTop = float3(0.24, 0.44, 0.72);
static const float3 skyBottom = float3(0.75, 0.86, 0.93);

// 0-7 bits are for material textures mask. E.g. Reused from PBRShaders.hlsl.
// 8-31 bits are for material index.
static dword MATERIAL_TEX_MASK = 0xFF;

static const uint SAMPLE_COUNT = 128;

RaytracingAccelerationStructure scene : register(t0);
StructuredBuffer<dword> instsMaterialsMasks : register(t1);
StructuredBuffer<ConstantMaterialData> cnstMaterials : register(t2);

RWTexture2D<float4> uav : register(u0);

float rand_1_05(in float2 uv)
{
    float2 noise = (frac(sin(dot(uv ,float2(12.9898,78.233)*2.0)) * 43758.5453));
    return abs(noise.x + noise.y) * 0.5;
}

float2 rand_2_10(in float2 uv) {
    float noiseX = (frac(sin(dot(uv, float2(12.9898,78.233) * 2.0)) * 43758.5453));
    float noiseY = sqrt(1 - noiseX * noiseX);
    return float2(noiseX, noiseY);
}

[shader("raygeneration")]
void RayGeneration()
{
    uint2 idx = DispatchRaysIndex().xy;
    float2 size = DispatchRaysDimensions().xy;
    float2 uv = idx / size;

    float fov = cameraInfo.x;
    float near = cameraInfo.y;
    float far = cameraInfo.z;

    float topOffset = tan(fov / 2.0) * near;
    float rightOffset = topOffset * (size.x / size.y);

    float3 cameraDirCenter = cameraPos.xyz +
                             cameraDir.xyz * near;

    float3 res = float3(0, 0, 0);
    for(uint i = 0; i < SAMPLE_COUNT; i++)
    {
        float2 noiseInput = uv * (i + 1);
        float2 noise = rand_2_10(noiseInput) - float2(0.5, 0.5);
        // float2 noise = float2(0, 0);
        float2 jitteredUV = uv + (noise / size);
        float3 targetOffset = float3((jitteredUV.x * 2.0 - 1.0) * rightOffset,
                                 (1.0 - jitteredUV.y * 2.0) * topOffset,
                                 0.0);
        targetOffset = targetOffset + cameraDirCenter;
        
        RayDesc ray;
        ray.Origin = cameraPos.xyz;
        ray.Direction = targetOffset - cameraPos.xyz;
        ray.TMin = 0.001;
        ray.TMax = 1000;

        Payload payload;
        payload.allowReflection = true;
        payload.missed = false;

        TraceRay(scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

        res += float3(payload.color);
    } 

    uav[idx] = float4(res / (float)SAMPLE_COUNT, 1);
}

[shader("miss")]
void Miss(inout Payload payload)
{
    float slope = normalize(WorldRayDirection()).y;
    float t = saturate(slope * 5 + 0.5);
    payload.color = lerp(skyBottom, skyTop, t);

    payload.missed = true;
}

[shader("closesthit")]
void ClosestHit(inout Payload payload,
                BuiltInTriangleIntersectionAttributes attrib)
{
    uint instId = InstanceID();
    dword instMaterialMask = instsMaterialsMasks[instId];

    // If there is any texture, then the material is not constant.
    bool isConstantMaterial = ((instMaterialMask & MATERIAL_TEX_MASK) > 0) ? false : true;
    if(isConstantMaterial)
    {
        uint instMaterialIdx = instMaterialMask >> 8;
        ConstantMaterialData cnstMaterialData = cnstMaterials[instMaterialIdx];
        payload.color = cnstMaterialData.albedo.xyz;
    }
    else
    {
        payload.color = float3(1, 0, 1);
    }
}
