module;
#include <array>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
module DeviceContextVulkan;

import SettingsRenderer;
import ShaderProgram;
import VertexVulkan;

namespace gg
{
	std::expected<std::shared_ptr<DeviceContextVulkan>, Error> DeviceContextVulkan::Create(
		VkPhysicalDevice physicalDevice,
		VkDevice         device,
		uint32_t         graphicsQueueFamily,
		VkQueue          graphicsQueue
	)
	{
		VkCommandPoolCreateInfo poolInfo{
			.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = graphicsQueueFamily,
		};
		VkCommandPool commandPool{};
		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
		{
			vkDestroyDevice(device, nullptr);
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Failed to create Vulkan command pool.",
			});
		}
		return std::make_shared<DeviceContextVulkan>(PassKey{}, physicalDevice, device, graphicsQueue, commandPool);
	}

	DeviceContextVulkan::DeviceContextVulkan(
		PassKey,
		VkPhysicalDevice physicalDevice,
		VkDevice         device,
		VkQueue          graphicsQueue,
		VkCommandPool    commandPool
	)
		: physicalDevice{ physicalDevice }
		, device{ device }
		, graphicsQueue{ graphicsQueue }
		, commandPool{ commandPool }
	{
	}

	DeviceContextVulkan::~DeviceContextVulkan() noexcept
	{
		if (!device)
			return;
		vkDeviceWaitIdle(device);
		if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
		vkDestroyDevice(device, nullptr);
	}

	std::expected<uint32_t, Error> DeviceContextVulkan::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
		for (uint32_t i{ 0 }; i < memProperties.memoryTypeCount; ++i)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		return std::unexpected(Error{
			.code    = ErrorCode::VulkanResourceCreationFailed,
			.message = "Failed to find a compatible Vulkan memory type.",
		});
	}

	std::expected<void, Error> DeviceContextVulkan::CreateBuffer(
		VkBuffer&             outBuffer,
		VkDeviceMemory&       outBufferMemory,
		uint64_t              sizeBytes,
		VkBufferUsageFlagBits usage,
		VkMemoryPropertyFlags properties
	)
	{
		struct BufferResources
		{
			VkDevice       device;
			VkBuffer       buffer{};
			VkDeviceMemory memory{};

			~BufferResources() noexcept
			{
				if (buffer)
					vkDestroyBuffer(device, buffer, nullptr);
				if (memory)
					vkFreeMemory(device, memory, nullptr);
			}
		} resources{ device };
		outBuffer       = VK_NULL_HANDLE;
		outBufferMemory = VK_NULL_HANDLE;

		VkBufferCreateInfo bufferInfo{
			.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size        = sizeBytes,
			.usage       = static_cast<VkBufferUsageFlags>(usage),
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		};
		VkResult const createResult{ vkCreateBuffer(device, &bufferInfo, nullptr, &resources.buffer) };
		if (createResult != VK_SUCCESS)
		{
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanResourceCreationFailed,
				.message = std::format("Failed to create Vulkan buffer (VkResult {})", static_cast<int>(createResult)),
			});
		}

		/* allocate and bind memory to this buffer */
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, resources.buffer, &memRequirements);

		auto memoryTypeResult{ FindMemoryType(memRequirements.memoryTypeBits, properties) };
		if (!memoryTypeResult)
			return std::unexpected(memoryTypeResult.error());
		VkMemoryAllocateInfo allocInfo{
			.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize  = memRequirements.size,
			.memoryTypeIndex = *memoryTypeResult,
		};
		VkResult const allocationResult{ vkAllocateMemory(device, &allocInfo, nullptr, &resources.memory) };
		if (allocationResult != VK_SUCCESS)
		{
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanResourceCreationFailed,
				.message = std::format("Failed to allocate memory for Vulkan buffer (VkResult {})", static_cast<int>(allocationResult)),
			});
		}
		VkResult const bindResult{ vkBindBufferMemory(device, resources.buffer, resources.memory, 0) };
		if (bindResult != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanResourceCreationFailed,
				.message = std::format("Failed to bind memory to Vulkan buffer (VkResult {})", static_cast<int>(bindResult)),
			});

		outBuffer       = std::exchange(resources.buffer, VK_NULL_HANDLE);
		outBufferMemory = std::exchange(resources.memory, VK_NULL_HANDLE);
		return {};
	}

	std::expected<void, Error> DeviceContextVulkan::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		auto commandBufferResult{ BeginSingleTimeCommands() };
		if (!commandBufferResult)
			return std::unexpected(commandBufferResult.error());
		VkCommandBuffer commandBuffer{ *commandBufferResult };

		VkBufferCopy copyRegion{ .size = size };
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		return EndSingleTimeCommands(commandBuffer);
	}

	std::expected<VkCommandBuffer, Error> DeviceContextVulkan::BeginSingleTimeCommands()
	{
		VkCommandBufferAllocateInfo allocInfo{
			.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool        = commandPool,
			.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		VkCommandBuffer commandBuffer{};
		VkResult const  allocationResult{ vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) };
		if (allocationResult != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanCommandFailed,
				.message = std::format("Failed to allocate a one-time command buffer (VkResult {})", static_cast<int>(allocationResult)),
			});

		VkCommandBufferBeginInfo beginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		};

		VkResult const beginResult{ vkBeginCommandBuffer(commandBuffer, &beginInfo) };
		if (beginResult != VK_SUCCESS)
		{
			vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanCommandFailed,
				.message = std::format("Failed to begin a one-time command buffer (VkResult {})", static_cast<int>(beginResult)),
			});
		}
		return commandBuffer;
	}

	std::expected<void, Error> DeviceContextVulkan::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
	{
		VkResult const endResult{ vkEndCommandBuffer(commandBuffer) };
		if (endResult != VK_SUCCESS)
		{
			vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanCommandFailed,
				.message = std::format("Failed to end a one-time command buffer (VkResult {})", static_cast<int>(endResult)),
			});
		}

		VkSubmitInfo submitInfo{
			.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers    = &commandBuffer,
		};

		VkResult const submitResult{ vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) };
		if (submitResult != VK_SUCCESS)
		{
			vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanCommandFailed,
				.message = std::format("Failed to submit a one-time command buffer (VkResult {})", static_cast<int>(submitResult)),
			});
		}
		VkResult const waitResult{ vkQueueWaitIdle(graphicsQueue) };
		if (waitResult != VK_SUCCESS)
		{
			vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
			auto const message{ std::format(
				"Failed to wait for a one-time command buffer to finish (VkResult {})",
				static_cast<int>(waitResult)
			) };
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanCommandFailed,
				.message = message,
			});
		}

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
		return {};
	}

	std::expected<VkSampler, Error> DeviceContextVulkan::CreateTextureSampler()
	{
		VkPhysicalDeviceProperties properties{ .apiVersion = 0 };
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		VkSamplerCreateInfo samplerInfo{
			.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter               = VK_FILTER_LINEAR,
			.minFilter               = VK_FILTER_LINEAR,
			.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.mipLodBias              = 0.f,
			.anisotropyEnable        = VK_TRUE,
			.maxAnisotropy           = properties.limits.maxSamplerAnisotropy,
			.compareEnable           = VK_FALSE,
			.compareOp               = VK_COMPARE_OP_ALWAYS,
			.minLod                  = 0.f,
			.maxLod                  = 0.f,
			.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
			.unnormalizedCoordinates = VK_FALSE,
		};
		VkSampler      textureSampler{};
		VkResult const result{ vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) };
		if (result != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanResourceCreationFailed,
				.message = std::format("Failed to create texture sampler (VkResult {}).", static_cast<int>(result)),
			});
		return textureSampler;
	}

	std::expected<GraphicsPipeline, Error> DeviceContextVulkan::CreateGraphicsPipeline(
		RenderTargetVulkan const&                    target,
		VkShaderModule                               vertexShader,
		VkShaderModule                               fragmentShader,
		VkPipelineVertexInputStateCreateInfo const&  vertexInputInfo,
		VkCullModeFlags                              cullMode,
		VkPipelineDepthStencilStateCreateInfo const& depthStencil,
		VkDescriptorSetLayout const&                 setLayout
	)
	{
		std::expected<GraphicsPipeline, Error> result{ std::in_place };

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertexShader,
			.pName  = entryPointVertexShader.data(), /* views a string literal, so null-terminated */
		};

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragmentShader,
			.pName  = entryPointFragmentShader.data(),
		};

		VkPipelineShaderStageCreateInfo const shaderStages[]{ vertShaderStageInfo, fragShaderStageInfo };

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE,
		};

		float const viewportY{
			flipVulkanViewport
				? static_cast<float>(target.extent.height)
				: 0.f
		};
		float const viewportHeight{
			flipVulkanViewport
				? -static_cast<float>(target.extent.height)
				: static_cast<float>(target.extent.height)
		};
		VkViewport viewport{
			.x        = 0.f,
			.y        = viewportY,
			.width    = static_cast<float>(target.extent.width),
			.height   = viewportHeight,
			.minDepth = 0.f,
			.maxDepth = 1.f,
		};
		VkRect2D scissor{
			.offset = { .x = 0, .y = 0 },
			.extent = target.extent,
		};

		VkPipelineViewportStateCreateInfo viewportState{
			.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.pViewports    = &viewport,
			.scissorCount  = 1,
			.pScissors     = &scissor,
		};

		VkPipelineRasterizationStateCreateInfo rasterizer{
			.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable        = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode             = VK_POLYGON_MODE_FILL,
			.cullMode                = cullMode,
			.frontFace               = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable         = VK_FALSE,
			.lineWidth               = 1.f,
		};

		VkPipelineMultisampleStateCreateInfo multisampling{
			.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable  = VK_FALSE,
		};

		VkPipelineColorBlendAttachmentState colorBlendAttachment{
			.blendEnable         = VK_FALSE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp        = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp        = VK_BLEND_OP_ADD,
			.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT
				| VK_COLOR_COMPONENT_G_BIT
				| VK_COLOR_COMPONENT_B_BIT
				| VK_COLOR_COMPONENT_A_BIT,
		};

		VkPipelineColorBlendStateCreateInfo colorBlending{
			.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable   = VK_FALSE,
			.logicOp         = VK_LOGIC_OP_COPY,
			.attachmentCount = 1,
			.pAttachments    = &colorBlendAttachment,
		};

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{
			.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 1,
			.pSetLayouts    = &setLayout,
		};

		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &result->layout) != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanResourceCreationFailed,
				.message = "Failed to create Vulkan graphics pipeline layout.",
			});

		VkGraphicsPipelineCreateInfo pipelineInfo{
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount          = 2,
			.pStages             = shaderStages,
			.pVertexInputState   = &vertexInputInfo,
			.pInputAssemblyState = &inputAssembly,
			.pViewportState      = &viewportState,
			.pRasterizationState = &rasterizer,
			.pMultisampleState   = &multisampling,
			.pDepthStencilState  = &depthStencil,
			.pColorBlendState    = &colorBlending,
			.layout              = result->layout,
			.renderPass          = target.renderPass,
			.subpass             = 0,
		};

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &result->pipeline) != VK_SUCCESS)
		{
			vkDestroyPipelineLayout(device, result->layout, nullptr);
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanResourceCreationFailed,
				.message = "Failed to create Vulkan graphics pipeline.",
			});
		}
		return result;
	}
} // namespace gg
