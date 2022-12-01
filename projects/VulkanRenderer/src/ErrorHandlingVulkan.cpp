module;
#include <string>
#include <format>
module ErrorHandlingVulkan;

namespace gg
{
	VulkanInitException::VulkanInitException(std::string const& msg)
		: std::runtime_error{ std::format("Failed to initialize the renderer: {}.", msg) }
	{
	}

	VulkanRenderException::VulkanRenderException(std::string const& msg)
		: std::runtime_error{ std::format("Failed to render a frame: {}.", msg) }
	{
	}
} // namespace gg
