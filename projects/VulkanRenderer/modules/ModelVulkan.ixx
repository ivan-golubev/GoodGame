module;
#include <memory>
#include <vulkan/vulkan.h>
export module ModelVulkan;

import Model;

namespace gg
{
	export struct ModelVulkan : public Model
	{
	public:
		ModelVulkan(std::unique_ptr<ShaderProgram>, std::shared_ptr<Texture>, VkDevice);
		~ModelVulkan() noexcept;

		void CreateVertexBuffers();
		VkBuffer GetVertexBuffer() const;
	private:
		void CreateVertexBuffer(Mesh const&);

		VkDevice device;
		VkBuffer VB{};
		VkDeviceMemory vertexBufferMemory{};
	};
} //namespace gg
