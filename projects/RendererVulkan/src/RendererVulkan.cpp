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
#include <vector>
#include <vulkan/vulkan.h>
#include <windows.h>
#include <WinPixEventRuntime/pix3.h> 
module RendererVulkan;

import Application;
import Camera;
import DebugUI_Vulkan;
import ErrorHandling;
import ErrorHandlingVulkan;
import GlobalSettings;
import Input;
import Lighting;
import ModelVulkan;
import ShaderProgramVulkan;
import Texture;
import TextureVulkan;
import Vertex;
import VertexVulkan;

using std::chrono::nanoseconds;

namespace
{
	std::string const shaderExtensionVulkan{ "spv" };
	std::string const shadersLocation{ "shaders/spirv" };

	constexpr uint32_t maxDescriptorSets = 1000;

	constexpr std::array<char const*, 2> deviceExtensions
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_MAINTENANCE1_EXTENSION_NAME // required for using negative viewport heights
	};

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<VkSurfaceFormatKHR> const& availableFormats)
	{
		for (auto const& availableFormat : availableFormats)
			if (VK_FORMAT_B8G8R8A8_UNORM == availableFormat.format && VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == availableFormat.colorSpace)
				return availableFormat;
		return availableFormats[0];
	}

	VkPresentModeKHR chooseSwapPresentMode(std::vector<VkPresentModeKHR> const& availablePresentModes)
	{
		for (auto const& m : availablePresentModes)
			if (VK_PRESENT_MODE_MAILBOX_KHR == m)
				return m;
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	bool checkDeviceExtensionSupport(VkPhysicalDevice const device)
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
	void RendererVulkan::Initialize(RendererSettings const& rs)
	{
		width = rs.width;
		height = rs.height;
		windowHandleSDL = rs.windowHandle;
		timeManager = rs.timeManager;
		camera = std::make_shared<Camera>(rs.inputManager);
		uint32_t extension_count;
		if (!SDL_Vulkan_GetInstanceExtensions(windowHandleSDL, &extension_count, nullptr))
			throw VulkanInitException("Could not get the number of required instance extensions from SDL.");
		std::vector<char const*> extensions(extension_count);
		if (!SDL_Vulkan_GetInstanceExtensions(windowHandleSDL, &extension_count, extensions.data()))
			throw VulkanInitException("Could not get the names of required instance extensions from SDL.");

		std::vector<char const*> layers;
		if constexpr (IsDebug())
		{ /* Enable the debug layer */
			layers.emplace_back("VK_LAYER_KHRONOS_validation");
		}

		CreateVkInstance(layers, extensions);

		/* Create a surface for rendering */
		if (!SDL_Vulkan_CreateSurface(windowHandleSDL, instance, &surface))
			throw VulkanInitException("Could not create a Vulkan surface.");

		SelectPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();

		CreateDescriptorPool();
		CreateDescriptorSetLayout();

		CreateFrameBuffers();
		CreateCommandPool();

		CreateCommandBuffers();
		CreateSyncObjects();

		{ /* ImGUI */
			QueueFamilyIndices indices{ FindQueueFamilies(physicalDevice) };
			DebugUI_VulkanCreateInfo debugUI_info{
				.device = device,
				.instance = instance,
				.graphicsQueueFamily = indices.graphicsFamily.value(),
				.graphicsQueue = graphicsQueue,
				.physicalDevice = physicalDevice,
				.renderPass = renderPass,
				.windowHandleSDL = windowHandleSDL,
				.renderer = this,
			};

			debugUI = std::make_unique<DebugUI_Vulkan>();
			debugUI->Initialize(debugUI_info);
		}
	}

	RendererType RendererVulkan::GetType() const
	{
		return RendererType::Vulkan;
	}

	std::shared_ptr<Camera> RendererVulkan::GetCamera()
	{
		return camera;
	}

	std::shared_ptr<RendererVulkan> RendererVulkan::Get()
	{
		std::shared_ptr<Application> app{ Application::Get() };
		std::shared_ptr<RendererVulkan> renderer{ dynamic_pointer_cast<RendererVulkan>(app->GetRenderer()) };
		return renderer;
	}

	void RendererVulkan::CreateVkInstance(std::vector<char const*> const& layers, std::vector<char const*> const& extensions)
	{
		VkApplicationInfo appInfo{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "gg Vulkan Renderer",
			.applicationVersion = VK_HEADER_VERSION_COMPLETE,
			.pEngineName = "gg Engine",
			.engineVersion = VK_HEADER_VERSION_COMPLETE,
			.apiVersion = VK_HEADER_VERSION_COMPLETE,
		};

		VkInstanceCreateInfo instInfo{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.flags = 0,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = static_cast<uint32_t>(layers.size()),
			.ppEnabledLayerNames = layers.data(),
			.enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
			.ppEnabledExtensionNames = extensions.data(),
		};

		VkResult const result = vkCreateInstance(&instInfo, nullptr, &instance);
		if (VK_ERROR_INCOMPATIBLE_DRIVER == result)
			throw VulkanInitException("Unable to find a compatible Vulkan Driver.");
		else if (result)
			throw VulkanInitException("Could not create a Vulkan instance (for unknown reasons).");
	}

	VkExtent2D RendererVulkan::ChooseSwapExtent(VkSurfaceCapabilitiesKHR const& capabilities) const
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			return capabilities.currentExtent;

		int width, height;
		SDL_Vulkan_GetDrawableSize(windowHandleSDL, &width, &height);

		return
		{
			.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
		};
	}

	void RendererVulkan::CreateSwapChain()
	{
		SwapChainSupportDetails swapChainSupport{ QuerySwapChainSupport(physicalDevice) };
		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
			imageCount = swapChainSupport.capabilities.maxImageCount;
		QueueFamilyIndices indices{ FindQueueFamilies(physicalDevice) };
		uint32_t queueFamilyIndices[]{ indices.graphicsFamily.value(), indices.presentFamily.value() };
		bool const useConcurrentSharing = indices.graphicsFamily != indices.presentFamily;
		VkSwapchainCreateInfoKHR createInfo{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = surface,
			.minImageCount = imageCount,
			.imageFormat = surfaceFormat.format,
			.imageColorSpace = surfaceFormat.colorSpace,
			.imageExtent = extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = useConcurrentSharing ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = useConcurrentSharing ? 2U : 0U,
			.pQueueFamilyIndices = useConcurrentSharing ? queueFamilyIndices : nullptr,
			.preTransform = swapChainSupport.capabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = presentMode,
			.clipped = VK_TRUE,
		};
		if (VK_SUCCESS != vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain))
			throw VulkanInitException("Failed to create swap chain!");
		/* Retrieve the swap chain images to render to */
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
	}

	void RendererVulkan::CreateImageViews()
	{
		swapChainImageViews.resize(swapChainImages.size());
		for (size_t i{ 0 }; i < swapChainImages.size(); ++i)
			swapChainImageViews[i] = CreateImageView(device, swapChainImages[i], swapChainImageFormat);
	}

	void RendererVulkan::CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment{
			.format = swapChainImageFormat,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		};

		VkAttachmentReference colorAttachmentRef{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDependency dependency{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
		};

		VkSubpassDescription subpass{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentRef,
		};

		VkRenderPassCreateInfo renderPassInfo{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &colorAttachment,
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = 1,
			.pDependencies = &dependency,
		};

		if (VK_SUCCESS != vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass))
		{
			throw VulkanInitException("failed to create render pass!");
		}

	}

	void RendererVulkan::CreateDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding uboLayoutBindingMVP{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		};

		VkDescriptorSetLayoutBinding uboLayoutBindingDirLight{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		};

		VkDescriptorSetLayoutBinding samplerLayoutBinding{
			.binding = 2,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		};

		std::array<VkDescriptorSetLayoutBinding, 3> bindings{
			uboLayoutBindingMVP, uboLayoutBindingDirLight, samplerLayoutBinding
		};
		VkDescriptorSetLayoutCreateInfo layoutInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = static_cast<uint32_t>(bindings.size()),
			.pBindings = bindings.data(),
		};

		if (VK_SUCCESS != vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout))
			throw VulkanInitException("failed to create descriptor set layout!");
	}

	void RendererVulkan::CreateGraphicsPipeline(std::shared_ptr<ModelVulkan> model)
	{
		BreakIfFalse(model->shaderProgram.get());
		ShaderProgramVulkan* shaderProgram = dynamic_cast<ShaderProgramVulkan*>(model->shaderProgram.get());

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shaderProgram->GetVertexShader(),
			.pName = entryPointVertexShader.data(),
		};

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = shaderProgram->GetFragmentShader(),
			.pName = entryPointFragmentShader.data(),
		};

		VkPipelineShaderStageCreateInfo const shaderStages[]{ vertShaderStageInfo, fragShaderStageInfo };

		auto bindingDescription = GetVertexBindingDescription();
		auto attributeDescriptions = GetVertexAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &bindingDescription,
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
			.pVertexAttributeDescriptions = attributeDescriptions.data(),
		};

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE,
		};

		float const viewportY = flipVulkanViewport
			? static_cast<float>(swapChainExtent.height)
			: 0.0f;
		float const viewportHeight = flipVulkanViewport
			? -static_cast<float>(swapChainExtent.height)
			: static_cast<float>(swapChainExtent.height);
		VkViewport viewport{
			.x = 0.0f,
			.y = viewportY,
			.width = static_cast<float>(swapChainExtent.width),
			.height = viewportHeight,
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		VkRect2D scissor{
			.offset = { .x = 0, .y = 0 },
			.extent = swapChainExtent,
		};

		VkPipelineViewportStateCreateInfo viewportState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.pViewports = &viewport,
			.scissorCount = 1,
			.pScissors = &scissor,
		};

		VkPipelineRasterizationStateCreateInfo rasterizer{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable = VK_FALSE,
			.lineWidth = 1.0f,
		};

		VkPipelineMultisampleStateCreateInfo multisampling{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
		};

		VkPipelineColorBlendAttachmentState colorBlendAttachment{
			.blendEnable = VK_FALSE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		};

		VkPipelineColorBlendStateCreateInfo colorBlending{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_LOGIC_OP_COPY,
			.attachmentCount = 1,
			.pAttachments = &colorBlendAttachment,
		};

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 1,
			.pSetLayouts = &descriptorSetLayout,
		};

		if (VK_SUCCESS != vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &model->pipelineLayout))
			throw VulkanInitException("failed to create pipeline layout!");

		VkGraphicsPipelineCreateInfo pipelineInfo{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = 2,
			.pStages = shaderStages,
			.pVertexInputState = &vertexInputInfo,
			.pInputAssemblyState = &inputAssembly,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizer,
			.pMultisampleState = &multisampling,
			.pColorBlendState = &colorBlending,
			.layout = model->pipelineLayout,
			.renderPass = renderPass,
			.subpass = 0,
		};

		if (VK_SUCCESS != vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &model->graphicsPipeline))
		{
			throw VulkanInitException("failed to create graphics pipeline!");
		}
	}

	void RendererVulkan::CreateFrameBuffers()
	{
		frameBuffers.resize(swapChainImageViews.size());
		for (size_t i{ 0 }; i < swapChainImageViews.size(); ++i)
		{
			VkImageView attachments[]{ swapChainImageViews[i] };
			VkFramebufferCreateInfo framebufferInfo{
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass = renderPass,
				.attachmentCount = 1,
				.pAttachments = attachments,
				.width = swapChainExtent.width,
				.height = swapChainExtent.height,
				.layers = 1,
			};
			if (VK_SUCCESS != vkCreateFramebuffer(device, &framebufferInfo, nullptr, &frameBuffers[i]))
				throw VulkanInitException("failed to create framebuffer!");
		}
	}

	void RendererVulkan::CreateCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(physicalDevice);
		VkCommandPoolCreateInfo poolInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(),
		};
		if (VK_SUCCESS != vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool))
		{
			throw VulkanInitException("failed to create command pool!");
		}
	}

	void RendererVulkan::CreateCommandBuffers()
	{
		VkCommandBufferAllocateInfo allocInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = commandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
		};
		if (VK_SUCCESS != vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()))
			throw VulkanInitException("failed to allocate command buffers!");
	}

	void RendererVulkan::CreateSyncObjects()
	{
		VkSemaphoreCreateInfo semaphoreInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};

		VkFenceCreateInfo fenceInfo{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};
		for (size_t i = 0; i < maxFramesInFlight; ++i)
		{
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS
				|| vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS
				|| vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
			{
				throw VulkanInitException("failed to create semaphores!");
			}
		}
	}

	RendererVulkan::QueueFamilyIndices RendererVulkan::FindQueueFamilies(VkPhysicalDevice const device) const
	{
		uint32_t queueFamilyCount{ 0 };
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		QueueFamilyIndices indices{
			.graphicsFamily = std::nullopt,
			.presentFamily = std::nullopt,
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

	uint32_t RendererVulkan::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
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
		throw VulkanInitException("failed to find suitable memory type!");
	}

	RendererVulkan::SwapChainSupportDetails RendererVulkan::QuerySwapChainSupport(VkPhysicalDevice device) const
	{
		SwapChainSupportDetails details{
			.capabilities = {},
			.formats = {},
			.presentModes = {},
		};

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (0 != formatCount)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (0 != presentModeCount)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	bool RendererVulkan::SwapChainRequirementsSatisfied(VkPhysicalDevice const device) const
	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
		return !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	bool RendererVulkan::IsDeviceSuitable(VkPhysicalDevice const device) const
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		return VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == deviceProperties.deviceType
			&& checkDeviceExtensionSupport(device)
			&& SwapChainRequirementsSatisfied(device)
			&& FindQueueFamilies(device).IsComplete()
			&& supportedFeatures.samplerAnisotropy;
	}

	void RendererVulkan::SelectPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (0 == deviceCount)
		{
			throw VulkanInitException("failed to find GPUs with Vulkan support!");
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
		if (VK_NULL_HANDLE == physicalDevice)
		{
			throw VulkanInitException("failed to find a suitable GPU!");
		}
	}

	void RendererVulkan::CreateLogicalDevice()
	{
		QueueFamilyIndices indices{ FindQueueFamilies(physicalDevice) };
		std::set<uint32_t> uniqueQueueFamilies{ indices.graphicsFamily.value(), indices.presentFamily.value() };
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

		float queuePriority{ 1.0f };
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = queueFamily,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority,
			};
			queueCreateInfos.push_back(queueCreateInfo);
		}
		VkPhysicalDeviceFeatures enabledVk10Features{
			.samplerAnisotropy = VK_TRUE
		};
		VkPhysicalDeviceVulkan12Features enabledVk12Features{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.descriptorIndexing = true,
			.shaderSampledImageArrayNonUniformIndexing = true,
			.descriptorBindingVariableDescriptorCount = true,
			.runtimeDescriptorArray = true,
			.bufferDeviceAddress = true
		};
		VkPhysicalDeviceVulkan13Features enabledVk13Features{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			.pNext = &enabledVk12Features,
			.synchronization2 = true,
			.dynamicRendering = true,
		};
		VkDeviceCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &enabledVk13Features,
			.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
			.ppEnabledExtensionNames = deviceExtensions.data(),
			.pEnabledFeatures = &enabledVk10Features,
		};
		if (VK_SUCCESS != vkCreateDevice(physicalDevice, &createInfo, nullptr, &device))
			throw VulkanInitException("failed to create logical device!");
		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
	}

	void RendererVulkan::CreateIndexBuffer(Mesh const& mesh)
	{
		VkBuffer IB{};
		VkDeviceMemory IndexBufferMemory{};

		uint64_t const IB_sizeBytes{ mesh.IndicesSizeBytes() };

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(stagingBuffer, stagingBufferMemory, IB_sizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* mappedData;
		vkMapMemory(device, stagingBufferMemory, 0, IB_sizeBytes, 0, &mappedData);
		memcpy(mappedData, mesh.indices.data(), static_cast<size_t>(IB_sizeBytes));
		vkUnmapMemory(device, stagingBufferMemory);

		VkBufferUsageFlagBits const usage = static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		CreateBuffer(IB, IndexBufferMemory, IB_sizeBytes, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CopyBuffer(stagingBuffer, IB, IB_sizeBytes);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	void RendererVulkan::CreateUniformBuffers(std::shared_ptr<ModelVulkan> model)
	{
		VkDeviceSize const bufferSizeMVP = sizeof(ModelViewProjectionCB);
		VkDeviceSize const bufferSizeDirLight = sizeof(DirectionalLight);

		for (size_t i{ 0 }; i < maxFramesInFlight; ++i)
		{
			CreateBuffer(model->uniformBuffersMVP[i], model->uniformBuffersMemoryMVP[i], bufferSizeMVP, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			CreateBuffer(model->uniformBuffersDirLight[i], model->uniformBuffersMemoryDirLight[i], bufferSizeDirLight, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		}
	}

	void RendererVulkan::CreateDescriptorPool()
	{
		std::array<VkDescriptorPoolSize, 3> poolSizes{
			VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = maxFramesInFlight },
			VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = maxFramesInFlight },
			VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = maxFramesInFlight },
		};

		VkDescriptorPoolCreateInfo poolInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = maxFramesInFlight * maxDescriptorSets,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data(),
		};

		if (VK_SUCCESS != vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool))
			throw VulkanInitException("failed to create descriptor pool!");
	}

	void RendererVulkan::CreateDescriptorSets(std::shared_ptr<ModelVulkan> model)
	{
		std::array<VkDescriptorSetLayout, maxFramesInFlight> const layouts{ descriptorSetLayout, descriptorSetLayout };
		VkDescriptorSetAllocateInfo allocInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = maxFramesInFlight,
			.pSetLayouts = layouts.data(),
		};

		if (VK_SUCCESS != vkAllocateDescriptorSets(device, &allocInfo, model->descriptorSets.data()))
			throw VulkanInitException("failed to allocate descriptor sets!");

		BreakIfFalse(model->textures.size() > 0);
		std::shared_ptr<TextureVulkan> texture{ dynamic_pointer_cast<TextureVulkan>(model->textures[0]) };

		for (size_t i = 0; i < maxFramesInFlight; ++i)
		{
			VkDescriptorBufferInfo bufferInfoMVP{
				.buffer = model->uniformBuffersMVP[i],
				.offset = 0,
				.range = sizeof(ModelViewProjectionCB),
			};

			VkDescriptorBufferInfo bufferInfoDirLight{
				.buffer = model->uniformBuffersDirLight[i],
				.offset = 0,
				.range = sizeof(DirectionalLight),
			};

			VkDescriptorImageInfo imageInfo{
				.sampler = texture->textureSampler,
				.imageView = texture->textureImageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};

			std::array<VkWriteDescriptorSet, 3> descriptorWrites{
				VkWriteDescriptorSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = model->descriptorSets[i], .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &bufferInfoMVP },
				VkWriteDescriptorSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = model->descriptorSets[i], .dstBinding = 1, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &bufferInfoDirLight },
				VkWriteDescriptorSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = model->descriptorSets[i], .dstBinding = 2, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &imageInfo },
			};

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}

	void RendererVulkan::ResizeWindow()
	{
		RecreateSwapChain();
		camera->UpdateProjectionMatrix(width, height);
		isWindowResized = false;
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
		for (auto model : models)
		{
			if (model->graphicsPipeline)
			{
				vkDestroyPipeline(device, model->graphicsPipeline, nullptr);
				model->graphicsPipeline = VK_NULL_HANDLE;
			}
			if (model->pipelineLayout)
			{
				vkDestroyPipelineLayout(device, model->pipelineLayout, nullptr);
				model->pipelineLayout = VK_NULL_HANDLE;
			}
		}
		if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);
		renderPass = VK_NULL_HANDLE;
		if (swapChain) vkDestroySwapchainKHR(device, swapChain, nullptr);
		swapChain = VK_NULL_HANDLE;
	}

	void RendererVulkan::RecreateSwapChain()
	{
		vkDeviceWaitIdle(device);
		CleanupSwapChain();

		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();
		for (auto model : models)
			CreateGraphicsPipeline(model);
		CreateFrameBuffers();
	}

	void RendererVulkan::CreateBuffer(
		VkBuffer& outBuffer,
		VkDeviceMemory& outBufferMemory,
		uint64_t sizeBytes,
		VkBufferUsageFlagBits usage,
		VkMemoryPropertyFlags properties
	)
	{
		VkBufferCreateInfo bufferInfo{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeBytes,
			.usage = static_cast<VkBufferUsageFlags>(usage),
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		};
		if (VK_SUCCESS != vkCreateBuffer(device, &bufferInfo, nullptr, &outBuffer))
		{
			throw VulkanInitException("failed to create vertex buffer!");
		}

		/* allocate and bind memory to this buffer */
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, outBuffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memRequirements.size,
			.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties),
		};
		if (VK_SUCCESS != vkAllocateMemory(device, &allocInfo, nullptr, &outBufferMemory))
		{
			throw VulkanInitException("failed to allocate vertex buffer memory!");
		}
		vkBindBufferMemory(device, outBuffer, outBufferMemory, 0);
	}

	void RendererVulkan::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

		VkBufferCopy copyRegion{ .size = size };
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		EndSingleTimeCommands(commandBuffer);
	}

	RendererVulkan::~RendererVulkan()
	{
		if (device)
		{
			/* Ensure that the GPU is no longer referencing resources that are about to be
			 cleaned up by the destructor. */
			vkDeviceWaitIdle(device);

			CleanupSwapChain();

			for (size_t i = 0; i < maxFramesInFlight; ++i)
			{
				if (imageAvailableSemaphores[i]) vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
				if (renderFinishedSemaphores[i]) vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
				if (inFlightFences[i]) vkDestroyFence(device, inFlightFences[i], nullptr);
			}
			if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
		}

		/* destroys the associated shaders and textures */
		models.clear();

		debugUI.reset();
		if (device && descriptorPool) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		if (device && descriptorSetLayout) vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		if (instance && surface) vkDestroySurfaceKHR(instance, surface, nullptr);
		if (device) vkDestroyDevice(device, nullptr);
		if (instance) vkDestroyInstance(instance, nullptr);

		if (windowHandleSDL)
		{
			SDL_DestroyWindow(windowHandleSDL);
			SDL_Quit();
		}
	}

	void RendererVulkan::OnWindowResized(uint32_t width, uint32_t height)
	{
		isWindowResized = true;
		width = std::max(8u, width);
		height = std::max(8u, height);
	}

	VkDevice RendererVulkan::GetDevice() const { return device; }

	void RendererVulkan::Render(nanoseconds deltaTime)
	{
		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			ResizeWindow();
			return;
		}
		else if (result != VK_SUCCESS)
			throw VulkanRenderException("failed to acquire swap chain image!");

		camera->UpdateCamera(deltaTime);

		vkResetFences(device, 1, &inFlightFences[currentFrame]);

		/* Record all the commands we need to render the scene into the command list. */
		RecordCommandBuffer(commandBuffers[currentFrame], imageIndex);
		/* Execute the commands */
		SubmitCommands();
		/* Present the frame and inefficiently wait for the frame to render. */
		result = Present(imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || isWindowResized)
			ResizeWindow();
		else if (result != VK_SUCCESS)
			throw VulkanRenderException("failed to present swap chain image!");
		currentFrame = (currentFrame + 1) % maxFramesInFlight;
	}

	std::shared_ptr<ShaderProgram> RendererVulkan::LoadShader(std::string const& shaderName)
	{
		std::string const vertexShaderRelativePath{ std::format("{}/{}_VS.{}", shadersLocation, shaderName, shaderExtensionVulkan) };
		std::string const fragmentShaderRelativePath{ std::format("{}/{}_PS.{}", shadersLocation, shaderName, shaderExtensionVulkan) };
		ShaderProgramVulkan* shader = new ShaderProgramVulkan(device);
		shader->Initialize(vertexShaderRelativePath, fragmentShaderRelativePath);
		return std::shared_ptr<ShaderProgram>{ shader };
	}

	void RendererVulkan::LoadTextures(std::shared_ptr<ModelVulkan> model)
	{
		for (std::string const& textureName : model->textureNames)
			model->textures.emplace_back(LoadTexture(textureName));
	}

	std::shared_ptr<Texture> RendererVulkan::LoadTexture(std::string const& name)
	{
		std::string const textureRelativePath{ std::format("{}/{}.{}", texturesLocation, name, texturesExtension) };
		TextureVulkan* texture = new TextureVulkan(device, textureRelativePath);
		texture->Initialize();
		return std::shared_ptr<Texture>(texture);
	}

	void RendererVulkan::LoadModel(std::string const& modelRelativePath, std::string const& shaderName, glm::vec3& position)
	{
		std::shared_ptr<ShaderProgram> shader = LoadShader(shaderName);
		std::shared_ptr<ModelVulkan> model = std::make_shared<ModelVulkan>(modelRelativePath, shader, position, device);
		model->Initialize();

		CreateVertexBuffer(model);
		CreateGraphicsPipeline(model);
		LoadTextures(model);

		CreateUniformBuffers(model);
		CreateDescriptorSets(model);

		models.push_back(model);
	}

	void RendererVulkan::CreateVertexBuffer(std::shared_ptr<ModelVulkan> inputModel)
	{
		// TODO: handle multiple meshes properly, need to create multiple Vertex Buffers
		BreakIfFalse(inputModel->meshes.size() < 2);
		Mesh const& mesh{ inputModel->meshes[0] };

		uint64_t const VB_sizeBytes{ mesh.VerticesSizeBytes() };

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(stagingBuffer, stagingBufferMemory, VB_sizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* mappedData;
		vkMapMemory(device, stagingBufferMemory, 0, VB_sizeBytes, 0, &mappedData);
		memcpy(mappedData, mesh.vertices.data(), static_cast<size_t>(VB_sizeBytes));
		vkUnmapMemory(device, stagingBufferMemory);

		VkBufferUsageFlagBits const usage = static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		CreateBuffer(inputModel->VB, inputModel->vertexBufferMemory, VB_sizeBytes, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CopyBuffer(stagingBuffer, inputModel->VB, VB_sizeBytes);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	VkResult RendererVulkan::Present(uint32_t imageIndex)
	{
		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		VkSwapchainKHR swapChains[] = { swapChain };
		VkPresentInfoKHR presentInfo{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = signalSemaphores,
			.swapchainCount = 1,
			.pSwapchains = swapChains,
			.pImageIndices = &imageIndex,
		};
		return vkQueuePresentKHR(graphicsQueue, &presentInfo);
	}

	void RendererVulkan::SubmitCommands()
	{
		VkSemaphore waitSemaphores[]{ imageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = waitSemaphores,
			.pWaitDstStageMask = waitStages,
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffers[currentFrame],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = signalSemaphores,
		};
		if (VK_SUCCESS != vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]))
			throw VulkanRenderException("failed to submit a command buffer!");
	}

	void RendererVulkan::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		vkResetCommandBuffer(commandBuffer, 0);

		VkCommandBufferBeginInfo beginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		};
		if (VK_SUCCESS != vkBeginCommandBuffer(commandBuffer, &beginInfo))
			throw VulkanRenderException("failed to begin recording command buffer!");

		VkClearValue clearColor{
			.color = { .float32 = { 0.0f, 0.2f, 0.4f, 1.0f } },
		};
		VkRenderPassBeginInfo renderPassInfo{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = renderPass,
			.framebuffer = frameBuffers[imageIndex],
			.renderArea = { .offset = { .x = 0, .y = 0 }, .extent = swapChainExtent },
			.clearValueCount = 1,
			.pClearValues = &clearColor,
		};

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		for (std::shared_ptr<ModelVulkan> model : models)
		{
			{  /* submit the UBO data - MVP matrix */
				ModelViewProjectionCB mvpMatrices{
					.MVP = CalculateMVP(model->translation, timeManager->GetCurrentTimeSec(), *camera),
					.ViewPosition = camera->GetPositionVec4(),
				};
				void* data;
				vkMapMemory(device, model->uniformBuffersMemoryMVP[currentFrame], 0, sizeof(ModelViewProjectionCB), 0, &data);
				memcpy(data, &mvpMatrices, sizeof(ModelViewProjectionCB));
				vkUnmapMemory(device, model->uniformBuffersMemoryMVP[currentFrame]);
			}
			{  /* submit the UBO data - Directional Light */
				void* data;
				vkMapMemory(device, model->uniformBuffersMemoryDirLight[currentFrame], 0, sizeof(DirectionalLight), 0, &data);
				memcpy(data, &globalDirectionalLight, sizeof(DirectionalLight));
				vkUnmapMemory(device, model->uniformBuffersMemoryDirLight[currentFrame]);
			}

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, model->graphicsPipeline);
			/* bind a desciptor for the UBO */
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, model->pipelineLayout, 0, 1, &model->descriptorSets[currentFrame], 0, nullptr);

			VkBuffer vertexBuffers[]{ model->VB };
			VkDeviceSize offsets[]{ 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

			for (auto& m : model->meshes)
				vkCmdDraw(commandBuffer, m.GetVertexCount(), 1, 0, 0);
		}

		debugUI->Render(commandBuffer);

		vkCmdEndRenderPass(commandBuffer);
		if (VK_SUCCESS != vkEndCommandBuffer(commandBuffer))
			throw VulkanRenderException("failed to record command buffer!");
	}

	VkCommandBuffer RendererVulkan::BeginSingleTimeCommands()
	{
		VkCommandBufferAllocateInfo allocInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = commandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		};

		vkBeginCommandBuffer(commandBuffer, &beginInfo);
		return commandBuffer;
	}

	void RendererVulkan::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
	{
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
		};

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}

	VkSampler RendererVulkan::CreateTextureSampler()
	{
			VkPhysicalDeviceProperties properties{ .apiVersion = 0 };
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		VkSamplerCreateInfo samplerInfo{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.mipLodBias = 0.0f,
			.anisotropyEnable = VK_TRUE,
			.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
			.compareEnable = VK_FALSE,
			.compareOp = VK_COMPARE_OP_ALWAYS,
			.minLod = 0.0f,
			.maxLod = 0.0f,
			.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
			.unnormalizedCoordinates = VK_FALSE,
		};
		VkSampler textureSampler{};
		if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
			throw VulkanInitException("failed to create texture sampler!");
		return textureSampler;
	}

} // namespace gg
