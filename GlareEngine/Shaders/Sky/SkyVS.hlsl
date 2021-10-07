#include "../Misc/CommonResource.hlsli"


cbuffer SkyPass : register(b1)
{
    float4x4 gWorld;
}


PosVSOut main(float3 PosL : POSITION)
{
    PosVSOut vout;

	// ʹ�þֲ�����λ����Ϊ��������ͼ����������
    vout.PosL = PosL;

	// ת��������ռ�
    float4 posW = mul(float4(PosL, 1.0f), gWorld);

	//����պй̶����۾�λ��
    posW.xyz += gEyePosW;

	// ����z = w����ʹz / w = 1���������ʼ��λ��Զƽ���ϣ���
    vout.PosH = mul(posW, gViewProj).xyww;

    return vout;
}