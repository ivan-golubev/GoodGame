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
import SettingsRenderer;
import SkyboxVulkan;
import Texture;
import TimeManager;
import Vertex;

using std::chrono::nanoseconds;

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

		std::shared_ptr<Camera> GetCamera() override;
		RendererType            GetType() const override;

		void                                                 OnWindowResized(uint32_t width, uint32_t height) override;
		std::expected<void, Error>                           Render(nanoseconds deltaTime) override;
		std::expected<std::shared_ptr<ShaderProgram>, Error> LoadShader(std::string const& shaderName) override;

		std::expected<void, Error> LoadModel(
			std::string const& modelRelativePath,
			std::string const& shaderName,
			glm::vec3&         position
		) override;

		std::expected<void, Error> LoadSkybox(std::string const& name, std::string const& shaderName) override;

	private:
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

		std::expected<void, Error> RecordCommandBuffer(VkCommandBuffer, uint32_t imageIndex);
		std::expected<void, Error> SubmitCommands(uint32_t imageIndex);

		VkResult           Present(uint32_t imageIndex);
		RenderTargetVulkan GetRenderTarget() const;

		void                       CleanupSwapChain();
		std::expected<void, Error> RecreateSwapChain();
		std::expected<void, Error> ResizeWindow();

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

		std::vector<std::shared_ptr<ModelVulkan>> models;
		std::shared_ptr<TimeManager>              timeManager;
		std::shared_ptr<Camera>                   camera;
		std::unique_ptr<DebugOverlayVulkan>       debugOverlay;
		std::unique_ptr<SkyboxVulkan>             skybox;

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
