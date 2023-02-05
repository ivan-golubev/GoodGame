module;
#include <directx/d3d12.h>
#include <memory>
#include <string>
#include <DirectXMath.h>
#include <wrl.h>
export module ModelD3D12;

import Model;

using Microsoft::WRL::ComPtr;
using DirectX::XMVECTOR;

namespace gg
{
	export struct ModelD3D12 : public Model
	{
	public:
		ModelD3D12(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram>, XMVECTOR& position);

	private:
		ComPtr<ID3D12Resource> VB_GPU_Resource;
		ComPtr<ID3D12Resource> VB_CPU_Resource;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

		ComPtr<ID3D12PipelineState> pipelineState;

		friend class RendererD3D12;
	};
} // namespace gg
