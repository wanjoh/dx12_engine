#include "command_queue.hpp"

CommandQueue::CommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
	: m_device(device)
	, m_commandListType(type)
	, m_fenceValue(0)
{
	D3D12_COMMAND_QUEUE_DESC desc = { };
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;
	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)));

	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

	m_fenceEvent = ::CreateEventW(NULL, FALSE, FALSE, NULL);
	assert(m_fenceEvent && "Failed to create fence event");
}

uint64_t CommandQueue::Signal()
{
	uint64_t fenceValueForSignal = ++m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceValueForSignal));
	return fenceValueForSignal;
}

void CommandQueue::WaitForFenceValue(uint64_t fenceValue)
{
	if (!IsFenceComplete(fenceValue))
	{
		m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
		::WaitForSingleObject(m_fenceEvent, DWORD_MAX);
	}
}

void CommandQueue::Flush()
{
	WaitForFenceValue(Signal());
}

bool CommandQueue::IsFenceComplete(uint64_t fenceValue)
{
	return m_fence->GetCompletedValue() >= fenceValue;
}

ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetCommandList()
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList2> commandList;

}

ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator()
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ThrowIfFailed(m_device->CreateCommandAllocator(m_commandListType, IID_PPV_ARGS(&commandAllocator)));
	return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(ComPtr<ID3D12CommandAllocator> allocator)
{
	ComPtr<ID3D12GraphicsCommandList2> commandList;
	ThrowIfFailed(m_device->CreateCommandList(0, m_commandListType, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
	return commandList;
}

