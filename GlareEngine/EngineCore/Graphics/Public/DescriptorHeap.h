#pragma once

#include <mutex>
#include <vector>
#include <queue>
#include <string>
#include "L3DUtil.h"
namespace GlareEngine
{
	//����һ���޽���Դ�������������� ּ���ڴ�����ԴʱΪCPU�ɼ�����Դ�������ṩ�ռ䡣 
	//������Щ��Ҫʹ��ɫ���ɼ��Ķ�����Ҫ�����Ǹ��Ƶ�UserDescriptorHeap��DynamicDescriptorHeap��
	class DescriptorAllocator
	{
	public:
		DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE Type) : m_Type(Type), m_CurrentHeap(nullptr) {}

		D3D12_CPU_DESCRIPTOR_HANDLE Allocate(uint32_t Count);

		static void DestroyAll(void);

	protected:

		static const uint32_t sm_NumDescriptorsPerHeap = 256;
		static std::mutex sm_AllocationMutex;
		static std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> sm_DescriptorHeapPool;
		static ID3D12DescriptorHeap* RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type);

		D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
		ID3D12DescriptorHeap* m_CurrentHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE m_CurrentHandle;
		uint32_t m_DescriptorSize;
		uint32_t m_RemainingFreeHandles;
	};

}