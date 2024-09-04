//// minimizes the number of header files that are included in Windows.h
//#define WIN32_LEAN_AND_MEAN
//
//#include <Windows.h>
//#include <shellapi.h>  // CommandLineToArgW
//
//// min/max macros conflict; only use std::min and std::max from <algorithm>
//#if defined(min)
//#undef min
//#endif
//
//#if defined(max)
//#undef max
//#endif
//
//#if defined(CreateWindow)
//#undef CreateWindow
//#endif
//
//#include <wrl.h>  // Microsoft::WRL::ComPtr<>
//using namespace Microsoft::WRL;
//
//#include <d3dx12.h>
//#include <d3d12.h>
//#include <dxgi1_6.h>
//#include <d3dcompiler.h>
//#include <DirectXMath.h>

#include <algorithm>
#include <cassert>
#include <chrono>

//const uint8_t g_numFrames = 3;  // swap chain buffers
bool g_useWarp = false;

//uint32_t g_clientWidth = 1280;
//uint32_t g_clientHeight = 720;

bool g_isInitialized = false;

//HWND g_hWnd;
//RECT g_windowRect;  // saves window size before full screen

//ComPtr<ID3D12Device2> g_device;
//ComPtr<ID3D12CommandQueue> g_commandQueue;
//ComPtr<IDXGISwapChain4> g_swapChain;
//ComPtr<ID3D12Resource> g_backBuffers[g_numFrames];
ComPtr<ID3D12GraphicsCommandList> g_commandList;
ComPtr<ID3D12CommandAllocator> g_commandAllocators[g_numFrames];
ComPtr<ID3D12DescriptorHeap> g_RTVDescriptorHeap;

UINT g_RTVDescriptorSize;
//UINT g_currentBackBufferIndex;

// synchronization
//ComPtr<ID3D12Fence> g_fence;
//uint64_t g_fenceValue = 0;
//uint64_t g_frameFenceValues[g_numFrames] = { };
//HANDLE g_fenceEvent;

//bool g_vSync = true;
bool g_tearingSupported = false;
//bool g_fullScreen = false;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void ParseCommandLineArguments()
{
	int argc;
	wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

	for (size_t i = 0; i < argc; i++)
	{
		if (::wcscmp(argv[i], L"-w") == 0 || ::wcscmp(argv[i], L"--width") == 0)
		{
			g_clientWidth = ::wcstol(argv[++i], nullptr, 10);
		}

		if (::wcscmp(argv[i], L"-h") == 0 || ::wcscmp(argv[i], L"--height") == 0)
		{
			g_clientHeight = ::wcstol(argv[++i], nullptr, 10);
		}

		if (::wcscmp(argv[i], L"-warp") == 0 || ::wcscmp(argv[i], L"--warp") == 0)
		{
			g_useWarp = true;
		}
	}

	::LocalFree(argv);
}

void EnableDebugLayer()
{
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName)
{
	WNDCLASSEXW windowClass = { };
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = &WndProc;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInst;
	windowClass.hIcon = ::LoadIcon(hInst, NULL);
	windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	windowClass.lpszMenuName = NULL;
	windowClass.lpszClassName = windowClassName;
	windowClass.hIconSm = ::LoadIcon(hInst, NULL);

	static ATOM atom = ::RegisterClassExW(&windowClass);
	assert(atom > 0);
}

HWND CreateWindow(const wchar_t* windowClassName, HINSTANCE hInst,
	const wchar_t* windowTitle, uint32_t width, uint32_t height)
{
	const int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
	const int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);
	
	RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
	::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	const int windowWidth = windowRect.right - windowRect.left;
	const int windowHeight = windowRect.bottom - windowRect.top;

	// center the window
	const int windowTopLeftX = std::max<int>(0, (screenWidth - windowWidth) / 2);
	const int windowTopLeftY = std::max<int>(0, (screenHeight - windowHeight) / 2);

	HWND hWnd = ::CreateWindowExW(NULL, windowClassName, windowTitle, WS_OVERLAPPEDWINDOW,
		windowTopLeftX, windowTopLeftY, windowWidth, windowHeight, NULL, NULL, hInst, nullptr);
	assert(hWnd && "Window creation failed");
	
	return hWnd;
}

ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp)
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	ComPtr<IDXGIAdapter1> dxgiAdapter1;
	ComPtr<IDXGIAdapter4> dxgiAdapter4;

	if (useWarp)
	{
		ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
		ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
	}
	else
	{
		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; i++)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 
				&& SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))
				&& dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
			}
		}
	}

	return dxgiAdapter4;
}

ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
	ComPtr<ID3D12Device2> d3d12Device2;
	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));

