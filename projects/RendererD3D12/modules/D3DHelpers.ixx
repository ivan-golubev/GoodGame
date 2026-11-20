module;
#include <directx/d3d12.h>
#include <dxgi1_6.h>
#include <string>
#include <wrl.h>
export module D3DHelpers;

export namespace gg
{

	/* Returns the first available hardware Direct3D 12 adapter
	   or nullptr if none can be found. */
	Microsoft::WRL::ComPtr<IDXGIAdapter1> GetHardwareAdapter(IDXGIFactory1* factory, bool requestHighPerformanceAdapter = false);

	void         SetName(ID3D12Object*, std::wstring const&);
	std::wstring GetName(ID3D12Object*);

} // namespace gg
