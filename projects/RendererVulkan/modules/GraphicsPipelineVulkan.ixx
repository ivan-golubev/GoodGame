module;
#include <vulkan/vulkan.h>
export module GraphicsPipelineVulkan;

export namespace gg
{
	struct GraphicsPipeline
	{
		VkPipelineLayout layout{};
		VkPipeline       pipeline{};
	};
} // namespace gg
