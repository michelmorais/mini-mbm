--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

package.path = 'editor/?.lua;' .. package.path
local Transform = require 'mesh_debug_transform'
local function fixture(skeletal, facial)
    local calls = {}
    local geometry = {
        [1] = {
            {vertices={{x=10,y=0,z=0},{x=12,y=0,z=0}}},
            {vertices={{x=0,y=0,z=0},{x=1,y=0,z=0}}},
        },
        [2] = {
            {vertices={{x=20,y=0,z=0},{x=22,y=0,z=0}}},
            {vertices={{x=2,y=0,z=0},{x=3,y=0,z=0}}},
        },
    }
    local mesh = { getTotalArticulatedParts = function() return 0 end,
        getTotalArticulatedAnimations = function() return 0 end,
        getModeDraw = function() return 'TRIANGLES' end, getSkeletonBindReport = function(_, dependencies)
        assert(dependencies == false)
        if skeletal then return {canonical=true, boneCount=2} end
        return nil
    end, getFacialExportData = function()
        if facial then return {vertexCount=3, targets={{name='jawOpen'}}} end
        return nil
    end,
        getTotalFrame = function() return #geometry end,
        getTotalSubset = function(_, frame) return #geometry[frame] end,
        getTotalVertex = function(_, frame, subset) return #geometry[frame][subset].vertices end,
        getVertex = function(_, frame, subset, vertex)
            local source = geometry[frame][subset].vertices[vertex]
            return {x=source.x,y=source.y,z=source.z}
        end,
        setVertex = function(_, frame, subset, vertex, value)
            geometry[frame][subset].vertices[vertex] = {x=value.x,y=value.y,z=value.z}
        end,
    }
    for _, name in ipairs({'rotateFrame','translateFrame','scaleFrame','scaleSkeletalAsset','centralize','centralizeItself'}) do
        if name == 'rotateFrame' then
            mesh[name] = function(_, frame, angleX, angleY, angleZ, subset)
                calls[#calls+1] = {name, frame, angleX, angleY, angleZ, subset}
                local firstFrame = frame <= 0 and 1 or frame
                local lastFrame = frame <= 0 and #geometry or frame
                for currentFrame = firstFrame, lastFrame do
                    local firstSubset = subset and subset > 0 and subset or 1
                    local lastSubset = subset and subset > 0 and subset or #geometry[currentFrame]
                    local cx, sx = math.cos(math.rad(angleX)), math.sin(math.rad(angleX))
                    local cy, sy = math.cos(math.rad(angleY)), math.sin(math.rad(angleY))
                    local cz, sz = math.cos(math.rad(angleZ)), math.sin(math.rad(angleZ))
                    for currentSubset = firstSubset, lastSubset do
                        for _, vertex in ipairs(geometry[currentFrame][currentSubset].vertices) do
                            if angleX ~= 0 then vertex.y, vertex.z = vertex.y * cx - vertex.z * sx, vertex.y * sx + vertex.z * cx end
                            if angleY ~= 0 then vertex.x, vertex.z = vertex.x * cy + vertex.z * sy, -vertex.x * sy + vertex.z * cy end
                            if angleZ ~= 0 then vertex.x, vertex.y = vertex.x * cz - vertex.y * sz, vertex.x * sz + vertex.y * cz end
                        end
                    end
                end
            end
        elseif name == 'translateFrame' then
            mesh[name] = function(_, frame, dx, dy, dz, subset)
                calls[#calls+1] = {name, frame, dx, dy, dz, subset}
                local firstFrame = frame <= 0 and 1 or frame
                local lastFrame = frame <= 0 and #geometry or frame
                for currentFrame = firstFrame, lastFrame do
                    local firstSubset = subset and subset > 0 and subset or 1
                    local lastSubset = subset and subset > 0 and subset or #geometry[currentFrame]
                    for currentSubset = firstSubset, lastSubset do
                        for _, vertex in ipairs(geometry[currentFrame][currentSubset].vertices) do
                            vertex.x, vertex.y, vertex.z = vertex.x + dx, vertex.y + dy, vertex.z + dz
                        end
                    end
                end
            end
        else
            mesh[name] = function(_, ...) calls[#calls+1] = {name, ...} end
        end
    end
    return mesh, calls
end
local function values(overrides)
    local xf = {frame=0,subset=0,rx=0,ry=0,rz=0,sx=1,sy=1,sz=1,dx=0,dy=0,dz=0}
    for k,v in pairs(overrides or {}) do xf[k]=v end
    return xf
end
local function blocked(operation, overrides, expected)
    local mesh,calls=fixture(true)
    local ok,err=pcall(Transform.apply,mesh,operation,values(overrides))
    assert(not ok and err==expected,tostring(err))
    assert(#calls==0,'rejection must precede every mutation')
end
for _, operation in ipairs({'rotate','translate','centralize','centralizeItself'}) do
    blocked(operation,{},'mesh_debug_skeletal_transform_blocked')
end
blocked('centralize',{subset=2},'mesh_debug_skeletal_transform_blocked')
blocked('combined',{sx=2,sy=2,sz=2,dx=4},'mesh_debug_skeletal_transform_blocked')
blocked('combined',{sx=2,sy=2,sz=2,rx=90},'mesh_debug_skeletal_transform_blocked')
for _, scale in ipairs({{2,1,2},{-1,-1,-1},{0,0,0},{math.huge,math.huge,math.huge},{0/0,1,1}}) do
    blocked('scale',{sx=scale[1],sy=scale[2],sz=scale[3]},'bones_uniform_positive_scale_required')
end
for _, operation in ipairs({'scale','combined'}) do
    local mesh,calls=fixture(true)
    Transform.apply(mesh,operation,values({sx=2,sy=2,sz=2}))
    assert(#calls==1 and calls[1][1]=='scaleSkeletalAsset' and calls[1][2]==2)
end
for _, operation in ipairs({'scale','combined'}) do
    local mesh,calls=fixture(false,true)
    Transform.apply(mesh,operation,values({sx=2,sy=2,sz=2}))
    assert(#calls==1 and calls[1][1]=='scaleSkeletalAsset' and calls[1][2]==2)
end
for _, skeletal in ipairs({false,true}) do
    local mesh,calls=fixture(skeletal)
    Transform.apply(mesh,'combined',values({frame=1,subset=2,rx=90,sx=2,sy=3,sz=4,dx=5}))
    assert(#calls==5 and calls[1][1]=='translateFrame' and calls[2][1]=='rotateFrame' and
        calls[3][1]=='translateFrame' and calls[4][1]=='scaleFrame' and calls[5][1]=='translateFrame')
end
local mesh,calls=fixture(false)
Transform.apply(mesh,'combined',values({rx=90,sx=2,sy=3,sz=4,dx=5}))
assert(#calls==8 and calls[1][1]=='translateFrame' and calls[2][1]=='rotateFrame' and
    calls[4][1]=='translateFrame' and calls[7][1]=='scaleFrame' and calls[8][1]=='translateFrame')
print('Mesh Debug transform policy OK')

local pivotMesh = fixture(false)
Transform.apply(pivotMesh, 'rotate', values({frame=1, subset=1, rz=90}))
local rotatedA = pivotMesh:getVertex(1, 1, 1)
local rotatedB = pivotMesh:getVertex(1, 1, 2)
local untouched = pivotMesh:getVertex(1, 2, 1)
assert(math.abs(rotatedA.x - 11) < 1e-6 and math.abs(rotatedA.y + 1) < 1e-6)
assert(math.abs(rotatedB.x - 11) < 1e-6 and math.abs(rotatedB.y - 1) < 1e-6)
assert(untouched.x == 0 and untouched.y == 0 and untouched.z == 0)
print('Mesh Debug subset rotation pivot OK')

-- Every axis can drive proportional dimensions, including flat meshes.
for mode = 2, 4 do
    local fields = {'', 'targetWidth', 'targetHeight', 'targetDepth'}
    local targets = {0, 8, 12, 20}
    local xf = {keepRatio=mode, targetWidth=1, targetHeight=1, targetDepth=1}
    xf[fields[mode]] = targets[mode]
    Transform.syncRatio(xf, {width=2, height=3, depth=5})
    assert(xf.targetWidth==8 and xf.targetHeight==12 and xf.targetDepth==20)
end
local xf = {keepRatio=2, targetWidth=8, targetHeight=1, targetDepth=1}
Transform.syncRatio(xf, {width=2, height=3, depth=0})
assert(xf.targetHeight==12 and xf.targetDepth==0)
xf.keepRatio=1
xf.targetWidth=10
Transform.syncRatio(xf, {width=2, height=3, depth=5})
assert(xf.targetHeight==12 and xf.targetDepth==0)
xf.keepRatio=4 -- A zero reference dimension cannot supply a ratio.
Transform.syncRatio(xf, {width=2, height=3, depth=0})
assert(xf.targetWidth==10 and xf.targetHeight==12)
-- Bulk inputs use each mesh's own proportions, not a shared reference mesh.
xf.keepRatio=2
Transform.syncRatio(xf, {width=5, height=1, depth=2})
assert(xf.targetHeight==2 and xf.targetDepth==4)
-- Use actual vertex data so the regression checks handedness, normals, UVs and scope.
local function geometryFixture(indexed)
    local frames = {}
    for f = 1, 2 do
        frames[f] = {}
        for sub = 1, 2 do
            frames[f][sub] = {
                vertices={{x=1,y=2,z=3,nx=0,ny=-0.8,nz=0.6,u=0,v=0},
                    {x=3,y=2,z=3,nx=0,ny=-0.8,nz=0.6,u=1,v=0},
                    {x=1,y=5,z=7,nx=0,ny=-0.8,nz=0.6,u=0,v=1}},
                indices=indexed and {1,2,3} or nil,
            }
        end
    end
    local mesh = fixture(false)
    function mesh:getTotalFrame() return #frames end
    function mesh:getTotalSubset(f) return #frames[f] end
    function mesh:getTotalVertex(f,sub) return #frames[f][sub].vertices end
    function mesh:getIndex(f,sub)
        local indices=frames[f][sub].indices
        return indices and {table.unpack(indices)}
    end
    function mesh:addIndex(f,sub,indices) frames[f][sub].indices=indices end
    function mesh:getVertex(f,sub,v)
        local result={}
        for k,value in pairs(frames[f][sub].vertices[v]) do result[k]=value end
        return result
    end
    function mesh:setVertex(f,sub,v,vertex) frames[f][sub].vertices[v]=vertex end
    return mesh
end
for _,indexed in ipairs({false,true}) do
    for mask = 1, 7 do
        local mesh=geometryFixture(indexed)
        local invert = {frame=2, subset=1, invertX=(mask & 1)~=0,
            invertY=(mask & 2)~=0, invertZ=(mask & 4)~=0}
        local sx,sy,sz=invert.invertX and -1 or 1,invert.invertY and -1 or 1,invert.invertZ and -1 or 1
        Transform.apply(mesh,'invert',invert)
        local indices=mesh:getIndex(2,1) or {1,2,3}
        local a,b,c=mesh:getVertex(2,1,indices[1]),mesh:getVertex(2,1,indices[2]),mesh:getVertex(2,1,indices[3])
        local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
        local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
        local dot=(uy*vz-uz*vy)*a.nx+(uz*vx-ux*vz)*a.ny+(ux*vy-uy*vx)*a.nz
        assert(dot>0,'reflection turned the triangle inside out')
        assert(a.x==sx and a.y==2*sy and a.z==3*sz)
        assert(a.ny==-0.8*sy and a.nz==0.6*sz)
        for _,v in ipairs({a,b,c}) do
            assert((v.x==3*sx)==(v.u==1) and (v.y==5*sy)==(v.v==1),'UV detached from position')
        end
        assert(mesh:getVertex(1,1,1).x==1 and mesh:getVertex(2,2,1).y==2,'unselected geometry changed')
        Transform.apply(mesh,'invert',invert)
        for v=1,3 do
            local original=geometryFixture(indexed):getVertex(2,1,v)
            for key,value in pairs(mesh:getVertex(2,1,v)) do assert(value==original[key]) end
        end
        if indexed then assert(table.concat(mesh:getIndex(2,1),',')=='1,2,3') end
        invert.frame,invert.subset=0,0
        Transform.apply(mesh,'invert',invert)
        assert(mesh:getVertex(1,2,1).x==sx and mesh:getVertex(2,2,1).z==3*sz)
    end
end
blocked('invert',{invertX=true},'bones_uniform_positive_scale_required')
local noOp, noCalls = fixture(false)
Transform.apply(noOp,'invert',{})
assert(#noCalls==0)
print('Mesh Debug ratio and inversion OK')

-- Unsupported topology must fail before any position or normal is mutated.
for _,mode in ipairs({'TRIANGLE_STRIP','TRIANGLE_FAN'}) do
    local mesh=geometryFixture(false)
    function mesh:getModeDraw() return mode end
    local ok,err=pcall(Transform.apply,mesh,'invert',{frame=1,subset=1,invertX=true})
    assert(not ok and err=='transform_invert_requires_triangles')
    assert(mesh:getVertex(1,1,1).x==1)
end
local skinned=geometryFixture(false)
function skinned:getSkeletonBindReport() return {canonical=true,boneCount=1} end
local ok,err=pcall(Transform.apply,skinned,'invert',{frame=1,subset=1,invertX=true})
assert(not ok and err=='transform_invert_skin_order')
assert(skinned:getVertex(1,1,1).x==1)
