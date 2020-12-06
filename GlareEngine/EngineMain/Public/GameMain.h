#pragma once

#include "resource.h"
#include "L3DBaseApp.h"
#include "L3DMathHelper.h"
#include "L3DUploadBuffer.h"
#include "L3DGeometryGenerator.h"
#include "Waves.h"
#include "FrameResource.h"

//shader head files
#include "BaseShader.h"
#include "GerstnerWaveShader.h"
#include "SkyShader.h"
#include "SimpleGeometryInstanceShader.h"
#include "SimpleGeometryShadowMapShader.h"
#include "ComplexStaticModelInstanceShader.h"
#include "SkinAnimeShader.h"
#include "WaterRefractionMaskShader.h"
#include "ShockWaveWaterShader.h"
#include "HeightMapTerrainShader.h"

#include "PSOManager.h"
#include "L3DCamera.h"
#include "L3DTextureManage.H"
#include "Sky.h"
#include "EngineGUI.h"
#include "ShadowMap.h"
#include "ModelLoader.h"
#include "SimpleGeoInstance.h"
#include "ShockWaveWater.h"
#include "HeightmapTerrain.h"
//#include "OzzAnimePlayBack.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

enum RenderItemType:int
{
	Normal=0,
	Reflection
};

// ���ͽṹ�洢�����Ի�����״,�⽫��Ӧ�ó�����졣
struct RenderItem
{
	RenderItem() = default;
	//Render Item Type
	RenderItemType ItemType= RenderItemType::Normal;

	// �����������������ռ�ľֲ��ռ����״��������󣬸�����������˶����������е�λ�ã�����ͱ�����
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	//����ת������
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// ָʾ���������Ѹ��ĵ����־��������Ҫ���³����������� 
	//��Ϊ����Ϊÿ��FrameResource�ṩ��һ�����󻺳������������Ǳ��뽫����Ӧ����ÿ��FrameResource�� 
	//��ˣ��������޸Ķ�������ʱ������Ӧ������NumFramesDirty = gNumFrameResources���Ա�ÿ��֡��Դ���õ����¡�
	int NumFramesDirty = gNumFrameResources;

	//����������Ⱦ��Ŀ��Ӧ��ObjectCB��GPU������������
	int ObjCBIndex = -1;

	Material* Mat = nullptr;
	std::vector<MeshGeometry*> Geo;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	//Instance Data
	BoundingBox Bounds;
	std::vector<InstanceConstants> Instances;


	// DrawIndexedInstanced parameters.
	UINT InstanceCount;
	std::vector<UINT> IndexCount;
	std::vector<UINT> StartIndexLocation;
	std::vector<int> BaseVertexLocation;
};

enum class RenderLayer : int
{
	Opaque = 0,
	InstanceSimpleItems,
	Sky,
	ShockWaveWater,
	HeightMapTerrain,
	Count
};

enum class MaterialType : int
{
	NormalPBRMat = 0,
    HeightMapTerrainPBRMat,
	ModelPBRMat,
	Count
};



