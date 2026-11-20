module;
#include <cstdint>
#include <expected>
#include <memory>
#include <vulkan/vulkan.h>
export module DeviceContextVulkan;

export import ErrorHandling;
export import GraphicsPipelineVulkan;

export namespace gg
{
	/* What a pipeline renders into. Owned by the renderer; changes when the swapchain is recreated. */
	struct RenderTargetVulkan
	{
		VkRenderPass renderPass{};
		VkExtent2D   extent{};
	};

	/* The Vulkan device and the services resources need to create and upload GPU objects.
	   Shared by the renderer and every resource: the device is destroyed only when the last owner lets go. */
	class DeviceContextVulkan
	{
	public:
		/* Takes ownership of the device and creates the command pool */
		static std::expected<std::shared_ptr<DeviceContextVulkan>, Error> Create(
			VkPhysicalDevice,
			VkDevice,
			uint32_t graphicsQueueFamily,
			VkQueue  graphicsQueue
		);

		/* Only Create() can make a PassKey */
		class PassKey
		{
			friend class DeviceContextVulkan;
			PassKey() = default;
		};
		DeviceContextVulkan(PassKey, VkPhysicalDevice, VkDevice, VkQueue graphicsQueue, VkCommandPool);
		~DeviceContextVulkan() noexcept;

		DeviceContextVulkan(DeviceContextVulkan const&)            = delete;
		DeviceContextVulkan& operator=(DeviceContextVulkan const&) = delete;
		DeviceContextVulkan(DeviceContextVulkan&&)                 = delete;
		DeviceContextVulkan& operator=(DeviceContextVulkan&&)      = delete;

		VkPhysicalDevice GetPhysicalDevice() const { return physicalDevice; }
		VkDevice         GetDevice() const { return device; }
		VkQueue          GetGraphicsQueue() const { return graphicsQueue; }
		VkCommandPool    GetCommandPool() const { return commandPool; }

		std::expected<uint32_t, Error> FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags) const;

		std::expected<void, Error> CreateBuffer(
			VkBuffer&       outBuffer,
			VkDeviceMemory& outMemory,
			uint64_t        sizeBytes,
			VkBufferUsageFlagBits,
			VkMemoryPropertyFlags
		);
		std::expected<void, Error> CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

		std::expected<VkCommandBuffer, Error> BeginSingleTimeCommands();
		std::expected<void, Error>            EndSingleTimeCommands(VkCommandBuffer);

		std::expected<VkSampler, Error> CreateTextureSampler();

		std::expected<GraphicsPipeline, Error> CreateGraphicsPipeline(
			RenderTargetVulkan const&,
			VkShaderModule vertexShader,
			VkShaderModule fragmentShader,
			VkPipelineVertexInputStateCreateInfo const&,
			VkCullModeFlags,
			VkPipelineDepthStencilStateCreateInfo const&,
			VkDescriptorSetLayout const&
		);

	private:
		VkPhysicalDevice physicalDevice{};
		VkDevice         device{};
		VkQueue          graphicsQueue{};
		VkCommandPool    commandPool{};
	};
} // namespace gg
