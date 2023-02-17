module;
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <memory>
#include <vulkan/vulkan_core.h>
module DebugUI_Vulkan;

import Camera;
import DebugUI;
import ErrorHandling;
import ErrorHandlingVulkan;
import RendererVulkan;
import SettingsRenderer;

namespace gg
{
	DebugUI_Vulkan::DebugUI_Vulkan(DebugUI_VulkanCreateInfo const& info)
		: device{ info.device }
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		{ /* create a descriptor pool for ImGUI data */
			VkDescriptorPoolSize pool_sizes[] =
			{
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
			};

			VkDescriptorPoolCreateInfo poolInfo = {};
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			poolInfo.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
			poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
			poolInfo.pPoolSizes = pool_sizes;

			if (VK_SUCCESS != vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool))
				throw VulkanInitException("failed to create descriptor pool!");
		}

		ImGui_ImplSDL2_InitForVulkan(info.windowHandleSDL);

		{
			ImGui_ImplVulkan_InitInfo init_info = {};
			init_info.Instance = info.instance;
			init_info.PhysicalDevice = info.physicalDevice;
			init_info.Device = device;
			init_info.QueueFamily = info.graphicsQueueFamily;
			init_info.Queue = info.graphicsQueue;
			init_info.PipelineCache = VK_NULL_HANDLE;
			init_info.DescriptorPool = descriptorPool;
			init_info.Subpass = 0;
			init_info.MinImageCount = maxFramesInFlight;
			init_info.ImageCount = maxFramesInFlight;
			init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
			init_info.Allocator = VK_NULL_HANDLE;
			init_info.CheckVkResultFn = [](VkResult err) {
				if (VK_SUCCESS != err)
					throw VulkanInitException("failed to initialize imGUI!");
			};

			ImGui_ImplVulkan_Init(&init_info, info.renderPass);
		}

		{ /* upload fonts for ImGUI */
			VkCommandBuffer commandBuffer{ info.renderer->BeginSingleTimeCommands() };
			ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
			info.renderer->EndSingleTimeCommands(commandBuffer);
		}
	}

	DebugUI_Vulkan::~DebugUI_Vulkan()
	{
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL2_Shutdown();
	}

	void DebugUI_Vulkan::Render(VkCommandBuffer commandBuffer, std::shared_ptr<Camera> camera)
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();

		RenderDebugUI(camera);

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	}
} //namespace gg
