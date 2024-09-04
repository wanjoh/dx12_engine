#include "application.hpp"

Application& Application::GetInstance()
{
    static Application instance;
    return instance;
}

void Application::Flush()
{
    m_commandQueue->Flush();
}

Application::Application()
{
}
