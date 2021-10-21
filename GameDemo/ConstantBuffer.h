#pragma once
#include "EngineUtility.h"
#include "MathHelper.h"
#include "Vertex.h"

struct MainConstants
{
	DirectX::XMFLOAT4X4 View				= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView				= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj				= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj				= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj			= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj			= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ShadowTransform		= MathHelper::Identity4x4();
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
	DirectX::XMFLOAT4 gAmbientLight;

	// ����[0��NUM_DIR_LIGHTS���Ƿ���ƣ�
	 //����[NUM_DIR_LIGHTS��NUM_DIR_LIGHTS + NUM_POINT_LIGHTS���ǵ��Դ��
	 //����[NUM_DIR_LIGHTS + NUM_POINT_LIGHTS��NUM_DIR_LIGHTS + NUM_POINT_LIGHT + NUM_SPOT_LIGHTS��
	 //�Ǿ۹�ƣ�ÿ����������ʹ��MaxLights��
	Light Lights[MaxLights];
};