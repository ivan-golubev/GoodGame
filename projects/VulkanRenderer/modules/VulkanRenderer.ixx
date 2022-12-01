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
export module VulkanRenderer;

import Camera;
import Input;
import Vertex;
import TimeManager;
import Model;
import Renderer;

using DirectX::XMMATRIX;

namespace gg
{
	export class VulkanRenderer : public Renderer
	{
	public:
		VulkanRenderer(RendererSettings const&);
		~VulkanRenderer();

		VulkanRenderer(VulkanRenderer const&) = delete;
		VulkanRenderer& operator=(VulkanRenderer const&) = delete;

		VulkanRenderer(VulkanRenderer&&) noexcept = default;
		VulkanRenderer& operator=(VulkanRenderer&&) noexcept = default;

		void UploadGeometry(std::unique_ptr<Model>) override;
		void OnWindowResized(uint32_t width, uint32_t height) override;
		void Render(std::chrono::milliseconds deltaTime) override;
		VkDevice GetDevice() const;
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
		void CreateTextureImage();
		void CreateImage(uint32_t width, uint32_t height, VkFormat, VkImageTiling, VkImageUsageFlags, VkMemoryPropertyFlags, VkImage&, VkDeviceMemory&);
		VkImageView CreateImageView(VkImage, VkFormat);
		void CreateTextureImageView();
		void CreateTextureSampler();

		void CreateCommandBuffers();
		void CreateSyncObjects();
		void CreateVertexBuffer(Mesh const&);
		void CreateIndexBuffer(Mesh const&);
		void CreateUniformBuffers();
		void CreateDescriptorPool();
		void CreateDescriptorSets();

		void RecordCommandBuffer(VkCommandBuffer, uint32_t imageIndex, XMMATRIX const& mvpMatrix);
		VkCommandBuffer BeginSingleTimeCommands();
		void EndSingleTimeCommands(VkCommandBuffer);
		void SubmitCommands();

		void TransitionImageLayout(VkImage, VkFormat, VkImageLayout oldLayout, VkImageLayout newLayout);
		void CopyBufferToImage(VkBuffer, VkImage, uint32_t width, uint32_t height);

		VkResult Present(uint32_t imageIndex);

		void CleanupSwapChain();
		void RecreateSwapChain();
		void ResizeWindow();

		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice const) const;
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags) const;
		bool IsDeviceSuitable(VkPhysicalDevice const) const;

		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice const) const;
		bool SwapChainRequirementsSatisfied(VkPhysicalDevice const) const;

		VkExtent2D ChooseSwapExtent(VkSurfaceCapabilitiesKHR const&) const;

		VkShaderModule createShaderModule(std::vector<char> const& shaderBlob);
		void CreateBuffer(
			VkBuffer& outBuffer,
			VkDeviceMemory& outMemory,
			uint64_t sizeBytes,
			VkBufferUsageFlagBits usage,
			VkMemoryPropertyFlags properties
		);

		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

		static constexpr int32_t maxFramesInFlight{ 2 };
		bool isWindowResized{ true };

		uint32_t width{};
		uint32_t height{};
		SDL_Window* windowHandle{ nullptr };

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

		/* Vertex Buffer for the cube. TODO: There is a better place for it. */
		VkBuffer VB{};
		VkDeviceMemory vertexBufferMemory{};

		/* Textures. TODO: move to a better place */
		VkImage textureImage;
		VkDeviceMemory textureImageMemory;
		VkImageView textureImageView;
		VkSampler textureSampler;

		std::array<VkBuffer, maxFramesInFlight> uniformBuffers;
		std::array<VkDeviceMemory, maxFramesInFlight> uniformBuffersMemory;
		VkDescriptorPool descriptorPool;
		std::array<VkDescriptorSet, maxFramesInFlight> descriptorSets;

		std::unique_ptr<Model> model;
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
