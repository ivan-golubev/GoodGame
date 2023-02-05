module;
#include <memory>
#include <string>
#include <vulkan/vulkan.h>
export module ModelVulkan;

import Model;

namespace gg
{
	export struct ModelVulkan : public Model
	{
	public:
		ModelVulkan(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram>, std::shared_ptr<Texture>, VkDevice);
		~ModelVulkan() noexcept;

	private:
		VkDevice device;
		VkBuffer VB{};
		VkDeviceMemory vertexBufferMemory{};

		friend class RendererVulkan;
	};
} //namespace gg
