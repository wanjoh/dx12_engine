#include "application.hpp"

#include <unordered_map>
#include <command_queue.hpp>
#include <window.hpp>
#include <game.hpp>


constexpr const wchar_t* WINDOW_CLASS_NAME = L"CheeseGraterWindowClass";

using WindowPtr = std::shared_ptr<Window>;
using WindowMap = std::unordered_map<HWND, WindowPtr>;
using WindowNameMap = std::unordered_map<std::wstring, WindowPtr>;

static Application* g_pSingleton = nullptr;
static WindowMap g_windows;
static WindowNameMap g_windowsByName;

// A wrapper struct to allow shared pointers for the window class.
struct MakeWindow : public Window 
{
	MakeWindow(HWND hWnd, const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync)
		: Window(hWnd, windowName, clientWidth, clientHeight, vSync)
	{}
};

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

Application::~Application()
{
	Flush();
}

void Application::Create(HINSTANCE hInst)
{
	if (!g_pSingleton)
	{
		g_pSingleton = new Application(hInst);
	}
}

void Application::Destroy()
{
	if (g_pSingleton)
	{
		assert(g_windows.empty() && g_windowsByName.empty() && "All windows should be destroyed before destroying the application instance");
		delete g_pSingleton;
		g_pSingleton = nullptr;
	}
}

Application& Application::Get()
{
	assert(g_pSingleton);
	return *g_pSingleton;
}

bool Application::IsTearingSupported() const
{
	return m_tearingSupported;
}

std::shared_ptr<Window> Application::CreateRenderWindow(const std::wstring& windowName, int width, int height, bool vSync)
{	
	// todo: center the window?
	//const int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
	//const int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

	auto windowIt = g_windowsByName.find(windowName);
	if (windowIt != g_windowsByName.end())
	{
		return windowIt->second;
	}

	RECT windowRect = { 0, 0, width, height };
	::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	const int windowWidth = windowRect.right - windowRect.left;
	const int windowHeight = windowRect.bottom - windowRect.top;

	// center the window
	//const int windowTopLeftX = std::max<int>(0, (screenWidth - windowWidth) / 2);
	//const int windowTopLeftY = std::max<int>(0, (screenHeight - windowHeight) / 2);

	HWND hWnd = ::CreateWindowExW(NULL, WINDOW_CLASS_NAME, windowName.c_str(), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight, nullptr, nullptr, m_hInstance, nullptr);
	if (!hWnd)
	{
		MessageBoxA(NULL, "Creation of render window failed.", "Error", MB_OK | MB_ICONERROR);
		return nullptr;
	}

	WindowPtr window = std::make_shared<MakeWindow>(hWnd, windowName, width, height, vSync);

	g_windows.insert({ hWnd, window });
	g_windowsByName.insert({ windowName, window });

	return window;
}

void Application::DestroyWindow(const std::wstring& windowName)
{
	if (WindowPtr window = GetWindowByName(windowName))
	{
		DestroyWindow(window);
	}
}

void Application::DestroyWindow(std::shared_ptr<Window> window)
{
	if (window)
	{
		window->Destroy();
	}
}

std::shared_ptr<Window> Application::GetWindowByName(const std::wstring& windowName)
{
	auto windowIt = g_windowsByName.find(windowName);
	return (windowIt != g_windowsByName.end()) ? windowIt->second : nullptr;
}

void Application::Flush()
{
    m_computeCommandQueue->Flush();
	m_copyCommandQueue->Flush();
	m_directCommandQueue->Flush();
}

int Application::Run(std::shared_ptr<Game> game)
{
	if (!game->Initialize())
	{
		return ErrorCode::GAME_NOT_INITIALIZED;
	}
	if (!game->LoadContent())
	{
		return ErrorCode::GAME_CONTENT_NOT_LOADED;
	}

	MSG msg = { 0 };
	while (msg.message != WM_QUIT)
	{
		if (::PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessageW(&msg);
		}
	}

	Flush();
	game->UnloadContent();
	game->Destroy();

	return static_cast<int>(msg.wParam);
}

void Application::Quit(int exitCode)
{
	::PostQuitMessage(exitCode);
}

Microsoft::WRL::ComPtr<ID3D12Device2> Application::GetDevice() const
{
	return m_device;
}

