module;
#include <array>
#include <cstdint>
#include <chrono>
#include <DirectXMath.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <SDL2/SDL_video.h>
#include <vulkan/vulkan.h>
export module RendererVulkan;

import Camera;
import Input;
import Vertex;
import TimeManager;
import Model;
import Renderer;
import Texture;

using DirectX::XMMATRIX;
using DirectX::XMVECTOR;
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

		void OnWindowResized(uint32_t width, uint32_t height) override;
		void Render(nanoseconds deltaTime) override;
		std::unique_ptr<ShaderProgram> LoadShader(std::string const& shaderName) override;
		void LoadModel(std::string const& modelRelativePath, std::unique_ptr<ShaderProgram>, XMVECTOR& position) override;

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
		void CreateGraphicsPipeline();
		void CreateFrameBuffers();
		void CreateCommandPool();

		void CreateImageViews();
		std::shared_ptr<Texture> LoadTexture(std::string const& textureRelativePath);

		void CreateVertexBuffer(std::shared_ptr<ModelVulkan>);

		void CreateCommandBuffers();
		void CreateSyncObjects();
		void CreateIndexBuffer(Mesh const&);
		void CreateUniformBuffers();
		void CreateDescriptorPool();
		void CreateDescriptorSets(VkImageView, VkSampler);

		void RecordCommandBuffer(VkCommandBuffer, uint32_t imageIndex, XMMATRIX const& mvpMatrix);
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

		static constexpr int32_t maxFramesInFlight{ 2 };
		bool isWindowResized{ true };

		uint32_t width{};
		uint32_t height{};
		SDL_Window* windowHandleSDL{ nullptr };

		VkCommandPool commandPool{};
		std::array<VkCommandBuffer, maxFramesInFlight> commandBuffers;
		VkRenderPass renderPass{};
		VkDescriptorSetLayout descriptorSetLayout{};
		VkPipelineLayout pipelineLayout{};
		VkPipeline graphicsPipeline{};

		/* Render Targets */
		std::vector<VkImage> swapChainImages;
		std::vector<VkImageView> swapChainImageViews;
		std::vector<VkFramebuffer> frameBuffers;
		VkFormat swapChainImageFormat{};
		VkExtent2D swapChainExtent{};

		uint32_t currentFrame{ 0 };

		std::array<VkBuffer, maxFramesInFlight> uniformBuffers;
		std::array<VkDeviceMemory, maxFramesInFlight> uniformBuffersMemory;
		VkDescriptorPool descriptorPool;
		std::array<VkDescriptorSet, maxFramesInFlight> descriptorSets;

		std::shared_ptr<ModelVulkan> model;
		std::shared_ptr<TimeManager> timeManager;
		std::unique_ptr<Camera> camera;

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
