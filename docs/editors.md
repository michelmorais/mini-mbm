# Editor inventory

This is the consolidated inventory of the Lua/ImGui editors shipped in `editor/`.
The launcher aliases and script paths are kept in `src/CMakeLists.txt`; this page records
the user-facing names and documentation coverage so the README does not become a second,
stale source of truth.

All editors require the Lua and ImGui features. On Linux and macOS, the CMake build emits
shell aliases for the names in the table. On other platforms, launch the script with the
engine's normal `--scene` option.

## Launchers

| Editor | Script | Shortcut | Documentation | Coverage |
|---|---|---|---|---|
| Asset Packager | `asset_packager.lua` | `asset_packager` | This inventory | Inventory only |
| Facial Rig Editor | `facial_rig_editor.lua` | `facial_rig_editor` | [Facial Rig Editor](facial-rig-editor.md) | Guide |
| Font Maker | `font_maker.lua` | `font_maker` | This inventory | Inventory only |
| GUI Editor | `gui_editor.lua` | `gui_editor` | This inventory | Inventory only |
| Image Mesh Editor | `image_mesh_editor.lua` | `imesh` | [Image Mesh Editor](image-mesh-editor.md) | Guide |
| Interaction Animation Editor | `interaction_animation_editor.lua` | `interaction_animation_editor` | [Interaction Animation Editor](interaction-animation-editor.md) | Guide |
| Mesh Debug | `mesh_debug.lua` | `mesh_debug` | [Mesh Simplification](mesh-simplification.md), [Mesh Audit](mesh-audit.md), [Mesh Debug capture](mesh-debug-auto-capture.md) | Related guides |
| Articulated Mesh Editor | `mesh_maker_articulated.lua` | `mesh_articulated` | [Articulated Mesh Editor](articulated-mesh-editor.md) | Guide |
| Mesh Texture Paint Editor | `mesh_texture_paint_editor.lua` | `mesh_texture_paint_editor` | [Mesh Texture Paint Editor](mesh-texture-paint-editor.md) | Guide |
| Mesh-to-Sprite Editor | `mesh_to_sprite_editor.lua` | `mesh_to_sprite_editor` | [Mesh-to-Sprite Editor](mesh-to-sprite-editor.md) | Guide |
| Morph Region Editor | `morph_region_editor.lua` | `morph_editor` | [Morph Editor](morph-editor.md) | Guide |
| Normal Map Editor | `normal_map_editor.lua` | `normal_map` | [Normal Map Editor](normal-map-editor.md) | Guide |
| Particle Editor | `particle_editor.lua` | `particle_editor` | This inventory | Inventory only |
| Physics Editor | `physic_editor.lua` | `physic_editor` | This inventory | Inventory only |
| Scene Editor 2D | `scene_editor2d.lua` | `scene_editor2d`, `scene_editor` | This inventory | Inventory only |
| Scene Editor 3D | `scene_editor3d.lua` | `scene_editor3d` | This inventory | Inventory only |
| Shader Editor | `shader_editor.lua` | `shader_editor` | This inventory | Inventory only |
| Skeletal Animation Editor | `skeletal_animation_editor.lua` | `skeletal_animation_editor` | [Skeletal Animation Editor](skeletal-animation-editor.md) | Guide |
| Sprite Maker | `sprite_maker.lua` | `sprite_maker` | This inventory | Inventory only |
| Articulated Sprite Editor | `sprite_maker_articulated.lua` | `sprite_maker_articulated` | [Articulated Sprite Editor](articulated-sprite-editor.md) | Guide |
| Texture Packer | `texture_packer.lua` | `texture_packer` | This inventory | Inventory only |
| Tilemap Editor | `tilemap_editor.lua` | `tilemap_editor` | This inventory | Inventory only |

The two scene launchers share `scene_editor_world.lua` and `scene_editor_view.lua`; those
files are implementation modules, not additional editors. `scene_editor2d.lua` also remains
the compatibility entry point named by the older `scene_editor` shortcut.

## Documentation gaps

The dedicated guides cover the newer mesh, animation, facial, image, and normal-map workflows.
The following launchers still have only the inventory entry and the general [Lua API reference](lua-api.md):

- Asset Packager
- Font Maker
- GUI Editor
- Particle Editor
- Physics Editor
- Scene Editor 2D
- Scene Editor 3D
- Shader Editor
- Sprite Maker
- Texture Packer
- Tilemap Editor

These are explicit coverage gaps, not undocumented launchers hidden from the inventory. A
future dedicated guide should be linked here and in the relevant README table when it is
written.

## Editor loop audit

The current editor frame dispatchers were checked for avoidable continuous work. Expensive
operations in the newer editors are gated by dirty flags, pending jobs, active strokes,
playback state, or queued thumbnail work. The normal per-frame ImGui drawing and active preview
updates are intentional. No editor loop change was required by this audit.

