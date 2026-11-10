# Mini MBM documentation

This directory is the maintained documentation set for the engine. Start with the documents
below, then use the topic index to find implementation notes, format references, and editor
guides.

## Start here

| Topic | Document |
|---|---|
| Lua game and engine API | [Lua API reference](lua-api.md) |
| Registered Lua plugin symbols | [Lua plugin symbol index](lua-plugin-symbols.md) |
| Built-in editors and launchers | [Editor inventory](editors.md) |
| Mesh v11 binary format | [Mesh v11 format](mesh-v11-format.md) |
| Lighting behavior and limits | [Lighting](light.md) |
| Runtime skeletal animation | [Realtime skeletal animation](realtime-skeletal-animation.md) |

## Editor and asset workflows

- [Articulated animation](articulated-animation.md)
- [Articulated mesh editor](articulated-mesh-editor.md)
- [Articulated sprite editor](articulated-sprite-editor.md)
- [Bones, armatures, and FBX](bones-armatures-and-fbx.md)
- [Facial Rig Editor](facial-rig-editor.md)
- [Image Mesh Editor](image-mesh-editor.md)
- [Mesh audit](mesh-audit.md)
- [Mesh Debug automatic capture](mesh-debug-auto-capture.md)
- [Mesh Debug normals](mesh-debug-normals.md)
- [Mesh simplification](mesh-simplification.md)
- [Mesh Texture Paint Editor](mesh-texture-paint-editor.md)
- [Mesh-to-Sprite Editor](mesh-to-sprite-editor.md)
- [Morph Editor](morph-editor.md)
- [Normal Map Editor](normal-map-editor.md)
- [Normal mapping](normal-mapping.md)
- [Skeletal Animation Editor](skeletal-animation-editor.md)
- [Interaction Animation Editor](interaction-animation-editor.md) (proposta)

The [editor inventory](editors.md) is the source of truth for all 21 launchers, aliases,
dedicated guides, and known guide gaps. The ordinary Font, GUI, Particle, Physics, Scene,
Shader, Sprite, Texture Packer, Tilemap, and Asset Packager workflows currently have inventory
coverage but no dedicated guide in this repository.

## Engine and platform references

- [Async loading and threading](async-loading-and-threading.md)
- [Async mesh path-resolution refactor](async-mesh-path-resolution-refactor.md)
- [Audio backend implementation](audio-new-implementation.md)
- [Box2D](box2d.md)
- [Box2D upgrade plan](box2d-upgrade-plan.md)
- [Core PIMPL status](core-pimpl-status.md)
- [Default shader program cache](default-shader-program-cache.md)
- [New render backend instructions](new-backend-instructions.md)
- [Particles 3D study](particles-3d-study.md)
- [Renderable architecture](renderizable-architecture.md)

## Plans, investigations, and known issues

- [Issue list](Issue-list.md)
- [Future features](future-features.md)
- [Future investigation](future_investigation.md)
- [Realtime skeletal animation future work](realtime-skeletal-animation-future-work-plan.md)

These documents describe plans, investigations, or known limitations. They are not API
contracts unless they explicitly link to an implementation reference.

## Maintenance rules

- When adding or renaming an editor launcher, update `src/CMakeLists.txt`, [editors.md](editors.md),
  and the README summary. Run `scripts/check-editor-docs.sh`.
- When changing a Lua binding, update [lua-api.md](lua-api.md) and, for plugin registrations,
  [lua-plugin-symbols.md](lua-plugin-symbols.md).
- When changing a binary layout or invariant, update the matching format document and run the
  relevant foundation tests.
- Keep dedicated editor guides linked from [editors.md](editors.md) instead of adding another
  editor inventory to the README.

