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
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>
#include <windows.h>
#include <WinPixEventRuntime/pix3.h>
module RendererVulkan;

import Camera;
import DebugOverlayVulkan;
import DeviceContextVulkan;
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

namespace
{
	constexpr std::string_view shaderExtensionVulkan{ "spv" };
	constexpr std::string_view shadersLocation{ "shaders/spirv" };

	constexpr uint32_t maxDescriptorSets{ 1000 };

	// conventions: allow string-constant (ppEnabledExtensionNames takes char pointers)
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

		auto physicalDeviceResult{ SelectPhysicalDevice() };
		if (!physicalDeviceResult) return std::unexpected(physicalDeviceResult.error());
		if (auto result{ CreateDeviceContext(*physicalDeviceResult) }; !result) return std::unexpected(result.error());
		if (auto result{ CreateSwapChain() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateImageViews() }; !result) return std::unexpected(result.error());
		{
			auto depthFormatResult{ DepthBufferVulkan::FindFormat(context->GetPhysicalDevice()) };
			if (!depthFormatResult) return std::unexpected(depthFormatResult.error());
			depthBuffer = std::make_unique<DepthBufferVulkan>(context->GetDevice(), context->GetPhysicalDevice(), *depthFormatResult);
		}
		if (auto result{ CreateRenderPass() }; !result) return std::unexpected(result.error());
		if (auto result{ depthBuffer->Create(swapChainExtent) }; !result) return std::unexpected(result.error());

		if (auto result{ CreateDescriptorPool() }; !result) return std::unexpected(result.error());

		if (auto result{ CreateFrameBuffers() }; !result) return std::unexpected(result.error());

		if (auto result{ CreateCommandBuffers() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateSyncObjects() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateRenderFinishedSemaphores() }; !result) return std::unexpected(result.error());

