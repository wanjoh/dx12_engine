#pragma once

#include <cheese_grater_common.hpp>

#include <command_queue.hpp>
#include <window.hpp>

class Application
{
public:
	Application(const Application& other) = delete;  // todo: use nonCopyable interface?
	~Application() = default;

	Application& operator=(const Application& other) = delete;

	static Application& GetInstance();
	void Flush();
	void Run();
	void Quit();
private:
	Application();

	std::unique_ptr<Application> m_instance;
	ComPtr<ID3D12Device> m_device;
	uint64_t m_fenceValue = 0;
	uint64_t m_frameFenceValues[Window::NUM_FRAMES] = { };
	HANDLE m_fenceEvent;

	std::shared_ptr<Window> m_window;
	std::shared_ptr<CommandQueue> m_commandQueue;
	
};

