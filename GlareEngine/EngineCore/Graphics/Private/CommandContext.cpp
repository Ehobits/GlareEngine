#include "L3DUtil.h"
#include "CommandContext.h"
#include "ColorBuffer.h"
#include "DepthBuffer.h"
#include "GraphicsCore.h"
#include "DescriptorHeap.h"

namespace GlareEngine 
{
    namespace DirectX12Graphics
    {


#pragma region ContextManager
		CommandContext* ContextManager::AllocateContext(D3D12_COMMAND_LIST_TYPE Type)
		{
			std::lock_guard<std::mutex> LockGuard(m_ContextAllocationMutex);
			auto& AvailableContexts = m_AvailableContexts[Type];

			CommandContext* ret = nullptr;
			if (AvailableContexts.empty())
			{
				ret = new CommandContext(Type);
				m_ContextPool[Type].emplace_back(ret);
				ret->Initialize();
			}
			else
			{
				ret = AvailableContexts.front();
				AvailableContexts.pop();
				ret->Reset();
			}
			assert(ret != nullptr);
			assert(ret->m_Type == Type);
			return ret;
		}

		void ContextManager::FreeContext(CommandContext* Context)
		{
			assert(Context != nullptr);
			std::lock_guard<std::mutex> LockGuard(m_ContextAllocationMutex);
			m_AvailableContexts[Context->m_Type].push(Context);
		}

		void ContextManager::DestroyAllContexts(void)
		{
			for (uint32_t i = 0; i < 4; ++i)
				m_ContextPool[i].clear();
		}
#pragma endregion

#pragma region CommandContext
		CommandContext::CommandContext(D3D12_COMMAND_LIST_TYPE Type) :
			m_Type(Type),
			m_DynamicViewDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
			m_DynamicSamplerDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
			m_CPULinearAllocator(CPUWritable),
			m_GPULinearAllocator(GPUExclusive)
		{
			m_OwningManager = nullptr;
			m_CommandList = nullptr;
			m_CurrentAllocator = nullptr;
			ZeroMemory(m_CurrentDescriptorHeaps, sizeof(m_CurrentDescriptorHeaps));

			m_CurGraphicsRootSignature = nullptr;
			m_CurPipelineState = nullptr;
			m_CurComputeRootSignature = nullptr;
			m_NumBarriersToFlush = 0;
		}

		CommandContext::~CommandContext(void)
		{
			if (m_CommandList != nullptr)
				m_CommandList->Release();
		}

		inline void CommandContext::FlushResourceBarriers(void)
		{
			if (m_NumBarriersToFlush > 0)
			{
				m_CommandList->ResourceBarrier(m_NumBarriersToFlush, m_ResourceBarrierBuffer);
				m_NumBarriersToFlush = 0;
			}
		}

		void CommandContext::Initialize(void)
		{
			g_CommandManager.CreateNewCommandList(m_Type, &m_CommandList, &m_CurrentAllocator);
		}


		void CommandContext::BindDescriptorHeaps(void)
		{
			UINT NumDescriptorHeaps = 0;
			ID3D12DescriptorHeap* HeapsToBind[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
			for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
			{
				ID3D12DescriptorHeap* HeapIter = m_CurrentDescriptorHeaps[i];
				if (HeapIter != nullptr)
					HeapsToBind[NumDescriptorHeaps++] = HeapIter;
			}

			if (NumDescriptorHeaps > 0)
				m_CommandList->SetDescriptorHeaps(NumDescriptorHeaps, HeapsToBind);
		}

		void CommandContext::Reset(void)
		{
			//����ֻ����ǰ�ͷŵ������ĵ���Reset()�� �����б��������ڣ������Ǳ�������һ���µķ������� 
			assert(m_CommandList != nullptr && m_CurrentAllocator == nullptr);
			m_CurrentAllocator = g_CommandManager.GetQueue(m_Type).RequestAllocator();
			m_CommandList->Reset(m_CurrentAllocator, nullptr);

			m_CurGraphicsRootSignature = nullptr;
			m_CurPipelineState = nullptr;
			m_CurComputeRootSignature = nullptr;
			m_NumBarriersToFlush = 0;

			BindDescriptorHeaps();
		}

		void CommandContext::DestroyAllContexts(void)
		{
			LinearAllocator::DestroyAll();
			DynamicDescriptorHeap::DestroyAll();
			g_ContextManager.DestroyAllContexts();
		}

		CommandContext& CommandContext::Begin(const std::wstring ID)
		{
			CommandContext* NewContext = g_ContextManager.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
			NewContext->SetID(ID);
			return *NewContext;
		}

		uint64_t CommandContext::Flush(bool WaitForCompletion)
		{
			FlushResourceBarriers();

			assert(m_CurrentAllocator != nullptr);

			uint64_t FenceValue = g_CommandManager.GetQueue(m_Type).ExecuteCommandList(m_CommandList);

			if (WaitForCompletion)
				g_CommandManager.WaitForFence(FenceValue);

			//
			// Reset the command list and restore previous state
			//

			m_CommandList->Reset(m_CurrentAllocator, nullptr);

			if (m_CurGraphicsRootSignature)
			{
				m_CommandList->SetGraphicsRootSignature(m_CurGraphicsRootSignature);
			}
			if (m_CurComputeRootSignature)
			{
				m_CommandList->SetComputeRootSignature(m_CurComputeRootSignature);
			}
			if (m_CurPipelineState)
			{
				m_CommandList->SetPipelineState(m_CurPipelineState);
			}

			BindDescriptorHeaps();

			return FenceValue;
		}


		uint64_t CommandContext::Finish(bool WaitForCompletion)
		{
			assert(m_Type == D3D12_COMMAND_LIST_TYPE_DIRECT || m_Type == D3D12_COMMAND_LIST_TYPE_COMPUTE);

			FlushResourceBarriers();

			assert(m_CurrentAllocator != nullptr);

			CommandQueue& Queue = g_CommandManager.GetQueue(m_Type);

			uint64_t FenceValue = Queue.ExecuteCommandList(m_CommandList);
			Queue.DiscardAllocator(FenceValue, m_CurrentAllocator);
			m_CurrentAllocator = nullptr;

			m_CPULinearAllocator.CleanupUsedPages(FenceValue);
			m_GPULinearAllocator.CleanupUsedPages(FenceValue);
			m_DynamicViewDescriptorHeap.CleanupUsedHeaps(FenceValue);
			m_DynamicSamplerDescriptorHeap.CleanupUsedHeaps(FenceValue);

			if (WaitForCompletion)
				g_CommandManager.WaitForFence(FenceValue);

			g_ContextManager.FreeContext(this);

			return FenceValue;
		}
#pragma endregion







    }//DirectX12Graphics
}//GlareEngine 

