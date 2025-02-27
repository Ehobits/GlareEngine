#include "PBRLighting.hlsli"

#define MAX2DSRVSIZE 256
#define MAXCUBESRVSIZE 32

struct InstanceData
{
    float4x4 World;
    float4x4 TexTransform;
    uint MaterialIndex;
    uint mPAD1;
    uint mPAD2;
    uint mPAD3;
};


struct MaterialData
{
    float4 mDiffuseAlbedo;
    float3 mFresnelR0;
    float mHeightScale;
    float4x4 mMatTransform;
    uint mRoughnessMapIndex;
    uint mDiffuseMapIndex;
    uint mNormalMapIndex;
    uint mMetallicMapIndex;
    uint mAOMapIndex;
    uint mHeightMapIndex;
};


//static sampler
SamplerState gSamplerLinearWrap         : register(s0);
SamplerState gSamplerAnisoWrap          : register(s1);
SamplerState gSamplerLinearClamp        : register(s3);
SamplerState gSamplerVolumeWrap         : register(s4);
SamplerState gSamplerPointClamp         : register(s5);
SamplerState gSamplerPointBorder        : register(s6);
SamplerState gSamplerLinearBorder       : register(s7);
SamplerComparisonState gSamplerShadow   : register(s2);

//Texture array, only supported for shader model 5.1+. Unlike Texture2DArray,
//Textures in this array can have different sizes and formats, making it more flexible than texture arrays.
TextureCube gCubeMaps[MAXCUBESRVSIZE]   : register(t0);
Texture2D gSRVMap[MAX2DSRVSIZE]         : register(t32);


StructuredBuffer<MaterialData> gMaterialData : register(t1, space1);
StructuredBuffer<InstanceData> gInstanceData : register(t0, space1);

//Constant data per frame.
cbuffer MainPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4x4 gShadowTransform;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

     //索引[0，NUM_DIR_LIGHTS）是方向灯；
     //索引[NUM_DIR_LIGHTS，NUM_DIR_LIGHTS + NUM_POINT_LIGHTS）是点光源；
     //索引[NUM_DIR_LIGHTS + NUM_POINT_LIGHTS，NUM_DIR_LIGHTS + NUM_POINT_LIGHT + NUM_SPOT_LIGHTS）
     //是聚光灯，每个对象最多可使用MaxLights。
    Light gLights[MaxLights];
    
    int gShadowMapIndex;
    int gSkyCubeIndex;
    int gBakingDiffuseCubeIndex;
    int gBakingPreFilteredEnvIndex;
    int gBakingIntegrationBRDFIndex;
};    


//Terrain
#if defined(TERRAIN)

#define NUM_CONTROL_POINTS 4

struct VertexIn
{
    float3 PosL     : POSITION;
    float2 Tex      : TEXCOORD;
    float2 BoundsY  : BoundY;
};

struct VertexOut
{
    float3 PosW     : POSITION;
    float2 Tex      : TEXCOORD;
    float2 BoundsY  : BoundY;
};

struct PatchTess
{
    float EdgeTess[4]   : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};

struct HullOut
{
    float3 PosW     : POSITION;
    float2 Tex      : TEXCOORD;
};

cbuffer TerrainCBPass : register(b1)
{
    float4 gWorldFrustumPlanes[6];

    float gTessellationScale;
    float gTexelCellSpaceU;
    float gTexelCellSpaceV;
    float gWorldCellSpace;

    int gHeightMapIndex;
    int gBlendMapIndex;

    float gMinDist;
    float gMaxDist;
    float gMinTess;
    float gMaxTess;
};
#endif


//Position
struct PosVSOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION0;
};

//Tex
struct TexVSOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEXCOORD0;
};


//PosNormalTangentTexc
struct PosNorTanTexIn
{
    float3 PosL         : POSITION;
    float3 NormalL      : NORMAL;
    float3 TangentL     : TANGENT;
    float2 TexC         : TEXCOORD;
};

struct PosNorTanTexOut
{
    float4 PosH         : SV_POSITION;
    float4 ShadowPosH   : POSITION0;
    float3 PosW         : POSITION1;
    float3 NormalW      : NORMAL;
    float3 TangentW     : TANGENT;
    float2 TexC         : TEXCOORD;
    uint MatIndex       : MATINDEX;
};

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to model space.
//---------------------------------------------------------------------------------------
float3 NormalSampleToModelSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
   // Uncompress each component from [0,1] to [-1,1].
    float3 normalT = 2.0f * normalMapSample - 1.0f;

	// Build orthonormal basis.
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
    float3 bumpedNormalW = mul(normalT, TBN);

    return bumpedNormalW;
}


float3 WorldSpaceToTBN(float3 WorldSpaceVector, float3 unitNormalW, float3 tangentW)
{

    // Build TBN basis.
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);
    float3 TBNSpaceVector = mul(WorldSpaceVector, transpose(TBN));

    return TBNSpaceVector;
}


//视差遮挡映射
float2 ParallaxMapping(uint HeightMapIndex, float2 texCoords, float3 viewDir, float height_scale)
{
    // number of depth layers
    const float minLayers = 10;
    const float maxLayers = 20;
    float numLayers = lerp(maxLayers, minLayers, abs(dot(float3(0.0, 0.0, 1.0), viewDir)));
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    float2 P = viewDir.xy * height_scale;
    float2 deltaTexCoords = P / numLayers;

    // get initial values
    float2 currentTexCoords = texCoords;
    float currentDepthMapValue = 1 - gSRVMap[HeightMapIndex].SampleLevel(gSamplerLinearWrap, currentTexCoords, 0).r;
    
    while (currentLayerDepth < currentDepthMapValue)
    {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = 1 - gSRVMap[HeightMapIndex].SampleLevel(gSamplerLinearWrap, currentTexCoords, 0).r;
        // get depth of next layer
        currentLayerDepth += layerDepth;
    }

    // -- parallax occlusion mapping interpolation from here on
    // get texture coordinates before collision (reverse operations)
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = 1 - gSRVMap[HeightMapIndex].SampleLevel(gSamplerLinearWrap, prevTexCoords, 0).r - currentLayerDepth + layerDepth;

    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}



//---------------------------------------------------------------------------------------
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    gSRVMap[48].GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float) width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += gSRVMap[gShadowMapIndex].SampleCmpLevelZero(gSamplerShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }
    percentLit /= 9.0f;
    return clamp(percentLit, 0.1f, 1.0f);
}


// 如果框完全位于平面的后面(负半空间),则返回true。
bool AABBBehindPlaneTest(float3 center, float3 extents, float4 plane)
{
    float3 n = abs(plane.xyz);

    // This is always positive.
    float r = dot(extents, n);
    //从中心点到平面的正负距离。
    float s = dot(float4(center, 1.0f), plane);

    //如果框的中心点在平面后面等于e或更大（在这种情况下s为负，
    //因为它在平面后面），则框完全位于平面的负半空间中。
    return (s + r) < 0.0f;
}


//如果该框完全位于平截头体之外，返回true。
bool AABBOutsideFrustumTest(float3 center, float3 extents, float4 frustumPlanes[6])
{
    for (int i = 0; i < 6; ++i)
    {
        // 如果盒子完全位于任何一个视锥平面的后面，那么它就在视锥之外。
        if (AABBBehindPlaneTest(center, extents, frustumPlanes[i]))
        {
            return true;
        }
    }
    return false;
}