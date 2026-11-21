module;
#include <array>
#include <cstring>
#include <format>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vulkan/vulkan.h>
module ModelVulkan;

import Camera;
import ErrorHandling;
import Lighting;
import Model;
import Renderer;
import SettingsRenderer;
import ShaderProgram;
import ShaderProgramVulkan;
import TextureVulkan;
import VertexVulkan;

namespace gg
{
	ModelVulkan::ModelVulkan(
		std::string const&                   modelRelativePath,
		std::shared_ptr<ShaderProgram>       s,
		std::shared_ptr<DeviceContextVulkan> c
	)
		: Model{ modelRelativePath, s }
		, context{ c }
	{
	}

	ModelVulkan::~ModelVulkan() noexcept
	{
		if (!context)
			return;

		DestroyPipeline();
		if (descriptorSetLayout)
			vkDestroyDescriptorSetLayout(context->GetDevice(), descriptorSetLayout, nullptr);

		if (vertexBuffer)
			vkDestroyBuffer(context->GetDevice(), vertexBuffer, nullptr);
		if (vertexBufferMemory)
			vkFreeMemory(context->GetDevice(), vertexBufferMemory, nullptr);

		for (size_t i{ 0 }; i < maxFramesInFlight; ++i)
		{
			if (uniformBuffersMVP[i])
				vkDestroyBuffer(context->GetDevice(), uniformBuffersMVP[i], nullptr);
			if (uniformBuffersMemoryMVP[i])
				vkFreeMemory(context->GetDevice(), uniformBuffersMemoryMVP[i], nullptr);

			if (uniformBuffersDirLight[i])
				vkDestroyBuffer(context->GetDevice(), uniformBuffersDirLight[i], nullptr);
			if (uniformBuffersMemoryDirLight[i])
				vkFreeMemory(context->GetDevice(), uniformBuffersMemoryDirLight[i], nullptr);
		}
	}

