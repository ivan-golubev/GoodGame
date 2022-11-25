module;
#include <cstdint>
#include <memory>
module ApplicationSettings;

import VulkanRenderer;

using gg::VulkanRenderer;

namespace gg
{
	std::shared_ptr<Application> MakeApplication(ApplicationSettings& s)
	{
		std::unique_ptr<Renderer> renderer{ 
			s.rendererType == RendererType::Vulkan
			? std::make_unique<VulkanRenderer>(s.width, s.height, s.windowHandle)
			: nullptr // TODO: add D3D12 here
		};
		return Application::Init(std::move(renderer));
	}
} // namespace gg 
