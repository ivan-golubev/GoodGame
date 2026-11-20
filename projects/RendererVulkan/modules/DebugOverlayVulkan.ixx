module;
#include <memory>
#include <expected>
#include <SDL2/SDL_video.h>
#include <vulkan/vulkan_core.h>
export module DebugOverlayVulkan;

import ErrorHandling;

export namespace gg
{
	class RendererVulkan;

	struct DebugOverlayVulkanCreateInfo
	{
		VkDevice device;
		VkInstance instance;
		uint32_t graphicsQueueFamily;
		VkQueue graphicsQueue;
		VkPhysicalDevice physicalDevice;
		VkRenderPass renderPass;
		SDL_Window* windowHandleSDL;
		RendererVulkan* renderer;
	};

	class DebugOverlayVulkan
	{
	public:
		~DebugOverlayVulkan() noexcept;
		std::expected<void, Error> Initialize(DebugOverlayVulkanCreateInfo const&);
		void Render(VkCommandBuffer);

	private:
		VkDevice device{};
		VkDescriptorPool descriptorPool{};
		bool contextCreated{};
		bool sdlBackendInitialized{};
		bool vulkanBackendInitialized{};
	};

} //namespace gg
