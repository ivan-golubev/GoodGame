module;
#include <directx/d3d12.h>
#include <memory>
#include <wrl.h>
export module ModelD3D12;

import Model;

using Microsoft::WRL::ComPtr;

namespace gg
{
	export struct ModelD3D12 : public Model
	{
	public:
		ModelD3D12(std::unique_ptr<ShaderProgram>, std::shared_ptr<Texture>);
		~ModelD3D12() noexcept;

		//void CreateVertexBuffers();
		//VkBuffer GetVertexBuffer() const;
	private:
		//void CreateVertexBuffer(Mesh const&);

		ComPtr<ID3D12Resource> VB_GPU_Resource;
		ComPtr<ID3D12Resource> VB_CPU_Resource;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

		ComPtr<ID3D12Resource> IB_GPU_Resource;
		ComPtr<ID3D12Resource> IB_CPU_Resource;
		D3D12_INDEX_BUFFER_VIEW indexBufferView;
		uint32_t indexCount;

		// TODO: check that this is indeed needed - use accessor methods instead ?
		friend class RendererD3D12;
	};
} // namespace gg