		{ /* ImGUI */
			QueueFamilyIndices indices{ FindQueueFamilies(context->GetPhysicalDevice()) };
			if (!indices.graphicsFamily)
				return std::unexpected(Error{
					.code    = ErrorCode::VulkanRendererInitializationFailed,
					.message = "No graphics queue family was found for the Vulkan debug UI.",
				});
			DebugOverlayVulkanCreateInfo debugOverlayInfo{
				.device              = context->GetDevice(),
				.instance            = instance,
				.graphicsQueueFamily = *indices.graphicsFamily,
				.graphicsQueue       = context->GetGraphicsQueue(),
				.physicalDevice      = context->GetPhysicalDevice(),
				.renderPass          = renderPass,
				.windowHandleSDL     = windowHandleSDL,
				.context             = context,
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
		SwapChainSupportDetails swapChainSupport{ QuerySwapChainSupport(context->GetPhysicalDevice()) };
		VkSurfaceFormatKHR      surfaceFormat{ ChooseSwapSurfaceFormat(swapChainSupport.formats) };
		VkPresentModeKHR        presentMode{ ChooseSwapPresentMode(swapChainSupport.presentModes) };
		VkExtent2D              extent{ ChooseSwapExtent(swapChainSupport.capabilities) };

		uint32_t imageCount{ swapChainSupport.capabilities.minImageCount + 1 };
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
			imageCount = swapChainSupport.capabilities.maxImageCount;
		QueueFamilyIndices indices{ FindQueueFamilies(context->GetPhysicalDevice()) };
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
		if (vkCreateSwapchainKHR(context->GetDevice(), &createInfo, nullptr, &swapChain) != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Failed to create swap chain.",
			});
		/* Retrieve the swap chain images to render to */
		vkGetSwapchainImagesKHR(context->GetDevice(), swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(context->GetDevice(), swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent      = extent;
		return {};
	}

	std::expected<void, Error> RendererVulkan::CreateImageViews()
	{
		swapChainImageViews.resize(swapChainImages.size());
		for (size_t i{ 0 }; i < swapChainImages.size(); ++i)
		{
			auto imageViewResult{ CreateImageView(context->GetDevice(), swapChainImages[i], swapChainImageFormat) };
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

		if (vkCreateRenderPass(context->GetDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
		{
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = "Failed to create Vulkan render pass.",
			});
		}
		return {};
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
			if (vkCreateFramebuffer(context->GetDevice(), &framebufferInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS)
				return std::unexpected(Error{
					.code    = ErrorCode::VulkanResourceCreationFailed,
					.message = "Failed to create Vulkan framebuffer.",
				});
		}
		return {};
	}

	std::expected<void, Error> RendererVulkan::CreateCommandBuffers()
	{
		VkCommandBufferAllocateInfo allocInfo{
			.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool        = context->GetCommandPool(),
			.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
		};
		if (vkAllocateCommandBuffers(context->GetDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
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
			if (vkCreateSemaphore(context->GetDevice(), &semaphoreInfo, nullptr, &frameImageAvailableSemaphores[i]) != VK_SUCCESS
			    || vkCreateFence(context->GetDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
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
			if (vkCreateSemaphore(context->GetDevice(), &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
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

	std::expected<VkPhysicalDevice, Error> RendererVulkan::SelectPhysicalDevice()
	{
		VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };
		uint32_t         deviceCount = 0;
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
		return physicalDevice;
	}

	std::expected<void, Error> RendererVulkan::CreateDeviceContext(VkPhysicalDevice physicalDevice)
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
		VkDevice       device{};
		VkResult const result{ vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) };
		if (result != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererInitializationFailed,
				.message = std::format("Failed to create Vulkan logical device (VkResult {}).", static_cast<int>(result)),
			});
		VkQueue graphicsQueue{};
		vkGetDeviceQueue(device, *indices.graphicsFamily, 0, &graphicsQueue);

		/* From here on the context owns the device */
		auto contextResult{ DeviceContextVulkan::Create(physicalDevice, device, *indices.graphicsFamily, graphicsQueue) };
		if (!contextResult)
			return std::unexpected(contextResult.error());
		context = std::move(*contextResult);
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
		} resources{ context->GetDevice() };

		uint64_t const indexBufferSizeBytes{ mesh.IndicesSizeBytes() };

		auto stagingBufferResult{ context->CreateBuffer(
			resources.stagingBuffer,
			resources.stagingMemory,
			indexBufferSizeBytes,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		) };
		if (!stagingBufferResult)
			return std::unexpected(stagingBufferResult.error());

		void*          mappedData{};
		VkResult const mapResult{ vkMapMemory(context->GetDevice(), resources.stagingMemory, 0, indexBufferSizeBytes, 0, &mappedData) };
		if (mapResult != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanResourceCreationFailed,
				.message = std::format("Failed to map staging buffer for index data (VkResult {})", static_cast<int>(mapResult)),
			});
		memcpy(mappedData, mesh.indices.data(), static_cast<size_t>(indexBufferSizeBytes));
		vkUnmapMemory(context->GetDevice(), resources.stagingMemory);

		VkBufferUsageFlagBits const usage{ static_cast<VkBufferUsageFlagBits>(
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
		) };
		auto                        indexBufferResult{ context->CreateBuffer(
			resources.buffer,
			resources.memory,
			indexBufferSizeBytes,
			usage,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		) };
		if (!indexBufferResult)
			return std::unexpected(indexBufferResult.error());
		if (auto result{ context->CopyBuffer(resources.stagingBuffer, resources.buffer, indexBufferSizeBytes) }; !result)
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

		if (vkCreateDescriptorPool(context->GetDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanResourceCreationFailed,
				.message = "Failed to create Vulkan descriptor pool.",
			});
		return {};
	}

	std::expected<void, Error> RendererVulkan::ResizeWindow(Scene& scene)
	{
		if (auto result{ RecreateSwapChain() }; !result)
			return std::unexpected(result.error());
		scene.GetCamera().UpdateProjectionMatrix(width, height);
		isWindowResized = false;
		return {};
	}

	void RendererVulkan::CleanupSwapChain()
	{
		if (!context->GetDevice())
			return;
		for (auto framebuffer : frameBuffers)
			if (framebuffer) vkDestroyFramebuffer(context->GetDevice(), framebuffer, nullptr);
		frameBuffers.clear();
		for (auto imageView : swapChainImageViews)
			if (imageView) vkDestroyImageView(context->GetDevice(), imageView, nullptr);
		swapChainImageViews.clear();
		if (renderPass) vkDestroyRenderPass(context->GetDevice(), renderPass, nullptr);
		renderPass = VK_NULL_HANDLE;
		if (swapChain) vkDestroySwapchainKHR(context->GetDevice(), swapChain, nullptr);
		swapChain = VK_NULL_HANDLE;
	}

	std::expected<void, Error> RendererVulkan::RecreateSwapChain()
	{
		vkDeviceWaitIdle(context->GetDevice());
		for (VkSemaphore semaphore : swapchainImageRenderFinishedSemaphores)
			if (semaphore) vkDestroySemaphore(context->GetDevice(), semaphore, nullptr);
		swapchainImageRenderFinishedSemaphores.clear();
		CleanupSwapChain();

		if (auto result{ CreateSwapChain() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateRenderFinishedSemaphores() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateImageViews() }; !result) return std::unexpected(result.error());
		if (auto result{ CreateRenderPass() }; !result) return std::unexpected(result.error());
		if (auto result{ depthBuffer->Create(swapChainExtent) }; !result) return std::unexpected(result.error());
		for (auto const& [id, skybox] : skyboxes)
		{
			if (auto result{ skybox->CreatePipeline(GetRenderTarget()) }; !result)
				return std::unexpected(result.error());
		}
		for (auto const& [id, model] : models)
		{
			if (auto result{ model->CreatePipeline(GetRenderTarget()) }; !result) return std::unexpected(result.error());
		}
		if (auto result{ CreateFrameBuffers() }; !result) return std::unexpected(result.error());
		return {};
	}

	RendererVulkan::~RendererVulkan() noexcept
	{
		if (context)
		{
			/* Ensure that the GPU is no longer referencing resources that are about to be
			 cleaned up by the destructor. */
			vkDeviceWaitIdle(context->GetDevice());

			CleanupSwapChain();
			depthBuffer.reset();

			for (size_t i{ 0 }; i < maxFramesInFlight; ++i)
			{
				if (frameImageAvailableSemaphores[i]) vkDestroySemaphore(context->GetDevice(), frameImageAvailableSemaphores[i], nullptr);
				if (inFlightFences[i]) vkDestroyFence(context->GetDevice(), inFlightFences[i], nullptr);
			}
			for (VkSemaphore semaphore : swapchainImageRenderFinishedSemaphores)
				if (semaphore) vkDestroySemaphore(context->GetDevice(), semaphore, nullptr);
		}

		/* destroys the associated shaders and textures */
		models.clear();
		skyboxes.clear();

		debugOverlay.reset();
		if (context && descriptorPool) vkDestroyDescriptorPool(context->GetDevice(), descriptorPool, nullptr);

		if (instance && surface) vkDestroySurfaceKHR(instance, surface, nullptr);
		/* The renderer holds the last reference: this destroys the command pool and the context->GetDevice() */
		BreakIfFalse(!context || context.use_count() == 1);
		context.reset();
		if (instance) vkDestroyInstance(instance, nullptr);
	}

	void RendererVulkan::OnWindowResized(uint32_t width, uint32_t height)
	{
		isWindowResized = true;
		width           = std::max(8u, width);
		height          = std::max(8u, height);
	}

	std::expected<void, Error> RendererVulkan::Render(Scene& scene)
	{
		vkWaitForFences(context->GetDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result{ vkAcquireNextImageKHR(
			context->GetDevice(),
			swapChain,
			UINT64_MAX,
			frameImageAvailableSemaphores[currentFrame],
			VK_NULL_HANDLE,
			&imageIndex
		) };

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			return ResizeWindow(scene);
		}
		else if (result != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererFrameFailed,
				.message = std::format("Failed to acquire swap chain image (VkResult {}).", static_cast<int>(result)),
			});

		vkResetFences(context->GetDevice(), 1, &inFlightFences[currentFrame]);

		/* Record all the commands we need to render the scene into the command list. */
		if (auto commandResult{ RecordCommandBuffer(commandBuffers[currentFrame], imageIndex, scene) }; !commandResult)
			return std::unexpected(commandResult.error());
		/* Execute the commands */
		if (auto submitResult{ SubmitCommands(imageIndex) }; !submitResult)
			return std::unexpected(submitResult.error());
		/* Present the frame and inefficiently wait for the frame to render. */
		result = Present(imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || isWindowResized)
		{
			if (auto resizeResult{ ResizeWindow(scene) }; !resizeResult)
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
		auto              shader{ std::make_unique<ShaderProgramVulkan>(context->GetDevice()) };
		if (auto const result{ shader->Initialize(vertexShaderRelativePath, fragmentShaderRelativePath) }; !result)
			return std::unexpected(result.error());
		return std::shared_ptr<ShaderProgram>{ std::move(shader) };
	}

	std::expected<std::shared_ptr<ModelVulkan>, Error> RendererVulkan::LoadModel(SceneModel const& sceneModel)
	{
		std::string const& modelRelativePath{ sceneModel.GetModelRelativePath() };
		std::string const& shaderName{ sceneModel.GetShaderName() };
		auto               shaderResult{ LoadShader(shaderName) };
		if (!shaderResult)
			return std::unexpected(shaderResult.error());
		std::shared_ptr<ShaderProgram> shader{ std::move(*shaderResult) };
		std::shared_ptr<ModelVulkan>   model{ std::make_shared<ModelVulkan>(modelRelativePath, shader, context) };
		if (auto const result{ model->Initialize(descriptorPool, GetRenderTarget()) }; !result)
			return std::unexpected(result.error());
		return model;
	}

	std::expected<std::unique_ptr<SkyboxVulkan>, Error> RendererVulkan::LoadSkybox(SceneSkybox const& sceneSkybox)
	{
		std::string const& name{ sceneSkybox.GetName() };
		std::string const& shaderName{ sceneSkybox.GetShaderName() };
		auto               shaderResult{ LoadShader(shaderName) };
		if (!shaderResult)
			return std::unexpected(shaderResult.error());
		std::shared_ptr<ShaderProgram> shader{ std::move(*shaderResult) };
		auto                           newSkybox{ std::make_unique<SkyboxVulkan>(name, shader, context) };
		if (auto result{ newSkybox->Initialize(descriptorPool, GetRenderTarget()) }; !result)
			return std::unexpected(result.error());
		return newSkybox;
	}

	std::vector<Error> RendererVulkan::LoadScene(Scene const& scene)
	{
		/* GPU models and skyboxes are kept by their scene object's id. One that fails to load has no entry
		   and is skipped when drawing. */
		BreakIfFalse(models.empty() && skyboxes.empty());
		std::vector<Error> errors;
		if (scene.GetSkybox())
		{
			auto skyboxResult{ LoadSkybox(*scene.GetSkybox()) };
			if (!skyboxResult)
				errors.push_back(skyboxResult.error());
			else
				skyboxes.emplace(scene.GetSkybox()->GetId(), std::move(*skyboxResult));
		}
		for (SceneModel const& sceneModel : scene.GetModels())
		{
			auto modelResult{ LoadModel(sceneModel) };
			if (!modelResult)
			{
				errors.push_back(modelResult.error());
				continue;
			}
			models.emplace(sceneModel.GetId(), std::move(*modelResult));
		}
		return errors;
	}

	RenderTargetVulkan RendererVulkan::GetRenderTarget() const
	{
		return RenderTargetVulkan{ .renderPass = renderPass, .extent = swapChainExtent };
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
		return vkQueuePresentKHR(context->GetGraphicsQueue(), &presentInfo);
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
		VkResult const result{ vkQueueSubmit(context->GetGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) };
		if (result != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererFrameFailed,
				.message = std::format("Failed to submit a command buffer (VkResult {}).", static_cast<int>(result)),
			});
		return {};
	}

	std::expected<void, Error> RendererVulkan::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, Scene& scene)
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

		for (SceneModel const& sceneModel : scene.GetModels())
		{
			auto const found{ models.find(sceneModel.GetId()) };
			if (found == models.end())
				continue; /* failed to load */
			found->second->Draw(commandBuffer, currentFrame, sceneModel.transform, scene.GetCamera(), scene.light);
		}

		/* After the models: the sky only fills pixels the depth test leaves at the far plane. */
		if (scene.GetSkybox())
		{
			if (auto const found{ skyboxes.find(scene.GetSkybox()->GetId()) }; found != skyboxes.end())
				found->second->Draw(commandBuffer, currentFrame, scene.GetCamera());
		}

		debugOverlay->Render(commandBuffer, *this, scene);

		vkCmdEndRenderPass(commandBuffer);
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
			return std::unexpected(Error{
				.code    = ErrorCode::VulkanRendererFrameFailed,
				.message = "Failed to finish recording Vulkan command buffer.",
			});
		return {};
	}

} // namespace gg
