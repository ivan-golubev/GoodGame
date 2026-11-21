# GG Engine conventions

These are the rules the code follows. Most of them are checked automatically: formatting by `.clang-format`, naming and several of the rules below by `.clang-tidy`, and the rest by `tools/check_conventions.py`. Run all three with `python tools/code_checks.py`.

## C++

| Kind | Rule | Examples |
|---|---|---|
| Types, classes, structs, enums | PascalCase | `RendererD3D12`, `RendererSettings`, `ErrorCode`, `SkyboxCamera`, `ModelViewProjection` |
| Enumerators | PascalCase | `ErrorCode::ModelImportFailed`, `DebugLevel::Info` |
| Member functions | PascalCase | `UpdateCamera`, `GetViewMatrix`, `CreateSyncObjects` |
| Free functions, including helpers in unnamed namespaces | PascalCase | `CalculateModelViewProjection`, `BreakIfFalse`, `MainLoop`, `ReadMesh`, `CreateShaderModule` |
| Data members | camelCase, no prefix or suffix | `cameraPosition`, `commandQueue`, `isWindowResized` |
| Locals and parameters | camelCase | `deltaTimeSeconds`, `hardwareAdapter` |
| `constexpr` constants | camelCase | `cameraMoveSpeed`, `maxFramesInFlight`, `uvSetNumber` |
| Macros | UPPER_SNAKE | `D3D12_CHECK_HRESULT` |
| Error codes | Backend prefix | `D3D12PipelineCreationFailed`, `VulkanRendererInitializationFailed`, `ModelImportFailed` |
| Handle members | Type word as suffix | `windowHandleSDL`, `renderTargetViewHeap`, `dsvHandle` |
| GPU object debug names (`SetName`) | `Kind_Purpose_Owner` | `PSO_Skybox_{}`, `RootSignature_Skybox_{}`, `SRV_DescriptorHeap_Skybox_{}` |

C++ types that mirror a constant buffer have **no** suffix. Examples: `SkyboxCamera`, `ModelViewProjection`, `DirectionalLight`.

**No Hungarian notation.** Names don't encode their type: no `p`/`pp`/`lp` for pointers, `sz`, `dw`, `h`, or `m_`/`g_`/`s_` scope prefixes. Write `factory`, `heaps`, `commandLists`, not `pFactory`, `ppHeaps`. Output parameters use an `out` prefix (`outBuffer`). Fields of API structs (`desc.pData`) keep their API names.

**No naked `new`; raw pointers never own.** Create objects with `std::make_unique` / `std::make_shared`. Own C and COM resources with `std::unique_ptr` and a deleter, or `ComPtr`. Raw pointers are fine for non-owning access (API handles, `.Get()`, observers).

**Don't limit who can create an object.** Constructors are public: no `PassKey`, and no private constructor opened to one class with `friend`. A factory such as `DeviceContextVulkan::Create()` can still be the usual way to build an object. Protect invariants with read-only access instead: a private member with a getter, like the scene objects' ids (`SceneModel::GetId()`).

## State, constants and errors

### No global mutable state

State belongs to an object that is passed to whoever needs it: `Application` (owned by `main`), `Scene` (camera, light), the renderers (GPU resources) and the device contexts (device, uploads). That means:

- No non-`const` variables at namespace scope, including unnamed namespaces.
- No non-`const` `static` data members.
- No function-local `static` variables that hold state.
- No singletons or global accessors (`Get()` / `Instance()` returning a shared object).

An exception needs a reason written next to it and a marker the checks recognise: `// NOLINT(cppcoreguidelines-avoid-non-const-global-variables)` for a namespace-scope variable, `// conventions: allow global-state` for a static (on the line itself or on a comment line directly above it). The only one today is `imguiVulkanInitResult` in `DebugOverlayVulkan.cpp`: ImGui's `CheckVkResultFn` is a C function pointer with no user data, so the callback can only report back through it.

State inside libraries (the ImGui context, SDL, stb_image's last error) is outside this rule; only its lifetime is ours to manage.

### String constants are `constexpr std::string_view`

Write string constants as `constexpr std::string_view` (`std::wstring_view` for wide strings):

- not `std::string const`: it is constructed when the program starts, so it is exposed to the static initialization order problem;
- not `char const*` / `wchar_t const*`.

Where a C API needs a null-terminated string, pass `.data()` of a view of a string literal (literals are null-terminated). Example: `.pName = entryPointVertexShader.data()`.

Strings computed at run time inside functions (`std::string const path{ std::format(...) }`) are not constants and are fine.

An API that takes an array of C strings (Vulkan's `ppEnabledExtensionNames`) needs an array of `char const*`; mark that constant with `// conventions: allow string-constant` and the reason, as `deviceExtensions` in `RendererVulkan.cpp` does.

### No exceptions: errors are values

The code does not throw. A function that can fail returns `std::expected<T, Error>` (`std::expected<void, Error>` when there is no value), and the caller checks it:

```cpp
if (auto result{ CreateSwapChain() }; !result)
	return std::unexpected(result.error());
```

- `Error` holds an `ErrorCode` and a human-readable `message` (built with `std::format`, including the failing call's result code).
- `ErrorCode` values that only one backend raises carry its prefix (`Vulkan...`, `D3D12...`).
- `BreakIfFalse` is for programmer errors (broken assumptions), not for runtime failures: it stops in the debugger and does nothing in release builds.
- `main` keeps one `catch (std::exception const&)` as a safety net for exceptions thrown by the standard library or third-party code. Our code adds no `throw`.

## Files and modules

- Module interface files (`.ixx`) and implementation files (`.cpp`) are PascalCase and match their module name. Example: `RendererD3D12.ixx` contains `export module RendererD3D12;`.
- Backend-specific files take a backend suffix: `D3D12` or `Vulkan`, as in `SkyboxD3D12.cpp` and `TextureVulkan.cpp`. Backend-agnostic files have no suffix, as in `Skybox.ixx`.
- Shader source files are lowercase snake_case: `skybox.hlsl`, `textured_surface.hlsl`, `common.hlsli`.
- Asset files are lowercase snake_case: `textured_cube.glb`.

## HLSL

| Kind | Rule | Examples |
|---|---|---|
| Structs | PascalCase, no suffix. Matches the C++ mirror type. | `SkyboxCamera`, `ModelViewProjection`, `VSOutput` |
| Struct members | camelCase. Same names, in the same order, as the C++ mirror. | `viewPosition`, `invProjection`, `specularColor`, `MVP` (abbreviation) |
| Constant buffer variables | `CB` suffix | `SkyboxCameraCB`, `ModelViewProjectionCB`, `DirectionalLightCB` |
| Textures and samplers | Descriptive camelCase | `diffuseTexture`, `skyboxTexture`, `linearSampler` |
| Functions | PascalCase | `BlinnPhong` |
| Entry points | PascalCase `VSMain` / `PSMain` | |
| Locals and parameters | camelCase. Single letters are fine for standard lighting math. | `viewDirection`, `N`, `V`, `L` |
| Semantics | UPPERCASE | `POSITION`, `TEXCOORD`, `SV_Position`, `SV_Target` |
| Macros | UPPER_SNAKE | `VK_BINDING` |

The C++ side has no suffix on the type, but the HLSL constant buffer variable keeps `CB`. This is intentional. The shader cannot use the same name for both the type and the variable.
