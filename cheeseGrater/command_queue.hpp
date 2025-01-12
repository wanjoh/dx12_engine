#pragma once
#include <cheese_grater_common.hpp>

#include <queue>

class CommandQueue
{
public:
	CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
	~CommandQueue() = default;

	uint64_t Signal();
	void WaitForFenceValue(uint64_t fenceValue);
	void Flush();
	bool IsFenceComplete(uint64_t fenceValue);

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCommandList();
	/// @return Fence value to wait for this command list
	uint64_t ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList);

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator);
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;
private:
	struct CommandAllocatorEntry
	{
		uint64_t fenceValue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	};

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_d3d12commandQueue;
	Microsoft::WRL::ComPtr<ID3D12Device2> m_device;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;

	D3D12_COMMAND_LIST_TYPE m_commandListType;
	HANDLE m_fenceEvent;
	uint64_t m_fenceValue;
	std::queue<CommandAllocatorEntry> m_commandAllocatorQueue;
	std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>> m_commandListQueue;
};
