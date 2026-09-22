--[[---------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------]]

package.path='editor/?.lua;'..package.path
local Model=require 'image_mesh_model'
local IO=require 'image_mesh_io'
local Presets=require 'image_mesh_presets'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local started
local function data(asset,report)
    assert(asset:check())
    local vertices=asset:getVertex(1,1,1,report.vertices)
    local indices=asset:getIndex(1,1)
    local edges={};local volume=0
    local function key(v) return string.format('%.5f,%.5f,%.5f',v.x+0.,v.y+0.,v.z+0.) end
    for _,v in ipairs(vertices) do
        assert(v.x==v.x and v.y==v.y and v.z==v.z)
        assert(math.abs(v.nx*v.nx+v.ny*v.ny+v.nz*v.nz-1)<1e-4)
    end
    for i=1,#indices,3 do
        local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
        volume=volume+(a.x*(b.y*c.z-b.z*c.y)+a.y*(b.z*c.x-b.x*c.z)+a.z*(b.x*c.y-b.y*c.x))/6
        for _,pair in ipairs{{a,b},{b,c},{c,a}} do
            local ka,kb=key(pair[1]),key(pair[2]);assert(ka~=kb,'degenerate edge')
            local k=ka<kb and ka..'/'..kb or kb..'/'..ka
            local e=edges[k] or {n=0,balance=0};edges[k]=e
            e.n=e.n+1;e.balance=e.balance+(ka<kb and 1 or -1)
        end
    end
    for _,e in pairs(edges) do assert(e.n==2 and e.balance==0,'open/nonmanifold painted mesh') end
    assert(volume>0)
    return vertices,indices
end

