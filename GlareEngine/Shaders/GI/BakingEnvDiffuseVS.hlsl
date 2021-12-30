#include "../Misc/CommonResource.hlsli"

cbuffer CubePass : register(b1)
{
    float4x4 View ;
    float4x4 Proj ;
    float4x4 ViewProj;
    float3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float ObjectPad1 = 0.0f;
    float2 RenderTargetSize = { 0.0f, 0.0f };
    float2 InvRenderTargetSize = { 0.0f, 0.0f };
    int mSkyCubeIndex = 0;
    float mRoughness = 0.0f;
}


PosVSOut main(float3 PosL : POSITION)
{
    PosVSOut vout;

	// ʹ�þֲ�����λ����Ϊ��������ͼ����������
    vout.PosL = PosL;
	// ����z = w����ʹz / w = 1���������ʼ��λ��Զƽ���ϣ���
    vout.PosH = mul(float4(PosL,1.0f), ViewProj).xyww;

    return vout;
}