module;
#include <string>
#include <memory>
#include <DirectXMath.h>
#include <vulkan/vulkan.h>
module ModelVulkan;

import Model;
import ShaderProgram;
import RendererVulkan;
import ErrorHandling;

using DirectX::XMVECTOR;

namespace gg
{
	ModelVulkan::ModelVulkan(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram> s, XMVECTOR& position, VkDevice d)
		: Model{ modelRelativePath, s, position }
		, device{ d }
	{
	}

	ModelVulkan::~ModelVulkan()
	{
		vkDestroyBuffer(device, VB, nullptr);
		vkFreeMemory(device, vertexBufferMemory, nullptr);

		for (size_t i{ 0 }; i < maxFramesInFlight; ++i)
		{
			vkDestroyBuffer(device, uniformBuffers[i], nullptr);
			vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
		}

	}
} //namespace gg
