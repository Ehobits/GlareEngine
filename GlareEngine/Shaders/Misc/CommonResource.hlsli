#include "PBRLighting.hlsli"

#define MAXSRVSIZE 256

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

//�������飬����ɫ��ģ��5.1+֧�֡� ��Texture2DArray��ͬ��
//�������е�������Ծ��в�ͬ�Ĵ�С�͸�ʽ��ʹ��������������
Texture2D gSRVMap[MAXSRVSIZE] : register(t1);

StructuredBuffer<MaterialData> gMaterialData : register(t1, space1);
StructuredBuffer<InstanceData> gInstanceData : register(t0, space1);

// ÿ֡�������ݡ�
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

     //����[0��NUM_DIR_LIGHTS���Ƿ���ƣ�
     //����[NUM_DIR_LIGHTS��NUM_DIR_LIGHTS + NUM_POINT_LIGHTS���ǵ��Դ��
     //����[NUM_DIR_LIGHTS + NUM_POINT_LIGHTS��NUM_DIR_LIGHTS + NUM_POINT_LIGHT + NUM_SPOT_LIGHTS��
     //�Ǿ۹�ƣ�ÿ����������ʹ��MaxLights��
    Light gLights[MaxLights];
    
    int gShadowMapIndex;
};    

//Position
struct PosVSOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
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
    //from [0,1] to [-1,1].
    float3 normalT = 2.0f * normalMapSample - 1.0f;

    // Build TBN basis.
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


//�Ӳ��ڵ�ӳ��
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


// �������ȫλ��ƽ��ĺ���(����ռ�),�򷵻�true��
bool AABBBehindPlaneTest(float3 center, float3 extents, float4 plane)
{
    float3 n = abs(plane.xyz);

    // This is always positive.
    float r = dot(extents, n);
    //�����ĵ㵽ƽ����������롣
    float s = dot(float4(center, 1.0f), plane);

    //���������ĵ���ƽ��������e����������������sΪ����
    //��Ϊ����ƽ����棩�������ȫλ��ƽ��ĸ���ռ��С�
    return (s + r) < 0.0f;
}


//����ÿ���ȫλ��ƽ��ͷ��֮�⣬����true��
bool AABBOutsideFrustumTest(float3 center, float3 extents, float4 frustumPlanes[6])
{
    for (int i = 0; i < 6; ++i)
    {
        // ���������ȫλ���κ�һ����׶ƽ��ĺ��棬��ô��������׶֮�⡣
        if (AABBBehindPlaneTest(center, extents, frustumPlanes[i]))
        {
            return true;
        }
    }
    return false;
}