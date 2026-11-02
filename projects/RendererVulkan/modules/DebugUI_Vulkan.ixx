module;
#include <memory>
#include <SDL2/SDL_video.h>
#include <vulkan/vulkan_core.h>
export module DebugUI_Vulkan;

export namespace gg
{
	class RendererVulkan;

	struct DebugUI_VulkanCreateInfo
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

	class DebugUI_Vulkan
	{
	public:
		~DebugUI_Vulkan() noexcept;
		void Initialize(DebugUI_VulkanCreateInfo const&);
		void Render(VkCommandBuffer);

	private:
		VkDevice device{};
		VkDescriptorPool descriptorPool{};
		bool contextCreated{};
		bool sdlBackendInitialized{};
		bool vulkanBackendInitialized{};
	};

} //namespace gg