#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// suppress whole categories of messages
		//D3D12_MESSAGE_CATEGORY Categories[] = { }

		// suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };

		// suppress individual messages by their id
		D3D12_MESSAGE_ID denyIDs[] =
		{
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,	// clearing rt with arbitrary clear color
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,							// using capture frame while graphics debugging
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
		};

		D3D12_INFO_QUEUE_FILTER filter = { };
		//filter.DenyList.NumCategories = _countof(categories);
		//filter.DenyList.pCategoryList = categories;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		filter.DenyList.NumIDs = _countof(denyIDs);
		filter.DenyList.pIDList = denyIDs;

		ThrowIfFailed(pInfoQueue->PushStorageFilter(&filter));
	}
#endif

	return d3d12Device2;
}

//ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
//{
//	ComPtr<ID3D12CommandQueue> commandQueue;
//
//	D3D12_COMMAND_QUEUE_DESC desc = { };
//	desc.Type = type;
//	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
//	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
//	desc.NodeMask = 0;
//
//	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)));
//
//	return commandQueue;
//}

bool CheckTearingSupport()
{
	BOOL allowTearing = FALSE;

	ComPtr<IDXGIFactory4> factory4;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
	{
		ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(factory4.As(&factory5)))
		{
			if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
			{
				allowTearing = FALSE;
			}
		}
	}
	return allowTearing == TRUE;
}

ComPtr<IDXGISwapChain4> CreateSwapChain(HWND hWnd, ComPtr<ID3D12CommandQueue> commandQueue, uint32_t width, uint32_t height, uint32_t bufferCount)
{
	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	ComPtr<IDXGIFactory4> dxgiFactory4;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { };
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	// it's recommended to always allow tearing if tiearing support is enabled
	swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(commandQueue.Get(), hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1));

	// disable alt+enter fullscreen toggle
	ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

	return dxgiSwapChain4;
}

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
{
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	D3D12_DESCRIPTOR_HEAP_DESC desc = { };
	desc.NumDescriptors = numDescriptors;
	desc.Type = type;

	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

void UpdateRenderTargetViews(ComPtr<ID3D12Device2> device, ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
	auto rtvDecriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < g_numFrames; i++)
	{
		ComPtr<ID3D12Resource> backBuffer;
		ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));
		device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);
		g_backBuffers[i] = backBuffer;
		rtvHandle.Offset(rtvDecriptorSize);
	}
}

//ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
//{
//	ComPtr<ID3D12CommandAllocator> commandAllocator;
//
//	ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));
//	
//	return commandAllocator;
//}

//ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device, ComPtr<ID3D12CommandAllocator> commandAllocator,
//	D3D12_COMMAND_LIST_TYPE type)
//{
//	ComPtr<ID3D12GraphicsCommandList> commandList;
//
//	ThrowIfFailed(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
//	ThrowIfFailed(commandList->Close());
//
//	return commandList;
//}

//ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device)
//{
//	ComPtr<ID3D12Fence> fence;
//
//	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
//
//	return fence;
//}

//HANDLE CreateEventHandle()
//{
//	HANDLE fenceEvent;
//
//	fenceEvent = ::CreateEventW(NULL, FALSE, FALSE, NULL);
//	assert(fenceEvent && "Failed to create fence event");
//
//	return fenceEvent;
//}

//uint64_t Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue)
//{
//	uint64_t fenceValueForSignal = ++fenceValue;
//
//	ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValueForSignal));
//
//	return fenceValueForSignal;
//}