std::shared_ptr<CommandQueue> Application::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type) const
{
	switch (type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		return m_directCommandQueue;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		return m_computeCommandQueue;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		return m_copyCommandQueue;
	default:
		assert(false && "Invalid command queue type.");
	}
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Application::CreateDescriptorHeap(UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = type;
	desc.NumDescriptors = numDescriptors;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

UINT Application::GetDescriptorandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
	return m_device->GetDescriptorHandleIncrementSize(type);
}

Application::Application(HINSTANCE hInst)
	: m_hInstance(hInst)
	, m_tearingSupported(false)
{
	// using this awareness context allows the client area of the window to achieve 100% scaling 
	// while still allowing non-client window content to be rendered in a DPI sensitive fashion.
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	EnableDebugLayer();

	RegisterWindowClass(hInst);

	m_dxgiAdapter = GetAdapter(false);
	if (m_dxgiAdapter)
	{
		m_device = CreateDevice(m_dxgiAdapter);
	}
	if (m_device)
	{
		m_computeCommandQueue = std::make_shared<CommandQueue>(m_device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
		m_copyCommandQueue = std::make_shared<CommandQueue>(m_device, D3D12_COMMAND_LIST_TYPE_COPY);
		m_directCommandQueue = std::make_shared<CommandQueue>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);

		m_tearingSupported = CheckTearingSupport();
	}
}

void Application::EnableDebugLayer()
{
#if defined(_DEBUG)
	Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

Microsoft::WRL::ComPtr<IDXGIAdapter4> Application::GetAdapter(bool useWarp)
{
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	Microsoft::WRL::ComPtr<IDXGIAdapter1> dxgiAdapter1;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> dxgiAdapter4;

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
				&& SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device2), nullptr))
				&& dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
			}
		}
	}

	return dxgiAdapter4;
}

Microsoft::WRL::ComPtr<ID3D12Device2> Application::CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter)
{
	Microsoft::WRL::ComPtr<ID3D12Device2> D3D12Device2;
	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&D3D12Device2)));

#if defined(_DEBUG)
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(D3D12Device2.As(&pInfoQueue)))
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

	return D3D12Device2;
}

bool Application::CheckTearingSupport()
{
	BOOL allowTearing = FALSE;

	Microsoft::WRL::ComPtr<IDXGIFactory7> factory;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
	{
		if (FAILED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
		{
			allowTearing = FALSE;
		}
	}
	return allowTearing == TRUE;
}

void Application::RegisterWindowClass(HINSTANCE hInst)
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
	windowClass.lpszClassName = WINDOW_CLASS_NAME;
	windowClass.hIconSm = ::LoadIcon(hInst, NULL);

	ATOM atom = ::RegisterClassExW(&windowClass);
	if (!atom)
	{
		MessageBoxA(NULL, "Registering window class failed", "Error", MB_OK | MB_ICONERROR);
		exit(EXIT_FAILURE);
	}
}

static void RemoveWindow(HWND hWnd)
{
	auto it = g_windows.find(hWnd);
	if (it != g_windows.end())
	{
		g_windowsByName.erase(it->second->GetWindowName());
		g_windows.erase(it);
	}
}

