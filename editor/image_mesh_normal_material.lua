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

-- Split materials only after simplification/remeshing. Copy triangle corners
-- unchanged: introducing material boundaries earlier changes QEM constraints.
local M={}
function M.split(source,options)
    local vertices=source:getVertex(1,1,1,source:getTotalVertex(1,1))
    local indices=source:getIndex(1,1)
    local total=source:getTotalSubset(1)
    local frontOnly=total==3 and options and (options.backSolid or options.backExternal)
    local groups={{vertices={},indices={},map={}},{vertices={},indices={},map={}},{vertices={},indices={},map={}}}
    for i=1,#indices,3 do
        local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
        -- Generated image meshes face +Z after the editor's rotation. Front/back
        -- are height fields; vertical walls have zero projected XY area.
        local abx,aby,acx,acy=b.x-a.x,b.y-a.y,c.x-a.x,c.y-a.y
        local cross=abx*acy-aby*acx
        -- Positions have float32 precision even though Lua computes in doubles.
        -- Refined/slightly slanted contour edges are not exactly collinear after
        -- native rounding and QEM. Bound that positional error, not just the
        -- double-precision cancellation in the determinant, or wall fans leak
        -- into the front residual bake (and into the back material).
        local scaleX=math.max(math.abs(a.x),math.abs(b.x),math.abs(c.x))
        local scaleY=math.max(math.abs(a.y),math.abs(b.y),math.abs(c.y))
        local tolerance=4*2^-23*(scaleX*(math.abs(aby)+math.abs(acy))
            +scaleY*(math.abs(abx)+math.abs(acx)))
        local group=groups[3]
        if frontOnly or cross>tolerance then group=groups[1]
        elseif cross < -tolerance then group=groups[2] end
        for j=i,i+2 do
            local old=indices[j]
            if not group.map[old] then group.vertices[#group.vertices+1]=vertices[old];group.map[old]=#group.vertices end
            group.indices[#group.indices+1]=group.map[old]
        end
        if i%1536==1 then coroutine.yield() end
    end
    assert(#groups[1].indices>0,'No front triangles for normal map')
    local asset=meshDebug:new()
    asset:setType('mesh');asset:setModeDraw(source:getModeDraw())
    asset:setModeFrontFace(source:getModeFrontFace());asset:setModeCullFace(source:getModeCullFace())
    asset:setMaterial(source:getMaterial());asset:addFrame(3)
    local normalSubsets={}
    for index,group in ipairs(groups) do
        if #group.indices>0 then
            asset:addSubSet(1)
            local subset=asset:getTotalSubset(1)
            assert(asset:addVertex(1,subset,group.vertices));assert(asset:addIndex(1,subset,group.indices))
            assert(asset:setTexture(1,subset,source:getTexture(1,1)))
            if index==1 or (index==3 and options and options.sideMode=='band') then
                normalSubsets[#normalSubsets+1]=subset
            end
        end
    end
    for subset=2,total do
        asset:copySubsetFrom(1,source,1,subset)
        -- A separate back material leaves the native walls in the last subset.
        if options and (options.sideMode=='band' or options.sideMode=='repeat') and subset==total then
            normalSubsets[#normalSubsets+1]=asset:getTotalSubset(1)
        end
    end
    asset:addAnim('Static',1,1,1,0)
    return asset,normalSubsets
end
function M.subsets(asset)
    local subsets={}
    for subset=1,asset:getTotalSubset(1) do
        if asset:getMaterialTexture(1,subset,'normal') then subsets[#subsets+1]=subset end
    end
    return subsets
end
return M
