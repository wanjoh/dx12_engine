#pragma once

#include <cheese_grater_common.hpp>

#include <game.hpp>
#include <map>
#include <window.hpp>

class RotatableCube : public Game
{
public:
	RotatableCube(const std::wstring& name, int width, int height, bool vSync = true);

	virtual bool LoadContent() override;
	virtual void UnloadContent() override;

protected:
	virtual void OnUpdate(UpdateEventArgs& e) override;
	virtual void OnRender(RenderEventArgs& e) override;
	virtual void OnKeyPressed(KeyEventArgs& e) override;
	virtual void OnKeyReleased(KeyEventArgs& e) override;
	virtual void OnMouseWheel(MouseWheelEventArgs& e) override;
	virtual void OnResize(ResizeEventArgs& e) override;

private:
	void TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, Microsoft::WRL::ComPtr<ID3D12Resource> resource,
		D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);
	void ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor);
	void ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth = 1.0f);
	void UpdateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource** pDestinationResource,
		ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	void ResizeDepthBuffer(int width, int height);

	void UpdateRotation(KeyCode::Key key, bool released = false);
	
	uint64_t m_fenceValues[Window::BUFFER_COUNT];

	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthBuffer;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;

	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;

	float m_fov;

	DirectX::XMMATRIX m_modelMatrix;
	DirectX::XMMATRIX m_viewMatrix;
	DirectX::XMMATRIX m_projectionMatrix;

	bool m_contentLoaded;

	const std::map<KeyCode::Key, int> m_keyToIndex =
	{
	{KeyCode::W, 0},
	{KeyCode::S, 1},
	{KeyCode::A, 2},
	{KeyCode::D, 3},
	};
	DirectX::XMVECTORF32 m_rotationDirection;
};

