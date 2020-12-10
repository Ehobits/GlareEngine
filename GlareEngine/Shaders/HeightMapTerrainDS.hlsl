#include "TerrainConstBuffer.hlsli"
#include "Common.hlsli"

#define gTexScale float2(5.0f,5.0f)


// ϸ����������ÿ�����㶼���������ɫ���� ����ϸ�ֺ�Ķ�����ɫ��һ����
[domain("quad")]
DomainOut DS(PatchTess patchTess,
	float2 uv : SV_DomainLocation,
	const OutputPatch<HullOut, 4> quad)
{
	DomainOut dout;

	// Bilinear interpolation.
	dout.PosW = lerp(
		lerp(quad[0].PosW, quad[1].PosW, uv.x),
		lerp(quad[2].PosW, quad[3].PosW, uv.x),
		uv.y);

	dout.Tex = lerp(
		lerp(quad[0].Tex, quad[1].Tex, uv.x),
		lerp(quad[2].Tex, quad[3].Tex, uv.x),
		uv.y);

	// Tile layer textures over terrain.
	dout.TiledTex = dout.Tex * gTexScale;

	dout.PosW = mul(float4(dout.PosW, 1.0f),gWorld).xyz;

	// Displacement mapping
	float height =  gSRVMap[mHeightMapIndex].SampleLevel(gsamLinearWrap, dout.Tex, 0).x;
	//������ռ䣬���ڳ��Բü���С����Ľ��вü����ü������������ļ����岿��
	if (isReflection)
	{
		dout.PosW.y -= height;
		float3 ClipPlane = float3(0.0f, -1.0f, 0.0f);
		dout.ClipValue = dot(dout.PosW, ClipPlane);
	}
	else
	{
		dout.PosW.y += height;
		dout.ClipValue = 1.0f;
	}
	// NOTE:���ǳ���ʹ�����޲����������ɫ���еķ��ߣ�
	//��������������ϵط����ƶ������������ŷ��ߵı仯���������Ե�����Ч����
	//��ˣ����ǽ���������������ɫ����

	// Project to homogeneous clip space.
	dout.PosH = mul(float4(dout.PosW, 1.0f), gViewProj);

	return dout;
}
