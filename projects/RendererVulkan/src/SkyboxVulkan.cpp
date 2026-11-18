module;
#include <cstdint>
#include <cstring>
#include <format>
#include <glm/glm.hpp>
#include <utility>
#include <vulkan/vulkan.h>
module SkyboxVulkan;

import Camera;
import ErrorHandling;
import RendererVulkan;
import SettingsRenderer;
import ShaderProgram;
import ShaderProgramVulkan;
import Skybox;
import Texture;
import TextureVulkan;

namespace gg
{
	SkyboxVulkan::SkyboxVulkan(std::string const& name, std::shared_ptr<ShaderProgram> s, VkDevice device)
		: Skybox{ name, s }
		, device{ device }
	{
	}

	SkyboxVulkan::~SkyboxVulkan() noexcept
	{
		if (!device)
			return;
		if (textureSampler) vkDestroySampler(device, textureSampler, nullptr);
		if (textureImageView) vkDestroyImageView(device, textureImageView, nullptr);
		if (textureImage) vkDestroyImage(device, textureImage, nullptr);
		if (textureImageMemory) vkFreeMemory(device, textureImageMemory, nullptr);
		DestroyPipeline();
		if (descriptorSetLayout) vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		for (size_t i{ 0 }; i < maxFramesInFlight; ++i)
		{
			if (uniformBuffersCamera[i]) vkDestroyBuffer(device, uniformBuffersCamera[i], nullptr);
			if (uniformBuffersMemoryCamera[i]) vkFreeMemory(device, uniformBuffersMemoryCamera[i], nullptr);
		}
	}

	std::expected<void, Error> SkyboxVulkan::Initialize(VkDescriptorPool descriptorPool)
	{
		if (auto result{ Skybox::Initialize() }; !result)
			return std::unexpected(result.error());

		if (auto result{ CreateTextureImage() }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreateTextureImageView() }; !result)
			return std::unexpected(result.error());
		auto samplerResult{ RendererVulkan::Get()->CreateTextureSampler() };
		if (!samplerResult)
			return std::unexpected(samplerResult.error());
		textureSampler = *samplerResult;

