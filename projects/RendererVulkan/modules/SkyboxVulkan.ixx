module;
#include <string>
#include <vulkan/vulkan_core.h>
export module SkyboxVulkan;

import Skybox;

export namespace gg
{
	class SkyboxVulkan : public Skybox
	{
	public:
		SkyboxVulkan(std::string const& name, VkDevice);
		~SkyboxVulkan() noexcept override;

		VkImageView textureImageView;
		VkSampler textureSampler;

	private:
		void CreateTextureImage();

		VkDevice device;
		VkImage textureImage;
		VkDeviceMemory textureImageMemory;
	};
} // namespace gg
