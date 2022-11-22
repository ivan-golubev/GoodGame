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
	export class VulkanRenderer : public Renderer {
	public:
		VulkanRenderer(uint32_t width, uint32_t height, SDL_Window*);
		~VulkanRenderer();
		void UploadGeometry(std::unique_ptr<Model>);
		void OnWindowResized(uint32_t width, uint32_t height);
		void Render(std::chrono::milliseconds deltaTime);
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

		void CreateVkInstance(std::vector<char const*> const & layers, std::vector<char const*> const & extensions);
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
		
		void RecordCommandBuffer(VkCommandBuffer, uint32_t imageIndex, XMMATRIX const & mvpMatrix);
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

		static constexpr int32_t MAX_FRAMES_IN_FLIGHT{ 2 };
		bool mWindowResized{ true };

		VkCommandPool mCommandPool{};
		std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> mCommandBuffers;
		VkRenderPass mRenderPass{};
		VkDescriptorSetLayout mDescriptorSetLayout{};
		VkPipelineLayout mPipelineLayout{};
		VkPipeline mGraphicsPipeline{};

		/* Render Targets */
		std::vector<VkImage> mSwapChainImages;
		std::vector<VkImageView> mSwapChainImageViews;
		std::vector<VkFramebuffer> mFrameBuffers;
		VkFormat mSwapChainImageFormat{};
		VkExtent2D mSwapChainExtent{};

		uint32_t mCurrentFrame{ 0 };

		/* Vertex Buffer for the cube. TODO: There is a better place for it. */
		VkBuffer mVB{};
		VkDeviceMemory mVertexBufferMemory{};

		/* Textures. TODO: move to a better place */
		VkImage mTextureImage;
		VkDeviceMemory mTextureImageMemory;
		VkImageView mTextureImageView;
		VkSampler mTextureSampler;

		std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> mUniformBuffers;
		std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> mUniformBuffersMemory;
		VkDescriptorPool mDescriptorPool;
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> mDescriptorSets;

		std::unique_ptr<Model> mModel;
		std::unique_ptr<Camera> mCamera;

		VkDevice mDevice{};
		VkInstance mInstance{};
		VkPhysicalDevice mPhysicalDevice{};
		VkQueue mGraphicsQueue{};
		VkSurfaceKHR mSurface{};
		VkSwapchainKHR mSwapChain{};
		/* Synchronization objects */
		std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> mImageAvailableSemaphores;
		std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> mRenderFinishedSemaphores;
		std::array<VkFence, MAX_FRAMES_IN_FLIGHT> mInFlightFences;
	};

} // namespace gg
