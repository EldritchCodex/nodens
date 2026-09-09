/// @file Application.cpp
/// @brief Implementation of the Application class - construction, main loop,
/// and event dispatch.
/// @ingroup Core

module;

#include <tracy/Tracy.hpp>

module Nodens.Application;

import Nodens.TimeStep;
import Nodens.Event;
import Nodens.LayerStack;
import Nodens.Log;
import Nodens.ImGuiRenderer;
import Nodens.ImGuiLayer;
import Nodens.OpenGLImGuiRenderer;
import std;

namespace Nodens
{

Application* Application::s_Instance = nullptr;

Application::Application(const ApplicationSpecification& specification)
    : m_Specification(specification)
{
    ZoneScoped;

    // Ensure strictly one Application instance exists
    if (s_Instance)
        FatalCore("Application already exists!");
    s_Instance = this;

    m_JobSystem = std::make_unique<JobSystem>();
    m_LayerStack = std::make_unique<LayerStack>();
    m_EventBus = std::make_unique<EventBus>();

    if (!m_Specification.IsHeadless)
    {
        WindowProps props(
            m_Specification.Name, m_Specification.WindowWidth, m_Specification.WindowHeight);
        m_Window = std::unique_ptr<Window>(Window::Create(props));
        m_Window->SetInputEventCallback([this](RoutedInputEvent& event) { OnInputEvent(event); });
    }

    if (m_Specification.EnableGUI && !m_Specification.IsHeadless)
    {
        std::shared_ptr<ImGuiRenderer> imguiRenderer = std::make_shared<OpenGLImGuiRenderer>();
        m_ImGuiLayer = new ImGuiLayer{imguiRenderer, m_Specification.DefaultTheme};
        m_ImGuiLayer->BlockEvents(m_Specification.ShouldImGuiBlockInputs);
        PushOverlay(m_ImGuiLayer);
    }
}

Application::~Application()
{
    ZoneScoped;
    // Cleanup logic if necessary
}

void Application::PushLayer(Layer* layer)
{
    ZoneScoped;
    m_LayerStack->PushLayer(layer);
    layer->OnAttach();
}

void Application::PushOverlay(Layer* overlay)
{
    ZoneScoped;
    m_LayerStack->PushOverlay(overlay);
    overlay->OnAttach();
}

Window& Application::GetWindow()
{
    if (!m_Window)
        FatalCore("Attempted to access Window in a headless application!");
    return *m_Window;
}

JobSystem& Application::GetJobSystem()
{
    return *m_JobSystem;
}

EventBus& Application::GetEventBus()
{
    return *m_EventBus;
}

const ApplicationSpecification& Application::GetSpecification() const
{
    return m_Specification;
}

Application& Application::Get()
{
    return *s_Instance;
}

void Application::Run()
{
    const auto startTime = std::chrono::steady_clock::now();

    while (m_Running)
    {
        ZoneScoped;

        const auto currentTime = std::chrono::steady_clock::now();
        float time = std::chrono::duration<float>(currentTime - startTime).count();
        TimeStep timestep = time - m_LastFrameTime;
        m_LastFrameTime = time;

        // Flush queued non-input events
        m_EventBus->Flush();

        // Update each layer
        for (Layer* layer : *m_LayerStack)
            layer->OnUpdate(timestep);

        if (m_ImGuiLayer)
        {
            m_ImGuiLayer->Begin();
            for (Layer* layer : *m_LayerStack)
                layer->OnImGuiRender(timestep);
            m_ImGuiLayer->End();
        }

        if (m_Window)
            m_Window->OnUpdate();

        FrameMark;
    }
}

bool Application::OnWindowClose(InputEvents::WindowClose& e)
{
    m_Running = false;
    return false; // Do not consume the event. Let other layers know that the application will be
                  // closed.
}

void Application::OnInputEvent(RoutedInputEvent& e)
{
    ZoneScoped;

    InputEventDispatcher dispatcher(e);
    dispatcher.Dispatch<InputEvents::WindowClose>([this](InputEvents::WindowClose& event)
                                                  { return OnWindowClose(event); });

    // Run through LayerStack from last to first
    for (auto layer : *m_LayerStack | std::views::reverse)
    {
        layer->OnInputEvent(e);
        if (e.Handled)
            break;
    }
}

} // namespace Nodens
