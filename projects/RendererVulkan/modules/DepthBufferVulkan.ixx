module;
#include <expected>
#include <vulkan/vulkan.h>
export module DepthBufferVulkan;

import ErrorHandling;

export namespace gg
{
	/* Depth buffer: a depth-only image used as the render pass depth attachment.
	   Sized like the swap chain; call Create() again when the swap chain is recreated. */
	class DepthBufferVulkan
	{
	public:
		DepthBufferVulkan(VkDevice, VkPhysicalDevice, VkFormat);
		~DepthBufferVulkan() noexcept;
		DepthBufferVulkan(DepthBufferVulkan const&)            = delete;
		DepthBufferVulkan& operator=(DepthBufferVulkan const&) = delete;

		/* Picks the best depth format the device supports as an optimal-tiling depth attachment. */
		static std::expected<VkFormat, Error> FindFormat(VkPhysicalDevice);

		/* (Re)creates the image for the given size, releasing the previous one. The GPU must be idle. */
		std::expected<void, Error> Create(VkExtent2D);

		VkFormat    GetFormat() const;
		VkImageView GetView() const;

	private:
		void                           Destroy();
		std::expected<uint32_t, Error> FindMemoryType(uint32_t typeFilter) const;

		VkDevice         device{};
		VkPhysicalDevice physicalDevice{};
		VkFormat         format{};

		VkImage        image{};
		VkDeviceMemory memory{};
		VkImageView    view{};
	};
} // namespace gg
