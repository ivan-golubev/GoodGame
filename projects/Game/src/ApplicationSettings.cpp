module;
#include <cstdint>
#include <memory>
#include <utility>
module ApplicationSettings;

import GlobalSettings;
import Renderer;
import RendererVulkan;
import RendererD3D12;

using gg::RendererVulkan;

namespace gg
{
	std::expected<std::unique_ptr<Renderer>, Error> MakeRenderer(ApplicationSettings const& s)
	{
		RendererSettings const rendererSettings{
			.width        = s.width,
			.height       = s.height,
			.windowHandle = s.windowHandle,
		};

		std::unique_ptr<Renderer> renderer{
			s.rendererType == RendererType::Vulkan
				? std::unique_ptr<Renderer>{ std::make_unique<RendererVulkan>() }
				: std::unique_ptr<Renderer>{ std::make_unique<RendererD3D12>() }
		};

		if (auto result{ renderer->Initialize(rendererSettings) }; !result)
			return std::unexpected(result.error());
		return renderer;
	}
} // namespace gg
