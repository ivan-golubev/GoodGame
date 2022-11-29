module;
#include <stdexcept>
#include <string>
export module ErrorHandlingVulkan;

export namespace gg
{
	class VulkanInitException : public std::runtime_error
	{
	public:
		VulkanInitException(std::string const& msg);
	};

	class VulkanRenderException : public std::runtime_error
	{
	public:
		VulkanRenderException(std::string const& msg);
	};

} // namespace gg