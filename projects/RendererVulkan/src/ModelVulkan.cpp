module;
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
	ModelVulkan::ModelVulkan(std::unique_ptr<ShaderProgram> s, std::shared_ptr<Texture> t, VkDevice d)
		: Model{ std::move(s), t }
		, device{ d }
	{
	}

	ModelVulkan::~ModelVulkan()
	{
		vkDestroyBuffer(device, VB, nullptr);
		vkFreeMemory(device, vertexBufferMemory, nullptr);
	}

	void ModelVulkan::CreateVertexBuffers()
	{
		// TODO: handle multiple meshes properly, need to create multiple Vertex Buffers
		BreakIfFalse(meshes.size() < 2);

		for (auto& mesh : meshes)
			CreateVertexBuffer(mesh);
	}

	void ModelVulkan::CreateVertexBuffer(Mesh const& mesh)
	{
		std::shared_ptr<RendererVulkan> renderer{ RendererVulkan::Get() };
		uint64_t const VB_sizeBytes{ mesh.VerticesSizeBytes() };
		VkDevice device = renderer->GetDevice();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		renderer->CreateBuffer(stagingBuffer, stagingBufferMemory, VB_sizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* mappedData;
		vkMapMemory(device, stagingBufferMemory, 0, VB_sizeBytes, 0, &mappedData);
		memcpy(mappedData, mesh.vertices.data(), static_cast<size_t>(VB_sizeBytes));
		vkUnmapMemory(device, stagingBufferMemory);

		VkBufferUsageFlagBits const usage = static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		renderer->CreateBuffer(VB, vertexBufferMemory, VB_sizeBytes, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		renderer->CopyBuffer(stagingBuffer, VB, VB_sizeBytes);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	VkBuffer ModelVulkan::GetVertexBuffer() const
	{
		return VB;
	}

} //namespace gg
