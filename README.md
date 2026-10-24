Vulkan/D3D12 renderer.
Features implemented:
- reading 3d models (.glb) and textures from files
- rendering in Vulkan / D3D12 (press F1 to switch between renderers)
- camera control
- directional light and Blinn–Phong reflection
- ImGUI to control the camera and lights

Install the official Vulkan SDK:
https://vulkan.lunarg.com/

DirectX headers taken from https://github.com/microsoft/DirectX-Headers/tree/main/include/directx
and are stored in Direct3D12Renderer/DirectX-Headers in the repository.

WinPixEventRuntime installed as a Nuget package
see https://devblogs.microsoft.com/pix/winpixeventruntime/

Update the powershell:
winget install --id Microsoft.PowerShell --source winget
