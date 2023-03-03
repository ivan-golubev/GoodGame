module;
#include <array>
#include <chrono>
#include <cstdint>
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

using std::chrono::nanoseconds;

namespace gg
{
	struct ModelVulkan;

	export class RendererVulkan : public Renderer
	{
	public:
		RendererVulkan(RendererSettings const&);
		~RendererVulkan();

		static std::shared_ptr<RendererVulkan> Get();

		RendererVulkan(RendererVulkan const&) = delete;
		RendererVulkan& operator=(RendererVulkan const&) = delete;

		RendererVulkan(RendererVulkan&&) noexcept = default;
		RendererVulkan& operator=(RendererVulkan&&) noexcept = default;

		std::shared_ptr<Camera> GetCamera() override;
		RendererType GetType() const override;

		void OnWindowResized(uint32_t width, uint32_t height) override;
		void Render(nanoseconds deltaTime) override;
		std::shared_ptr<ShaderProgram> LoadShader(std::string const& shaderName) override;
		void LoadModel(std::string const& modelRelativePath, std::string const& shaderName, glm::vec3& position) override;
		void LoadSkybox(std::string const& name) override;

		VkDevice GetDevice() const;

		VkCommandBuffer BeginSingleTimeCommands();
		void EndSingleTimeCommands(VkCommandBuffer);

		VkSampler CreateTextureSampler();
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags) const;

		void CreateBuffer(
			VkBuffer& outBuffer,
			VkDeviceMemory& outMemory,
			uint64_t sizeBytes,
			VkBufferUsageFlagBits,
			VkMemoryPropertyFlags
		);

		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

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

		void CreateVkInstance(std::vector<char const*> const& layers, std::vector<char const*> const& extensions);
		void SelectPhysicalDevice();
		void CreateLogicalDevice();
		void CreateSwapChain();
		void CreateRenderPass();
		void CreateDescriptorSetLayout();
		void CreateCommandPool();

		void CreateImageViews();

		void CreateVertexBuffer(std::shared_ptr<ModelVulkan>);
		void CreateGraphicsPipeline(std::shared_ptr<ModelVulkan>);
		void LoadTextures(std::shared_ptr<ModelVulkan>);
		void CreateUniformBuffers(std::shared_ptr<ModelVulkan>);
		void CreateDescriptorSets(std::shared_ptr<ModelVulkan>);

		std::shared_ptr<Texture> LoadTexture(std::string const& name);

		void CreateFrameBuffers();

		void CreateCommandBuffers();
		void CreateSyncObjects();
		void CreateIndexBuffer(Mesh const&);
		void CreateDescriptorPool();

		void RecordCommandBuffer(VkCommandBuffer, uint32_t imageIndex);
		void SubmitCommands();

		VkResult Present(uint32_t imageIndex);

		void CleanupSwapChain();
		void RecreateSwapChain();
		void ResizeWindow();

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
		std::array<VkCommandBuffer, maxFramesInFlight> commandBuffers;
		VkRenderPass renderPass{};
		VkDescriptorSetLayout descriptorSetLayout{};

		/* Render Targets */
		std::vector<VkImage> swapChainImages;
		std::vector<VkImageView> swapChainImageViews;
		std::vector<VkFramebuffer> frameBuffers;
		VkFormat swapChainImageFormat{};
		VkExtent2D swapChainExtent{};

		uint32_t currentFrame{ 0 };

		VkDescriptorPool descriptorPool;

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
		std::array<VkSemaphore, maxFramesInFlight> imageAvailableSemaphores;
		std::array<VkSemaphore, maxFramesInFlight> renderFinishedSemaphores;
		std::array<VkFence, maxFramesInFlight> inFlightFences;
	};

} // namespace gg
