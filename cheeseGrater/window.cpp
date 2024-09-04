#include "window.hpp"

#include <application.hpp>

void Window::Resize(uint32_t width, uint32_t height)
{
	if (m_width != width || m_height != height)
	{
		m_width = std::max(1u, width);
		m_height = std::max(1u, height);

		Application::GetInstance().Flush();

		for (int i = 0; i < NUM_FRAMES; i++)
		{
			m_backBuffers[i].Reset();
			//m_frameFenceValues[i] = m_frameFenceValues[m_currentBackBufferIndex]; ?
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = { };
		ThrowIfFailed(m_swapChain->GetDesc(&swapChainDesc));
		ThrowIfFailed(m_swapChain->ResizeBuffers(NUM_FRAMES, m_width, m_height, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));
		m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

		UpdateRenderTargetViews(m_device, m_swapChain, m_RTVDescriptorHeap);
	}
}

void Window::SetFullscreen(bool fullscreen)
{
	if (m_fullscreen == fullscreen)
	{
		return;
	}

	m_fullscreen = fullscreen;

	if (m_fullscreen)
	{
		// save current window dimensions
		::GetWindowRect(m_hWnd, &m_windowRect);

		// remove window decorations and set style to "borderless window"
		UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
		::SetWindowLongPtr(m_hWnd, GWL_STYLE, windowStyle);

		HMONITOR hMonitor = ::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFOEX monitorInfo = { };
		monitorInfo.cbSize = sizeof(MONITORINFOEX);
		::GetMonitorInfo(hMonitor, &monitorInfo);

		::SetWindowPos(m_hWnd, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);
		::ShowWindow(m_hWnd, SW_MAXIMIZE);
	}
	else
	{
		// restore all window decorators
		::SetWindowLong(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		::SetWindowPos(m_hWnd, HWND_NOTOPMOST, m_windowRect.left, m_windowRect.top,
			m_windowRect.right - m_windowRect.left, m_windowRect.bottom - m_windowRect.top, SWP_FRAMECHANGED | SWP_NOACTIVATE);
		::ShowWindow(m_hWnd, SW_NORMAL);
	}
}
