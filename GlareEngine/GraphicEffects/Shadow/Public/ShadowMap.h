#pragma once
#include "EngineUtility.h"
#include "ShadowBuffer.h"
#include "RenderObject.h"

struct ShadowConstantBuffer 
{
	XMFLOAT4X4 gShadowViewProj = MathHelper::Identity4x4();
};


class ShadowMap:public RenderObject
{
public:
	ShadowMap(XMFLOAT3 LightDirection, UINT width, UINT height);

	ShadowMap(const ShadowMap& rhs) = delete;
	ShadowMap& operator=(const ShadowMap& rhs) = delete;
	~ShadowMap() = default;

	UINT Width()const;
	UINT Height()const;
	D3D12_VIEWPORT Viewport()const;
	D3D12_RECT ScissorRect()const;


	virtual void Draw(GraphicsContext& Context, GraphicsPSO* PSO = nullptr) {};
	void Draw(GraphicsContext& Context, vector<RenderObject*> RenderObjects);

	static void BuildPSO(const RootSignature& rootSignature);

	void OnResize(UINT newWidth, UINT newHeight);

	void SetSceneBoundCenter(XMFLOAT3 center) { mSceneBounds.Center = center; }
	void SetSceneBoundRadius(float Radius) { mSceneBounds.Radius = Radius; }

	int GetShadowMapIndex() { return mShadowMapIndex; }

	void UpdateShadowTransform(float Detailtime);

	XMFLOAT4X4 GetShadowTransform()const { return mShadowTransform; }

	XMFLOAT3 GetShadowedLightDir()const { return mBaseLightDirection; }
private:
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV()const { return mShadowBuffer.GetDSV(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRV()const { return mShadowBuffer.GetSRV(); }

private:
	//PSO
	static GraphicsPSO mShadowPSO;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_D32_FLOAT;

	ShadowBuffer mShadowBuffer;

	bool IsShadowTransformed = true;

	//���յ�Ӱ�췶Χ
	DirectX::BoundingSphere mSceneBounds;
	//����ͶӰ��Զ��zֵ
	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	//�������λ��
	XMFLOAT3 mLightPosW;
	//����shadow map ��ת������
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();
	//�����ת�Ƕ�(��ʵʱ�仯)
	float mLightRotationAngle = 0.0f;
	//�����ⷽ��
	XMFLOAT3 mBaseLightDirection;
	//��ת���ķ���
	XMFLOAT3 mRotatedLightDirection;
	//constant buffer
	ShadowConstantBuffer mConstantBuffer;
	int mShadowMapIndex = 0;
};

