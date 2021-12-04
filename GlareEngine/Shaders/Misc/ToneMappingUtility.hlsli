#include "ShaderUtility.hlsli"

#ifndef TONE_MAPPING_UTILITY
#define TONE_MAPPING_UTILITY

//
// Reinhard
// 

// The Reinhard tone operator.  Typically, the value of k is 1.0, but you can adjust exposure by 1/k.
// I.e. TM_Reinhard(x, 0.5) == TM_Reinhard(x * 2.0, 1.0)
float3 TM_Reinhard(float3 hdr, float k = 1.0)
{
    return hdr / (hdr + k);
}

// The inverse of Reinhard
float3 ITM_Reinhard(float3 sdr, float k = 1.0)
{
    return k * sdr / (k - sdr);
}

//
// Reinhard-Squared
//

//����һЩ�ܺõ����ԣ����ԸĽ������� Reinhard�� 
//���ȣ�����һ������ֺ������Ư������������������ǿ�˺ڰ��еĶԱȶȺ�ɫ�ʱ��Ͷȡ� 
//��Σ�����һ�����粿�������ڸ߹⴦�ṩ����ϸ�ڣ�������Ҫ������ʱ��ȥ���͡� 
//���ǿ���ģ������ŵ� HDR ��ʾ�����������ڿ��ơ�
//
//ѡ��Ĭ�ϳ��� 0.25 ������ԭ�� ���� Reinhard ��Ч���ǳ��ӽ�������Ϊ 1.0�� 
//���ҳ���Ϊ 0.25���� 0.25 ����һ���յ㣬�������� y=x �Ӵ���Ȼ��ʼ�粿��
//
// Note:  �������ǰ����ʹ�� ACES �������� 0.6 ����Ԥ���ţ���ô k=0.30 ��Ϊ����������������������κ�����������
float3 TM_ReinhardSq(float3 hdr, float k = 0.25)
{
    float3 reinhard = hdr / (hdr + k);
    return reinhard * reinhard;
}

float3 ITM_ReinhardSq(float3 sdr, float k = 0.25)
{
    return k * (sdr + sqrt(sdr)) / (1.0 - sdr);
}

//
// Stanard (New)
//

// This is the new tone operator.  ������෽�������� ACES����ʹ�� ALU �����������򵥡� 
//�� Reinhard-Squared ��ȣ�����һ�������Ǽ粿����ر�Ϊ��ɫ����Ϊͼ���ṩ���ߵ��������ȺͶԱȶȡ�

float3 TM_Stanard(float3 hdr)
{
    return TM_Reinhard(hdr * sqrt(hdr), sqrt(4.0 / 27.0));
}

float3 ITM_Stanard(float3 sdr)
{
    return pow(ITM_Reinhard(sdr, sqrt(4.0 / 27.0)), 2.0 / 3.0);
}

//
// Stanard (Old)
//

//���������� HemiEngine �� MiniEngine ��ʹ�õľ� tone operators�� 
//���򵥡���Ч�����棬���ṩ�˲���Ľ��������û�н�ֺ�����Ҽ粿�ܿ�ͻ��ס�
//
//��ע�⣬��ɾ����ɫ��ӳ�� RGB ��ɫ��ӳ�� Luma ֮������� 
//����ѧ�Ͻ�����ͬ���ڱ���ɫ����ͬʱ���Խ���������ӳ�䵽����ʾֵ���뷨�� 
//������������һ��������ɫͨ�����ձ� 1.0 �����������õ����⡣

float3 ToneMap(float3 hdr)
{
    return 1 - exp2(-hdr);
}

float3 InverseToneMap(float3 sdr)
{
    return -log2(max(1e-6, 1 - sdr));
}

float ToneMapLuma(float luma)
{
    return 1 - exp2(-luma);
}

float InverseToneMapLuma(float luma)
{
    return -log2(max(1e-6, 1 - luma));
}

//
// ACES
//

//��һ����Ӱ tone operators.

float3 ToneMapACES(float3 hdr)
{
    const float A = 2.51, B = 0.03, C = 2.43, D = 0.59, E = 0.14;
    return saturate((hdr * (A * hdr + B)) / (hdr * (C * hdr + D) + E));
}

float3 InverseToneMapACES(float3 sdr)
{
    const float A = 2.51, B = 0.03, C = 2.43, D = 0.59, E = 0.14;
    return 0.5 * (D * sdr - sqrt(((D * D - 4 * C * E) * sdr + 4 * A * E - 2 * B * D) * sdr + B * B) - B) / (A - C * sdr);
}

#endif