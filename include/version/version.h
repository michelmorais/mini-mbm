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
    Example: "1.0.b"
    1 - Major change
    0 - New functionalities
    a - Fix some issues or even improved non impact of new functionalities

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
*/
#define MBM_VERSION "4.0"
#endif

#endif
