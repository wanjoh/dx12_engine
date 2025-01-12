#pragma once

#include <cheese_grater_common.hpp>

#include <events.hpp>

class Game;

class Window
{
public:
	static constexpr uint8_t BUFFER_COUNT = 3;  // number of swapchain buffers
	
	/// @returns Handle to the window or nullptr if it is not a valid window
	HWND GetWindowHandle() const;

	void Destroy();

	const std::wstring& GetWindowName() const;

	int GetClientWidth() const;
	int GetClientHeihgt() const;

	bool IsVSyncEnabled() const;
	void SetVSync(bool vSync);
	void ToggleVSync();

	bool IsFullscreen() const;
	void SetFullscreen(bool fullscreen);
	void ToggleFullscreen();

	void Show();
	void Hide();

	UINT GetCurrentBackBufferIndex() const;
	/// Present swapchain's backbuffer to the screen 
	/// @returns Current backbuffer index after the present
	UINT Present();
	
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const;
	Microsoft::WRL::ComPtr<ID3D12Resource> GetCurrentBackBuffer() const;
protected:
	friend LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	// only application can create a window
	friend class Application;
	friend class Game;

	Window() = delete;
	Window(HWND hwnd, const std::wstring& windowName, int width, int height, bool vSync);
	virtual ~Window();

	void RegisterCallbacks(std::shared_ptr<Game> game);

	virtual void OnUpdate(UpdateEventArgs& e);
	virtual void OnRender(RenderEventArgs& e);
	virtual void OnKeyPressed(KeyEventArgs& e);
	virtual void OnKeyReleased(KeyEventArgs& e);
	virtual void OnMouseMoved(MouseMotionEventArgs& e);
	virtual void OnMouseButtonPressed(MouseButtonEventArgs& e);
	virtual void OnMouseButtonReleased(MouseButtonEventArgs& e);
	virtual void OnMouseWheel(MouseWheelEventArgs& e);

	void OnResize(ResizeEventArgs& e);
	Microsoft::WRL::ComPtr<IDXGISwapChain4> CreateSwapChain();
	void UpdateRenderTargetViews();

private:
	Window(const Window& other) = delete;
	Window& operator=(const Window& other) = delete;

	std::wstring m_windowName;
	HWND m_hwnd;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[BUFFER_COUNT];

	uint32_t m_width;
	uint32_t m_height;
	uint64_t m_frameCounter;

	bool m_fullscreen;
	bool m_vSync;
	bool m_isTearingSupported;

	UINT m_currentBackBufferIndex;
	UINT m_rtvDescriptorSize;

	RECT m_windowRect;  // saves window size before full screen

	std::weak_ptr<Game> m_game;
};

