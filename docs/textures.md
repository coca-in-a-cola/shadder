# Textured materials

The framework uses Direct3D 11. DirectXTK's WIC loader decodes JPG/PNG/BMP images,
creates an sRGB shader-resource view and generates mipmaps where supported.
COM initialization, loading and GPU binding run on the render thread.

```cpp
auto& material = world.AddComponent<PhongMaterialComponent>(entity);
material.diffuseTexturePath = L"data/textures/1.jpg";
material.baseColor = { 1, 1, 1, 1 };
material.uvScale = { 2, 2 };
material.shading = PhongMaterialDesc::Shading::Diffuse; // Or Phong (default).
```

`RenderSystem` uploads new mesh/material descriptions before drawing. Existing
`ResourceLoader::UploadAll(world, device)` calls still work as explicit preloading.
The example needs only a texture path and material parameters.

## Reusable presets and runtime changes

```cpp
PhongMaterialDesc brick;
brick.diffuseTexturePath = L"data/textures/1.jpg";
brick.filter = D3D11_FILTER_ANISOTROPIC;
brick.maxAnisotropy = 8;
brick.addressMode = D3D11_TEXTURE_ADDRESS_WRAP;

world.GetComponent<PhongMaterialComponent>(first)->SetMaterial(brick);
world.GetComponent<PhongMaterialComponent>(second)->SetMaterial(brick);
```

`SetMaterial` invalidates uploaded resources; the next render uploads the new
description. Use it when changing a texture path, shader, shading preset or sampler.
`baseColor`, lighting coefficients and UV scale/offset can be edited directly each
frame; they are constant-buffer values and do not rebuild geometry.

Materials share texture ownership. The weak cache is keyed by D3D device and
canonical resolved path; it reuses live resources across upload calls and never
keeps an unused GPU resource/device alive. Paths resolve against the working
directory first, then the executable directory. Empty path uses white; a failed
load logs the path/HRESULT and uses a magenta/black error texture. Failed paths are
not cached; assigning the description again through `SetMaterial` retries loading.

## Shaders and color

- `data/shaders/phong/DiffuseShader.hlsl`: ambient + Lambert.
- `data/shaders/phong/PixelShader.hlsl`: ambient + Lambert + Phong highlights.
- Both use `Surface.hlsli`, `Texture2D t0`, `SamplerState s0`, material buffer `b4`.
- Albedo is `texture * baseColor * vertexColor`. For neutral texture colors, use
  white vertex/base colors and neutral ambient/diffuse coefficients.
- Textures decode sRGB to linear through the SRV. Material and vertex colors are
  linear factors. Textured lighting is encoded back to sRGB in the pixel shader
  because the framework's back buffer is UNORM; ImGui/2D need no changes.
- Solid materials retain legacy output by default. Set `linearLighting = true`
  for the same linear workflow and separate specular highlights as textured ones.
- Alpha is sampled and multiplied, but these are opaque-pass materials; alpha
  blending/sorting is not enabled by assigning a texture.

`Vertex3D` is the shared 52-byte ABI: position, normal, color, UV. Cube faces have
independent UVs; planes and spheres supply UVs (including the sphere seam).
OBJ reads `vt`, splits UV seams and flips V to the Direct3D top-left convention.
Existing custom shaders must account for this stride when supplying raw buffers.

## Verification

From the repository root:

```powershell
scons -C tests
tests/bin/test_textures.exe
scons -C examples/phong
```

The integration test uses WARP and the D3D11 debug layer. It checks real JPG
loading, mipmaps, cache lifetime, material reassignment, mesh UVs, shader
compilation and offscreen GPU readback, including sRGB encoding and binding reset.
