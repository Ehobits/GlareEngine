#include "Common.hlsli"
#include "TerrainConstBuffer.hlsli"

struct VertexIn
{
	float3 PosL     : POSITION;
	float2 Tex      : TEXCOORD;
	float2 BoundsY  : BoundY;
};

struct VertexOut
{
	float4 PosW     : SV_POSITION;
	float2 Tex      : TEXCOORD0;
	float2 BoundsY  : TEXCOORD1;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Terrain specified directly in world space.
	vout.PosW = vin.PosL.xyzz;

	//��patch corners�Ƶ�����ռ䡣 ����Ϊ��ʹ�۾���patch�ľ���������׼ȷ��
	vout.PosW.y = gSRVMap[1].SampleLevel(gsamLinearWrap, vin.Tex, 0).r;

	// Output vertex attributes to next stage.
	vout.Tex = vin.Tex;
	vout.BoundsY = vin.BoundsY;

	return vout;
}