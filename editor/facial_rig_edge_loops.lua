--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Closed open-boundary loops in canonical frame-0 topology. A sealed mesh has no boundary loop.
]]--

local M={}

function M.build(topology)
    local vertices=topology and topology.vertices or {}
    local edges={}
    local function addEdge(a,b)
        if not vertices[a] or not vertices[b] or a==b then return end
        if a>b then a,b=b,a end
        local key=a..':'..b
        local edge=edges[key]
        if edge then edge.count=edge.count+1
        else edges[key]={a=a,b=b,count=1} end
    end
    for _,triangle in ipairs(topology and topology.triangles or {}) do
        local a,b,c=triangle[1],triangle[2],triangle[3]
        if vertices[a] and vertices[b] and vertices[c] and a~=b and b~=c and c~=a then
            addEdge(a,b); addEdge(b,c); addEdge(c,a)
        end
    end
    local adjacency={}
    local surfaceAdjacency={}
    for _,edge in pairs(edges) do
        surfaceAdjacency[edge.a]=surfaceAdjacency[edge.a] or {}
        surfaceAdjacency[edge.b]=surfaceAdjacency[edge.b] or {}
        surfaceAdjacency[edge.a][#surfaceAdjacency[edge.a]+1]=edge.b
        surfaceAdjacency[edge.b][#surfaceAdjacency[edge.b]+1]=edge.a
        if edge.count==1 then
            adjacency[edge.a]=adjacency[edge.a] or {}
            adjacency[edge.b]=adjacency[edge.b] or {}
            adjacency[edge.a][#adjacency[edge.a]+1]=edge.b
            adjacency[edge.b][#adjacency[edge.b]+1]=edge.a
        end
    end
    local loops,byVertex,visited={},{},{}
    for first in pairs(adjacency) do
        if not visited[first] then
            local component,stack={}, {first}
            local closed=true
            visited[first]=true
            while #stack>0 do
                local index=table.remove(stack)
                component[#component+1]=index
                local neighbors=adjacency[index]
                if #neighbors~=2 then closed=false end
                for _,neighbor in ipairs(neighbors) do
                    if not visited[neighbor] then
                        visited[neighbor]=true
                        stack[#stack+1]=neighbor
                    end
                end
            end
            if #component<3 then closed=false end
            table.sort(component)
            local entry={vertices=component,closed=closed}
            loops[#loops+1]=entry
            for _,index in ipairs(component) do byVertex[index]=entry end
        end
    end
    return {loops=loops,byVertex=byVertex,vertices=vertices,
        adjacency=surfaceAdjacency,signature=topology and topology.signature}
end

function M.nearest(cache,point,radius,seedIndices)
    if not cache or not point or not radius or radius<=0 then return nil,'no_boundary' end
    local closest,closestIndex,distanceSquared=nil,nil,radius*radius
    local function consider(index)
        local entry=cache.byVertex[index]
        if not entry then return end
        local vertex=cache.vertices[index]
        if vertex then
            local dx,dy,dz=vertex.x-point.x,vertex.y-point.y,vertex.z-point.z
            local distance=dx*dx+dy*dy+dz*dz
            if distance<distanceSquared or distance==distanceSquared and
                    (not closestIndex or index<closestIndex) then
                closest=entry; closestIndex=index; distanceSquared=distance
            end
        end
    end
    if seedIndices and #seedIndices>0 then
        -- Search the clicked triangle's connected surface first. Coincident UV or
        -- material seams can contain different loops at identical 3D positions.
        local queue,seen={},{}
        for _,index in ipairs(seedIndices) do
            if cache.vertices[index] and not seen[index] then
                seen[index]=true; queue[#queue+1]=index
            end
        end
        local cursor=1
        while cursor<=#queue and not closest do
            local levelEnd=#queue
            while cursor<=levelEnd do
                local index=queue[cursor]
                cursor=cursor+1
                consider(index)
                for _,neighbor in ipairs(cache.adjacency[index] or {}) do
                    if not seen[neighbor] then
                        seen[neighbor]=true
                        queue[#queue+1]=neighbor
                    end
                end
            end
        end
    else
        for index in pairs(cache.byVertex or {}) do consider(index) end
    end
    if not closest then return nil,'no_boundary' end
    if not closest.closed then return nil,'open_chain' end
    return closest.vertices
end

return M
