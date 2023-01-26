module;
#include <cstdint>
#include <memory>
module ApplicationSettings;

import Application;
import GlobalSettings;
import VulkanRenderer;

using gg::VulkanRenderer;

namespace gg
{
	Application MakeApplication(ApplicationSettings const& s)
	{
		std::shared_ptr<TimeManager> timeManager{ std::make_shared<TimeManager>() };
		std::shared_ptr<InputManager> inputManager{ std::make_shared<InputManager>() };
		RendererSettings const rendererSettings{ s.width, s.height, s.windowHandle, timeManager, inputManager };

		std::unique_ptr<Renderer> renderer{
			s.rendererType == RendererType::Vulkan
			? std::make_unique<VulkanRenderer>(rendererSettings)
			: nullptr // TODO: add D3D12 here
		};
		return Application(std::move(renderer), timeManager, inputManager);
	}
} // namespace gg 
