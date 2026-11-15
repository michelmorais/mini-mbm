# Mini MBM 2D.5 Game Guide

Use this profile when the game renders 3D assets but behaves as a 2D game. A
typical example uses 3D meshes and lighting, restricts gameplay to X/Y, and uses
Box2D for collisions and rigid-body simulation.

## Separate the three concerns

### Rendering

Use `mesh` objects with the `"3d"` coordinate system. Lighting, materials,
normal maps and camera depth belong to the visual layer. The `light.md` and
`normal-mapping.md` documents describe those features.

### Gameplay

Keep gameplay positions and velocities in X/Y. Treat Z as a presentation value:
it can control visual depth, parallax, layer ordering or camera composition, but
it should not become a third gameplay axis accidentally.

### Physics

Use Box2D for the simulation. Box2D remains two-dimensional even when the
rendered object is a 3D mesh. Update the mesh from the Box2D body position and
apply a deliberate scale and Z-layer conversion at that boundary.
See `box2d.md` for plugin and build-profile details.

## Camera choices

An orthographic camera is usually the easiest way to preserve a 2D composition
while using 3D meshes. A perspective camera can be useful for parallax and
depth, but then visual scale changes with Z and should be treated as a rendering
effect rather than gameplay movement.

## Practical rules

- Keep one explicit function for Box2D-to-render coordinates.
- Do not create Bullet bodies unless the game really needs 3D physics.
- Keep collision shapes independent from mesh topology.
- Decide whether Z is a layer, a parallax depth or a camera depth, and use that
  convention consistently.
- Use `lua-api.md` for the runtime API and `light.md` for the lighting model.

This profile is a documentation selection only. It does not change the engine
or restrict the APIs available to the game.
