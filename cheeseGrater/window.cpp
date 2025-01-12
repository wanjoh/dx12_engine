#include "window.hpp"

#include <application.hpp>
#include <command_queue.hpp>
#include <game.hpp>


Window::Window(HWND hwnd, const std::wstring& windowName, int width, int height, bool vSync)
	: m_windowName(windowName)
	, m_hwnd(hwnd)
	, m_width(width)
	, m_height(height)
	, m_frameCounter(0)
	, m_fullscreen(false)
	, m_vSync(vSync)
{
	Application& app = Application::Get();

	m_isTearingSupported = app.IsTearingSupported();

	m_swapChain = CreateSwapChain();
	m_rtvDescriptorHeap = app.CreateDescriptorHeap(BUFFER_COUNT, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_rtvDescriptorSize = app.GetDescriptorandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UpdateRenderTargetViews();
}

Window::~Window()
{
	assert(!m_hwnd && "Use Application::DestroyWindow before destruction");
}

void Window::RegisterCallbacks(std::shared_ptr<Game> game)
{
	m_game = game;
}

void Window::OnUpdate(UpdateEventArgs& e)
{
	if (auto game = m_game.lock())
	{
		m_frameCounter++;

		game->OnUpdate(e);
	}
}

void Window::OnRender(RenderEventArgs& e)
{
	if (auto game = m_game.lock())
	{
		game->OnRender(e);
	}
}

void Window::OnKeyPressed(KeyEventArgs& e)
{
	if (auto game = m_game.lock())
	{
		game->OnKeyPressed(e);
	}
}

void Window::OnKeyReleased(KeyEventArgs& e)
{
	if (auto game = m_game.lock())
	{
		game->OnKeyReleased(e);
	}
}

void Window::OnMouseMoved(MouseMotionEventArgs& e)
{
	if (auto game = m_game.lock())
	{
		game->OnMouseMoved(e);
	}
}

void Window::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
	if (auto game = m_game.lock())
	{
		game->OnMouseButtonPressed(e);
	}
}

void Window::OnMouseButtonReleased(MouseButtonEventArgs& e)
{
	if (auto game = m_game.lock())
	{
		game->OnMouseButtonReleased(e);
	}
}

void Window::OnMouseWheel(MouseWheelEventArgs& e)
{
	if (auto game = m_game.lock())
	{
		game->OnMouseWheel(e);
	}
}

void Window::OnResize(ResizeEventArgs& e)
{
	if (m_width != e.Width || m_height != e.Height)
	{
		m_width = std::max(1, e.Width);
		m_height = std::max(1, e.Height);

		Application::Get().Flush();

		for (int i = 0; i < BUFFER_COUNT; i++)
		{
			m_backBuffers[i].Reset();
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = { };
		ThrowIfFailed(m_swapChain->GetDesc(&swapChainDesc));
		ThrowIfFailed(m_swapChain->ResizeBuffers(BUFFER_COUNT, m_width, m_height, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));
		m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

		UpdateRenderTargetViews();
	}

	if (auto game = m_game.lock())
	{
		game->OnResize(e);
	}
}

Microsoft::WRL::ComPtr<IDXGISwapChain4> Window::CreateSwapChain()
{
	Application& app = Application::Get();

	Microsoft::WRL::ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory4;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { };
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = BUFFER_COUNT;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	// it's recommended to always allow tearing if tearing support is enabled
	swapChainDesc.Flags = m_isTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ID3D12CommandQueue* commandQueue = app.GetCommandQueue()->GetD3D12CommandQueue().Get();
	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(commandQueue, m_hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1));

	// disable alt+enter fullscreen toggle
	ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));
	m_currentBackBufferIndex = dxgiSwapChain4->GetCurrentBackBufferIndex();

	return dxgiSwapChain4;
}

void Window::UpdateRenderTargetViews()
{
	auto device = Application::Get().GetDevice();
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < BUFFER_COUNT; i++)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
		ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));
		device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);
		m_backBuffers[i] = backBuffer;
		rtvHandle.Offset(m_rtvDescriptorSize);
	}
}

HWND Window::GetWindowHandle() const
{
	return m_hwnd;
}

void Window::Destroy()
{
	if (auto game = m_game.lock())
	{
		game->OnWindowDestroy();
	}
	if (m_hwnd)
	{
		::DestroyWindow(m_hwnd);
		m_hwnd = nullptr;
	}
}

const std::wstring& Window::GetWindowName() const
{
	return m_windowName;
}

int Window::GetClientWidth() const
{
	return m_width;
}

int Window::GetClientHeihgt() const
{
	return m_height;
}

bool Window::IsVSyncEnabled() const
{
	return m_vSync;
}

void Window::SetVSync(bool vSync)
{
	m_vSync = vSync;
}

void Window::ToggleVSync()
{
	SetVSync(!m_vSync);
}

bool Window::IsFullscreen() const
{
	return m_fullscreen;
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
		::GetWindowRect(m_hwnd, &m_windowRect);

		// remove window decorations and set style to "borderless window"
		UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
		::SetWindowLongPtr(m_hwnd, GWL_STYLE, windowStyle);

		HMONITOR hMonitor = ::MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFOEX monitorInfo = { };
		monitorInfo.cbSize = sizeof(MONITORINFOEX);
		::GetMonitorInfo(hMonitor, &monitorInfo);

		::SetWindowPos(m_hwnd, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);
		::ShowWindow(m_hwnd, SW_MAXIMIZE);
	}
	else
	{
		// restore all window decorators
		::SetWindowLong(m_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		::SetWindowPos(m_hwnd, HWND_NOTOPMOST, m_windowRect.left, m_windowRect.top,
			m_windowRect.right - m_windowRect.left, m_windowRect.bottom - m_windowRect.top, SWP_FRAMECHANGED | SWP_NOACTIVATE);
		::ShowWindow(m_hwnd, SW_NORMAL);
	}
}

void Window::ToggleFullscreen()
{
	SetFullscreen(!m_fullscreen);
}

void Window::Show()
{
	::ShowWindow(m_hwnd, SW_SHOW);
}

void Window::Hide()
{
	::ShowWindow(m_hwnd, SW_HIDE);
}

UINT Window::GetCurrentBackBufferIndex() const
{
	return m_currentBackBufferIndex;
}

UINT Window::Present()
{
	UINT syncInterval = m_vSync ? 1 : 0;
	UINT presentFlags = m_isTearingSupported && !m_vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(m_swapChain->Present(syncInterval, presentFlags));
	m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

	return m_currentBackBufferIndex;
}

D3D12_CPU_DESCRIPTOR_HANDLE Window::GetCurrentRenderTargetView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_currentBackBufferIndex, m_rtvDescriptorSize);
}

Microsoft::WRL::ComPtr<ID3D12Resource> Window::GetCurrentBackBuffer() const
{
	return m_backBuffers[m_currentBackBufferIndex];
}
