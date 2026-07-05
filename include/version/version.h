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
*/
#define MBM_VERSION "6.7.0" // MBM_VERSION must be in format X.Y or X.Y.Z
#endif

#endif
