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

-- Run through mini-mbm; optional MBM_CAPTURE_TEST_ASSET points to a read-only real asset.
package.path='editor/?.lua;'..package.path
dofile('editor/mesh_debug.lua')
local paths={}
local function snapshot(d)
    local frames={}
    for f=1,d:getTotalFrame() do
        frames[f]={}
        for s=1,d:getTotalSubset(f) do
            local vertices={}
            for v=1,d:getTotalVertex(f,s) do vertices[v]=d:getVertex(f,s,v) end
            frames[f][s]={vertices=vertices,indices=d:getIndex(f,s)}
        end
    end
    return frames
end
local function equal(a,b)
    if type(a)~=type(b) then return false end
    if type(a)~='table' then return a==b end
    for k,v in pairs(a) do if not equal(v,b[k]) then return false end end
    for k in pairs(b) do if a[k]==nil then return false end end
    return true
end
local function clone(d)
    local path=os.tmpname(); paths[#paths+1]=path
    assert(d:save(path,false,false))
    local copy=meshDebug:new(); assert(copy:load(path)); return copy
end
local function fixture(unindexedSubset, weighted)
    local d=meshDebug:new();d:setType('mesh');d:setModeDraw('TRIANGLES')
    for f=1,weighted and 1 or 2 do
        d:addFrame(3)
        for s=1,5 do
            d:addSubSet(f)
            local vertices={}
            for i=1,s+1 do
                for v=1,3 do
                    vertices[#vertices+1]={x=s*20+i,y=v,z=f,nx=0,ny=0,nz=1,u=i/10,v=v/10}
                end
            end
            assert(d:addVertex(f,s,vertices))
            local indices={};for i=1,#vertices do indices[i]=i end
            if s~=unindexedSubset then assert(d:addIndex(f,s,indices)) end
            d:setTexture(f,s,'#FFFFFFFF')
        end
        if not weighted then assert(d:moveSubsetUp(f,4));assert(d:moveSubsetUp(f,3)) end
    end
    if weighted then
        d:initializeSkeletalSkeleton('root',0,0,0,1,1)
        d:addSkeletalBone(1,'child',60,0,0,1,1)
        d:initializeSkeletalVertexWeights(1)
        local edits,index={},0
        for s=1,5 do for v=1,d:getTotalVertex(1,s) do
            index=index+1
            edits[#edits+1]={index,d:getVertex(1,s,v).x>60 and 'child' or 'root',1}
        end end
        assert(d:setSkeletalVertexWeightsBatch(edits))
        assert(d:moveSubsetUp(1,4));assert(d:moveSubsetUp(1,3))
    end
    assert(d:addAnim('Static',1,weighted and 1 or 2,1,0));return d
end
local function triangles(data,out)
    out=out or {}
    local indices=data.indices or {}
    if not data.indices then for i=1,#data.vertices do indices[i]=i end end
    for i=1,#indices,3 do
        local parts={}
        for j=0,2 do
            local p=data.vertices[indices[i+j]]
            parts[#parts+1]=string.format('%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g',p.x,p.y,p.z,p.nx,p.ny,p.nz,p.u,p.v)
        end
        local key=table.concat(parts,';');out[key]=(out[key] or 0)+1
    end
    return out
end
local function verify(d)
    local before=snapshot(d)
    local source=before[1][1]
    local indices=source.indices
    local texture=d:getTexture(1,1)
    local group={frame=1,subset=1,vertices=source.vertices,indices=indices,
        triangles={{indices[1],indices[2],indices[3]}},texture=texture,
        signature=splitCaptureGetSubsetSignature(d,1,1)}
    local entry={info={hasNormal=true}}
    assert(splitCaptureApply(entry,d,{groups={group},faces=1,frames=1}))
    local after=snapshot(d)
    if d:hasSkeletalVertexWeights() then
        for _,asset in ipairs({d,clone(d)}) do
            for s=1,asset:getTotalSubset(1) do for v=1,asset:getTotalVertex(1,s) do
                local bone,weight=asset:getSkeletalVertexWeight(v,s)
                assert(bone==(asset:getVertex(1,s,v).x>60 and 'child' or 'root') and weight==1,'capture detached skin weights')
            end end
        end
    end
    local n=#before[1]
    assert(#after[1]==n+1)
    for s=2,n do assert(equal(before[1][s],after[1][s-1]),'capture changed unrelated subset '..s) end
    for f=2,#before do assert(equal(before[f],after[f]),'capture changed another frame') end
    assert(equal(triangles(source),triangles(after[1][n],triangles(after[1][n+1]))),'capture changed source triangles')
    assert(equal(after,snapshot(clone(d))),'roundtrip changed geometry')
    -- Subset visibility previews remove omitted subsets from a serialized clone.
    for hidden=1,n+1 do
        local preview=clone(d);preview:removeSubset(1,hidden)
        local visible=snapshot(preview)
        for s=1,n+1 do
            if s~=hidden then
                local target=s>hidden and s-1 or s
                assert(equal(after[1][s],visible[1][target]),'hiding subset changed another subset')
            end
        end
    end
end
function onInitScene()
    local ok,err=pcall(function()
        verify(clone(fixture()))
        verify(clone(fixture(nil,true)))
        local weighted=fixture(nil,true)
        local weightedBefore=snapshot(weighted)
        assert(not pcall(function() weighted:addVertex(1,1,1) end),'weighted insertion must be explicit')
        assert(equal(weightedBefore,snapshot(weighted)))
        local unchanged=fixture()
        local initial=snapshot(unchanged)
        assert(unchanged:addVertex(1,2,0))
        assert(equal(initial,snapshot(unchanged)),'zero count cleared indices')
        for _,count in ipairs({-1,2147483648,4294967296}) do
            assert(not pcall(function() unchanged:addVertex(1,2,count) end))
            assert(equal(initial,snapshot(unchanged)),'invalid count mutated geometry')
        end
        -- Extending a reordered unindexed range must shift surviving indexed ranges too.
        local d=fixture()
        local target=d:addSubSet(1);d:addVertex(1,target,3)
        assert(d:moveSubsetUp(1,target))
        local before=snapshot(d)
        d:addVertex(1,target-1,3)
        local after=snapshot(d)
        for s=1,#before[1] do if s~=target-1 then assert(equal(before[1][s],after[1][s])) end end
        d=fixture(2) -- physical subset 2 is logical subset 3 after reordering.
        before=snapshot(d)
        d:addVertex(1,3,3)
        after=snapshot(d)
        assert(#after[1][3].vertices==#before[1][3].vertices+3)
        for s=1,#before[1] do if s~=3 then assert(equal(before[1][s],after[1][s]),'middle insertion changed another subset') end end
        assert(equal(before[2],after[2]))
        local path=os.getenv('MBM_CAPTURE_TEST_ASSET')
        if path then local real=meshDebug:new();assert(real:load(path));verify(real);print('REAL CAPTURE ISOLATION OK') end
        print('CAPTURE RANGES SMOKETEST OK')
    end)
    for _,path in ipairs(paths) do meshDebug:fakeRelease(path);os.remove(path) end
    if not ok then print('CAPTURE RANGES SMOKETEST FAIL: '..tostring(err)) end
    mbm.quit()
end
function onLoop(delta) end
