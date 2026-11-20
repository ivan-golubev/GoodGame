module;
#include <directx/d3d12.h>
#include <expected>
#include <memory>
#include <string>
#include <wrl.h>
export module SkyboxD3D12;

import Camera;
import ErrorHandling;
import ShaderProgram;
import Skybox;

using Microsoft::WRL::ComPtr;

export namespace gg
{
	class SkyboxD3D12 : public Skybox
	{
	public:
		SkyboxD3D12(std::string const& name, std::shared_ptr<ShaderProgram>, ID3D12Device*);
		~SkyboxD3D12() noexcept override = default;

		std::expected<void, Error> Initialize();
		void                       Draw(ID3D12GraphicsCommandList*, Camera const&);

	private:
		std::expected<void, Error> CreateCubeTexture();
		std::expected<void, Error> CreateRootSignature();
		std::expected<void, Error> CreatePipeline();

		std::string          name;
		ComPtr<ID3D12Device> device;

		ComPtr<ID3D12DescriptorHeap> srvHeap; /* shader-visible, holds the cube map SRV */
		ComPtr<ID3D12Resource>       cubeTexture; /* 2D texture array with one slice per face */

		ComPtr<ID3D12RootSignature> rootSignature;
		ComPtr<ID3D12PipelineState> pipelineState;
	};
} // namespace gg
