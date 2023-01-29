module;
#include <memory>
#include <vulkan/vulkan.h>
export module ModelVulkan;

import Model;
import RendererVulkan;

namespace gg
{
	export struct ModelVulkan : public Model
	{
	public:
		ModelVulkan(std::unique_ptr<ShaderProgram>, std::shared_ptr<Texture>, std::shared_ptr<RendererVulkan>);
		~ModelVulkan() noexcept;

		void CreateVertexBuffers();
		VkBuffer GetVertexBuffer() const;
	private:
		void CreateVertexBuffer(Mesh const&);

		std::shared_ptr<RendererVulkan> renderer;
		VkBuffer VB{};
		VkDeviceMemory vertexBufferMemory{};
	};
} //namespace gg
