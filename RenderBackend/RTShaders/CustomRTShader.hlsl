struct Payload
{
    float3 color;
    float3 radiance;
    float3 nextPos;
    float3 nextDir;
    uint recursionDepth;
    bool hitLight;
    bool missed;
};

struct VertexRaw
{
    float4 data[3];
};

struct VertexReal
{
    float3 pos;
    float3 normal;
    float4 tangent;
    float2 uv;
};

/*
const uint32_t ALBEDO_MASK            = 1;
const uint32_t NORMAL_MASK            = 2;
const uint32_t ROUGHNESS_METALIC_MASK = 4;
const uint32_t AO_MASK                = 8;
const uint32_t EMISSIVE_MASK          = 16;
const uint32_t DIELECTRIC_MASK        = 32;
const uint32_t DOUBLE_SIDE_MASK       = 64;
*/

struct InstInfo
{
    uint4 instUintInfo0; // x: material mask, y: scene vert buffer starts float, z: scene index buffer starts int, w: unused.
    float4 instFloatInfo0; // xyz: emissive radiance, w: unused.

    // Constant material info. If material mask is 0, then we will use material here.
    float4 instAlbedo;
    float4 instMetallicRoughness;
};

cbuffer FrameConstantBuffer : register(b0)
{
    float4 cameraPos;
    float4 cameraDir;
    float4 cameraUp;
    float4 cameraRight;
    float4 cameraInfo; // x: fov, y: near, z: far.
    uint4  frameUintInfo; // x: frame count; y: random number (0-100); z: low-32 current time in nanosecond.
};

static const float3 skyTop = float3(0.24, 0.44, 0.72);
static const float3 skyBottom = float3(0.75, 0.86, 0.93);
static const float REFRACTION_INDEX = 1.5;

// 0-7 bits are for material states mask. E.g. Reused from PBRShaders.hlsl.
// 8-31 bits are for material index.
static const dword MATERIAL_TEX_MASK = 0x1F;
static const dword DIELECTRIC_MASK   = 32;
static const dword DOUBLE_SIDE_MASK  = 64;

static const uint SAMPLE_COUNT = 1;
static const uint HIT_CHILD_RAY_COUNT = 4;
static const uint MAX_BOUNCE_DEPTH = 4;

RaytracingAccelerationStructure scene : register(t0);
StructuredBuffer<InstInfo> instsInfo : register(t1);

StructuredBuffer<VertexRaw> sceneVertices : register(t2);
ByteAddressBuffer sceneIndices : register(t3);

RWTexture2D<float4> uavRTPresent : register(u0);
RWTexture2D<float4> uavRTRadiance : register(u1);

void ParseVertex(in uint vertId, out float3 pos, out float3 normal, out float4 tangent, out float2 uv)
{
    VertexRaw vert = sceneVertices[vertId];
    pos = vert.data[0].xyz;
    normal = float3(vert.data[0].w, vert.data[1].xy);
    tangent = float4(vert.data[1].zw, vert.data[2].xy);
    uv = vert.data[2].zw;
}

// Load three 16 bit indices from a byte addressed buffer.
uint3 Load3x16BitIndices(uint offsetBytes)
{
    uint3 indices;

    // ByteAdressBuffer loads must be aligned at a 4 byte boundary.
    // Since we need to read three 16 bit indices: { 0, 1, 2 } 
    // aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
    // we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
    // based on first index's offsetBytes being aligned at the 4 byte boundary or not:
    //  Aligned:     { 0 1 | 2 - }
    //  Not aligned: { - 0 | 1 2 }
    const uint dwordAlignedOffset = offsetBytes & ~3;    
    const uint2 four16BitIndices = sceneIndices.Load2(dwordAlignedOffset);
 
    // Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
    if (dwordAlignedOffset == offsetBytes)
    {
        indices.x = four16BitIndices.x & 0xffff;
        indices.y = (four16BitIndices.x >> 16) & 0xffff;
        indices.z = four16BitIndices.y & 0xffff;
    }
    else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
    {
        indices.x = (four16BitIndices.x >> 16) & 0xffff;
        indices.y = four16BitIndices.y & 0xffff;
        indices.z = (four16BitIndices.y >> 16) & 0xffff;
    }

    return indices;
}

