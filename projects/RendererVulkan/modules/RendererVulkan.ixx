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
import DebugUI_Vulkan;
import GlobalSettings;
import Input;
import Model;
import Renderer;
import SettingsRenderer;
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

		static std::shared_ptr<RendererVulkan> Get();

		RendererVulkan(RendererVulkan const&) = delete;
		RendererVulkan& operator=(RendererVulkan const&) = delete;

		RendererVulkan(RendererVulkan&&) noexcept = default;
		RendererVulkan& operator=(RendererVulkan&&) noexcept = default;

		std::shared_ptr<Camera> GetCamera() override;
		RendererType GetType() const override;

		void OnWindowResized(uint32_t width, uint32_t height) override;
		std::expected<void, Error> Render(nanoseconds deltaTime) override;
		std::expected<std::shared_ptr<ShaderProgram>, Error> LoadShader(std::string const& shaderName) override;
		std::expected<void, Error> LoadModel(std::string const& modelRelativePath, std::string const& shaderName, glm::vec3& position) override;

		VkDevice GetDevice() const;

		std::expected<VkCommandBuffer, Error> BeginSingleTimeCommands();
		std::expected<void, Error> EndSingleTimeCommands(VkCommandBuffer);

		std::expected<VkSampler, Error> CreateTextureSampler();
		std::expected<uint32_t, Error> FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags) const;

		std::expected<void, Error> CreateBuffer(
			VkBuffer& outBuffer,
			VkDeviceMemory& outMemory,
			uint64_t sizeBytes,
			VkBufferUsageFlagBits,
			VkMemoryPropertyFlags
		);

		std::expected<void, Error> CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	private:
		struct QueueFamilyIndices
		{
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;
			bool IsComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
		};

		struct SwapChainSupportDetails
		{
			VkSurfaceCapabilitiesKHR capabilities;
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> presentModes;
		};

		std::expected<void, Error> CreateVkInstance(std::vector<char const*> const& layers, std::vector<char const*> const& extensions);
		std::expected<void, Error> SelectPhysicalDevice();
		std::expected<void, Error> CreateLogicalDevice();
		std::expected<void, Error> CreateSwapChain();
		std::expected<void, Error> CreateRenderPass();
		std::expected<void, Error> CreateDescriptorSetLayout();
		std::expected<void, Error> CreateCommandPool();

		std::expected<void, Error> CreateImageViews();

		std::expected<void, Error> CreateVertexBuffer(std::shared_ptr<ModelVulkan>);
		std::expected<void, Error> CreateGraphicsPipeline(std::shared_ptr<ModelVulkan>);
		std::expected<void, Error> LoadTextures(std::shared_ptr<ModelVulkan>);
		std::expected<void, Error> CreateUniformBuffers(std::shared_ptr<ModelVulkan>);
		std::expected<void, Error> CreateDescriptorSets(std::shared_ptr<ModelVulkan>);

		std::expected<std::shared_ptr<Texture>, Error> LoadTexture(std::string const& name);

		std::expected<void, Error> CreateFrameBuffers();

		std::expected<void, Error> CreateCommandBuffers();
		std::expected<void, Error> CreateSyncObjects();
		std::expected<void, Error> CreateIndexBuffer(Mesh const&);
		std::expected<void, Error> CreateDescriptorPool();

		std::expected<void, Error> RecordCommandBuffer(VkCommandBuffer, uint32_t imageIndex);
		std::expected<void, Error> SubmitCommands();

		VkResult Present(uint32_t imageIndex);

		void CleanupSwapChain();
		std::expected<void, Error> RecreateSwapChain();
		std::expected<void, Error> ResizeWindow();

		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice const) const;
		bool IsDeviceSuitable(VkPhysicalDevice const) const;

		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice const) const;
		bool SwapChainRequirementsSatisfied(VkPhysicalDevice const) const;

		VkExtent2D ChooseSwapExtent(VkSurfaceCapabilitiesKHR const&) const;

		bool isWindowResized{ true };

		uint32_t width{};
		uint32_t height{};
		SDL_Window* windowHandleSDL{ nullptr };

		VkCommandPool commandPool{};
		std::array<VkCommandBuffer, maxFramesInFlight> commandBuffers{};
		VkRenderPass renderPass{};
		VkDescriptorSetLayout descriptorSetLayout{};

		/* Render Targets */
		std::vector<VkImage> swapChainImages;
		std::vector<VkImageView> swapChainImageViews;
		std::vector<VkFramebuffer> frameBuffers;
		VkFormat swapChainImageFormat{};
		VkExtent2D swapChainExtent{};

		uint32_t currentFrame{ 0 };

		VkDescriptorPool descriptorPool{};

		std::vector<std::shared_ptr<ModelVulkan>> models;
		std::shared_ptr<TimeManager> timeManager;
		std::shared_ptr<Camera> camera;
		std::unique_ptr<DebugUI_Vulkan> debugUI;

		VkDevice device{};
		VkInstance instance{};
		VkPhysicalDevice physicalDevice{};
		VkQueue graphicsQueue{};
		VkSurfaceKHR surface{};
		VkSwapchainKHR swapChain{};
		/* Synchronization objects */
		std::array<VkSemaphore, maxFramesInFlight> imageAvailableSemaphores{};
		std::array<VkSemaphore, maxFramesInFlight> renderFinishedSemaphores{};
		std::array<VkFence, maxFramesInFlight> inFlightFences{};
	};

} // namespace gg
