module;
#include <directx/d3d12.h>
#include <memory>
#include <string>
#include <wrl.h>
export module TextureD3D12;

import DeviceContextD3D12;
export import ErrorHandling;
import Texture;

using Microsoft::WRL::ComPtr;

export namespace gg
{
	class TextureD3D12 : public Texture
	{
	public:
		explicit TextureD3D12(std::string const& textureRelativePath);

		/* Loads the image, uploads it into a new default-heap texture and writes its SRV to srvHandle */
		std::expected<void, Error> Initialize(DeviceContextD3D12&, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle);

	private:
		ComPtr<ID3D12Resource> texture;
	};

}
