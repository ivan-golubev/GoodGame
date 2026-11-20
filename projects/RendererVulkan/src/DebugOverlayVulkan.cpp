module;
#include <format>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <memory>
#include <vulkan/vulkan_core.h>
module DebugOverlayVulkan;

import Camera;
import DebugOverlay;
import ErrorHandling;
import RendererVulkan;
import SettingsRenderer;

namespace gg
{
	namespace
	{
		VkResult imguiVulkanInitResult{ VK_SUCCESS };
	}

	std::expected<void, Error> DebugOverlayVulkan::Initialize(DebugOverlayVulkanCreateInfo const& info)
	{
		device = info.device;
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		contextCreated = true;
		ImGui::StyleColorsDark();

		{ /* create a descriptor pool for ImGUI data */
			VkDescriptorPoolSize poolSizes[] = {
				{ .type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1000 },
				{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1000 },
				{ .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1000 },
				{ .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1000 },
				{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, .descriptorCount = 1000 },
				{ .type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, .descriptorCount = 1000 },
				{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1000 },
				{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1000 },
				{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 1000 },
				{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .descriptorCount = 1000 },
				{ .type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, .descriptorCount = 1000 }
			};

			VkDescriptorPoolCreateInfo poolInfo{
				.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
				.maxSets       = 1000 * IM_ARRAYSIZE(poolSizes),
				.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(poolSizes)),
				.pPoolSizes    = poolSizes,
			};

			if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
				return std::unexpected(Error{
					.code    = ErrorCode::VulkanResourceCreationFailed,
					.message = "Failed to create the ImGui Vulkan descriptor pool.",
				});
		}

		sdlBackendInitialized = ImGui_ImplSDL2_InitForVulkan(info.windowHandleSDL);
		if (!sdlBackendInitialized)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Failed to initialize the ImGui SDL Vulkan backend.",
			});

		{
			ImGui_ImplVulkan_InitInfo initInfo{
				.Instance        = info.instance,
				.PhysicalDevice  = info.physicalDevice,
				.Device          = device,
				.QueueFamily     = info.graphicsQueueFamily,
				.Queue           = info.graphicsQueue,
				.PipelineCache   = VK_NULL_HANDLE,
				.DescriptorPool  = descriptorPool,
				.Subpass         = 0,
				.MinImageCount   = maxFramesInFlight,
				.ImageCount      = maxFramesInFlight,
				.MSAASamples     = VK_SAMPLE_COUNT_1_BIT,
				.Allocator       = VK_NULL_HANDLE,
				.CheckVkResultFn = [](VkResult err)
				{
					if (err != VK_SUCCESS)
						imguiVulkanInitResult = err;
				},
			};

			imguiVulkanInitResult    = VK_SUCCESS;
			vulkanBackendInitialized = ImGui_ImplVulkan_Init(&initInfo, info.renderPass);
		}
		if (!vulkanBackendInitialized || imguiVulkanInitResult != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = std::format("Failed to initialize ImGui Vulkan backend (VkResult {}).", static_cast<int>(imguiVulkanInitResult)),
			});

		{ /* upload fonts for ImGUI */
			auto commandBufferResult{ info.renderer->BeginSingleTimeCommands() };
			if (!commandBufferResult)
				return std::unexpected(commandBufferResult.error());
			VkCommandBuffer const commandBuffer{ *commandBufferResult };
			ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
			if (auto result{ info.renderer->EndSingleTimeCommands(commandBuffer) }; !result)
				return std::unexpected(result.error());
		}
		return {};
	}

	DebugOverlayVulkan::~DebugOverlayVulkan() noexcept
	{
		if (vulkanBackendInitialized)
			ImGui_ImplVulkan_Shutdown();
		if (sdlBackendInitialized)
			ImGui_ImplSDL2_Shutdown();
		if (contextCreated)
			ImGui::DestroyContext();
		if (device && descriptorPool)
			vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	}

	void DebugOverlayVulkan::Render(VkCommandBuffer commandBuffer)
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();

		RenderDebugOverlay();

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	}
} // namespace gg