//void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent,
//	std::chrono::milliseconds duration = std::chrono::milliseconds::max())
//{
//	if (fence->GetCompletedValue() < fenceValue)
//	{
//		ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
//		::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
//	}
//}

//void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue, HANDLE fenceEvent)
//{
//	uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
//	WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
//}

constexpr size_t BUFFER_SIZE = 128;
void Update()
{
	static uint64_t frameCounter = 0;
	static double elapsedSeconds = 0.0;
	static std::chrono::high_resolution_clock clock;
	static auto t0 = clock.now();

	frameCounter++;
	auto t1 = clock.now();
	auto deltaTime = t1 - t0;
	t0 = t1;
	elapsedSeconds += deltaTime.count() * 1e-9;

	if (elapsedSeconds > 1.0)
	{
		char buffer[BUFFER_SIZE];
		wchar_t wbuffer[BUFFER_SIZE];
		auto fps = frameCounter / elapsedSeconds;
		sprintf_s(buffer, BUFFER_SIZE, "FPS= %f\n", fps);
		::MultiByteToWideChar(CP_ACP, 0, buffer, -1, wbuffer, BUFFER_SIZE);
		OutputDebugStringW(wbuffer);

		frameCounter = 0;
		elapsedSeconds = 0.0;
	}
}


constexpr FLOAT CLEAR_COLOR[4] = {0.18f, 0.5f, 0.44f, 1.f};
void Render()
{
	auto commandAllocator = g_commandAllocators[g_currentBackBufferIndex];
	auto backBuffer = g_backBuffers[g_currentBackBufferIndex];

	commandAllocator->Reset();
	g_commandList->Reset(commandAllocator.Get(), nullptr);

	// clear RT
	{
		CD3DX12_RESOURCE_BARRIER barrier = 
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		g_commandList->ResourceBarrier(1, &barrier);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), g_currentBackBufferIndex, g_RTVDescriptorSize);
		g_commandList->ClearRenderTargetView(rtv, CLEAR_COLOR, 0, nullptr);
	}

	// present
	{
		CD3DX12_RESOURCE_BARRIER barrier = 
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		g_commandList->ResourceBarrier(1, &barrier);

		ThrowIfFailed(g_commandList->Close());
		ID3D12CommandList* const commandLists[] = { g_commandList.Get() };
		g_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

		UINT syncInterval = g_vSync ? 1 : 0;
		UINT presentFlags = g_tearingSupported && !g_vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		ThrowIfFailed(g_swapChain->Present(syncInterval, presentFlags));

		g_frameFenceValues[g_currentBackBufferIndex] = Signal(g_commandQueue, g_fence, g_fenceValue);

		g_currentBackBufferIndex = g_swapChain->GetCurrentBackBufferIndex();
		WaitForFenceValue(g_fence, g_frameFenceValues[g_currentBackBufferIndex], g_fenceEvent);
	}
}

//void Resize(uint32_t width, uint32_t height)
//{
//	if (g_clientWidth != width || g_clientHeight != height)
//	{
//		g_clientWidth = std::max(1u, width);
//		g_clientHeight = std::max(1u, height);
//
//		Flush(g_commandQueue, g_fence, g_fenceValue, g_fenceEvent);
//
//		for (int i = 0; i < g_numFrames; i++)
//		{
//			g_backBuffers[i].Reset();
//			g_frameFenceValues[i] = g_frameFenceValues[g_currentBackBufferIndex];
//		}
//
//		DXGI_SWAP_CHAIN_DESC swapChainDesc = { };
//		ThrowIfFailed(g_swapChain->GetDesc(&swapChainDesc));
//		ThrowIfFailed(g_swapChain->ResizeBuffers(g_numFrames, g_clientWidth, g_clientHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));
//		g_currentBackBufferIndex = g_swapChain->GetCurrentBackBufferIndex();
//
//		UpdateRenderTargetViews(g_device, g_swapChain, g_RTVDescriptorHeap);
//	}
//}

