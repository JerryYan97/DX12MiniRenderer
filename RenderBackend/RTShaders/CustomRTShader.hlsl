struct Payload
{
    float3 color;
    bool allowReflection;
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

cbuffer FrameConstantBuffer : register(b0)
{
    float4 cameraPos;
    float4 cameraDir;
    float4 cameraUp;
    float4 cameraRight;
    float4 cameraInfo; // x: fov, y: near, z: far.
    uint4  randomSeeds; // x: random number; y: current time in microseconds. 
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
static const uint HIT_CHILD_RAY_COUNT = 4;

RaytracingAccelerationStructure scene : register(t0);
StructuredBuffer<dword> instsMaterialsMasks : register(t1);
StructuredBuffer<ConstantMaterialData> cnstMaterials : register(t2);
StructuredBuffer<VertexRaw> vertices : register(t3);
ByteAddressBuffer Indices : register(t4);

RWTexture2D<float4> uav : register(u0);

void ParseVertex(in uint vertId, out float3 pos, out float3 normal, out float4 tangent, out float2 uv)
{
    VertexRaw vert = vertices[vertId];
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
    const uint2 four16BitIndices = Indices.Load2(dwordAlignedOffset);
 
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

float3 random3(in float3 p)
{   
    float a = frac(cos(dot(p,float3(23.14069263277926,2.665144142690225,-27.90511001)))*12345.6789);
    float2 bc = rand_2_10(float2(a, p.z));
    float3 res = float3(a, bc.x, bc.y);
    return res;
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
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

    uint indexSizeInBytes = 2;
    uint indicesPerTriangle = 3;
    uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
    uint baseIndex = PrimitiveIndex() * triangleIndexStride;

    // Load up 3 16 bit indices for the triangle.
    const uint3 indices = Load3x16BitIndices(baseIndex);

    VertexReal vert0, vert1, vert2;
    ParseVertex(indices.x, vert0.pos, vert0.normal, vert0.tangent, vert0.uv);
    ParseVertex(indices.y, vert1.pos, vert1.normal, vert1.tangent, vert1.uv);
    ParseVertex(indices.z, vert2.pos, vert2.normal, vert2.tangent, vert2.uv);

    // Retrieve corresponding vertex normals for the triangle vertices.
    float3 vertexNormals[3] = { vert0.normal, vert1.normal, vert2.normal };
    float3 triangleNormal = HitAttribute(vertexNormals, attrib);

    float3x4 objToWorld = ObjectToWorld3x4();
    triangleNormal = mul(objToWorld, float4(triangleNormal, 0)).xyz;

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
