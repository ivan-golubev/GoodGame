module;
#include <directx/d3d12.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string>
#include <wrl.h>
export module ModelD3D12;

import Model;

using Microsoft::WRL::ComPtr;

namespace gg
{
	export struct ModelD3D12 : public Model
	{
	public:
		ModelD3D12(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram>, glm::vec3& position);

	private:
		ComPtr<ID3D12Resource> VB_GPU_Resource;
		ComPtr<ID3D12Resource> VB_CPU_Resource;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

		ComPtr<ID3D12DescriptorHeap> srvHeap;
		ComPtr<ID3D12PipelineState> pipelineState;

		friend class RendererD3D12;
	};
} // namespace gg
