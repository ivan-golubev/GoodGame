module;
#include <expected>
#include <memory>
#include <SDL2/SDL_video.h>
#include <vulkan/vulkan_core.h>
export module DebugOverlayVulkan;

import DeviceContextVulkan;
import ErrorHandling;
import Renderer;
import Scene;

export namespace gg
{
	struct DebugOverlayVulkanCreateInfo
	{
		VkDevice                             device;
		VkInstance                           instance;
		uint32_t                             graphicsQueueFamily;
		VkQueue                              graphicsQueue;
		VkPhysicalDevice                     physicalDevice;
		VkRenderPass                         renderPass;
		SDL_Window*                          windowHandleSDL;
		std::shared_ptr<DeviceContextVulkan> context;
	};

	class DebugOverlayVulkan
	{
	public:
		~DebugOverlayVulkan() noexcept;
		std::expected<void, Error> Initialize(DebugOverlayVulkanCreateInfo const&);
		void                       Render(VkCommandBuffer, Renderer&, Scene&);

	private:
		VkDevice         device{};
		VkDescriptorPool descriptorPool{};
		bool             contextCreated{};
		bool             sdlBackendInitialized{};
		bool             vulkanBackendInitialized{};
	};

} // namespace gg
