module;
#include <cstdint>
#include <memory>
#include <utility>
module ApplicationSettings;

import Application;
import GlobalSettings;
import Input;
import Renderer;
import RendererVulkan;
import RendererD3D12;
import TimeManager;

using gg::RendererVulkan;

namespace gg
{
	std::expected<std::shared_ptr<Application>, Error> MakeApplication(ApplicationSettings const& s)
	{
		std::shared_ptr<TimeManager> timeManager{ std::make_shared<TimeManager>() };
		std::shared_ptr<InputManager> inputManager{ std::make_shared<InputManager>() };
		RendererSettings const rendererSettings{
			.width = s.width,
			.height = s.height,
			.windowHandle = s.windowHandle,
			.timeManager = timeManager,
			.inputManager = inputManager,
		};

		std::unique_ptr<Renderer> renderer = s.rendererType == RendererType::Vulkan
			? std::unique_ptr<Renderer>{ std::make_unique<RendererVulkan>() }
			: std::unique_ptr<Renderer>{ std::make_unique<RendererD3D12>() };

		if (auto result{ renderer->Initialize(rendererSettings) }; !result)
			return std::unexpected(result.error());
		Application::Init(std::move(renderer), timeManager, inputManager);

		return Application::Get();
	}
} // namespace gg 
