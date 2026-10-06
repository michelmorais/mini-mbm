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

-- Run with mini-mbm --scene src/test-lib/mesh_debug_articulated_scale_smoke.lua
-- --disable_select_monitor --nosplash. Optional MBM_SCALE_TEST_ASSET is read-only.
package.path = 'editor/?.lua;' .. package.path
local Transform = require 'mesh_debug_transform'
local files = {}

local function snapshot(mesh)
    local out = {parts={}, clips={}, vertices={}}
    for p=1,mesh:getTotalArticulatedParts() do
        out.parts[p] = table.pack(mesh:getArticulatedPart(p))
    end
    for c=1,mesh:getTotalArticulatedAnimations() do
        local clip = {meta=table.pack(mesh:getArticulatedAnimation(c)), tracks={}}
        out.clips[c] = clip
        for t=1,mesh:getTotalArticulatedTracks(c) do
            local track = {meta=table.pack(mesh:getArticulatedTrack(c,t)), keys={}}
            clip.tracks[t] = track
            for k=1,track.meta[3] do track.keys[k]=table.pack(mesh:getArticulatedKey(c,t,k)) end
        end
    end
    for f=1,mesh:getTotalFrame() do
        for s=1,mesh:getTotalSubset(f) do
            for v=1,mesh:getTotalVertex(f,s) do
                out.vertices[#out.vertices+1]=mesh:getVertex(f,s,v)
            end
        end
    end
    return out
end

local function equal(a,b)
    assert(type(a)==type(b), 'type changed')
    if type(a)=='table' then
        for k,v in pairs(a) do equal(v,b[k]) end
        for k in pairs(b) do assert(a[k]~=nil,'field added') end
    elseif type(a)=='number' then
        assert(math.abs(a-b)<=math.max(1,math.abs(a))*2e-6, tostring(a)..' ~= '..tostring(b))
    else assert(a==b,'metadata changed') end
end

local function expectedScaled(before,sx,sy,sz)
    for _,p in ipairs(before.parts) do p[5],p[6],p[7]=p[5]*sx,p[6]*sy,p[7]*sz end
    for _,c in ipairs(before.clips) do
        for _,t in ipairs(c.tracks) do
            for _,k in ipairs(t.keys) do k[2],k[3],k[4]=k[2]*sx,k[3]*sy,k[4]*sz end
        end
    end
    for _,v in ipairs(before.vertices) do v.x,v.y,v.z=v.x*sx,v.y*sy,v.z*sz end
    return before
end

local function roundtrip(mesh)
    local path=os.tmpname()
    files[#files+1]=path
    assert(mesh:save(path,false,false))
    local loaded=meshDebug:new()
    assert(loaded:load(path))
    return loaded
end

local function fixture()
    local mesh=meshDebug:new()
    mesh:setType('mesh')
    mesh:setModeDraw('TRIANGLES')
    for f=1,2 do
        mesh:addFrame(3)
        for s=1,2 do
            mesh:addSubSet(f)
            mesh:addVertex(f,s,{
                {x=10*s,y=2,z=f,u=0,v=0},
                {x=10*s+4,y=2,z=f,u=1,v=0},
                {x=10*s,y=6,z=f+3,u=0,v=1}})
            mesh:addIndex(f,s,{1,2,3})
            mesh:setTexture(f,s,'#FFFFFFFF')
            local id=(f-1)*2+s
            mesh:addArticulatedPart(id,f,s,'part'..id,10*s+1,3,f+1,0,0,0,1,
                s==2 and id-1 or 0)
        end
    end
    for c=1,2 do
        local clip=mesh:addArticulatedAnimation('clip'..c,2,1.5,c,true,c-1)
        for id=1,4 do
            local track=mesh:addArticulatedTrack(clip,id,7)
            for k=1,3 do
                local time=(k-1)*0.5
                mesh:addArticulatedKey(clip,track,time,k,-2*k,3*k,0,0,0,1,1.2,0.8,1)
                mesh:setArticulatedKeyEuler(clip,track,time,20*k,359.99,0)
                mesh:setArticulatedKeyEasing(clip,track,k,5)
                mesh:setArticulatedKeyBezier(clip,track,k,0.2,-0.3,0.8,1.3)
            end
        end
    end
    return roundtrip(mesh)
end

local function verify(mesh,sx,sy,sz)
    local before=snapshot(mesh)
    assert(#before.parts>0 and #before.clips>0,'fixture must contain articulation')
    Transform.apply(mesh,'scale',{frame=0,subset=0,sx=sx,sy=sy,sz=sz})
    equal(expectedScaled(before,sx,sy,sz),snapshot(mesh))
    local loaded=roundtrip(mesh)
    equal(snapshot(mesh),snapshot(loaded))
    Transform.apply(loaded,'scale',{frame=0,subset=0,sx=1/sx,sy=1/sy,sz=1/sz})
    equal(expectedScaled(before,1/sx,1/sy,1/sz),snapshot(loaded))
end

function onInitScene()
    local ok,err=pcall(function()
        verify(fixture(),0.71,0.71,0.71)
        verify(fixture(),2,3,4)
        local partial=fixture()
        local before=snapshot(partial)
        Transform.apply(partial,'scale',{frame=1,subset=2,sx=2,sy=2,sz=2})
        local after=snapshot(partial)
        equal(before.parts,after.parts)
        equal(before.clips,after.clips)
        local path=os.getenv('MBM_SCALE_TEST_ASSET')
        if path and path~='' then
            local real=meshDebug:new()
            assert(real:load(path))
            local initial=snapshot(real)
            local lo,hi=math.huge,-math.huge
            for _,v in ipairs(initial.vertices) do lo=math.min(lo,v.x);hi=math.max(hi,v.x) end
            local factor=60/(hi-lo)
            verify(real,factor,factor,factor)
            local resized=snapshot(real)
            lo,hi=math.huge,-math.huge
            for _,v in ipairs(resized.vertices) do lo=math.min(lo,v.x);hi=math.max(hi,v.x) end
            equal(60,hi-lo)
            print('REAL ASSET RESIZE TO 60 OK: '..#initial.parts..' parts, '..#initial.clips..' clips')
        end
        print('ARTICULATED SCALE SMOKETEST OK')
    end)
    for _,path in ipairs(files) do meshDebug:fakeRelease(path);os.remove(path) end
    if not ok then print('ARTICULATED SCALE SMOKETEST FAIL: '..tostring(err)) end
    mbm.quit()
end
function onLoop(delta) end
