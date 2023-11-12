module;
#include <cstdint>
#include <memory>
module ApplicationSettings;

import Application;
import GlobalSettings;
import RendererVulkan;
import RendererD3D12;
import TimeManager;
import Input;

using gg::RendererVulkan;

namespace gg
{
	std::shared_ptr<Application> MakeApplication(ApplicationSettings const& s)
	{
		std::shared_ptr<TimeManager> timeManager{ std::make_shared<TimeManager>() };
		std::shared_ptr<InputManager> inputManager{ std::make_shared<InputManager>() };
		RendererSettings const rendererSettings{ s.width, s.height, s.windowHandle, timeManager, inputManager };

		if (s.rendererType == RendererType::Vulkan)
			Application::Init(std::make_unique<RendererVulkan>(rendererSettings), timeManager, inputManager);
		else
			Application::Init(std::make_unique<RendererD3D12>(rendererSettings), timeManager, inputManager);

		return Application::Get();
	}
} // namespace gg 
