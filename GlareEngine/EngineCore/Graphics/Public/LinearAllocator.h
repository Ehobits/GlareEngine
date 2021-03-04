#pragma once

#include "GPUResource.h"
#include <vector>
#include <queue>
#include <mutex>


// �����������16�������ı�����ÿ������Ϊ16�ֽڡ�
#define DEFAULT_ALIGN 256
namespace GlareEngine {
	namespace DirectX12Graphics {
		//  if you are unsure.
		struct DynAlloc
		{
			//�������͵ķ�����ܰ���NULLָ�롣 �ڽ������֮ǰ������
			DynAlloc(GPUResource& BaseResource, size_t ThisOffset, size_t ThisSize)
				: Buffer(BaseResource), Offset(ThisOffset), Size(ThisSize) {}

			GPUResource& Buffer;      // ����ڴ��������D3D��������
			size_t Offset;            // ��������Դ��ʼ��ƫ����.
			size_t Size;              // ���η���ı�����С
			void* DataPtr;            // CPU��д��ַ
			D3D12_GPU_VIRTUAL_ADDRESS GPUAddress;    // GPU�ɼ���ַ
		};


		class LinearAllocator
		{
		};
	}
}
