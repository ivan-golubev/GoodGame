module;
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vulkan/vulkan.h>
module ModelVulkan;

import Model;
import ShaderProgram;
import RendererVulkan;
import ErrorHandling;


namespace gg
{
	ModelVulkan::ModelVulkan(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram> s, glm::vec3& position, VkDevice d)
		: Model{ modelRelativePath, s, position }
		, device{ d }
	{
	}

	ModelVulkan::~ModelVulkan() noexcept
	{
		if (!device)
			return;

		if (graphicsPipeline)
			vkDestroyPipeline(device, graphicsPipeline, nullptr);
		if (pipelineLayout)
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

		if (VB)
			vkDestroyBuffer(device, VB, nullptr);
		if (vertexBufferMemory)
			vkFreeMemory(device, vertexBufferMemory, nullptr);

		for (size_t i{ 0 }; i < maxFramesInFlight; ++i)
		{
			if (uniformBuffersMVP[i])
				vkDestroyBuffer(device, uniformBuffersMVP[i], nullptr);
			if (uniformBuffersMemoryMVP[i])
				vkFreeMemory(device, uniformBuffersMemoryMVP[i], nullptr);

			if (uniformBuffersDirLight[i])
				vkDestroyBuffer(device, uniformBuffersDirLight[i], nullptr);
			if (uniformBuffersMemoryDirLight[i])
				vkFreeMemory(device, uniformBuffersMemoryDirLight[i], nullptr);
		}

	}
} //namespace gg
