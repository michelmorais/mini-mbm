/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef VERSION_MBM_H
#define VERSION_MBM_H

#ifndef MBM_VERSION
/*
    Format: "X.Y" or "X.Y.Z"
    X - Major change
    Y - New functionalities
    Z - Fix some issues or even improved non impact of new functionalities (optional patch component)

    Historical entries below predate the "Z" patch component and used a trailing
    letter (e.g. "3.1.a") instead; new entries should prefer numeric "X.Y.Z".

    1.0.a Framework began using Directx
    1.2.a Framework changed to Opengl-ES
    1.3.a Added wrapper Java for Android
    1.9.e Removed Directx
    2.0.a Added LUA as Language script
    2.1.a Added Editor LUA (sprite maker, scene2dw,scene2ds,scene3d)
    2.2.a Added Editor LUA (particle editor)
    2.3.a Added Editor LUA (font editor)
    2.4.a Changed internal way of binary mesh (introduced version as define)
    2.5.x Added TMX binary file
    2.6.x General re work
    2.7.x Included CMake
    2.8.a Make shared library of core-mbm
    3.0.a Plugins introduced (ImGui, box2d) as Modules
    3.1.a Sprite maker editor based on ImGui plugin added
    3.1.b Improved / fix Sprite maker editor based on ImGui
    3.1.c Created Shader editor based on ImGui / fix Minor issue on Shader Lua
    3.1.d Created Scene 2d Editor / removed old scenes editor and resources
    3.1.e Scene 2d Editor added isRelative2ds option
    3.1.f Updated sqlite3 to version 3.24.0. (fixed issue: ERROR attempt to index a string value) replaced luaL_register by luaL_newlib.
    3.2.a Added Tile Map from own engine. Removed TMX support
    3.2.b Added texture packer editor
    3.2.c Fixed endFX issue
    3.2.e Update animation even when is not in the screen
    3.2.f Fixed issue for vertex shader (when set to null it was not freeing )
    3.2.g Added compatibility to load Triangle from version 3
    3.2.f Parser for txt on font created
    3.2.g Upgrade IM-GUI version to 1.78, tinyfiledialog (v3.6.3)
    3.3   mini-mbm on windows does not depend anymore on core_mbm
    3.4   refactoried files on LUA wrapper
    3.5   Frozen box 2d (2.3.2)
    3.6   Migrated to box2d (2.4.1)
    4.0   LiquidFun for box 2d, this version forces to drawback Box2D to 2.3.0.
    4.1   LiquidFun for box 2d 2.3.0 and box 2d 2.4.1 in library separated
    4.2   Reorganized framework, moved needed function to plugins, LiquidFun and Box2d Available.
    5.0   Include Directx 9 backend support. Refactoried core-manager-opengl_es to core-manager-renderer with support to Directx9 and Opengl-ES and future renderers.
    5.1   Updated IM-GUI version to 1.92.6
    5.2   Introduced MacOs support
    5.3   Modernized Android Project, Added native Activity,removed old Java wrapper for AUDIO jni, added support to generate project on Android Studio using cmake
    6.0   The first Mini MBM lighting implementation using the classic model of Phong.
    6.1   TextureAnimationEffect ownership normalized to animation-level metadata with explicit legacy mismatch validation.
    6.2   Mesh v10 stores TextureAnimationEffect once per animation FX block instead of duplicating it in PS/VS stage records.
    6.3   Mesh v11 stores TextureAnimationEffect once per animation FX block instead of duplicating it in PS/VS stage records.
    6.4   Lighting system created with support multiple light sources and improved performance. Mesh v12 introduces a new lighting.
    6.5.0 Added reusable Blender-style 3D orbit navigation gizmo (editor_utils.lua drawOrbitGizmo) to mesh_debug's Camera window; split the Light panel into its own window. First entry using the X.Y.Z format.
    6.6.0 Added Scene Editor 3D (editor/scene_editor3d.lua): grid/free placement of any renderizable, per-layer Y control, cached mesh thumbnails via render2texture, async mesh loading with progress, and a self-contained requirable scene export.
    6.6.1 Fixed Scene Editor 3D: Combo() off-by-one index bug (Map type/snap-scale combos), ColorEdit4 crash in the light panel (wrong calling convention + wrong 0-255 color range), background color menu parity with Scene Editor 2D, thumbnail cache location, per-tab orbit camera, sync reload of already-cached meshes, and scroll-into-scene leak while hovering ImGui windows.
    6.6.2 Added tImGui.GetWantCaptureMouse()/GetWantCaptureKeyboard() (plugins/imGui): expose io.WantCaptureMouse/WantCaptureKeyboard for reading, the correct/documented way to decide whether input should reach the game scene vs the UI, replacing IsAnyWindowHovered() which repeatedly let clicks/scroll on Scene Editor 3D's own windows leak into the 3D scene. Also fixed the editor's stray white render-to-texture quad, mislabeled/too-short camera zoom, and made Layer-tab placement synchronous.
    6.7.0 Physic Editor: fixed a crash loading 3D meshes (selection/highlight API was 2D-only); added a real 3D orbit camera + nav gizmo, mouse-based selection of physics shapes via native :collide(), and full editing support for all physics types on 3D meshes. Added a "Complex" primitive (8-point box, plus a 12-triangle decomposition) to the Add-Physic UI. Mesh Debug: added a bulk "Reset Physics To" operation in Apply-to-all, computing each mesh's own frame-1 vertex bounds instead of assuming mesh:getSize().
    6.8.0 Added a real ray/AABB test for 3D screen-space picking: DEVICE::rayIntersectsAABB (slab method) plus a new mbm.getPickRay(sx,sy) Lua binding exposing DEVICE::rayCast (origin + normalized direction), previously only used internally to back mbm.to3d. Fixed obj:collide(x,y) on 3D objects to use this instead of unprojecting at a guessed depth (the object's own raw world Z, treated as camera distance) -- that approximation degenerated to the camera's own position for any object near world Z=0 (e.g. anything at its default placement position), so the test could never register a hit there; it also mattered generally for any free-orbiting camera since an object's Z has no fixed relationship to its real camera distance. Also fixed obj:collide(x, y, useAABB) (the 4-argument form): onCheckCollisionBoundingBoxRenderizable only accepted an argument count of exactly 3, so passing the documented optional 4th useAABB argument missed that branch entirely and hit a generic "Expected: [renderizable],[renderizable] or [x,y]" error instead. Discovered and fixed while building Scene Editor 3D's Shift+drag rectangle-select. Added Scene Editor 3D: grid show/hide toggle, Free-mode grid bordering (snapping still skipped), live mesh-offset propagation to preview/placed instances, Main Scene/Mesh property tab renames, replace-not-duplicate placement on occupied grid cells, and a full select/rotate/delete workflow (multi-select, Shift+drag rectangle select, Esc to unselect, bottom-right rotate/delete tool window) mirroring tilemap_editor.lua.
    6.9.0 Added a true AABB-center concept, engine-wide: RENDERIZABLE::getBoundingAABBCenterOffset() (new, Impl-backed, virtual) reports the world-space offset from an object's pivot to its actual geometric center -- (0,0,0) when geometry is centered on its own pivot (the common case), non-zero for anything anchored elsewhere (a mesh pivoted at its floor/base, a character at its feet). Backed by a new INFO_PHYSICS::getBoundsMinMax() (both 2D/3D), which also properly unions bounds across every physics shape kind on a mesh, unlike getBounds(). New Lua binding obj:getAABBCenter(). TEXT_DRAW overrides the offset directly from its own alignment-driven aabbMin/aabbMax. Replaced the old doOffsetIfText()/undoOffsetIfText() mutate-position-then-restore hack (font-only, used inside obj:collide) and editor_utils.lua's independently-written font-only correction in setShapeToMesh with this one general mechanism -- obj:collide and the shared 2D editor selection-highlight shape now both use it uniformly. Also fixed INFO_PHYSICS::getBounds()'s CUBE/SPHERE branches (both 2D/3D overloads), which derived size via 2*max(|absCenter-halfDim|,|absCenter+halfDim|) -- only correct when absCenter==(0,0,0); for an off-center shape (e.g. the single CUBE MESH_MBM_DEBUG::fillAtLeastOneBound() auto-bakes into any mesh saved with no hand-authored physics, whose absCenter is the mesh's own true center) this could inflate the reported size up to 2x on the anchored axis. Physic Editor: new physics shapes now default their position to the mesh's true AABB center instead of the hardcoded local origin, and a small marker (distinct from the existing hover-highlight point) shows that center while editing. Scene Editor 3D: hover wire-box and Mesh Set thumbnail camera focus now use the true center instead of the pivot. Root-caused while investigating a Scene Editor 3D hover-box bug reported against a floor-anchored test building mesh.
    6.10.0 Scene Editor 3D (Map edition tab): replaced the plain yellow wireframe hover box with a solid translucent shape (green when the hovered/selected mesh belongs to the active layer, red otherwise; selected meshes blink between alpha 0.2-0.5, a per-frame sine wave, no shader needed), mirroring tilemap_editor.lua's/physic_editor.lua's own tinted-overlay visual language instead of a bespoke wireframe. Added Ctrl+Click to toggle one mesh into/out of the current selection (additive), complementing the existing Shift+drag rectangle-select (which replaces). Placed Meshes list rows and the active layer's "(active)" label now render in green to match. Mesh property tab: added per-asset Rotation and Scale offsets (added on top of each instance's own rotation/scale, same as the existing Position offset), applied consistently in the live preview, every placed instance, and the exported game script's runtime loader. Map tab: directional light direction is now edited via the same orbit trackball widget already used for the camera (tUtil.drawOrbitGizmo, reused unmodified since it only touches .azimuth/.elevation) instead of three raw x/y/z InputFloats, plus a visual arrow in the 3D scene showing where the light comes from and a small tinted marker at each point light's position -- there was previously no way to see where a light actually was/pointed relative to the placed scene, only numbers in a panel.
    6.10.1 Fixed MESH_MBM_DEBUG::getInfo(filePath, ...) (the lightweight file-only info query used by Mesh Debug's UI, e.g. the Normals/Light 3D panels) never reading each mesh's actual hasNormal/hasUv flags from its SECTION_FRAME_STATIC payload -- it only counted frames, leaving hasNorText at its struct-default (normals always false), so freshly-saved normals appeared to vanish from the UI even though MESH_MBM_DEBUG::loadV11 (the real load path used for rendering) already parsed them correctly and the file was fine on disk. Now parses frame 0's FRAME_HEADER_V11 the same way loadV11 does.
*/
#define MBM_VERSION "6.10.1" // MBM_VERSION must be in format X.Y or X.Y.Z
#endif

#endif
