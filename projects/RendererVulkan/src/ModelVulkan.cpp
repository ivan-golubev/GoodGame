module;
#include <string>
#include <memory>
#include <vulkan/vulkan.h>
module ModelVulkan;

import Model;
import ShaderProgram;
import Texture;
import RendererVulkan;
import ErrorHandling;

namespace gg
{
	ModelVulkan::ModelVulkan(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram> s, std::shared_ptr<Texture> t, VkDevice d)
		: Model{ modelRelativePath, s, t }
		, device{ d }
	{
	}

	ModelVulkan::~ModelVulkan()
	{
		vkDestroyBuffer(device, VB, nullptr);
		vkFreeMemory(device, vertexBufferMemory, nullptr);
	}
} //namespace gg
