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

-- Run with mini-mbm --scene src/test-lib/mesh_debug_invert_smoke.lua --disable_select_monitor --nosplash.
-- Optional: MBM_INVERT_TEST_ASSET=/path/to/asset.msh also exercises a real file, read-only.
package.path = 'editor/?.lua;' .. package.path
local Transform = require 'mesh_debug_transform'
local temporaryFiles = {}

local function snapshot(mesh)
    local frames = {}
    for f = 1, mesh:getTotalFrame() do
        frames[f] = {}
        for s = 1, mesh:getTotalSubset(f) do
            local vertices = {}
            for v = 1, mesh:getTotalVertex(f,s) do vertices[v] = mesh:getVertex(f,s,v) end
            frames[f][s] = {vertices=vertices, indices=mesh:getIndex(f,s)}
        end
    end
    return frames
end

local function equal(a,b)
    if type(a) ~= type(b) then return false end
    if type(a) ~= 'table' then return a == b end
    for k,v in pairs(a) do if not equal(v,b[k]) then return false end end
    for k in pairs(b) do if a[k] == nil then return false end end
    return true
end

local function unchangedExcept(before, after, frame, subset)
    assert(#before == #after)
    for f,subsets in ipairs(before) do
        assert(#subsets == #after[f])
        for s,data in ipairs(subsets) do
            if f ~= frame or s ~= subset then
                assert(equal(data,after[f][s]), 'untargeted geometry changed: frame '..f..', subset '..s)
            end
        end
    end
end

local function roundtrip(mesh)
    local path = os.tmpname()
    temporaryFiles[#temporaryFiles+1] = path
    assert(mesh:save(path,false,false))
    local loaded = meshDebug:new()
    assert(loaded:load(path))
    return loaded
end

local function fixture(indexed)
    local mesh = meshDebug:new()
    mesh:setType('mesh')
    mesh:setModeDraw('TRIANGLES')
    for f = 1, 2 do
        assert(mesh:addFrame(3) == f)
        for s = 1, 5 do
            assert(mesh:addSubSet(f) == s)
            local vertices, indices = {}, {}
            for triangle = 1, s do
                local offset = #vertices
                local x = s*10 + triangle
                vertices[offset+1] = {x=x,y=2,z=f,u=0,v=0}
                vertices[offset+2] = {x=x+2,y=2,z=f,u=1,v=0}
                vertices[offset+3] = {x=x,y=5,z=f+4,u=0,v=1}
                for v = 1, 3 do indices[#indices+1] = offset+v end
            end
            assert(mesh:addVertex(f,s,vertices))
            if indexed then assert(mesh:addIndex(f,s,indices)) end
        end
        -- Descriptor order now differs from the physical vertex/index buffer order.
        assert(mesh:moveSubsetUp(f,4))
        assert(mesh:moveSubsetUp(f,3))
    end
    mesh:addNormals()
    for f = 1, 2 do for s = 1, 5 do for v = 1, mesh:getTotalVertex(f,s) do
        local data = mesh:getVertex(f,s,v)
        data.nx,data.ny,data.nz = 0,-0.8,0.6
        mesh:setVertex(f,s,v,data)
    end end end
    assert(mesh:addAnim('Static',1,2,1,0))
    return mesh
end

local function verifyInversion(mesh, mask, checkNormals)
    local before = snapshot(mesh)
    local xf = {frame=1,subset=2,invertX=(mask & 1)~=0,invertY=(mask & 2)~=0,invertZ=(mask & 4)~=0}
    Transform.apply(mesh,'invert',xf)
    local after = snapshot(mesh)
    unchangedExcept(before,after,1,2)
    assert(not equal(before[1][2],after[1][2]), 'selected subset did not change')
    if checkNormals then
        local data = after[1][2]
        local indices = data.indices or {}
        if not data.indices then for v = 1, #data.vertices do indices[v] = v end end
        for i = 1, #indices, 3 do
            local a,b,c = data.vertices[indices[i]],data.vertices[indices[i+1]],data.vertices[indices[i+2]]
            local ux,uy,uz = b.x-a.x,b.y-a.y,b.z-a.z
            local vx,vy,vz = c.x-a.x,c.y-a.y,c.z-a.z
            assert((uy*vz-uz*vy)*a.nx+(uz*vx-ux*vz)*a.ny+(ux*vy-uy*vx)*a.nz > 0,
                'triangle winding disagrees with reflected normal')
        end
    end
    local reloaded = roundtrip(mesh)
    assert(equal(after,snapshot(reloaded)), 'save/reload changed geometry')
    Transform.apply(mesh,'invert',xf)
    assert(equal(before,snapshot(mesh)), 'inversion twice did not restore all subsets')
end

function onInitScene()
    local ok,err = pcall(function()
        for _,indexed in ipairs({false,true}) do
            for mask = 1, 7 do verifyInversion(roundtrip(fixture(indexed)),mask,true) end
        end
        -- Replacement with a different index count must also preserve all other ranges.
        local mesh = fixture(true)
        local before = snapshot(mesh)
        assert(mesh:addIndex(1,2,{1,3,2}))
        unchangedExcept(before,snapshot(mesh),1,2)
        before = snapshot(mesh)
        assert(mesh:addIndex(1,2,{1,2,3,4,5,6,7,8,9}))
        unchangedExcept(before,snapshot(mesh),1,2)
        before = snapshot(mesh)
        local valid = pcall(function() mesh:addIndex(1,2,{65535}) end)
        assert(not valid and equal(before,snapshot(mesh)), 'invalid replacement mutated geometry')
        local path = os.getenv('MBM_INVERT_TEST_ASSET')
        if path and path ~= '' then
            local real = meshDebug:new()
            assert(real:load(path))
            verifyInversion(real,4,false) -- frame 1, subset 2, Invert Z
            print('REAL ASSET SUBSET ISOLATION OK')
        end
        print('MESH INVERT SMOKETEST OK')
    end)
    for _,path in ipairs(temporaryFiles) do
        meshDebug:fakeRelease(path)
        os.remove(path)
    end
    if not ok then print('MESH INVERT SMOKETEST FAIL: '..tostring(err)) end
    mbm.quit()
end
function onLoop(delta) end
