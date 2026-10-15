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

package.path='editor/?.lua;'..package.path
dofile('editor/scene_editor3d.lua')
local init,loop=onInitScene,onLoop
local panel=require 'scene_mesh_panel'
local math3d=require 'scene_mesh_tools'
local path='/tmp/scene3d-bake-smoke.msh'
local function near(a,b) assert(math.abs(a-b)<.0001,tostring(a)..' ~= '..tostring(b)) end
local function read(p) local d=meshDebug:new();assert(d:load(p));return d end
local function fixture()
    local d=meshDebug:new();d:setType('mesh');d:setModeDraw('TRIANGLES');d:addFrame(3);d:addSubSet(1)
    assert(d:addVertex(1,1,{{x=1,y=2,z=3,nx=0,ny=0,nz=1,u=0,v=0},
        {x=4,y=2,z=3,nx=0,ny=0,nz=1,u=1,v=0},{x=1,y=5,z=6,nx=0,ny=0,nz=1,u=0,v=1}}))
    assert(d:addIndex(1,1,{1,2,3}));d:setTexture(1,1,'#FFFFFFFF');d:addAnim('Static',1,1,1,0)
    math3d.resetPhysics(d);assert(d:save(path,false,false))
end
local function positions()
    local out={}
    for i,p in ipairs(tPlacedMeshes) do
        local d=read(p.fileName);local o=getMeshOffset(p.fileName)
        local r=math3d.rotation(p.rotationX+o.rx,p.rotationY+o.ry,p.rotationZ+o.rz)
        local m=math3d.mul(r,math3d.scale(p.scale.x*o.sx,p.scale.y*o.sy,p.scale.z*o.sz))
        out[i]={}
        for v=1,3 do local q=d:getVertex(1,1,v);out[i][v]={math3d.vector(m,q.x,q.y,q.z)} end
    end
    return out
end
local function unchanged(before)
    local after=positions()
    for i,verts in ipairs(before) do for j,v in ipairs(verts) do for k=1,3 do near(v[k],after[i][j][k]) end end end
end
local function checkAnimationFilters()
    assert(panel.includeArticulated==false and panel.includeSkeletal==false)
    local function asset(articulated,skeletal)
        return {
            load=function() return true end,
            getTotalArticulatedParts=function() return articulated and 1 or 0 end,
            getTotalArticulatedAnimations=function() return 0 end,
            getSkeletonBindReport=function() return {boneCount=skeletal and 1 or 0} end,
            getTotalSkeletalWeightBones=function() return 0 end,
        }
    end
    for _,includeA in ipairs({false,true}) do for _,includeS in ipairs({false,true}) do
        panel.includeArticulated,panel.includeSkeletal=includeA,includeS
        for _,hasA in ipairs({false,true}) do for _,hasS in ipairs({false,true}) do
            assert(panel.includesAsset(asset(hasA,hasS))==((not hasA or includeA) and (not hasS or includeS)))
        end end
    end end
    panel.includeArticulated,panel.includeSkeletal=false,false
    local legacy=asset(false,false)
    legacy.getSkeletonBindReport=function() return nil end
    legacy.getTotalSkeletalWeightBones=function() return 2 end
    assert(not panel.includesAsset(legacy))
    -- Exclusion precedes any file staging or scene work, for every bulk operation.
    local factory=meshDebug
    meshDebug={new=function() return asset(true,true) end}
    local ok,err=pcall(function()
        for _,operation in ipairs({'scale','rotate','normals'}) do
            assert(panel.apply({fileName='excluded-animation.msh'},operation,true)==nil)
        end
    end)
    meshDebug=factory
    assert(ok,err)