MouseButtonEventArgs::MouseButton DecodeMouseButton(UINT messageID)
{
	MouseButtonEventArgs::MouseButton mouseButton = MouseButtonEventArgs::None;
	switch (messageID)
	{
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
		{
			mouseButton = MouseButtonEventArgs::Left;
		}
		break;
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
		{
			mouseButton = MouseButtonEventArgs::Right;
		}
		break;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDBLCLK:
		{
			mouseButton = MouseButtonEventArgs::Middel;
		}
		break;
	}

	return mouseButton;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WindowPtr window;
	auto it = g_windows.find(hwnd);
	if (it != g_windows.end())
	{
		window = it->second;
	}

	if (window)
	{

		switch (message)
		{
		case WM_PAINT:
		{
			// delta time will be filled in by the window
			UpdateEventArgs updateEventArgs(0.f, 0.f);
			window->OnUpdate(updateEventArgs);
			RenderEventArgs renderEventArgs(0.f, 0.f);
			window->OnRender(renderEventArgs);
		}
		break;
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		{
			MSG charMsg;
			// unicode char (UTF-16)
			unsigned int c = 0;
			if (::PeekMessage(&charMsg, hwnd, 0, 0, PM_NOREMOVE) && charMsg.message == WM_CHAR)
			{
				::GetMessage(&charMsg, hwnd, 0, 0);
				c = static_cast<unsigned int>(charMsg.wParam);
			}
			bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
			bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
			KeyCode::Key key = (KeyCode::Key)wParam;
			KeyEventArgs keyEventArgs(key, c, KeyEventArgs::Pressed, shift, control, alt);
			window->OnKeyPressed(keyEventArgs);
		}
		break;
		case WM_SYSKEYUP:
		case WM_KEYUP:
		{
			bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
			bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
			KeyCode::Key key = (KeyCode::Key)wParam;
			unsigned int c = 0;
			unsigned int scanCode = (lParam & 0x00FF0000) >> 16;

			// Determine which key was released by converting the key code and the scan code
			// to a printable character (if possible).
			// Inspired by the SDL 1.2 implementation.
			unsigned char keyboardState[256];
			GetKeyboardState(keyboardState);
			wchar_t translatedCharacters[4];
			if (int result = ToUnicodeEx(static_cast<UINT>(wParam), scanCode, keyboardState, translatedCharacters, 4, 0, NULL) > 0)
			{
				c = translatedCharacters[0];
			}

			KeyEventArgs keyEventArgs(key, c, KeyEventArgs::Released, shift, control, alt);
			window->OnKeyReleased(keyEventArgs);
		}
		break;
		// default window procedure will play system notification sound upon pressing alt+enter if this message is not handled
		case WM_SYSCHAR:
			break;
		case WM_MOUSEMOVE:
		{
			bool lButton = (wParam & MK_LBUTTON) != 0;
			bool rButton = (wParam & MK_RBUTTON) != 0;
			bool mButton = (wParam & MK_MBUTTON) != 0;
			bool shift = (wParam & MK_SHIFT) != 0;
			bool control = (wParam & MK_CONTROL) != 0;

			int x = ((int)(short)LOWORD(lParam));
			int y = ((int)(short)HIWORD(lParam));

			MouseMotionEventArgs mouseMotionEventArgs(lButton, mButton, rButton, control, shift, x, y);
			window->OnMouseMoved(mouseMotionEventArgs);
		}
		break;
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		{
			bool lButton = (wParam & MK_LBUTTON) != 0;
			bool rButton = (wParam & MK_RBUTTON) != 0;
			bool mButton = (wParam & MK_MBUTTON) != 0;
			bool shift = (wParam & MK_SHIFT) != 0;
			bool control = (wParam & MK_CONTROL) != 0;

			int x = ((int)(short)LOWORD(lParam));
			int y = ((int)(short)HIWORD(lParam));

			MouseButtonEventArgs mouseButtonEventArgs(DecodeMouseButton(message), MouseButtonEventArgs::Pressed, lButton, mButton, rButton, control, shift, x, y);
			window->OnMouseButtonPressed(mouseButtonEventArgs);
		}
		break;
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		{
			bool lButton = (wParam & MK_LBUTTON) != 0;
			bool rButton = (wParam & MK_RBUTTON) != 0;
			bool mButton = (wParam & MK_MBUTTON) != 0;
			bool shift = (wParam & MK_SHIFT) != 0;
			bool control = (wParam & MK_CONTROL) != 0;

			int x = ((int)(short)LOWORD(lParam));
			int y = ((int)(short)HIWORD(lParam));

			MouseButtonEventArgs mouseButtonEventArgs(DecodeMouseButton(message), MouseButtonEventArgs::Released, lButton, mButton, rButton, control, shift, x, y);
			window->OnMouseButtonReleased(mouseButtonEventArgs);
		}
		break;
		case WM_MOUSEWHEEL:
		{
			// The distance the mouse wheel is rotated.
			// A positive value indicates the wheel was rotated to the right.
			// A negative value indicates the wheel was rotated to the left.
			float zDelta = ((int)(short)HIWORD(wParam)) / (float)WHEEL_DELTA;
			short keyStates = (short)LOWORD(wParam);

			bool lButton = (keyStates & MK_LBUTTON) != 0;
			bool rButton = (keyStates & MK_RBUTTON) != 0;
			bool mButton = (keyStates & MK_MBUTTON) != 0;
			bool shift = (keyStates & MK_SHIFT) != 0;
			bool control = (keyStates & MK_CONTROL) != 0;

			int x = ((int)(short)LOWORD(lParam));
			int y = ((int)(short)HIWORD(lParam));

			// Convert the screen coordinates to client coordinates.
			POINT clientToScreenPoint;
			clientToScreenPoint.x = x;
			clientToScreenPoint.y = y;
			ScreenToClient(hwnd, &clientToScreenPoint);

			MouseWheelEventArgs mouseWheelEventArgs(zDelta, lButton, mButton, rButton, control, shift, (int)clientToScreenPoint.x, (int)clientToScreenPoint.y);
			window->OnMouseWheel(mouseWheelEventArgs);
		}
		break;
		case WM_SIZE:
		{
			RECT clientRect = { };
			::GetClientRect(hwnd, &clientRect);
			const int width = clientRect.right - clientRect.left;
			const int height = clientRect.bottom - clientRect.top;
			
			ResizeEventArgs resizeEventArgs(width, height);
			window->OnResize(resizeEventArgs);
		}
		break;
		case WM_DESTROY:
		{
			RemoveWindow(hwnd);

			if (g_windows.empty())
			{
				::PostQuitMessage(0);
			}
			break;
		}
		default:
			return ::DefWindowProcW(hwnd, message, wParam, lParam);
		}
	}
	else
	{
		return ::DefWindowProcW(hwnd, message, wParam, lParam);
	}

	return 0;
}

