#pragma once
#include "L3DUtil.h"
#include "DescriptorHeap.h"

class CommandContext;
class CommandListManager;
class CommandSignature;
class ContextManager;
class ColorBuffer;
class DepthBuffer;
class GraphicsPSO;

namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		using namespace Microsoft::WRL;

		void Initialize(void);
		void Resize(uint32_t width, uint32_t height);
		void Terminate(void);
		void Shutdown(void);
		void Present(void);

		extern uint32_t g_DisplayWidth;
		extern uint32_t g_DisplayHeight;

		//������Ӧ�ó�����������������֡��.
		uint64_t GetFrameCount(void);

		//�������ɵ�֡�о�����ʱ������ �ڲ���֡�ڼ䣬CPU/GPU���ܴ��ڿ���״̬��
		//֡ʱ�������ʾÿ��֡�ĵ���֮���ʱ�䡣
		float GetFrameTime(void);

		//ÿ�����֡��
		float GetFrameRate(void);

		extern ID3D12Device* g_Device;
		extern CommandListManager g_CommandManager;
		extern ContextManager g_ContextManager;

		extern D3D_FEATURE_LEVEL g_D3DFeatureLevel;
		extern bool g_bTypedUAVLoadSupport_R11G11B10_FLOAT;
		extern bool g_bEnableHDROutput;

		extern DescriptorAllocator g_DescriptorAllocator[];
		inline D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1)
		{
			return g_DescriptorAllocator[Type].Allocate(Count);
		}

		extern RootSignature g_GenerateMipsRS;
		extern ComputePSO g_GenerateMipsLinearPSO[4];
		extern ComputePSO g_GenerateMipsGammaPSO[4];

		enum eResolution { E720p,E900p, E1080p, E1440p, E1800p, E2160p };

		extern bool s_EnableVSync;
		extern eResolution TargetResolution;
		extern uint32_t g_DisplayWidth;
		extern uint32_t g_DisplayHeight;
	}
}
