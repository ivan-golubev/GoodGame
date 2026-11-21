module;
#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <optional>
#include <SDL2/SDL_video.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>
export module RendererVulkan;

import Camera;
import DebugOverlayVulkan;
import DeviceContextVulkan;
import DepthBufferVulkan;
import GlobalSettings;
import GraphicsPipelineVulkan;
import Input;
import Model;
import Renderer;
import ObjectId;
import SettingsRenderer;
import ShaderProgram;
import SkyboxVulkan;
import Texture;
import TimeManager;
import Vertex;

namespace gg
{
	struct ModelVulkan;

	export class RendererVulkan : public Renderer
	{
	public:
		RendererVulkan() = default;
		~RendererVulkan() noexcept;
		std::expected<void, Error> Initialize(RendererSettings const&) override;

		RendererVulkan(RendererVulkan const&)            = delete;
		RendererVulkan& operator=(RendererVulkan const&) = delete;

		RendererVulkan(RendererVulkan&&)            = delete;
		RendererVulkan& operator=(RendererVulkan&&) = delete;

		RendererType GetType() const override;

		void                       OnWindowResized(uint32_t width, uint32_t height) override;
		std::expected<void, Error> Render(Scene&) override;

		std::vector<Error> LoadScene(Scene const&) override;

	private:
		std::expected<std::shared_ptr<ShaderProgram>, Error> LoadShader(std::string const& shaderName);
		std::expected<std::shared_ptr<ModelVulkan>, Error>   LoadModel(SceneModel const&);
		std::expected<std::unique_ptr<SkyboxVulkan>, Error>  LoadSkybox(SceneSkybox const&);

		struct QueueFamilyIndices
		{
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;
			bool                    IsComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
		};

		struct SwapChainSupportDetails
		{
			VkSurfaceCapabilitiesKHR        capabilities;
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR>   presentModes;
		};

		std::expected<void, Error> CreateVkInstance(
			std::vector<char const*> const& layers,
			std::vector<char const*> const& extensions
		);
		std::expected<VkPhysicalDevice, Error> SelectPhysicalDevice();
		std::expected<void, Error>             CreateDeviceContext(VkPhysicalDevice);
		std::expected<void, Error>             CreateSwapChain();
		std::expected<void, Error>             CreateRenderPass();

		std::expected<void, Error> CreateImageViews();

		std::expected<void, Error> CreateFrameBuffers();

		std::expected<void, Error> CreateCommandBuffers();
		std::expected<void, Error> CreateSyncObjects();
		std::expected<void, Error> CreateRenderFinishedSemaphores();
		std::expected<void, Error> CreateIndexBuffer(Mesh const&);
		std::expected<void, Error> CreateDescriptorPool();

		std::expected<void, Error> RecordCommandBuffer(VkCommandBuffer, uint32_t imageIndex, Scene&);
		std::expected<void, Error> SubmitCommands(uint32_t imageIndex);

		VkResult           Present(uint32_t imageIndex);
		RenderTargetVulkan GetRenderTarget() const;

		void                       CleanupSwapChain();
		std::expected<void, Error> RecreateSwapChain();
		std::expected<void, Error> ResizeWindow(Scene&);

		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice const) const;
		bool               IsDeviceSuitable(VkPhysicalDevice const) const;

		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice const) const;
		bool                    SwapChainRequirementsSatisfied(VkPhysicalDevice const) const;

		VkExtent2D ChooseSwapExtent(VkSurfaceCapabilitiesKHR const&) const;

		bool isWindowResized{ true };

		uint32_t    width{};
		uint32_t    height{};
		SDL_Window* windowHandleSDL{ nullptr };

		std::array<VkCommandBuffer, maxFramesInFlight> commandBuffers{};
		VkRenderPass                                   renderPass{};

		/* Render Targets */
		std::vector<VkImage>       swapChainImages;
		std::vector<VkImageView>   swapChainImageViews;
		std::vector<VkFramebuffer> frameBuffers;
		VkFormat                   swapChainImageFormat{};
		VkExtent2D                 swapChainExtent{};

		std::unique_ptr<DepthBufferVulkan> depthBuffer;

		uint32_t currentFrame{ 0 };

		VkDescriptorPool descriptorPool{};

		std::unordered_map<ObjectId, std::shared_ptr<ModelVulkan>>  models; /* by scene object id */
		std::unique_ptr<DebugOverlayVulkan>                         debugOverlay;
		std::unordered_map<ObjectId, std::unique_ptr<SkyboxVulkan>> skyboxes; /* by scene object id */

		std::shared_ptr<DeviceContextVulkan> context;
		VkInstance                           instance{};
		VkSurfaceKHR                         surface{};
		VkSwapchainKHR                       swapChain{};
		/* Synchronization objects */
		std::array<VkSemaphore, maxFramesInFlight> frameImageAvailableSemaphores{};
		std::vector<VkSemaphore>                   swapchainImageRenderFinishedSemaphores;
		std::array<VkFence, maxFramesInFlight>     inFlightFences{};
	};

} // namespace gg
