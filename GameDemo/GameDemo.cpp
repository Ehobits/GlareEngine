﻿#include "EngineUtility.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "TextureManager.h"
#include "ConstantBuffer.h"
#include "Camera.h"

#include "CSky.h"

//lib
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib,"dxguid.lib")

using namespace GlareEngine;
using namespace GlareEngine::GameCore;
using namespace GlareEngine::DirectX12Graphics;

class App :public GameApp
{
public:
	virtual ~App() {}

	//此功能可用于初始化应用程序状态，并将在分配必要的硬件资源后运行。 
	//不依赖于这些资源的某些状态仍应在构造函数中初始化，例如指针和标志。
	virtual void Startup(void);// = 0;
	virtual void Cleanup(void);// = 0;

	//确定您是否要退出该应用。 默认情况下，应用程序继续运行，直到按下“ ESC”键为止。
	//virtual bool IsDone(void);

	//每帧将调用一次update方法。 状态更新和场景渲染都应使用此方法处理。
	virtual void Update(float deltaT);

	void UpdateMainConstantBuffer(float deltaT);



	//rendering pass
	virtual void RenderScene(void);// = 0;

	//可选的UI（覆盖）渲染阶段。 这是LDR。 缓冲区已被清除。 
	virtual void RenderUI() {};

private:
	//Main Constant Buffer
	MainConstants mMainConstants;



	//Sky
	unique_ptr<CSky> mSky;


};


//////////////////////////////////////////////////////////////

//Game App entry
CREATE_APPLICATION(App);

//////////////////////////////////////////////////////////////


void App::Startup(void)
{
	GraphicsContext& InitializeContext = GraphicsContext::Begin(L"Initialize");

	ID3D12GraphicsCommandList* CommandList = InitializeContext.GetCommandList();

	g_TextureManager.SetCommandList(CommandList);
	//Sky Initialize
	mSky = make_unique<CSky>(CommandList, 5.0f, 20, 20);
	
	
	
	InitializeContext.Finish(true);
}

void App::Cleanup(void)
{
}

void App::Update(float deltaT)
{
	UpdateMainConstantBuffer(deltaT);


}

void App::UpdateMainConstantBuffer(float deltaT)
{

}

void App::RenderScene(void)
{
	GraphicsContext& RenderContext = GraphicsContext::Begin(L"RenderScene");

	RenderContext.Finish(true);
}