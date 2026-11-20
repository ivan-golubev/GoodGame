module;
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <limits>
#include <optional>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_vulkan.h>
#include <set>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>
#include <windows.h>
#include <WinPixEventRuntime/pix3.h>
module RendererVulkan;

import Camera;
import DebugOverlayVulkan;
import ErrorHandling;
import GlobalSettings;
import Input;
import Lighting;
import ModelVulkan;
import ShaderProgramVulkan;
import SkyboxVulkan;
import Texture;
import TextureVulkan;
import Vertex;
import VertexVulkan;

using std::chrono::nanoseconds;

namespace
{
	std::string const shaderExtensionVulkan{ "spv" };
	std::string const shadersLocation{ "shaders/spirv" };

	constexpr uint32_t maxDescriptorSets{ 1000 };

	constexpr std::array<char const*, 2> deviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_MAINTENANCE1_EXTENSION_NAME // required for using negative viewport heights
	};

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(std::vector<VkSurfaceFormatKHR> const& availableFormats)
	{
		for (auto const& availableFormat : availableFormats)
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return availableFormat;
		return availableFormats[0];
	}

	VkPresentModeKHR ChooseSwapPresentMode(std::vector<VkPresentModeKHR> const& availablePresentModes)
	{
		for (auto const& m : availablePresentModes)
			if (m == VK_PRESENT_MODE_MAILBOX_KHR)
				return m;
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	bool CheckDeviceExtensionSupport(VkPhysicalDevice const device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> availableExtensionNames{};
		for (auto& ext : availableExtensions)
			availableExtensionNames.insert(ext.extensionName);

		for (auto& ext : deviceExtensions)
			if (!availableExtensionNames.contains(ext))
				return false;
		return true;
	}
} // namespace

