# GG Engine naming conventions

These are the naming rules the code follows. Formatting and other conventions are in `claude/coding-conventions.md` in the GG Engine project.

## C++

| Kind | Rule | Examples |
|---|---|---|
| Types, classes, structs, enums | PascalCase | `RendererD3D12`, `RendererSettings`, `ErrorCode`, `SkyboxCamera`, `ModelViewProjection` |
| Enumerators | PascalCase | `ErrorCode::ModelImportFailed`, `DebugLevel::Info` |
| Member functions | PascalCase | `UpdateCamera`, `GetViewMatrix`, `CreateSyncObjects` |
| Free functions, including helpers in unnamed namespaces | PascalCase | `CalculateModelViewProjection`, `ThrowIfFailed`, `MainLoop`, `ReadMesh`, `CreateShaderModule` |
| Data members | camelCase, no prefix or suffix | `cameraPosition`, `commandQueue`, `isWindowResized` |
| Locals and parameters | camelCase | `deltaTimeSeconds`, `hardwareAdapter` |
| `constexpr` constants | camelCase | `cameraMoveSpeed`, `maxFramesInFlight`, `uvSetNumber` |
| Macros | UPPER_SNAKE | `D3D12_CHECK_HRESULT` |
| Error codes | Backend prefix | `D3D12PipelineCreationFailed`, `VulkanRendererInitializationFailed`, `ModelImportFailed` |
| Handle members | Type word as suffix | `windowHandleSDL`, `renderTargetViewHeap`, `dsvHandle` |
| GPU object debug names (`SetName`) | `Kind_Purpose_Owner` | `PSO_Skybox_{}`, `RootSignature_Skybox_{}`, `SRV_DescriptorHeap_Skybox_{}` |

C++ types that mirror a constant buffer have **no** suffix. Examples: `SkyboxCamera`, `ModelViewProjection`, `DirectionalLight`.

**No Hungarian notation.** Names don't encode their type: no `p`/`pp`/`lp` for pointers, `sz`, `dw`, `h`, or `m_`/`g_`/`s_` scope prefixes. Write `factory`, `heaps`, `commandLists`, not `pFactory`, `ppHeaps`. Output parameters use an `out` prefix (`outBuffer`). Fields of API structs (`desc.pData`) keep their API names.

**No naked `new`; raw pointers never own.** Create objects with `std::make_unique` / `std::make_shared`. When only one class may create an object, give it a `PassKey` (see `ShaderProgramD3D12`) instead of a private constructor. Own C and COM resources with `std::unique_ptr` and a deleter, or `ComPtr`. Raw pointers are fine for non-owning access (API handles, `.Get()`, observers).

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