float rand_1(in float seed)
{
    return frac(sin(seed * -652.125) * 1234.9898);
}

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

// NOTE! Frac is the frac part of a float. It's always from 0 to 1.
float3 random3(in float3 p)
{   
    float a = frac(cos(dot(p,float3(23.14069263277926,2.665144142690225,-27.90511001)))*12345.6789);
    float2 bc = rand_2_10(float2(p.x + p.z, p.y));
    float d = rand_1_05(float2(p.y * p.z, p.x));
    float3 res = float3(a, bc.x, d);
    return res;
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

float3 DiffuseScatter(in float3 normal, in float3 hitPos)
{
    return float3(0.0, 0.0, 0.0);
}

float3 MetalScatter(in float3 normal, in float3 hitPos)
{
    return float3(0.0, 0.0, 0.0);
}

float3 refract(in float3 uv, in float3 n, in float etai_over_etat) {
    float cos_theta = min(dot(-uv, n), 1.0);
    float3 r_out_perp =  etai_over_etat * (uv + cos_theta*n);
    float3 r_out_parallel = -sqrt(abs(1.0 - length(r_out_perp) * length(r_out_perp))) * n;
    return r_out_perp + r_out_parallel;
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
        float2 noiseInput = uv * (float(i) / float(SAMPLE_COUNT) + rand_1(frameUintInfo.x));
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
        ray.TMin = 0.0001;
        ray.TMax = 1000;

        Payload payload;
        payload.hitLight = false;
        payload.missed = false;
        payload.radiance = float3(0, 0, 0);
        payload.color = float3(1.0, 1.0, 1.0);
        payload.nextDir = float3(0, 0, 0);
        payload.nextPos = float3(0, 0, 0);
        payload.recursionDepth = 0;

        while((payload.missed == false) &&
              (payload.recursionDepth <= MAX_BOUNCE_DEPTH) &&
              (payload.hitLight == false))
        {
            TraceRay(scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
            ray.Origin = payload.nextPos;
            ray.Direction = payload.nextDir;
        }

        res += float3(payload.radiance);
    } 

    float4 thisRes = float4(res / (float)SAMPLE_COUNT, 1);
    thisRes += (uavRTRadiance[idx] * frameUintInfo.x);
    thisRes /= float(frameUintInfo.x + 1);

    uavRTRadiance[idx] = thisRes;

    // Gamma Correction
    float3 colorPresent = thisRes.xyz / (thisRes.xyz + float3(1.0, 1.0, 1.0));
    colorPresent = pow(colorPresent, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    uavRTPresent[idx] = float4(colorPresent, 1.0);
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
    InstInfo instInfo = instsInfo[instId];
    uint instMaterialMask = instInfo.instUintInfo0.x;
    uint instVertStartFloat = instInfo.instUintInfo0.y;
    uint instIdxStartInt = instInfo.instUintInfo0.z;
    float3 instEmissive = instInfo.instFloatInfo0.xyz;
    payload.recursionDepth++;

    if(length(instEmissive) > 0.0)
    {
        // Hit light. Stops the recursion.
        payload.hitLight = true;        
        payload.radiance = instEmissive * payload.color;
    }
    else
    {
        uint indexSizeInBytes = 2;
        uint indicesPerTriangle = 3;
        uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
        uint baseIndex = instIdxStartInt * indexSizeInBytes + PrimitiveIndex() * triangleIndexStride;

        // Load up 3 16 bit indices for the triangle.
        const uint3 indices = Load3x16BitIndices(baseIndex);
        const uint vertOffsetId = instVertStartFloat / 12;

        VertexReal vert0, vert1, vert2;
        ParseVertex(indices.x + vertOffsetId, vert0.pos, vert0.normal, vert0.tangent, vert0.uv);
        ParseVertex(indices.y + vertOffsetId, vert1.pos, vert1.normal, vert1.tangent, vert1.uv);
        ParseVertex(indices.z + vertOffsetId, vert2.pos, vert2.normal, vert2.tangent, vert2.uv);

        // Retrieve corresponding vertex normals for the triangle vertices.
        float3 vertexNormals[3] = { vert0.normal, vert1.normal, vert2.normal };
        float3 vertexPos[3] = { vert0.pos, vert1.pos, vert2.pos };
        float3 triangleNormal = HitAttribute(vertexNormals, attrib);
        // float3 triangleNormal = vert0.normal;
        float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

        float3x4 objToWorld = ObjectToWorld3x4();
        triangleNormal = normalize(triangleNormal);
        triangleNormal = mul(objToWorld, float4(triangleNormal, 0)).xyz;

        // Test whether the normal and ray are at the same direction
        float3 rayDir = WorldRayDirection();
        float rayNormalDot = dot(rayDir, triangleNormal);
        if((instMaterialMask & DOUBLE_SIDE_MASK) > 0)
        {
            if(rayNormalDot >= 0.f)
            {
                // Need to flip the normal
                triangleNormal = -triangleNormal;
            }
        }

        // If there is any texture, then the material is not constant.
        if((instMaterialMask & MATERIAL_TEX_MASK) == 0)
        {
            if((instMaterialMask & DIELECTRIC_MASK) > 0)
            {
                // Glass material that needs to refract and reflect
                float refractIdx = rayNormalDot >= 0.f ? REFRACTION_INDEX : (1.0 / REFRACTION_INDEX);
                triangleNormal = rayNormalDot >= 0.f ? -triangleNormal : triangleNormal;

                float cos_theta = min(dot(-rayDir, triangleNormal), 1.0);
                double sin_theta = sqrt(1.0 - cos_theta*cos_theta);

                bool cannot_refract = refractIdx * sin_theta > 1.0;

                if(cannot_refract)
                {
                    float3 reflectDir = reflect(rayDir, triangleNormal);
                    payload.nextPos = hitPos + (triangleNormal * 0.00001);
                    payload.nextDir = normalize(reflectDir);
                }
                else
                {
                    float3 nextDir = refract(normalize(rayDir), triangleNormal, refractIdx);
                    payload.nextPos = hitPos;
                    payload.nextDir = nextDir;
                }
            }
            else if (instInfo.instMetallicRoughness.x == 1.0)
            // if(instInfo.instMetallicRoughness.x == 1.0)
            {
                // Metal material
                // Reflect the ray according to the roughness
                float roughness = clamp(instInfo.instMetallicRoughness.y, 0.0, 0.8);
                float3 randDir = random3(random3(hitPos) * 3.412 + rayDir * rand_1(payload.recursionDepth) * -12.7 + triangleNormal * rand_1(frameUintInfo.x) * 5.12145);
                randDir = randDir * 2.0 - 1.0;
                if(length(randDir) == 0.0)
                {
                    randDir += 0.000001;
                }
                randDir = normalize(randDir) * roughness;

                float3 reflectDir = reflect(rayDir, triangleNormal);
                payload.nextPos = hitPos + (triangleNormal * 0.00001);
                payload.nextDir = normalize(reflectDir) + randDir;
            }
            else
            {
                // Diffuse random scatter material
                // Randomly choose a out direction
                float3 randDir = random3(random3(hitPos) * 3.412 + rayDir * rand_1(payload.recursionDepth) * -12.7 + triangleNormal * rand_1(frameUintInfo.x) * 5.12145);
                randDir = randDir * 2.0 - 1.0;
                if(length(randDir) == 0.0)
                {
                    randDir += 0.000001;
                }
                randDir = normalize(randDir);
                triangleNormal = normalize(triangleNormal);
                randDir += triangleNormal;
                if(length(randDir) < 0.000001)
                {
                    randDir = triangleNormal;
                }
                payload.nextPos = hitPos;
                payload.nextDir = randDir;
            }
            
            // hitPos += (triangleNormal * 0.001);
            /*
            if(dot(randDir, triangleNormal) < 0)
            {
                randDir = -randDir;
            }
            */

            // Assembly the new out-going ray
            payload.color *= instInfo.instAlbedo.xyz;
        }
        else
        {
            payload.color *= float3(1, 0, 1);
        }
    }
    
}

/*
            RayDesc ray;
            ray.Origin = hitPos + randDir * 0.001;
            ray.Direction = randDir;
            ray.TMin = 0.001;
            ray.TMax = 1000;

            payload.color *= instInfo.instAlbedo.xyz;
            payload.recursionDepth++;
            if(instId == 0 && triangleNormal.y > 2.0)
            {
                TraceRay(scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
            }
            payload.radiance = float3(payload.recursionDepth, 0.0, 0.0) / 2;
            */