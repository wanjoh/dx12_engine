#pragma once
#include <cheese_grater_common.hpp>

#include <queue>

class CommandQueue
{
public:
	CommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
	~CommandQueue() = default;

	uint64_t Signal();
	void WaitForFenceValue(uint64_t fenceValue);
	void Flush();
	bool IsFenceComplete(uint64_t fenceValue);

	ComPtr<ID3D12GraphicsCommandList2> GetCommandList();
	/// <returns>Fence value to wait for this command list</returns>
	uint64_t ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList2> commandList);

	ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
	ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(ComPtr<ID3D12CommandAllocator> allocator);
private:
	struct CommandAllocatorEntry
	{
		uint64_t fenceValue;
		ComPtr<ID3D12CommandAllocator> commandAllocator;
	};

	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12Device2> m_device;
	ComPtr<ID3D12Fence> m_fence;
	
	D3D12_COMMAND_LIST_TYPE m_commandListType;
	HANDLE m_fenceEvent;
	uint64_t m_fenceValue;
	std::queue<CommandAllocatorEntry> m_commandAllocatorQueue;
	std::queue<ComPtr<ID3D12GraphicsCommandList2>> g_commandListQueue;
};
