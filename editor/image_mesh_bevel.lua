--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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
|------------------------------------------------------------------------------------------------------------------------|

]]--

local Model=require 'image_mesh_model'
local M={}
local switches={'bevelFront','bevelBack','bevelCorners'}
local function L(key) return tLang.L('ime_bevel_'..key) end
-- Translate native diagnostics at the editor boundary, before adding the region name.
local errors={
    ['Bevel width must be finite and in [0.001,1000000] mesh units']='width_range',
    ['Bevel requires a closed convex extrusion without holes, with locked border and flat back']='requirements',
    ['Bevel supports convex contours with 3..128 corners']='corners_count',
    ['Bevel width is too large for the block thickness']='thickness',
    ['Bevel width must be smaller than half of every contour edge']='edge_length',
    ['Bevel currently requires a convex contour']='convex',
    ['Bevel width is too large for the contour interior']='interior',
    ['Invalid bevel corner']='corner_invalid',
    ['Bevel width is too large for a contour corner']='corner_width',
    ['Bevel removed the surface or produced an empty cut']='empty_cut',
    ['Bevel intersection is not a closed loop; reduce width or relief']='open_loop',
    ['Bevel cut has multiple loops; reduce width or relief']='multiple_loops',
    ['Geometry budget exceeded by bevel vertices']='vertices',
    ['Cannot triangulate final bevel face']='triangulation',
    ['Degenerate bevel triangle; change the width or contour resolution']='degenerate',
    ['Geometry budget exceeded by bevel triangles']='triangles',
}
function M.error(message)
    local key=errors[message]
    if key then return L('error_'..key) end
end
function M.panel(E)
    if not tImGui.CollapsingHeader(L('title')) then return end
    local v=E.values
    v.bevel=tImGui.Checkbox(L('enabled'),v.bevel==true)
    if not v.bevel then return end
    tImGui.TextWrapped(L('help'))
    local changed,width=tImGui.InputFloat(L('width'),v.bevelWidth or 1,0.1,1,'%.3f')
    if changed then v.bevelWidth=Model.clampOption('bevelWidth',width,v.bevelWidth or 1) end
    for _,key in ipairs(switches) do
        v[key]=tImGui.Checkbox(L(key),v[key]~=false)
    end
    if v.voxelized or v.heightSource=='curved' or v.backOpen or v.backRelief or not v.lockBorder or
        (E.draft and E.draft.holes and #E.draft.holes>0) then
        tImGui.TextWrapped(L('unsupported'))
    end
end
return M
