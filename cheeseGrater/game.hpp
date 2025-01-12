#pragma once

#include <cheese_grater_common.hpp>
#include <events.hpp>

#include <memory>  // std::enable_shared_from_this

class Window;

/// Abstract base class for demos. Performs initialization of directX12 runtime; creates a window to render to;
/// loads and unloads content (frees memory used by loading content); responds to window messages (keyboard and mouse events)
class Game : public std::enable_shared_from_this<Game>
{
public:
	/// Create DirectX demo using the specified window dimensions
	Game(const std::wstring& name, int width, int height, bool vSync);
	virtual ~Game();

	int GetClientWidth() const {return m_width;}
	int GetClientHeight() const {return m_height;}
	
	/// Initialize DirectX runtime
	/// @returns true if initialized, false otherwise
	virtual bool Initialize();
	virtual bool LoadContent() = 0;
	virtual void UnloadContent() = 0;

	virtual void Destroy();
protected:
	friend class Window;

	virtual void OnUpdate(UpdateEventArgs& e) { };
	virtual void OnRender(RenderEventArgs& e) { };
	virtual void OnKeyPressed(KeyEventArgs& e) { };
	virtual void OnKeyReleased(KeyEventArgs& e) { };
	virtual void OnMouseMoved(MouseMotionEventArgs& e) { };
	virtual void OnMouseButtonPressed(MouseButtonEventArgs& e) { };
	virtual void OnMouseButtonReleased(MouseButtonEventArgs& e) { };
	virtual void OnMouseWheel(MouseWheelEventArgs& e) { };
	virtual void OnResize(ResizeEventArgs& e);
	virtual void OnWindowDestroy();

	std::shared_ptr<Window> m_window;

private:
	std::wstring m_name;
	
	int m_width;
	int m_height;

	bool m_vSync;
};

