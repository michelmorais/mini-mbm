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

local M={}
function M.release(E)
    if E.wireObject then E.wireObject:destroy(); E.wireObject=nil end
end
function M.sync(E)
    local visible=not E.editMode and not E.dirty
    if E.preview then E.preview.visible=visible and not E.wireframe end
    if E.wireObject then E.wireObject.visible=visible and E.wireframe end
end
function M.ensure(E,asset)
    if E.wireObject or not E.preview then return end
    if not asset then asset=meshDebug:new(); assert(asset:load(E.previewPath)) end
    local vertices=asset:getVertex(1,1,1,asset:getTotalVertex(1,1))
    local indices=asset:getIndex(1,1)
    local adjacency,edges={},{}
    local remap,positions={},{}
    for i,v in ipairs(vertices) do
        local key=string.pack('fff',v.x+0.0,v.y+0.0,v.z+0.0)
        positions[key]=positions[key] or i; remap[i]=positions[key]
    end
    local function edge(a,b)
        a,b=remap[a],remap[b]
        if a==b then return end
        if a>b then a,b=b,a end
        local key=a*65536+b
        if edges[key] then return end
        edges[key]=true
        adjacency[a]=adjacency[a] or {}; adjacency[b]=adjacency[b] or {}
        adjacency[a][#adjacency[a]+1]=b; adjacency[b][#adjacency[b]+1]=a
    end
    for i=1,#indices,3 do
        edge(indices[i],indices[i+1]); edge(indices[i+1],indices[i+2]); edge(indices[i+2],indices[i])
    end
    local object=line:new('3d'); E.wireObject=object
    object.visible=false; object.alwaysRender=true
    local points={}
    local function append(id)
        local v=vertices[id]
        points[#points+1]=v.x; points[#points+1]=v.y; points[#points+1]=v.z
        if #points>=8192*3 then
            assert(object:add(points)>0); points={v.x,v.y,v.z}
        end
    end
    -- Walk each graph component along real edges, backtracking to avoid false
    -- connections. Bounded line strips avoid a GPU draw call per triangle.
    for root=1,#vertices do
        if adjacency[root] and #adjacency[root]>0 then
            local stack={root}; append(root)
            while #stack>0 do
                local a=stack[#stack]; local neighbors=adjacency[a]
                local b=table.remove(neighbors)
                if b then
                    local key=math.min(a,b)*65536+math.max(a,b)
                    if edges[key] then edges[key]=nil; append(b); stack[#stack+1]=b end
                else
                    stack[#stack]=nil
                    if #stack>0 then append(stack[#stack]) end
                end
            end
            if #points>3 then assert(object:add(points)>0) end
            points={}
        end
    end
    object:setColor(0.2,0.9,1)
    E.wireBuilds=(E.wireBuilds or 0)+1
end
return M
