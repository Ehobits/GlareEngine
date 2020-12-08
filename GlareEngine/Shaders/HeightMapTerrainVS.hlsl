#include "Common.hlsli"
#include "TerrainConstBuffer.hlsli"



VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Terrain specified directly in world space.
	vout.PosW = vin.PosL;

	//��patch corners�Ƶ�����ռ䡣 ����Ϊ��ʹ�۾���patch�ľ���������׼ȷ��
	vout.PosW.y = gSRVMap[mHeightMapIndex].SampleLevel(gsamLinearWrap, vin.Tex, 0).r;

	// Output vertex attributes to next stage.
	vout.Tex = vin.Tex;
	vout.BoundsY = vin.BoundsY;

	return vout;
}