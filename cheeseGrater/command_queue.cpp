#include "command_queue.hpp"

CommandQueue::CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
	: m_device(device)
	, m_commandListType(type)
	, m_fenceValue(0)
{
	D3D12_COMMAND_QUEUE_DESC desc = { };
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;
	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_d3d12commandQueue)));

	ThrowIfFailed(device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

	m_fenceEvent = ::CreateEventW(NULL, FALSE, FALSE, NULL);
	assert(m_fenceEvent && "Failed to create fence event");
}

uint64_t CommandQueue::Signal()
{
	ThrowIfFailed(m_d3d12commandQueue->Signal(m_fence.Get(), ++m_fenceValue));
	return m_fenceValue;
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

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetCommandList()
{
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList;

	if (!m_commandAllocatorQueue.empty() && IsFenceComplete(m_commandAllocatorQueue.front().fenceValue))
	{
		commandAllocator = m_commandAllocatorQueue.front().commandAllocator;
		m_commandAllocatorQueue.pop();

		ThrowIfFailed(commandAllocator->Reset());
	}
	else
	{
		commandAllocator = CreateCommandAllocator();
	}

	if (!m_commandListQueue.empty())
	{
		commandList = m_commandListQueue.front();
		m_commandListQueue.pop();

		ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));
	}
	else
	{
		commandList = CreateCommandList(commandAllocator);
	}

	ThrowIfFailed(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));

	return commandList;
}

uint64_t CommandQueue::ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList)
{
	commandList->Close();

	ID3D12CommandAllocator* commandAllocator;
	UINT dataSize = sizeof(commandAllocator);
	ThrowIfFailed(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));

	ID3D12CommandList* const ppCommandLists[] = { commandList.Get() };

	m_d3d12commandQueue->ExecuteCommandLists(1, ppCommandLists);

	uint64_t fenceValue = Signal();

	m_commandAllocatorQueue.emplace(CommandAllocatorEntry(fenceValue, commandAllocator));
	m_commandListQueue.emplace(commandList);

	// The ownership of the command allocator has been transferred to the Microsoft::WRL::ComPtr
	// in the command allocator queue. It is safe to release the reference 
	// in this temporary COM pointer here.
	commandAllocator->Release();
	return fenceValue;
}

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator()
{
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	ThrowIfFailed(m_device->CreateCommandAllocator(m_commandListType, IID_PPV_ARGS(&commandAllocator)));
	return commandAllocator;
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator)
{
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList;
	ThrowIfFailed(m_device->CreateCommandList(0, m_commandListType, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
	return commandList;
}

Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const
{
	return m_d3d12commandQueue;
}

