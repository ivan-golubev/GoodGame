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

Open assimp in Cmake and set the target folder to GoodGame\lib\assimp-out.
Build the assimp project in Debug and Release configurations. Make sure it is built with single-precision float.
This will generated the static libs.

WinPixEventRuntime installed as a Nuget package
see https://devblogs.microsoft.com/pix/winpixeventruntime/

Update the powershell:
winget install --id Microsoft.PowerShell --source winget

## Visual Studio configurations

The solution provides `Debug`, `Release`, and `Final` configurations for x64.

- **Debug** is for development. The C++ projects define `_DEBUG` and use the
  debug libraries. The shader build uses `-Od -Zi` for both vertex and pixel
  shaders.
- **Release** uses the non-debug runtime and defines `NDEBUG`. It enables
  whole-program optimization, function-level linking, intrinsics, and linker
  reference optimization. C++ debug information is still generated. The
  shader build uses `-Od -Zi`, so shaders remain convenient to debug.
- **Final** uses the non-debug runtime and defines `NDEBUG` and `FINAL`. It
  keeps the release-style whole-program optimization and linker optimizations,
  but disables linker debug information so the executable does not generate a
  PDB. Its shader build takes a separate path and adds `-Qstrip_debug` to
  pixel-shader compilation. Vertex shaders do not get that option in the
  current script.

`Release` and `Final` both define `NDEBUG`, so model and texture paths retained
for debugging are released after successful initialization in either
configuration. `Final` also defines `FINAL` through `props/Global.props`, so
C++ code using `IsFinal()` can distinguish it from `Release`. The solution
maps ImGUI’s `Final` configuration to its `Final|x64` project configuration.
