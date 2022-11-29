module;
#include <string>
module ErrorHandlingVulkan;

namespace gg
{
	VulkanInitException::VulkanInitException(std::string const& msg)
		: std::runtime_error{ msg }
	{
	}

	VulkanRenderException::VulkanRenderException(std::string const& msg)
		: std::runtime_error{ msg }
	{
	}
} // namespace gg