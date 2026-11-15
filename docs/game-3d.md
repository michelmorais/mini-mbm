# Mini MBM 3D Game Guide

Use this profile when rendering and gameplay both depend on three-dimensional
positions. The complete runtime API is in `lua-api.md`.

## Recommended runtime model

- Use `"3d"` objects and a 3D camera for world content.
- Use meshes for geometry and `light.md` for materials and lighting.
- Use `normal-mapping.md` when the project uses normal-map materials.
- Use Bullet3D only when the game needs 3D physics; visual 3D alone does not
  require a physics backend.

Keep the coordinate conventions, camera setup and physics scale explicit. Use
`onInitScene()` for asset creation and `onLoop(delta)` for gameplay updates.
