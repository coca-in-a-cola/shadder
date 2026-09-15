# Directional shadow maps

`DirectionalLightComponent` has one orthographic D3D11 shadow map. It is disabled
by default, so scenes without an explicit opt-in allocate no map and make no
depth-only shadow draw calls.

```cpp
auto& light = world.AddComponent<DirectionalLightComponent>(entity);
light.direction = { -0.4f, -0.8f, 0.45f };
light.shadowsEnabled = true;
light.shadowResolution = 2048;
light.shadowOrthoSize = 40.0f;
light.shadowDistance = 50.0f;
light.shadowFarZ = 120.0f;
light.shadowCenter = { 0.0f, 0.0f, 0.0f };

// Disable immediately. Next frame skips depth pass and sampling.
light.shadowsEnabled = false;
```

`RenderSystem` renders all `Transform3D + MeshComponent` objects with
`castsShadow == true` into a `D32_FLOAT` map, then restores default targets and
samples its `R32_FLOAT` SRV from Phong/Diffuse shaders. A 3x3 comparison PCF
filter softens edges. Ambient stays unshadowed; diffuse and specular are shadowed.

`PhongMaterialComponent::receivesShadow` defaults to `true`. Set it to `false`
for an unshadowed material. `MeshComponent::castsShadow` defaults to `true`; set
it to `false` for particles, decals or helper geometry.

Depth acne control:

- `shadowDepthBias`: hardware depth-unit offset. Default `1200`.
- `shadowSlopeScaledDepthBias`: extra offset on grazing faces. Default `2`.
- `shadowBias`: receiver comparison offset. Default `0.0015`.

Tune smallest values that remove acne. Excessive bias detaches shadows from
casters. `shadowResolution` is clamped to 256..8192; changing resolution recreates
map resources. `shadowOrthoSize` should tightly contain casters/receivers; large
coverage lowers texel density. Current system has one map for first active
directional light, no cascades. `phong` and `katamari` opt in; all other examples
remain unshadowed.
