module;
#include <directx/d3d12.h>
#include <memory>
#include <string>
#include <wrl.h>
export module TextureD3D12;

import Texture;

using Microsoft::WRL::ComPtr;

export namespace gg
{
	class TextureD3D12 : public Texture
	{
	public:
		TextureD3D12(std::string const& textureRelativePath);

	private:
		ComPtr<ID3D12Resource> texture_CPU_Resource;
		ComPtr<ID3D12Resource> texture_GPU_Resource;

		friend class RendererD3D12;
	};

}