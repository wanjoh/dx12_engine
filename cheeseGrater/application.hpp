#pragma once

#include <cheese_grater_common.hpp>

class CommandQueue;
class Game;
class Window;

class Application
{
public:
	enum ErrorCode : int
	{
		GAME_NOT_INITIALIZED = 1,
		GAME_CONTENT_NOT_LOADED = 2,
	};
	Application(const Application& other) = delete;  // todo: use nonCopyable interface?
	Application& operator=(const Application& other) = delete;
	~Application();

	static void Create(HINSTANCE hInst);
	static void Destroy();
	static Application& Get();

	bool IsTearingSupported() const;

	/// @returns The created window instance. If an error occurred while creating the window an invalid 
	/// window instance is returned.If a window with the given name already exists, that window will be returned.
	std::shared_ptr<Window> CreateRenderWindow(const std::wstring& windowName, int width, int height, bool vSync = true);

	void DestroyWindow(const std::wstring& windowName);
	void DestroyWindow(std::shared_ptr<Window> window);
	std::shared_ptr<Window> GetWindowByName(const std::wstring& windowName);


	/// Run the application loop 
	/// @returns Error code if error occured
	int Run(std::shared_ptr<Game> game);
	void Quit(int exitCode);

	Microsoft::WRL::ComPtr<ID3D12Device2> GetDevice() const;
	std::shared_ptr<CommandQueue> GetCommandQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) const;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type);
	UINT GetDescriptorandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;
	/// Flush all command queues
	void Flush();
	
	static void RemoveWindow(HWND hWnd);

private:
	Application(HINSTANCE hInst);

	Microsoft::WRL::ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp);
	Microsoft::WRL::ComPtr<ID3D12Device2> CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter);
	bool CheckTearingSupport();

	void RegisterWindowClass(HINSTANCE hInst);
	void EnableDebugLayer();

	HINSTANCE m_hInstance;

	Microsoft::WRL::ComPtr<ID3D12Device2> m_device;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> m_dxgiAdapter;

	std::shared_ptr<CommandQueue> m_computeCommandQueue;
	std::shared_ptr<CommandQueue> m_copyCommandQueue;
	std::shared_ptr<CommandQueue> m_directCommandQueue;

	bool m_tearingSupported;

};