		if (auto result{ CreateDescriptorSetLayout() }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreateUniformBuffers() }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreateDescriptorSets(descriptorPool) }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreatePipeline() }; !result)
			return std::unexpected(result.error());
		return {};
	}

	// Resources read by skybox.hlsl. Binding numbers match its VK_BINDING annotations; binding 1 is unused.
	std::expected<void, Error> SkyboxVulkan::CreateDescriptorSetLayout()
	{
		// SkyboxCamera: inverse projection and camera rotation, used to rebuild the view ray in the pixel shader.
		VkDescriptorSetLayoutBinding const cameraBinding{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		};

		// Cube map and its sampler, sampled with the world-space view ray.
		VkDescriptorSetLayoutBinding const cubeMapBinding{
			.binding = 2,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		};

		std::array<VkDescriptorSetLayoutBinding, 2> const bindings{ cameraBinding, cubeMapBinding };
		VkDescriptorSetLayoutCreateInfo const layoutInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = static_cast<uint32_t>(bindings.size()),
			.pBindings = bindings.data(),
		};

		if (VK_SUCCESS != vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout))
			return std::unexpected(Error{ .code = ErrorCode::VulkanResourceCreationFailed, .message = "Failed to create Vulkan skybox descriptor set layout." });
		return {};
	}

	std::expected<void, Error> SkyboxVulkan::CreateUniformBuffers()
	{
		std::shared_ptr<RendererVulkan> renderer{ RendererVulkan::Get() };
		for (size_t i{ 0 }; i < maxFramesInFlight; ++i)
		{
			if (auto result{ renderer->CreateBuffer(uniformBuffersCamera[i], uniformBuffersMemoryCamera[i], sizeof(SkyboxCameraCB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) }; !result)
				return std::unexpected(result.error());
		}
		return {};
	}

	std::expected<void, Error> SkyboxVulkan::CreateDescriptorSets(VkDescriptorPool descriptorPool)
	{
		std::array<VkDescriptorSetLayout, maxFramesInFlight> layouts{};
		layouts.fill(descriptorSetLayout);
		VkDescriptorSetAllocateInfo const allocInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = maxFramesInFlight,
			.pSetLayouts = layouts.data(),
		};

		if (VK_SUCCESS != vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()))
			return std::unexpected(Error{ .code = ErrorCode::VulkanResourceCreationFailed, .message = "Failed to allocate Vulkan descriptor sets for skybox." });

		for (size_t i{ 0 }; i < maxFramesInFlight; ++i)
		{
			VkDescriptorBufferInfo const cameraInfo{
				.buffer = uniformBuffersCamera[i],
				.offset = 0,
				.range = sizeof(SkyboxCameraCB),
			};

			VkDescriptorImageInfo const cubeMapInfo{
				.sampler = textureSampler,
				.imageView = textureImageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};

			std::array<VkWriteDescriptorSet, 2> const descriptorWrites{
				VkWriteDescriptorSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSets[i], .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &cameraInfo },
				VkWriteDescriptorSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSets[i], .dstBinding = 2, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &cubeMapInfo },
			};

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
		return {};
	}

	std::expected<void, Error> SkyboxVulkan::CreatePipeline()
	{
		BreakIfFalse(shaderProgram.get());
		ShaderProgramVulkan* const shader{ dynamic_cast<ShaderProgramVulkan*>(shaderProgram.get()) };
		BreakIfFalse(shader);

		DestroyPipeline();

		// The skybox is a single fullscreen triangle generated in the vertex shader from SV_VertexID,
		// so it reads no vertex attributes: no bindings, no attributes, and no vertex buffer to bind.
		// Draw it with vkCmdDraw(commandBuffer, 3, 1, 0, 0).
		VkPipelineVertexInputStateCreateInfo const vertexInputInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 0,
			.pVertexBindingDescriptions = nullptr,
			.vertexAttributeDescriptionCount = 0,
			.pVertexAttributeDescriptions = nullptr,
		};

		// One triangle that always faces the viewer; culling has nothing to remove.
		VkCullModeFlags const cullMode{ VK_CULL_MODE_NONE };

		// TODO(skybox): depth state. The render pass has no depth attachment yet, so the skybox is drawn before the models.
		// Once depth exists: depthTest = TRUE, depthWrite = FALSE, compare = LESS_OR_EQUAL (the shader already writes z = 1).

		auto result{ RendererVulkan::Get()->CreateGraphicsPipeline(
			shader->GetVertexShader()
			, shader->GetFragmentShader()
			, vertexInputInfo
			, cullMode
			, descriptorSetLayout) };
		if (!result)
			return std::unexpected(std::move(result.error()));
		graphicsPipeline = *result;
		return {};
	}

	void SkyboxVulkan::DestroyPipeline()
	{
		if (graphicsPipeline.pipeline) vkDestroyPipeline(device, graphicsPipeline.pipeline, nullptr);
		if (graphicsPipeline.layout) vkDestroyPipelineLayout(device, graphicsPipeline.layout, nullptr);
		graphicsPipeline = {};
	}

	void SkyboxVulkan::UpdateCamera(uint32_t frame, Camera const& camera)
	{
		SkyboxCameraCB const skyboxCamera{
			/* screen position -> view-space point, used to rebuild the view ray per pixel */
			.InvProjection = glm::inverse(camera.GetProjectionMatrix()),
			/* view-space ray -> world space; rotation only, so the sky ignores the camera position.
			   The inverse of a pure rotation is its transpose. */
			.InvViewRotation = glm::mat4x4{ glm::transpose(glm::mat3x3{ camera.GetViewMatrix() }) },
		};
		void* data;
		vkMapMemory(device, uniformBuffersMemoryCamera[frame], 0, sizeof(SkyboxCameraCB), 0, &data);
		memcpy(data, &skyboxCamera, sizeof(SkyboxCameraCB));
		vkUnmapMemory(device, uniformBuffersMemoryCamera[frame]);
	}

	void SkyboxVulkan::Draw(VkCommandBuffer commandBuffer, uint32_t frame, Camera const& camera)
	{
		UpdateCamera(frame, camera);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.layout, 0, 1, &descriptorSets[frame], 0, nullptr);
		/* One fullscreen triangle generated from SV_VertexID: no vertex buffer to bind. */
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	}

	std::expected<void, Error> SkyboxVulkan::CreateTextureImage()
	{
		std::shared_ptr<RendererVulkan> renderer{ RendererVulkan::Get() };
		VkDeviceSize const layerSize{ textures[0]->SizeBytes() };
		VkDeviceSize const imageSize{ layerSize * textures.size() };
		struct StagingResources
		{
			VkDevice device;
			VkBuffer buffer{};
			VkDeviceMemory memory{};
			~StagingResources() noexcept
			{
				if (buffer) vkDestroyBuffer(device, buffer, nullptr);
				if (memory) vkFreeMemory(device, memory, nullptr);
			}
		} staging{ device };
		if (auto result{ renderer->CreateBuffer(staging.buffer, staging.memory, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) }; !result)
			return std::unexpected(result.error());

		void* mappedData{};
		VkResult const mapResult{ vkMapMemory(device, staging.memory, 0, imageSize, 0, &mappedData) };
		if (mapResult != VK_SUCCESS)
			return std::unexpected(Error{ .code = ErrorCode::VulkanTextureResourceCreationFailed, .message = std::format("Failed to map Vulkan skybox staging buffer (VkResult {}).", static_cast<int>(mapResult)) });
		for (size_t i{}; i < textures.size(); ++i)
			std::memcpy(static_cast<uint8_t*>(mappedData) + layerSize * i, textures[i]->pixels, static_cast<size_t>(layerSize));
		vkUnmapMemory(device, staging.memory);

		VkImageCreateInfo imageInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = textureFormat,
			.extent = { .width = textures[0]->width, .height = textures[0]->height, .depth = 1 },
			.mipLevels = 1,
			.arrayLayers = static_cast<uint32_t>(textures.size()),
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		};
		VkResult const createResult{ vkCreateImage(device, &imageInfo, nullptr, &textureImage) };
		if (createResult != VK_SUCCESS)
			return std::unexpected(Error{ .code = ErrorCode::VulkanTextureResourceCreationFailed, .message = std::format("Failed to create Vulkan cubemap image (VkResult {}).", static_cast<int>(createResult)) });

		VkMemoryRequirements requirements{};
		vkGetImageMemoryRequirements(device, textureImage, &requirements);
		auto memoryTypeResult{ renderer->FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
		if (!memoryTypeResult)
			return std::unexpected(memoryTypeResult.error());
		VkMemoryAllocateInfo allocationInfo{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = requirements.size,
			.memoryTypeIndex = *memoryTypeResult,
		};
		VkResult const allocateResult{ vkAllocateMemory(device, &allocationInfo, nullptr, &textureImageMemory) };
		if (allocateResult != VK_SUCCESS)
			return std::unexpected(Error{ .code = ErrorCode::VulkanTextureResourceCreationFailed, .message = std::format("Failed to allocate Vulkan cubemap memory (VkResult {}).", static_cast<int>(allocateResult)) });
		VkResult const bindResult{ vkBindImageMemory(device, textureImage, textureImageMemory, 0) };
		if (bindResult != VK_SUCCESS)
			return std::unexpected(Error{ .code = ErrorCode::VulkanTextureResourceCreationFailed, .message = std::format("Failed to bind Vulkan cubemap memory (VkResult {}).", static_cast<int>(bindResult)) });

		if (auto result{ TransitionImageLayout(textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) }; !result)
			return std::unexpected(result.error());
		if (auto result{ CopyBufferToImage(staging.buffer, textures[0]->width, textures[0]->height) }; !result)
			return std::unexpected(result.error());
		return TransitionImageLayout(textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	std::expected<void, Error> SkyboxVulkan::TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkPipelineStageFlags sourceStage{ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
		VkPipelineStageFlags destinationStage{ VK_PIPELINE_STAGE_TRANSFER_BIT };
		VkAccessFlags sourceAccess{};
		VkAccessFlags destinationAccess{ VK_ACCESS_TRANSFER_WRITE_BIT };
		if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			sourceAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
			destinationAccess = VK_ACCESS_SHADER_READ_BIT;
		}
		auto commandResult{ RendererVulkan::Get()->BeginSingleTimeCommands() };
		if (!commandResult) return std::unexpected(commandResult.error());
		VkImageMemoryBarrier barrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = sourceAccess,
			.dstAccessMask = destinationAccess,
			.oldLayout = oldLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = static_cast<uint32_t>(textures.size()) },
		};
		vkCmdPipelineBarrier(*commandResult, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		return RendererVulkan::Get()->EndSingleTimeCommands(*commandResult);
	}

	std::expected<void, Error> SkyboxVulkan::CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height)
	{
		auto commandResult{ RendererVulkan::Get()->BeginSingleTimeCommands() };
		if (!commandResult) return std::unexpected(commandResult.error());
		std::array<VkBufferImageCopy, 6> regions{};
		VkDeviceSize layerSize{ textures[0]->SizeBytes() };
		for (uint32_t i{}; i < regions.size(); ++i)
		{
			regions[i] = VkBufferImageCopy{
				.bufferOffset = layerSize * i,
				.imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = i, .layerCount = 1 },
				.imageExtent = { .width = width, .height = height, .depth = 1 },
			};
		}
		vkCmdCopyBufferToImage(*commandResult, buffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(regions.size()), regions.data());
		return RendererVulkan::Get()->EndSingleTimeCommands(*commandResult);
	}

	std::expected<void, Error> SkyboxVulkan::CreateTextureImageView()
	{
		VkImageViewCreateInfo viewInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = textureImage,
			.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
			.format = textureFormat,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = static_cast<uint32_t>(textures.size()) },
		};
		VkResult const result{ vkCreateImageView(device, &viewInfo, nullptr, &textureImageView) };
		if (result != VK_SUCCESS)
			return std::unexpected(Error{ .code = ErrorCode::VulkanTextureResourceCreationFailed, .message = std::format("Failed to create Vulkan cubemap image view (VkResult {}).", static_cast<int>(result)) });
		return {};
	}
}
