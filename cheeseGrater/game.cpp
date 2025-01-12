#include "game.hpp"

#include <application.hpp>
#include <window.hpp>


Game::Game(const std::wstring& name, int width, int height, bool vSync)
	: m_name(name)
	, m_width(width)
	, m_height(height)
	, m_vSync(vSync)
{
}

Game::~Game()
{
	assert(!m_window && "Use Game::Destroy() before destruction");
}

bool Game::Initialize()
{
	if (!DirectX::XMVerifyCPUSupport())
	{
		MessageBoxA(NULL, "Failed to verify DirectX Math library support", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	m_window = Application::Get().CreateRenderWindow(m_name, m_width, m_height, m_vSync);
	m_window->RegisterCallbacks(shared_from_this());
	m_window->Show();

	return true;
}

void Game::Destroy()
{
	Application::Get().DestroyWindow(m_window);
	m_window.reset();
}

void Game::OnResize(ResizeEventArgs& e)
{
	m_width = e.Width;
	m_height = e.Height;
}

void Game::OnWindowDestroy()
{
	UnloadContent();
}
