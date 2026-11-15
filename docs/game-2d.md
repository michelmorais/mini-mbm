# Mini MBM 2D Game Guide

This guide is the starting point for a conventional 2D Lua game. The complete
API is in `lua-api.md`; this file only describes the choices that matter first.

## Recommended runtime model

- Use `"2dw"` for game objects that move with the world and camera.
- Use `"2ds"` for HUD and screen-space interface elements.
- Use sprites, textures, fonts, particles and tile maps for visual content.
- Use Box2D for collision and rigid-body simulation when the game needs physics.

The engine calls `onInitScene()` once and `onLoop(delta)` every frame. Load
assets and initialize game state in `onInitScene()`. Keep gameplay updates in
`onLoop(delta)`, using `delta` as seconds since the previous frame.

## First steps

1. Start from `main.lua` and keep the entry point small.
2. Put game assets under `assets/` and use `mbm.addPath()` when loading from
   another directory.
3. Create render objects in `onInitScene()` and retain Lua references while they
   are needed.
4. Read the coordinate-system and constructor sections of `lua-api.md` before
   adding camera, input or physics code.

## Physics boundary

Box2D is a 2D simulation. Keep the physics position and the render position in
the same world convention, or provide one explicit conversion function if the
game uses a different scale. A sprite or other render object can be updated from
the body position each frame; rendering and simulation remain separate systems.
See `box2d.md` for plugin and build-profile details.

For a game that renders 3D meshes but keeps movement and physics in X/Y, use the
`2d5` profile instead of this one.