//APPLICATION
class GameApp : public D3DApp
{
public:
	GameApp(HINSTANCE hInstance);
	GameApp(const GameApp& rhs) = delete;
	GameApp& operator=(const GameApp& rhs) = delete;
	~GameApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	virtual void CreateRtvAndDsvDescriptorHeaps()override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);

	void UpdateCamera(const GameTimer& gt);
	
	void UpdateObjectCBs(const GameTimer& gt);

	void UpdateInstanceCBs(const GameTimer& gt);
	
	void UpdateMaterialCBs(const GameTimer& gt);
	
	void UpdateMainPassCB(const GameTimer& gt);
	
	void UpdateWaves(const GameTimer& gt);

	void UpdateShadowPassCB(const GameTimer& gt);

	void UpdateTerrainPassCB(const GameTimer& gt);

	void UpdateAnimation(const GameTimer& gt);

	void DrawSceneToShadowMap();

	//������ǩ��
	void BuildRootSignature();
	//����Shader�����벼��
	void BuildShadersAndInputLayout();
	//�����򵥼�����
	void BuildSimpleGeometry();
	//�������漸���壨���Ľ���
	void BuildLandGeometry();
	//�������˼������壨���Ľ���
	void BuildWavesGeometryBuffers();
	//����PSO
	void BuildPSOs();
	//����֡��Դ
	void BuildFrameResources();
	//����������Ϣ
	void BuildAllMaterials();
	void BuildMaterials(wstring name, int MatCBIndex, float Height_Scale, XMFLOAT4 DiffuseAlbedo, XMFLOAT3 FresnelR0,XMFLOAT4X4 MatTransform, MaterialType MatType= MaterialType::NormalPBRMat);
	
	
	//������Ⱦ��
	void BuildRenderItems();
	void BuildModelGeoInstanceItems();
	//������Ⱦ��
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	//������Դ������
	void CreateDescriptorHeaps();
	void CreatePBRSRVinDescriptorHeap(unordered_map<std::string, ID3D12Resource*> TexResource,int* SRVIndex, CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor,wstring MaterialName);

	//sampler
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();
	//Load model
	void LoadModel();

	//waves
	float GetHillsHeight(float x, float z)const;
	XMFLOAT3 GetHillsNormal(float x, float z)const;

	//ShockWaveWater
	void DrawShockWaveWater(const GameTimer& gt);
	void DrawWaterReflectionMap(const GameTimer& gt);
	void DrawWaterRefractionMap(const GameTimer& gt);

	//Height map terrain
	HeightmapTerrain::InitInfo HeightmapTerrainInit();

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mGUISrvDescriptorHeap = nullptr;


	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::wstring, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;


	std::unique_ptr<PSO> mPSOs;
	//Shaders
	std::unordered_map<std::string, BaseShader*> mShaders;



	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	RenderItem* mSphereRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	std::vector<RenderItem*> mReflectionWaterLayer[(int)RenderLayer::Count];

	std::unique_ptr<Waves> mWaves;

	PassConstants mMainPassCB;  // index 0 of pass cbuffer.
	PassConstants mShadowPassCB;// index 1 of pass cbuffer.
	TerrainConstants mTerrainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV2 - 0.1f;
	float mRadius = 500.0f;

	float mSunTheta = 1.25f * XM_PI;
	float mSunPhi = XM_PIDIV4;

	POINT mLastMousePos;

	//���
	Camera mCamera;
	//TEXTURE MANAGER
	std::unique_ptr<L3DTextureManage> mTextureManage;
	int mSRVSize = 0;
	std::vector<wstring> mPBRTextureName;
	//sky
	std::unique_ptr<Sky> mSky;

	int mSkyMapIndex = 0;
	//UI
	std::unique_ptr<EngineGUI> mEngineUI;

	//Instance Culling
	bool mFrustumCullingEnabled = true;
	BoundingFrustum mCamFrustum;
	std::unique_ptr<SimpleGeoInstance> mSimpleGeoInstance;

	//shadow map 
	std::unique_ptr<ShadowMap> mShadowMap;
	int mShadowMapIndex = 0;
	static bool RedrawShadowMap;

	//Model Loader
	std::unique_ptr<ModelLoader> mModelLoder;

	//animation transform
	std::vector<XMFLOAT4X4> transforms;

	//ShockWaveWater
	std::unique_ptr<ShockWaveWater> mShockWaveWater;
	int mWaterReflectionMapIndex = 0;
	int mWaterRefractionMapIndex = 0;
	int mWaterDumpWaveIndex = 0;

	//Terrain
	std::unique_ptr<HeightmapTerrain> mHeightMapTerrain;
	int mBlendMapIndex = 0;
	int mHeightMapIndex = 0;
	//play back anime 
	//std::unordered_map<std::string, AnimePlayback> AnimationPlayback;
};