namespace gg
{
	std::expected<void, Error> RendererVulkan::Initialize(RendererSettings const& rs)
	{
		width           = rs.width;
		height          = rs.height;
		windowHandleSDL = rs.windowHandle;
		timeManager     = rs.timeManager;
		camera          = std::make_shared<Camera>(rs.inputManager);
		uint32_t extensionCount;
		if (!SDL_Vulkan_GetInstanceExtensions(windowHandleSDL, &extensionCount, nullptr))
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Could not get the number of required instance extensions from SDL.",
			});
		std::vector<char const*> extensions(extensionCount);
		if (!SDL_Vulkan_GetInstanceExtensions(windowHandleSDL, &extensionCount, extensions.data()))
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Could not get the names of required instance extensions from SDL.",
			});

		std::vector<char const*> layers;
		if constexpr (IsDebug())
		{ /* Enable the debug layer */
			layers.emplace_back("VK_LAYER_KHRONOS_validation");
		}

		if (auto result{ CreateVkInstance(layers, extensions) }; !result)
			return std::unexpected(result.error());

		/* Create a surface for rendering */
		if (!SDL_Vulkan_CreateSurface(windowHandleSDL, instance, &surface))
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Could not create a Vulkan surface.",
			});

		if (auto result{ SelectPhysicalDevice() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateLogicalDevice() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateSwapChain() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateImageViews() }; !result) return std::unexpected(result.error());
		{
			auto depthFormatResult{ DepthBufferVulkan::FindFormat(physicalDevice) };
			if (!depthFormatResult) return std::unexpected(depthFormatResult.error());
			depthBuffer = std::make_unique<DepthBufferVulkan>(device, physicalDevice, *depthFormatResult);
		}
		if (auto result{ CreateRenderPass() }; !result) return std::unexpected(result.error());
		if (auto result{ depthBuffer->Create(swapChainExtent) }; !result) return std::unexpected(result.error());

		if (auto result{ CreateDescriptorPool() }; !result) return std::unexpected(result.error());

		if (auto result{ CreateFrameBuffers() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateCommandPool() }; !result) return std::unexpected(result.error());

		if (auto result{ CreateCommandBuffers() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateSyncObjects() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateRenderFinishedSemaphores() }; !result) return std::unexpected(result.error());

		{ /* ImGUI */
			QueueFamilyIndices indices{ FindQueueFamilies(physicalDevice) };
			if (!indices.graphicsFamily)
				return std::unexpected(Error{
					.code    = ErrorCode::VulkanRendererInitializationFailed,
					.message = "No graphics queue family was found for the Vulkan debug UI.",
				});
			DebugOverlayVulkanCreateInfo debugOverlayInfo{
				.device              = device,
				.instance            = instance,
				.graphicsQueueFamily = *indices.graphicsFamily,
				.graphicsQueue       = graphicsQueue,
				.physicalDevice      = physicalDevice,
				.renderPass          = renderPass,
				.windowHandleSDL     = windowHandleSDL,
				.renderer            = this,
			};

			debugOverlay = std::make_unique<DebugOverlayVulkan>();
			if (auto result{ debugOverlay->Initialize(debugOverlayInfo) }; !result)
				return std::unexpected(result.error());
		}
		return {};
	}

	RendererType RendererVulkan::GetType() const
	{
		return RendererType::Vulkan;
	}

	std::shared_ptr<Camera> RendererVulkan::GetCamera()
	{
		return camera;
	}

	RendererVulkan* RendererVulkan::current{ nullptr };

	RendererVulkan::RendererVulkan()
	{
		BreakIfFalse(current == nullptr);
		current = this;
	}

	RendererVulkan& RendererVulkan::Get()
	{
		BreakIfFalse(current != nullptr);
		return *current;
	}

	std::expected<void, Error> RendererVulkan::CreateVkInstance(
		std::vector<char const*> const& layers,
		std::vector<char const*> const& extensions
	)
	{
		VkApplicationInfo appInfo{
			.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName   = "gg Vulkan Renderer",
			.applicationVersion = VK_HEADER_VERSION_COMPLETE,
			.pEngineName        = "gg Engine",
			.engineVersion      = VK_HEADER_VERSION_COMPLETE,
			.apiVersion         = VK_HEADER_VERSION_COMPLETE,
		};

		VkInstanceCreateInfo instInfo{
			.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.flags                   = 0,
			.pApplicationInfo        = &appInfo,
			.enabledLayerCount       = static_cast<uint32_t>(layers.size()),
			.ppEnabledLayerNames     = layers.data(),
			.enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
			.ppEnabledExtensionNames = extensions.data(),
		};

		VkResult const result{ vkCreateInstance(&instInfo, nullptr, &instance) };
		if (result == VK_ERROR_INCOMPATIBLE_DRIVER)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Unable to find a compatible Vulkan Driver.",
			});
		else if (result)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = std::format("Could not create a Vulkan instance (VkResult {}).", static_cast<int>(result)),
			});
		return {};
	}

	VkExtent2D RendererVulkan::ChooseSwapExtent(VkSurfaceCapabilitiesKHR const& capabilities) const
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			return capabilities.currentExtent;

		int width, height;
		SDL_Vulkan_GetDrawableSize(windowHandleSDL, &width, &height);

		return {
			.width  = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
		};
	}

	std::expected<void, Error> RendererVulkan::CreateSwapChain()
	{
		SwapChainSupportDetails swapChainSupport{ QuerySwapChainSupport(physicalDevice) };
		VkSurfaceFormatKHR      surfaceFormat{ ChooseSwapSurfaceFormat(swapChainSupport.formats) };
		VkPresentModeKHR        presentMode{ ChooseSwapPresentMode(swapChainSupport.presentModes) };
		VkExtent2D              extent{ ChooseSwapExtent(swapChainSupport.capabilities) };

		uint32_t imageCount{ swapChainSupport.capabilities.minImageCount + 1 };
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
			imageCount = swapChainSupport.capabilities.maxImageCount;
		QueueFamilyIndices indices{ FindQueueFamilies(physicalDevice) };
		if (!indices.IsComplete())
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "No complete queue family was found while creating the swap chain.",
			});
		uint32_t                 queueFamilyIndices[]{ *indices.graphicsFamily, *indices.presentFamily };
		bool const               useConcurrentSharing{ indices.graphicsFamily != indices.presentFamily };
		VkSwapchainCreateInfoKHR createInfo{
			.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface               = surface,
			.minImageCount         = imageCount,
			.imageFormat           = surfaceFormat.format,
			.imageColorSpace       = surfaceFormat.colorSpace,
			.imageExtent           = extent,
			.imageArrayLayers      = 1,
			.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode      = useConcurrentSharing ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = useConcurrentSharing ? 2U : 0U,
			.pQueueFamilyIndices   = useConcurrentSharing ? queueFamilyIndices : nullptr,
			.preTransform          = swapChainSupport.capabilities.currentTransform,
			.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode           = presentMode,
			.clipped               = VK_TRUE,
		};
		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Failed to create swap chain.",
			});
		/* Retrieve the swap chain images to render to */
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent      = extent;
		return {};
	}

	std::expected<void, Error> RendererVulkan::CreateImageViews()
	{
		swapChainImageViews.resize(swapChainImages.size());
		for (size_t i{ 0 }; i < swapChainImages.size(); ++i)
		{
			auto imageViewResult{ CreateImageView(device, swapChainImages[i], swapChainImageFormat) };
			if (!imageViewResult)
				return std::unexpected(Error{
					.code    = ErrorCode::VulkanRendererInitializationFailed,
					.message = std::format("Failed to create swap chain image view {}: {}", i, imageViewResult.error().message),
				});
			swapChainImageViews[i] = *imageViewResult;
		}
		return {};
	}

	std::expected<void, Error> RendererVulkan::CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment{
			.format         = swapChainImageFormat,
			.samples        = VK_SAMPLE_COUNT_1_BIT,
			.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		};

		/* Cleared to 1.0 (far plane) every frame; the contents are not needed after the pass. */
		VkAttachmentDescription depthAttachment{
			.format         = depthBuffer->GetFormat(),
			.samples        = VK_SAMPLE_COUNT_1_BIT,
			.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkAttachmentReference colorAttachmentRef{
			.attachment = 0,
			.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkAttachmentReference depthAttachmentRef{
			.attachment = 1,
			.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		/* The previous frame may still be testing against the shared depth image: wait for its depth tests
		   before this frame clears it, in addition to the existing colour output dependency. */
		VkSubpassDependency dependency{
			.srcSubpass    = VK_SUBPASS_EXTERNAL,
			.dstSubpass    = 0,
			.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		};

		VkSubpassDescription subpass{
			.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount    = 1,
			.pColorAttachments       = &colorAttachmentRef,
			.pDepthStencilAttachment = &depthAttachmentRef,
		};

		std::array<VkAttachmentDescription, 2> const attachments{ colorAttachment, depthAttachment };
		VkRenderPassCreateInfo                       renderPassInfo{
			.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments    = attachments.data(),
			.subpassCount    = 1,
			.pSubpasses      = &subpass,
			.dependencyCount = 1,
			.pDependencies   = &dependency,
		};

		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
		{
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Failed to create Vulkan render pass.",
			});
		}
		return {};
	}

	std::expected<GraphicsPipeline, Error> RendererVulkan::CreateGraphicsPipeline(
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
			.pName  = entryPointVertexShader.data(),
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
				? static_cast<float>(swapChainExtent.height)
				: 0.f
		};
		float const viewportHeight{
			flipVulkanViewport
				? -static_cast<float>(swapChainExtent.height)
				: static_cast<float>(swapChainExtent.height)
		};
		VkViewport viewport{
			.x        = 0.f,
			.y        = viewportY,
			.width    = static_cast<float>(swapChainExtent.width),
			.height   = viewportHeight,
			.minDepth = 0.f,
			.maxDepth = 1.f,
		};
		VkRect2D scissor{
			.offset = { .x = 0, .y = 0 },
			.extent = swapChainExtent,
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
			.renderPass          = renderPass,
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

	std::expected<void, Error> RendererVulkan::CreateFrameBuffers()
	{
		frameBuffers.resize(swapChainImageViews.size());
		for (size_t i{ 0 }; i < swapChainImageViews.size(); ++i)
		{
			/* Every framebuffer shares the one depth image; the render pass dependency orders its use across frames. */
			std::array<VkImageView, 2> const attachments{ swapChainImageViews[i], depthBuffer->GetView() };
			VkFramebufferCreateInfo          framebufferInfo{
				.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass      = renderPass,
				.attachmentCount = static_cast<uint32_t>(attachments.size()),
				.pAttachments    = attachments.data(),
				.width           = swapChainExtent.width,
				.height          = swapChainExtent.height,
				.layers          = 1,
			};
			if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS)
				return std::unexpected(Error{
					.code    = ErrorCode::VulkanResourceCreationFailed,
					.message = "Failed to create Vulkan framebuffer.",
				});
		}
		return {};
	}

	std::expected<void, Error> RendererVulkan::CreateCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices{ FindQueueFamilies(physicalDevice) };
		if (!queueFamilyIndices.graphicsFamily)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "No graphics queue family was found while creating the Vulkan command pool.",
			});
		VkCommandPoolCreateInfo poolInfo{
			.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = *queueFamilyIndices.graphicsFamily,
		};
		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
		{
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Failed to create Vulkan command pool.",
			});
		}
		return {};
	}

	std::expected<void, Error> RendererVulkan::CreateCommandBuffers()
	{
		VkCommandBufferAllocateInfo allocInfo{
			.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool        = commandPool,
			.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
		};
		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Failed to allocate Vulkan command buffers.",
			});
		return {};
	}

	std::expected<void, Error> RendererVulkan::CreateSyncObjects()
	{
		VkSemaphoreCreateInfo semaphoreInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};

		VkFenceCreateInfo fenceInfo{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};
		for (size_t i{ 0 }; i < maxFramesInFlight; ++i)
		{
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frameImageAvailableSemaphores[i]) != VK_SUCCESS
			    || vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
			{
				return std::unexpected(Error{
					.code    = ErrorCode::VulkanRendererInitializationFailed,
					.message = "Failed to create Vulkan frame synchronization objects.",
				});
			}
		}
		return {};
	}

	std::expected<void, Error> RendererVulkan::CreateRenderFinishedSemaphores()
	{
		VkSemaphoreCreateInfo semaphoreInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};
		swapchainImageRenderFinishedSemaphores.resize(swapChainImages.size(), VK_NULL_HANDLE);
		for (VkSemaphore& semaphore : swapchainImageRenderFinishedSemaphores)
		{
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
				return std::unexpected(Error{
					.code    = ErrorCode::VulkanRendererInitializationFailed,
					.message = "Failed to create Vulkan swap chain image synchronization semaphore.",
				});
		}
		return {};
	}

	RendererVulkan::QueueFamilyIndices RendererVulkan::FindQueueFamilies(VkPhysicalDevice const device) const
	{
		uint32_t queueFamilyCount{ 0 };
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		QueueFamilyIndices indices{
			.graphicsFamily = std::nullopt,
			.presentFamily  = std::nullopt,
		};
		uint32_t i{ 0 };
		for (auto const& q : queueFamilies)
		{
			if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.graphicsFamily = i;

			VkBool32 presentSupport{ false };
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (presentSupport)
				indices.presentFamily = i;

			if (indices.IsComplete())
				break;
			i++;
		}
		return indices;
	}

	std::expected<uint32_t, Error> RendererVulkan::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
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

	RendererVulkan::SwapChainSupportDetails RendererVulkan::QuerySwapChainSupport(VkPhysicalDevice device) const
	{
		SwapChainSupportDetails details{
			.capabilities = {},
			.formats      = {},
			.presentModes = {},
		};

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	bool RendererVulkan::SwapChainRequirementsSatisfied(VkPhysicalDevice const device) const
	{
		SwapChainSupportDetails swapChainSupport{ QuerySwapChainSupport(device) };
		return !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	bool RendererVulkan::IsDeviceSuitable(VkPhysicalDevice const device) const
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
			&& CheckDeviceExtensionSupport(device)
			&& SwapChainRequirementsSatisfied(device)
			&& FindQueueFamilies(device).IsComplete()
			&& supportedFeatures.samplerAnisotropy;
	}

	std::expected<void, Error> RendererVulkan::SelectPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0)
		{
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Failed to find GPUs with Vulkan support.",
			});
		}
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
		for (auto const& d : devices)
		{
			if (IsDeviceSuitable(d))
			{
				physicalDevice = d;
				break;
			}
		}
		if (physicalDevice == VK_NULL_HANDLE)
		{
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Failed to find a suitable GPU.",
			});
		}
		return {};
	}

	std::expected<void, Error> RendererVulkan::CreateLogicalDevice()
	{
		QueueFamilyIndices indices{ FindQueueFamilies(physicalDevice) };
		if (!indices.IsComplete())
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "No complete queue family was found while creating the Vulkan device.",
			});
		std::set<uint32_t>                   uniqueQueueFamilies{ *indices.graphicsFamily, *indices.presentFamily };
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

		float queuePriority{ 1.f };
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{
				.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = queueFamily,
				.queueCount       = 1,
				.pQueuePriorities = &queuePriority,
			};
			queueCreateInfos.push_back(queueCreateInfo);
		}
		VkPhysicalDeviceFeatures enabledVk10Features{
			.samplerAnisotropy = VK_TRUE
		};
		VkPhysicalDeviceVulkan12Features enabledVk12Features{
			.sType                                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.descriptorIndexing                        = true,
			.shaderSampledImageArrayNonUniformIndexing = true,
			.descriptorBindingVariableDescriptorCount  = true,
			.runtimeDescriptorArray                    = true,
			.bufferDeviceAddress                       = true
		};
		VkPhysicalDeviceVulkan13Features enabledVk13Features{
			.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			.pNext            = &enabledVk12Features,
			.synchronization2 = true,
			.dynamicRendering = true,
		};
		VkDeviceCreateInfo createInfo{
			.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext                   = &enabledVk13Features,
			.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size()),
			.pQueueCreateInfos       = queueCreateInfos.data(),
			.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()),
			.ppEnabledExtensionNames = deviceExtensions.data(),
			.pEnabledFeatures        = &enabledVk10Features,
		};
		VkResult const result{ vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) };
		if (result != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = std::format("Failed to create Vulkan logical device (VkResult {}).", static_cast<int>(result)),
			});
		vkGetDeviceQueue(device, *indices.graphicsFamily, 0, &graphicsQueue);
		return {};
	}

	std::expected<void, Error> RendererVulkan::CreateIndexBuffer(Mesh const& mesh)
	{
		struct BufferResources
		{
			VkDevice       device;
			VkBuffer       buffer{};
			VkDeviceMemory memory{};
			VkBuffer       stagingBuffer{};
			VkDeviceMemory stagingMemory{};

			~BufferResources() noexcept
			{
				if (stagingBuffer)
					vkDestroyBuffer(device, stagingBuffer, nullptr);
				if (stagingMemory)
					vkFreeMemory(device, stagingMemory, nullptr);
				if (buffer)
					vkDestroyBuffer(device, buffer, nullptr);
				if (memory)
					vkFreeMemory(device, memory, nullptr);
			}
		} resources{ device };

		uint64_t const indexBufferSizeBytes{ mesh.IndicesSizeBytes() };

		auto stagingBufferResult{ CreateBuffer(
			resources.stagingBuffer,
			resources.stagingMemory,
			indexBufferSizeBytes,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		) };
		if (!stagingBufferResult)
			return std::unexpected(stagingBufferResult.error());

		void*          mappedData{};
		VkResult const mapResult{ vkMapMemory(device, resources.stagingMemory, 0, indexBufferSizeBytes, 0, &mappedData) };
		if (mapResult != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanResourceCreationFailed,
				.message = std::format("Failed to map staging buffer for index data (VkResult {})", static_cast<int>(mapResult)),
			});
		memcpy(mappedData, mesh.indices.data(), static_cast<size_t>(indexBufferSizeBytes));
		vkUnmapMemory(device, resources.stagingMemory);

		VkBufferUsageFlagBits const usage{ static_cast<VkBufferUsageFlagBits>(
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
		) };
		auto                        indexBufferResult{ CreateBuffer(
			resources.buffer,
			resources.memory,
			indexBufferSizeBytes,
			usage,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		) };
		if (!indexBufferResult)
			return std::unexpected(indexBufferResult.error());
		if (auto result{ CopyBuffer(resources.stagingBuffer, resources.buffer, indexBufferSizeBytes) }; !result)
			return std::unexpected(result.error());
		return {};
	}

	std::expected<void, Error> RendererVulkan::CreateDescriptorPool()
	{
		// Per frame in flight, a model set uses 2 uniform buffers + 1 combined image sampler,
		// and the skybox set uses 1 uniform buffer + 1 combined image sampler.
		// Budget for up to maxDescriptorSets sets of either kind.
		std::array<VkDescriptorPoolSize, 2> poolSizes{
			VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = maxFramesInFlight * maxDescriptorSets * 2 },
			VkDescriptorPoolSize{
				.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = maxFramesInFlight * maxDescriptorSets,
			},
		};

		VkDescriptorPoolCreateInfo poolInfo{
			.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets       = maxFramesInFlight * maxDescriptorSets,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes    = poolSizes.data(),
		};

		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanResourceCreationFailed,
				.message = "Failed to create Vulkan descriptor pool.",
			});
		return {};
	}

	std::expected<void, Error> RendererVulkan::ResizeWindow()
	{
		if (auto result{ RecreateSwapChain() }; !result)
			return std::unexpected(result.error());
		camera->UpdateProjectionMatrix(width, height);
		isWindowResized = false;
		return {};
	}

	void RendererVulkan::CleanupSwapChain()
	{
		if (!device)
			return;
		for (auto framebuffer : frameBuffers)
			if (framebuffer) vkDestroyFramebuffer(device, framebuffer, nullptr);
		frameBuffers.clear();
		for (auto imageView : swapChainImageViews)
			if (imageView) vkDestroyImageView(device, imageView, nullptr);
		swapChainImageViews.clear();
		if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);
		renderPass = VK_NULL_HANDLE;
		if (swapChain) vkDestroySwapchainKHR(device, swapChain, nullptr);
		swapChain = VK_NULL_HANDLE;
	}

	std::expected<void, Error> RendererVulkan::RecreateSwapChain()
	{
		vkDeviceWaitIdle(device);
		for (VkSemaphore semaphore : swapchainImageRenderFinishedSemaphores)
			if (semaphore) vkDestroySemaphore(device, semaphore, nullptr);
		swapchainImageRenderFinishedSemaphores.clear();
		CleanupSwapChain();

		if (auto result{ CreateSwapChain() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateRenderFinishedSemaphores() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateImageViews() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateRenderPass() }; !result) return std::unexpected(result.error());
		if (auto result{ depthBuffer->Create(swapChainExtent) }; !result) return std::unexpected(result.error());
		if (skybox)
			if (auto result{ skybox->CreatePipeline() }; !result) return std::unexpected(result.error());
		for (auto model : models)
			if (auto result{ model->CreatePipeline() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateFrameBuffers() }; !result) return std::unexpected(result.error());
		return {};
	}

	std::expected<void, Error> RendererVulkan::CreateBuffer(
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

	std::expected<void, Error> RendererVulkan::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		auto commandBufferResult{ BeginSingleTimeCommands() };
		if (!commandBufferResult)
			return std::unexpected(commandBufferResult.error());
		VkCommandBuffer commandBuffer{ *commandBufferResult };

		VkBufferCopy copyRegion{ .size = size };
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		return EndSingleTimeCommands(commandBuffer);
	}

	RendererVulkan::~RendererVulkan() noexcept
	{
		if (device)
		{
			/* Ensure that the GPU is no longer referencing resources that are about to be
			 cleaned up by the destructor. */
			vkDeviceWaitIdle(device);

			CleanupSwapChain();
			depthBuffer.reset();

			for (size_t i{ 0 }; i < maxFramesInFlight; ++i)
			{
				if (frameImageAvailableSemaphores[i]) vkDestroySemaphore(device, frameImageAvailableSemaphores[i], nullptr);
				if (inFlightFences[i]) vkDestroyFence(device, inFlightFences[i], nullptr);
			}
			for (VkSemaphore semaphore : swapchainImageRenderFinishedSemaphores)
				if (semaphore) vkDestroySemaphore(device, semaphore, nullptr);
			if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
		}

		/* destroys the associated shaders and textures */
		models.clear();
		skybox.reset();

		debugOverlay.reset();
		if (device && descriptorPool) vkDestroyDescriptorPool(device, descriptorPool, nullptr);

		if (instance && surface) vkDestroySurfaceKHR(instance, surface, nullptr);
		if (device) vkDestroyDevice(device, nullptr);
		if (instance) vkDestroyInstance(instance, nullptr);

		current = nullptr;
	}

	void RendererVulkan::OnWindowResized(uint32_t width, uint32_t height)
	{
		isWindowResized = true;
		width           = std::max(8u, width);
		height          = std::max(8u, height);
	}

	VkDevice RendererVulkan::GetDevice() const { return device; }

	std::expected<void, Error> RendererVulkan::Render(nanoseconds deltaTime)
	{
		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result{ vkAcquireNextImageKHR(
			device,
			swapChain,
			UINT64_MAX,
			frameImageAvailableSemaphores[currentFrame],
			VK_NULL_HANDLE,
			&imageIndex
		) };

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			return ResizeWindow();
		}
		else if (result != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererFrameFailed,
				.message = std::format("Failed to acquire swap chain image (VkResult {}).", static_cast<int>(result)),
			});

		camera->UpdateCamera(deltaTime);

		vkResetFences(device, 1, &inFlightFences[currentFrame]);

		/* Record all the commands we need to render the scene into the command list. */
		if (auto commandResult{ RecordCommandBuffer(commandBuffers[currentFrame], imageIndex) }; !commandResult)
			return std::unexpected(commandResult.error());
		/* Execute the commands */
		if (auto submitResult{ SubmitCommands(imageIndex) }; !submitResult)
			return std::unexpected(submitResult.error());
		/* Present the frame and inefficiently wait for the frame to render. */
		result = Present(imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || isWindowResized)
		{
			if (auto resizeResult{ ResizeWindow() }; !resizeResult)
				return std::unexpected(resizeResult.error());
		}
		else if (result != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererFrameFailed,
				.message = std::format("Failed to present swap chain image (VkResult {}).", static_cast<int>(result)),
			});
		currentFrame = (currentFrame + 1) % maxFramesInFlight;
		return {};
	}

	std::expected<std::shared_ptr<ShaderProgram>, Error> RendererVulkan::LoadShader(std::string const& shaderName)
	{
		std::string const vertexShaderRelativePath{ std::format(
			"{}/{}_VS.{}",
			shadersLocation,
			shaderName,
			shaderExtensionVulkan
		) };
		std::string const fragmentShaderRelativePath{ std::format(
			"{}/{}_PS.{}",
			shadersLocation,
			shaderName,
			shaderExtensionVulkan
		) };
		auto              shader{ std::make_unique<ShaderProgramVulkan>(ShaderProgramVulkan::PassKey{}, device) };
		if (auto const result{ shader->Initialize(vertexShaderRelativePath, fragmentShaderRelativePath) }; !result)
			return std::unexpected(result.error());
		return std::shared_ptr<ShaderProgram>{ std::move(shader) };
	}

	std::expected<std::shared_ptr<Texture>, Error> RendererVulkan::LoadTexture(std::string const& name)
	{
		std::string const textureRelativePath{ std::format("{}/{}.{}", texturesLocation, name, texturesExtension) };
		auto              texture{ std::make_unique<TextureVulkan>(device, textureRelativePath) };
		if (auto const result{ texture->Initialize() }; !result)
			return std::unexpected(result.error());
		return std::shared_ptr<Texture>{ std::move(texture) };
	}

	std::expected<void, Error> RendererVulkan::LoadModel(
		std::string const& modelRelativePath,
		std::string const& shaderName,
		glm::vec3&         position
	)
	{
		auto shaderResult{ LoadShader(shaderName) };
		if (!shaderResult)
			return std::unexpected(shaderResult.error());
		std::shared_ptr<ShaderProgram> shader{ std::move(*shaderResult) };
		std::shared_ptr<ModelVulkan>   model{ std::make_shared<ModelVulkan>(modelRelativePath, shader, position, device) };
		if (auto const result{ model->Initialize(descriptorPool) }; !result)
			return std::unexpected(result.error());
		models.push_back(model);
		return {};
	}

	std::expected<void, Error> RendererVulkan::LoadSkybox(std::string const& name, std::string const& shaderName)
	{
		auto shaderResult{ LoadShader(shaderName) };
		if (!shaderResult)
			return std::unexpected(shaderResult.error());
		std::shared_ptr<ShaderProgram> shader{ std::move(*shaderResult) };
		skybox = std::make_unique<SkyboxVulkan>(name, shader, device);
		if (auto result{ skybox->Initialize(descriptorPool) }; !result)
			return std::unexpected(result.error());
		return {};
	}

	VkResult RendererVulkan::Present(uint32_t imageIndex)
	{
		VkSemaphore      signalSemaphores[] = { swapchainImageRenderFinishedSemaphores[imageIndex] };
		VkSwapchainKHR   swapChains[]       = { swapChain };
		VkPresentInfoKHR presentInfo{
			.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores    = signalSemaphores,
			.swapchainCount     = 1,
			.pSwapchains        = swapChains,
			.pImageIndices      = &imageIndex,
		};
		return vkQueuePresentKHR(graphicsQueue, &presentInfo);
	}

	std::expected<void, Error> RendererVulkan::SubmitCommands(uint32_t imageIndex)
	{
		VkSemaphore          waitSemaphores[]{ frameImageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[]       = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSemaphore          signalSemaphores[] = { swapchainImageRenderFinishedSemaphores[imageIndex] };
		VkSubmitInfo         submitInfo{
			.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount   = 1,
			.pWaitSemaphores      = waitSemaphores,
			.pWaitDstStageMask    = waitStages,
			.commandBufferCount   = 1,
			.pCommandBuffers      = &commandBuffers[currentFrame],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores    = signalSemaphores,
		};
		VkResult const result{ vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) };
		if (result != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererFrameFailed,
				.message = std::format("Failed to submit a command buffer (VkResult {}).", static_cast<int>(result)),
			});
		return {};
	}

	std::expected<void, Error> RendererVulkan::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		vkResetCommandBuffer(commandBuffer, 0);

		VkCommandBufferBeginInfo beginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		};
		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererFrameFailed,
				.message = "Failed to begin recording Vulkan command buffer.",
			});

		/* Same order as the render pass attachments: colour, then depth (cleared to the far plane). */
		std::array<VkClearValue, 2> const clearValues{
			VkClearValue{ .color = { .float32 = { 0.f, 0.2f, 0.4f, 1.f } } },
			VkClearValue{ .depthStencil = { .depth = 1.f, .stencil = 0 } },
		};
		VkRenderPassBeginInfo renderPassInfo{
			.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass      = renderPass,
			.framebuffer     = frameBuffers[imageIndex],
			.renderArea      = { .offset = { .x = 0, .y = 0 }, .extent = swapChainExtent },
			.clearValueCount = static_cast<uint32_t>(clearValues.size()),
			.pClearValues    = clearValues.data(),
		};

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		double const currentTimeSec{ timeManager->GetCurrentTimeSec() };
		for (std::shared_ptr<ModelVulkan> model : models)
			model->Draw(commandBuffer, currentFrame, *camera, currentTimeSec);

		/* After the models: the sky only fills pixels the depth test leaves at the far plane. */
		if (skybox)
			skybox->Draw(commandBuffer, currentFrame, *camera);

		debugOverlay->Render(commandBuffer, *this);

		vkCmdEndRenderPass(commandBuffer);
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererFrameFailed,
				.message = "Failed to finish recording Vulkan command buffer.",
			});
		return {};
	}

	std::expected<VkCommandBuffer, Error> RendererVulkan::BeginSingleTimeCommands()
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

	std::expected<void, Error> RendererVulkan::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
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

	std::expected<VkSampler, Error> RendererVulkan::CreateTextureSampler()
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

} // namespace gg