	std::expected<void, Error> ModelVulkan::Initialize(VkDescriptorPool descriptorPool, RenderTargetVulkan const& target)
	{
		if (auto result{ Model::Initialize() }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreateVertexBuffer() }; !result)
			return std::unexpected(result.error());
		if (auto result{ LoadTextures() }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreateDescriptorSetLayout() }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreatePipeline(target) }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreateUniformBuffers() }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreateDescriptorSets(descriptorPool) }; !result)
			return std::unexpected(result.error());
		return {};
	}

	std::expected<void, Error> ModelVulkan::CreateVertexBuffer()
	{
		// TODO: handle multiple meshes properly, need to create multiple Vertex Buffers
		BreakIfFalse(meshes.size() < 2);
		Mesh const& mesh{ meshes[0] };

		uint64_t const vertexBufferSizeBytes{ mesh.VerticesSizeBytes() };

		struct StagingResources
		{
			VkDevice       device;
			VkBuffer       buffer{};
			VkDeviceMemory memory{};

			~StagingResources() noexcept
			{
				if (buffer)
					vkDestroyBuffer(device, buffer, nullptr);
				if (memory)
					vkFreeMemory(device, memory, nullptr);
			}
		} staging{ context->GetDevice() };

		auto stagingBufferResult{ context->CreateBuffer(
			staging.buffer,
			staging.memory,
			vertexBufferSizeBytes,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		) };
		if (!stagingBufferResult)
			return std::unexpected(stagingBufferResult.error());

		void*          mappedData{};
		VkResult const mapResult{ vkMapMemory(context->GetDevice(), staging.memory, 0, vertexBufferSizeBytes, 0, &mappedData) };
		if (mapResult != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanResourceCreationFailed,
				.message = std::format("Failed to map staging buffer for vertex data (VkResult {})", static_cast<int>(mapResult)),
			});
		memcpy(mappedData, mesh.vertices.data(), static_cast<size_t>(vertexBufferSizeBytes));
		vkUnmapMemory(context->GetDevice(), staging.memory);

		VkBufferUsageFlagBits const usage{ static_cast<VkBufferUsageFlagBits>(
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
		) };
		auto                        vertexBufferResult{ context->CreateBuffer(
			vertexBuffer,
			vertexBufferMemory,
			vertexBufferSizeBytes,
			usage,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		) };
		if (!vertexBufferResult)
			return std::unexpected(vertexBufferResult.error());
		if (auto result{ context->CopyBuffer(staging.buffer, vertexBuffer, vertexBufferSizeBytes) }; !result)
			return std::unexpected(result.error());
		return {};
	}

	std::expected<void, Error> ModelVulkan::LoadTextures()
	{
		for (std::string const& textureName : textureNames)
		{
			std::string const textureRelativePath{ std::format("{}/{}.{}", texturesLocation, textureName, texturesExtension) };
			auto              texture{ std::make_unique<TextureVulkan>(context, textureRelativePath) };
			if (auto const result{ texture->Initialize() }; !result)
				return std::unexpected(result.error());
			textures.emplace_back(std::move(texture));
		}
		return {};
	}

	// Resources read by textured_surface.hlsl. Binding numbers match its VK_BINDING annotations.
	std::expected<void, Error> ModelVulkan::CreateDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding const uboLayoutBindingMVP{
			.binding         = 0,
			.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		};

		VkDescriptorSetLayoutBinding const uboLayoutBindingDirLight{
			.binding         = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
		};

		VkDescriptorSetLayoutBinding const samplerLayoutBinding{
			.binding         = 2,
			.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
		};

		std::array<VkDescriptorSetLayoutBinding, 3> const bindings{
			uboLayoutBindingMVP, uboLayoutBindingDirLight, samplerLayoutBinding
		};
		VkDescriptorSetLayoutCreateInfo const layoutInfo{
			.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = static_cast<uint32_t>(bindings.size()),
			.pBindings    = bindings.data(),
		};

		if (vkCreateDescriptorSetLayout(context->GetDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanResourceCreationFailed,
				.message = "Failed to create Vulkan model descriptor set layout.",
			});
		return {};
	}

	std::expected<void, Error> ModelVulkan::CreatePipeline(RenderTargetVulkan const& target)
	{
		BreakIfFalse(shaderProgram.get());
		ShaderProgramVulkan* const shader{ dynamic_cast<ShaderProgramVulkan*>(shaderProgram.get()) };
		BreakIfFalse(shader);

		DestroyPipeline();

		auto const                                 bindingDescription{ GetVertexBindingDescription() };
		auto const                                 attributeDescriptions{ GetVertexAttributeDescriptions() };
		VkPipelineVertexInputStateCreateInfo const vertexInputInfo{
			.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount   = 1,
			.pVertexBindingDescriptions      = &bindingDescription,
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
			.pVertexAttributeDescriptions    = attributeDescriptions.data(),
		};

		/* Opaque geometry: nearer surfaces win and write their depth. */
		VkPipelineDepthStencilStateCreateInfo const depthStencil{
			.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable       = VK_TRUE,
			.depthWriteEnable      = VK_TRUE,
			.depthCompareOp        = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable     = VK_FALSE,
		};

		auto result{ context->CreateGraphicsPipeline(
			target,
			shader->GetVertexShader(),
			shader->GetFragmentShader(),
			vertexInputInfo,
			VK_CULL_MODE_BACK_BIT,
			depthStencil,
			descriptorSetLayout
		) };
		if (!result)
			return std::unexpected(std::move(result.error()));
		graphicsPipeline = *result;
		return {};
	}

	void ModelVulkan::DestroyPipeline()
	{
		if (graphicsPipeline.pipeline)
			vkDestroyPipeline(context->GetDevice(), graphicsPipeline.pipeline, nullptr);
		if (graphicsPipeline.layout)
			vkDestroyPipelineLayout(context->GetDevice(), graphicsPipeline.layout, nullptr);
		graphicsPipeline = {};
	}

	std::expected<void, Error> ModelVulkan::CreateUniformBuffers()
	{
		for (size_t i{ 0 }; i < maxFramesInFlight; ++i)
		{
			auto mvpBufferResult{ context->CreateBuffer(
				uniformBuffersMVP[i],
				uniformBuffersMemoryMVP[i],
				sizeof(ModelViewProjection),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			) };
			if (!mvpBufferResult)
				return std::unexpected(mvpBufferResult.error());
			auto dirLightBufferResult{ context->CreateBuffer(
				uniformBuffersDirLight[i],
				uniformBuffersMemoryDirLight[i],
				sizeof(DirectionalLight),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			) };
			if (!dirLightBufferResult)
				return std::unexpected(dirLightBufferResult.error());
		}
		return {};
	}

	std::expected<void, Error> ModelVulkan::CreateDescriptorSets(VkDescriptorPool descriptorPool)
	{
		std::array<VkDescriptorSetLayout, maxFramesInFlight> layouts{};
		layouts.fill(descriptorSetLayout);
		VkDescriptorSetAllocateInfo const allocInfo{
			.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool     = descriptorPool,
			.descriptorSetCount = maxFramesInFlight,
			.pSetLayouts        = layouts.data(),
		};

		if (vkAllocateDescriptorSets(context->GetDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanResourceCreationFailed,
				.message = "Failed to allocate Vulkan descriptor sets for model.",
			});

		BreakIfFalse(textures.size() > 0);
		std::shared_ptr<TextureVulkan> texture{ std::dynamic_pointer_cast<TextureVulkan>(textures[0]) };

		for (size_t i{ 0 }; i < maxFramesInFlight; ++i)
		{
			VkDescriptorBufferInfo const bufferInfoMVP{
				.buffer = uniformBuffersMVP[i],
				.offset = 0,
				.range  = sizeof(ModelViewProjection),
			};

			VkDescriptorBufferInfo const bufferInfoDirLight{
				.buffer = uniformBuffersDirLight[i],
				.offset = 0,
				.range  = sizeof(DirectionalLight),
			};

			VkDescriptorImageInfo const imageInfo{
				.sampler     = texture->textureSampler,
				.imageView   = texture->textureImageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};

			std::array<VkWriteDescriptorSet, 3> const descriptorWrites{
				VkWriteDescriptorSet{
					.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet          = descriptorSets[i],
					.dstBinding      = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pBufferInfo     = &bufferInfoMVP,
				},
				VkWriteDescriptorSet{
					.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet          = descriptorSets[i],
					.dstBinding      = 1,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pBufferInfo     = &bufferInfoDirLight,
				},
				VkWriteDescriptorSet{
					.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet          = descriptorSets[i],
					.dstBinding      = 2,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.pImageInfo      = &imageInfo,
				},
			};

			vkUpdateDescriptorSets(
				context->GetDevice(),
				static_cast<uint32_t>(descriptorWrites.size()),
				descriptorWrites.data(),
				0,
				nullptr
			);
		}
		return {};
	}

	void ModelVulkan::UpdateUniformBuffers(
		uint32_t                frame,
		glm::mat4x4 const&      transform,
		Camera const&           camera,
		DirectionalLight const& light
	)
	{
		{ /* MVP matrix */
			ModelViewProjection const mvpMatrices{ CalculateModelViewProjection(transform, camera) };
			void*                     data;
			vkMapMemory(context->GetDevice(), uniformBuffersMemoryMVP[frame], 0, sizeof(ModelViewProjection), 0, &data);
			memcpy(data, &mvpMatrices, sizeof(ModelViewProjection));
			vkUnmapMemory(context->GetDevice(), uniformBuffersMemoryMVP[frame]);
		}
		{ /* Directional Light */
			void* data;
			vkMapMemory(context->GetDevice(), uniformBuffersMemoryDirLight[frame], 0, sizeof(DirectionalLight), 0, &data);
			memcpy(data, &light, sizeof(DirectionalLight));
			vkUnmapMemory(context->GetDevice(), uniformBuffersMemoryDirLight[frame]);
		}
	}

	void ModelVulkan::Draw(
		VkCommandBuffer         commandBuffer,
		uint32_t                frame,
		glm::mat4x4 const&      transform,
		Camera const&           camera,
		DirectionalLight const& light
	)
	{
		UpdateUniformBuffers(frame, transform, camera, light);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipeline);
		vkCmdBindDescriptorSets(
			commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			graphicsPipeline.layout,
			0,
			1,
			&descriptorSets[frame],
			0,
			nullptr
		);

		VkBuffer const     vertexBuffers[]{ vertexBuffer };
		VkDeviceSize const offsets[]{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

		for (auto const& mesh : meshes)
			vkCmdDraw(commandBuffer, mesh.GetVertexCount(), 1, 0, 0);
	}
} // namespace gg
