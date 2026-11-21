module;
#include <directx/d3d12.h>
#include <glm/fwd.hpp>
#include <memory>
#include <string>
#include <wrl.h>
export module ModelD3D12;

import Camera;
import DeviceContextD3D12;
import ErrorHandling;
import Lighting;
import Model;

using Microsoft::WRL::ComPtr;

namespace gg
{
	export class ModelD3D12 : public Model
	{
	public:
		ModelD3D12(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram>);

		/* Loads the meshes and textures and creates the pipeline, vertex buffer and SRV heap. All models share
		   the renderer's root signature. */
		std::expected<void, Error> Initialize(DeviceContextD3D12&, ComPtr<ID3D12RootSignature>);

		void Draw(ID3D12GraphicsCommandList*, glm::mat4x4 const& transform, Camera const&, DirectionalLight const&);

	private:
		std::expected<void, Error> CreateGraphicsPipeline(DeviceContextD3D12&);
		std::expected<void, Error> CreateVertexBuffer(DeviceContextD3D12&);
		std::expected<void, Error> CreateSRVHeap(DeviceContextD3D12&);
		std::expected<void, Error> LoadTextures(DeviceContextD3D12&);

		ComPtr<ID3D12Resource>   vertexBuffer;
		ComPtr<ID3D12Resource>   vertexBufferUpload;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

		ComPtr<ID3D12DescriptorHeap> srvHeap;
		ComPtr<ID3D12RootSignature>  rootSignature;
		ComPtr<ID3D12PipelineState>  pipelineState;
	};
} // namespace gg
