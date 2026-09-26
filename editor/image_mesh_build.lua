--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to     |
| permit persons to whom the Software is furnished to do so, subject to the following conditions:                         |
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

   Shared Image Mesh generation pipeline for the Image Mesh Editor and Mesh Debug.
]]--

local Model=require 'image_mesh_model'
local Generation=require 'image_mesh_generation'
local Asset=require 'image_mesh_asset'
local Budget=require 'image_mesh_budget'
local Simplify=require 'image_mesh_simplify'

local M={}

-- `callbacks.beforeSimplify` receives the generated source geometry before optional simplify or
-- remesh. `callbacks.needsCurvedSource` requests the unsimplified curved source used by the editor's
-- comparison and geometry cache.
function M.generate(E,project,region,callbacks)
    local options=Model.options(project,region)
    if options.heightSource=='curved' then options.simplify=false end
    local asset,report=Generation.generate(E,project.image.path,options)
    if not asset then
        if E.generationCancelled then return nil,report end
        return nil,Budget.error(region,report)
    end

    -- Match Mesh Debug's +Z front view. Rotate positions and authored normals by 180 degrees around
    -- Y while preserving UVs, winding, and the source's smooth or hard edges.
    local vertices=Asset.vertices(asset,true)
    if callbacks and callbacks.beforeSimplify then
        callbacks.beforeSimplify(asset,report,vertices,options)
    end

    if report.curvedSourceTriangles and callbacks and callbacks.needsCurvedSource then
        local originalOptions=Model.copy(options)
        originalOptions.curvedSimplify=false
        local original,originalReport=Generation.generate(E,project.image.path,originalOptions)
        if not original then
            if E.generationCancelled then return nil,originalReport end
            return nil,Budget.error(region,originalReport)
        end
        local originalVertices=Asset.vertices(original,true)
        if callbacks.curvedSource then
            callbacks.curvedSource(original,originalReport,originalVertices,options)
        end
        report.sourceVertices=originalReport.vertices
        report.sourceTriangles=originalReport.triangles
    end

    report.vertexLimit=math.min(options.maxVertices,65535)
    report.triangleLimit=options.maxTriangles
    Simplify.apply(E,asset,options,report)
    return asset,report,options,vertices
end

return M
