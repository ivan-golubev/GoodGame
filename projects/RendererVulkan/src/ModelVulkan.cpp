module;
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string>
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
		__debugbreak();
		// TODO: skybox only has positions, need to change the model reading code
		// there are no directional lights either
	}

	ModelVulkan::~ModelVulkan()
	{
		vkDestroyBuffer(device, VB, nullptr);
		vkFreeMemory(device, vertexBufferMemory, nullptr);

		for (size_t i{ 0 }; i < maxFramesInFlight; ++i)
		{
			vkDestroyBuffer(device, uniformBuffersMVP[i], nullptr);
			vkFreeMemory(device, uniformBuffersMemoryMVP[i], nullptr);

			vkDestroyBuffer(device, uniformBuffersDirLight[i], nullptr);
			vkFreeMemory(device, uniformBuffersMemoryDirLight[i], nullptr);
		}

	}
} //namespace gg