//void SetFullScreen(bool fullscreen)
//{
//	if (g_fullScreen == fullscreen)
//	{
//		return;
//	}
//
//	g_fullScreen = fullscreen;
//
//	if (g_fullScreen)
//	{
//		// save current window dimensions
//		::GetWindowRect(g_hWnd, &g_windowRect);
//	
//		// remove window decorations and set style to "borderless window"
//		UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
//		::SetWindowLongPtr(g_hWnd, GWL_STYLE, windowStyle);
//
//		HMONITOR hMonitor = ::MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
//		MONITORINFOEX monitorInfo = { };
//		monitorInfo.cbSize = sizeof(MONITORINFOEX);
//		::GetMonitorInfo(hMonitor, &monitorInfo);
//
//		::SetWindowPos(g_hWnd, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
//			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
//			SWP_FRAMECHANGED | SWP_NOACTIVATE);
//		::ShowWindow(g_hWnd, SW_MAXIMIZE);
//	}
//	else
//	{
//		// restore all window decorators
//		::SetWindowLong(g_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
//		::SetWindowPos(g_hWnd, HWND_NOTOPMOST, g_windowRect.left, g_windowRect.top,
//			g_windowRect.right - g_windowRect.left, g_windowRect.bottom - g_windowRect.top, SWP_FRAMECHANGED | SWP_NOACTIVATE);
//		::ShowWindow(g_hWnd, SW_NORMAL);
//	}
//}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// todo: refactor somehow
	if (!g_isInitialized)
	{
		return ::DefWindowProcW(hwnd, message, wParam, lParam);
	}

	switch (message)
	{
	case WM_PAINT:
		Update();
		Render();
	break;
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
		{
			bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
			switch (wParam)
			{
			case 'V':
				g_vSync = !g_vSync;
				break;
			case VK_ESCAPE:
				::PostQuitMessage(0);
				break;
			case VK_RETURN:
				if (alt)
				{
			case VK_F11:
				SetFullScreen(!g_fullScreen);
				}
			break;
			}
		}
	break;
	// default window procedure will play system notification sound upon pressing alt+enter if this message is not handled
	case WM_SYSCHAR:
	break;
	case WM_SIZE:
	{
		RECT clientRect = { };
		::GetClientRect(hwnd, &clientRect);
		const int width = clientRect.right - clientRect.left;
		const int height = clientRect.bottom - clientRect.top;
		Resize(width, height);
	}
	break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		break;
	default:
		return ::DefWindowProcW(hwnd, message, wParam, lParam);
	}
	
	return 0;
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	// using this awareness context allows the client area of the window to achieve 100% scaling 
	// while still allowing non-client window content to be rendered in a DPI sensitive fashion.
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	const wchar_t* windowClassName = L"DX12WindowClass";
	
	ParseCommandLineArguments();

	EnableDebugLayer();

	g_tearingSupported = CheckTearingSupport();

	RegisterWindowClass(hInstance, windowClassName);

	g_hWnd = CreateWindow(windowClassName, hInstance, L"Mfkin cheese grater", g_clientWidth, g_clientHeight);

	::GetWindowRect(g_hWnd, &g_windowRect);

	ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter(g_useWarp);
	g_device = CreateDevice(dxgiAdapter4);
	g_commandQueue = CreateCommandQueue(g_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	g_swapChain = CreateSwapChain(g_hWnd, g_commandQueue, g_clientWidth, g_clientHeight, g_numFrames);
	g_currentBackBufferIndex = g_swapChain->GetCurrentBackBufferIndex();
	g_RTVDescriptorHeap = CreateDescriptorHeap(g_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_numFrames);
	g_RTVDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UpdateRenderTargetViews(g_device, g_swapChain, g_RTVDescriptorHeap);

	for (int i = 0; i < g_numFrames; i++)
	{
		g_commandAllocators[i] = CreateCommandAllocator(g_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}
	g_commandList = CreateCommandList(g_device, g_commandAllocators[g_currentBackBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);

	g_fence = CreateFence(g_device);
	g_fenceEvent = CreateEventHandle();

	g_isInitialized = true;
	::ShowWindow(g_hWnd, SW_SHOW);
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessageW(&msg);
		}
	}

	Flush(g_commandQueue, g_fence, g_fenceValue, g_fenceEvent);
	::CloseHandle(g_fenceEvent);

	return 0;
}