end
local function run()
    checkAnimationFilters()
    init();fixture();tMapOptions.sMapType='Free';addLayer(false)
    local entry=registerMeshSetEntry(path)
    for i=1,7 do
        local p=addPlacedMesh(path,'mesh',1,0,0,i*10,0,true)
        p.scale=i<=5 and {x=3,y=2,z=2} or {x=1,y=1.5,z=1.5}
        p.rotationX,p.rotationY,p.rotationZ=.2,.3,.4
        syncPlacedMeshTransform(p)
    end
    local tie={{s={2,2,2}},{s={3,3,3}},{s={3,3,3}},{s={2,2,2}}}
    assert(math3d.dominant(tie,'s')==tie[1])
    local periodic={{r={0,0,0}},{r={.2,0,0}},{r={0,math.pi*2,0}}}
    assert(math3d.dominant(periodic,'r')==periodic[1])
    local before=positions()
    panel.apply(entry,'scale',true)
    unchanged(before)
    near(tPlacedMeshes[1].scale.x,1);near(tPlacedMeshes[6].scale.x,1/3);near(tPlacedMeshes[6].scale.y,.75)
    local b=math3d.bounds(read(path));near(b.width,9);near(b.height,6);near(b.depth,6)
    tPlacedMeshes[6].rotationX=.8;tPlacedMeshes[7].rotationZ=-.7
    before=positions()
    local variants=panel.apply(entry,'rotate',true)
    assert(variants>0,'nonuniform minority scale needs a variant')
    unchanged(before)
    near(tPlacedMeshes[1].rotationX,0);near(tPlacedMeshes[1].rotationY,0);near(tPlacedMeshes[1].rotationZ,0)
    local o=getMeshOffset(path);o.rx,o.ry,o.rz=.31,-.23,.42;tMeshOffsets[path]=o
    before=positions();panel.apply(entry,'rotate',false);unchanged(before)
    o=getMeshOffset(path);near(o.rx,0);near(o.ry,0);near(o.rz,0)
    o.sx,o.sy,o.sz=1.2,2.3,3.4;tMeshOffsets[path]=o
    before=positions();panel.apply(entry,'scale',false);unchanged(before)
    -- A failed staged rename must restore the exact source bytes and leave live instances alone.
    local function bytes(file) local f=assert(io.open(file,'rb'));local data=f:read('*a');f:close();return data end
    local oldBytes,oldObject=bytes(path),tPlacedMeshes[1].tObj
    local rename=os.rename
    os.rename=function(from,to)
        if from:find('scene3d%-pending') then return nil,'injected rename failure' end
        return rename(from,to)
    end
    local ok=pcall(panel.apply,entry,'scale',false,{2,2,2})
    os.rename=rename
    assert(not ok and bytes(path)==oldBytes and tPlacedMeshes[1].tObj==oldObject)
    local previous=math3d.bounds(read(path))
    panel.apply(entry,'scale',false,{2,2,2})
    local resized=math3d.bounds(read(path))
    near(resized.width,previous.width*2);near(resized.height,previous.height*2);near(resized.depth,previous.depth*2)
    for mode=1,3 do panel.normalMethod=mode;panel.apply(entry,'normals',false) end
    local d=read(path)
    for v=1,3 do local p=d:getVertex(1,1,v);near(p.nx*p.nx+p.ny*p.ny+p.nz*p.nz,1) end
    local bounds=math3d.bounds(d);local physics=d:getPhysics();assert(#physics==1)
    -- Engine AABB at identity must use the rebuilt physics, including off-center geometry.
    local m=mesh:new('3d');assert(m:load(path));m:setAngle(0,0,0);m:setScale(1,1,1)
    local w,h,depth=m:getAABB(true);near(w,bounds.width);near(h,bounds.height);near(depth,bounds.depth);m:destroy()
    bShowSceneObjectMarkers=false
    assert(writeScene3d('/tmp/scene3d-bake-save.lua',false,false))
    local saved=require('editor_asset_paths').loadScene('/tmp/scene3d-bake-save.lua',mbm);assert(saved.bShowSceneObjectMarkers==false)
    bShowSceneObjectMarkers=true;applyLoadedScene3d(saved);assert(bShowSceneObjectMarkers==false)
    saved.bShowSceneObjectMarkers=nil;applyLoadedScene3d(saved);assert(bShowSceneObjectMarkers==true)
    assert(writeScene3d('/tmp/scene3d-bake-export.lua',false,true))
    assert(dofile('/tmp/scene3d-bake-export.lua').bShowSceneObjectMarkers==nil)
    sActiveTab='mesh_set';iPreviewedMeshSetIndex=1
    print('SCENE3D BAKE GEOMETRY / PHYSICS / NORMALS / SAVE OK')
end
local started,failed,frames=0,false,0
local boundsCalls=0
local bounds=math3d.bounds
function onInitScene()
    local ok,err=pcall(run)
    if not ok then failed=true;print('SCENE3D BAKE FAIL: '..tostring(err));mbm.quit() end
    started=mbm.getTimeRun()
    math3d.bounds=function(...) boundsCalls=boundsCalls+1;return bounds(...) end
end
function onLoop(delta)
    if failed then return end
    local ok,err=pcall(function()
        loop(delta)
        -- Exercise the complete new controls regardless of saved ImGui tab selection.
        tImGui.Begin('Scene mesh transform smoke',false,0)
        local e=registerMeshSetEntry(path)
        panel.button(e,'rotate');panel.button(e,'scale');panel.dimensions(e);panel.footer(e)
        tImGui.End()
    end)
    if not ok then failed=true;print('SCENE3D BAKE FAIL: '..tostring(err));mbm.quit();return end
    frames=frames+1
    if mbm.getTimeRun()-started>2 then assert(boundsCalls==1,'idle UI rescanned mesh geometry');print('SCENE3D BAKE SMOKE OK frames='..frames);mbm.quit() end
end
