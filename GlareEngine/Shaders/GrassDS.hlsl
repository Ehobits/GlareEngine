#include "Common.hlsli"
struct DomainOut
{
	float4 PosH     : SV_POSITION;
	float3 PosW     : POSITION0;
};

struct PatchTess
{
	float EdgeTess[4]   : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};

struct HullOut
{
	float3 PosW     : POSITION;
};


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

	// Project to homogeneous clip space.
	dout.PosH = mul(float4(dout.PosW, 1.0f), gViewProj);

	return dout;
}