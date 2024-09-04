#pragma once

#include <cheese_grater_common.hpp>

class Window
{
public:
	static constexpr uint8_t NUM_FRAMES = 3;  // number of swapchain buffers

	Window();
	Window(const Window& other) = delete;
	~Window() = default;

	Window& operator=(const Window& other) = delete;

	void Resize(uint32_t width, uint32_t height);
	void SetFullscreen(bool fullScreen);
	void ToggleVSync();
private:
	ComPtr<IDXGISwapChain4> m_swapChain;
	ComPtr<ID3D12Resource> m_backBuffers[NUM_FRAMES];

	uint32_t m_width;
	uint32_t m_height;
	RECT m_windowRect;  // saves window size before full screen
	HWND m_hWnd;

	bool m_fullscreen = false;
	bool m_vSync = true;

	UINT m_currentBackBufferIndex;

};