local function close(a,b) assert(math.abs(a-b)<0.00001,tostring(a)..' != '..tostring(b)) end
local function run()
    init()
    local pixels={}
    for y=0,32 do for x=0,48 do
        local value=x<24 and 30 or 230
        pixels[#pixels+1]=value;pixels[#pixels+1]=value;pixels[#pixels+1]=value
    end end
    local path='/tmp/ime_back.png'
    assert(mbm.createTexture(pixels,49,33,3,'ime_back',path))
    for _,adaptive in ipairs{false,true} do
        for _,shape in ipairs{'rectangle','ellipse','polygon'} do
            local o={shape=shape,followImage=adaptive,twoLevels=true,columns=8,rows=8,ellipseSegments=16,
                width=50,height=40,depth=10,relief=6,lockBorder=false,x=3,y=2,cropWidth=41,cropHeight=29,
                contour={{x=0,y=0},{x=1,y=0},{x=.6,y=.5},{x=1,y=1},{x=0,y=1}},
                heightEdits={{x=.3,y=.5,radius=.15,strength=.6,height=.6,mode='flatten'}}}
            local flat,fr=mbm.generateImageMesh(path,o);assert(flat,fr)
            local fv=data(flat,fr)
            o.backRelief=true
            local asset,report=mbm.generateImageMesh(path,o);assert(asset,report)
            local vertices=data(asset,report)
            local front=0
            for _,v in ipairs(vertices) do if v.z>0 then break end;front=front+1 end
            assert(front>0)
            for i=1,front do
                local a,b=vertices[i],vertices[front+i]
                close(a.x,b.x);close(a.y,b.y);close(a.z,-b.z)
                close(a.nx,b.nx);close(a.ny,b.ny);close(a.nz,-b.nz)
                close(a.u,b.u);close(a.v,b.v)
                for _,k in ipairs{'x','y','z','nx','ny','nz','u','v'} do close(a[k],fv[i][k]) end
                assert(b.z-a.z>=o.depth,'relief reduced thickness')
            end
            o.backMirror=true
            local mirrored,mr=mbm.generateImageMesh(path,o);assert(mirrored,mr)
            local mv=data(mirrored,mr)
            assert(mr.vertices==report.vertices and mr.triangles==report.triangles)
            for i,a in ipairs(vertices) do
                local b=mv[i]
                for _,k in ipairs{'x','y','z','nx','ny','nz','v'} do close(a[k],b[k]) end
                if i>front and i<=2*front then close(a.u+b.u,(2*o.x+o.cropWidth)/49)
                else close(a.u,b.u) end
            end
            o.maxVertices=report.vertices-1
            assert(not mbm.generateImageMesh(path,o),'copied back escaped vertex budget')
            o.maxVertices=65535;o.maxTriangles=report.triangles-1
            assert(not mbm.generateImageMesh(path,o),'copied back escaped triangle budget')
            o.maxTriangles=131070;o.backRelief=false
            local mirroredFlat,mfr=mbm.generateImageMesh(path,o);assert(mirroredFlat,mfr)
            local mfv=data(mirroredFlat,mfr)
            assert(mfr.vertices==fr.vertices and mfr.triangles==fr.triangles,'mirror changed counts')
            for i,a in ipairs(fv) do
                local b=mfv[i]
                for _,k in ipairs{'x','y','z','nx','ny','nz','v'} do close(a[k],b[k]) end
            end
        end
    end
    local E=api.state
    api.openImage(path)
    assert(api.action(function(p) Model.primitive(p,'circle',30,30) end))
    api.select(1,false)
    E.values.backRelief=true;E.values.backMirror=true;E.values.followImage=true
    assert(api.applyProperties())
    assert(api.action(function(p) Presets.store(p,'Verso',E.values) end))
    api.saveProject('/tmp/ime_back.imesh')
    api.openProject('/tmp/ime_back.imesh')
    assert(E.project.presets[1].settings.backRelief and E.project.presets[1].settings.backMirror)
    api.select(1,false)
    assert(E.values.backRelief and E.values.backMirror)
    assert(api.exportOne('/tmp/ime_back_export.msh'))
    local original,report=mbm.generateImageMesh(path,Model.options(E.project,E.project.regions[1]))
    assert(original,report)
    local exported=meshDebug:new();assert(exported:load('/tmp/ime_back_export.msh'))
    local a=original:getVertex(1,1,1,report.vertices)
    local b=exported:getVertex(1,1,1,report.vertices)
    for i,v in ipairs(a) do
        for _,k in ipairs{'x','y','z','nx','ny','nz','u','v'} do
            local sign=(k=='x' or k=='z' or k=='nx' or k=='nz') and -1 or 1
            close(b[i][k],sign*v[k])
        end
    end
    local legacy=Model.copy(E.project)
    legacy.defaults.backRelief=nil;legacy.defaults.backMirror=nil
    legacy.regions[1].overrides.backRelief=nil;legacy.regions[1].overrides.backMirror=nil
    legacy.presets[1].settings.backRelief=nil;legacy.presets[1].settings.backMirror=nil
    IO.save(legacy,'/tmp/ime_back_legacy.imesh',tUtil.save)
    local loaded=IO.load('/tmp/ime_back_legacy.imesh')
    assert(not Model.options(loaded,loaded.regions[1]).backRelief)
    assert(not loaded.presets[1].settings.backMirror)
    api.setEditMode(false); E.orbit.azimuth=math.pi+.4; api.camera()
    print('BACK GEOMETRY / NORMALS / CLOSURE / UV / BUDGET / PRESETS / EXPORT OK')
    started=mbm.getTimeRun()
end
function onInitScene()
    local ok,err=pcall(run)
    if not ok then print('BACK FAIL '..tostring(err));mbm.quit() end
end
function onLoop(delta)
    if not started then return end
    local header=tImGui.CollapsingHeader
    tImGui.CollapsingHeader=function(label,...)
        if label==tLang.L('ime_back_group') then tImGui.SetNextItemOpen(true,0) end
        return header(label,...)
    end
    loop(delta)
    tImGui.CollapsingHeader=header
    if mbm.getTimeRun()-started>3 then print('BACK EDITOR UI OK');mbm.quit() end
end
