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
local Asset=require 'image_mesh_asset'
local function data(asset,report)
    assert(asset:check())
    local vertices,indices=Asset.geometry(asset)
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
    for _,e in pairs(edges) do assert(e.n==2 and e.balance==0,'open/nonmanifold side mesh') end
    assert(volume>0)
    return vertices,indices
end

local job,expected,started
local function close(a,b) assert(math.abs(a-b)<.00002,tostring(a)..' != '..tostring(b)) end
local function run()
    mbm.addPath('/tmp')
    local pixels={}
    for y=0,200 do for x=0,300 do
        local value=(x%100<8 or y%66<8) and 20 or 220
        pixels[#pixels+1]=value;pixels[#pixels+1]=value;pixels[#pixels+1]=value
    end end
    local path='/tmp/ime_perpendicular.png'
    assert(mbm.createTexture(pixels,301,201,3,'ime_perpendicular',path))
    local options={cropWidth=301,cropHeight=201,columns=12,rows=12,width=150,height=100,
        depth=20,relief=12,lockBorder=false,sideMode='band',sideInset=50,
        sideBandPerpendicular=true,backSolid=true}
    local points,limit=mbm.getImageMeshSideContour(options);assert(points,limit)
    assert(#points==8);close(limit,100)
    close(points[1].x,0);close(points[2].x,1);close(points[1].y,.25)
    close(points[3].y,0);close(points[4].y,1);close(points[3].x,250/300)
    for _,adaptive in ipairs{false,true} do for _,invert in ipairs{false,true} do
        options.followImage=adaptive;options.sideBandInvert=invert
        local asset,report=mbm.generateImageMesh(path,options);assert(asset,report);data(asset)
        local sides=asset:getVertex(1,3,1,asset:getTotalVertex(1,3))
        for _,v in ipairs(sides) do
            local x=(v.x/150+.5)*300;local y=(.5-v.y/100)*200
            if (v.z>0)~=invert then
                if math.abs(v.nx)>.9 then x=x+(v.nx>0 and -50 or 50)
                else y=y+(v.ny>0 and 50 or -50) end
            end
            close(v.u,(x+.5)/301);close(v.v,(y+.5)/201)
        end
        assert(asset:getTotalIndex(1,2)==6,'minimal back changed')
        local out='/tmp/ime_perpendicular.msh';assert(asset:save(out,false,false,true))
        local loaded=meshDebug:new();assert(loaded:load(out));data(loaded)
        local a,b=Asset.geometry(asset),Asset.geometry(loaded)
        for i,v in ipairs(a) do close(v.u,b[i].u);close(v.v,b[i].v) end
    end end
    for _,shape in ipairs{'ellipse','polygon'} do
        options.shape=shape;options.ellipseSegments=16
        options.contour={{x=.1,y=.1},{x=.9,y=.15},{x=.7,y=.8},{x=.2,y=.9}}
        options.sideInset=30
        local asset,report=mbm.generateImageMesh(path,options);assert(asset,report)
        local vertices=data(asset)
        for _,v in ipairs(vertices) do assert(v.u>=0 and v.u<=1 and v.v>=0 and v.v<=1) end
        local pairs,why=mbm.getImageMeshSideContour(options);assert(pairs,why)
        assert(#pairs==(shape=='ellipse' and 32 or 8))
        if shape=='polygon' then
            for _,v in ipairs(asset:getVertex(1,3,1,asset:getTotalVertex(1,3))) do
                local px=(v.x/150+.5)*300;local py=(.5-v.y/100)*200
                local du=v.u*301-.5-px;local dv=v.v*201-.5-py
                local matched=false
                for i,a in ipairs(options.contour) do
                    local b=options.contour[i%#options.contour+1]
                    local dx=(b.x-a.x)*300;local dy=(b.y-a.y)*200
                    local length=math.sqrt(dx*dx+dy*dy)
                    if math.abs((px-a.x*300)*dy-(py-a.y*200)*dx)/length<.001 and
                        math.abs(du*dx+dv*dy)/length<.001 then matched=true end
                end
                assert(matched,'skew edge shifted along its tangent')
            end
        end
    end
    print('PERPENDICULAR UV / INVERSION / SHAPES / ROUNDTRIP OK')
    local Model=require 'image_mesh_model'
    local IO=require 'image_mesh_io'
    local Presets=require 'image_mesh_presets'
    tImGui=require 'ImGui'
    local util=require 'editor_utils'
    local project=Model.new(path,301,201)
    Model.primitive(project,'rectangle',150,100)
    project.regions[1].overrides.sideBandPerpendicular=true
    project.regions[1].overrides.sideMode='band'
    Presets.store(project,'Perpendicular',Model.options(project,project.regions[1]))
    IO.save(project,'/tmp/ime_perpendicular.imesh',util.save)
    local loaded=IO.load('/tmp/ime_perpendicular.imesh')
    assert(Model.options(loaded,loaded.regions[1]).sideBandPerpendicular)
    Presets.save(loaded.presets[1],'/tmp/ime_perpendicular.imeshpreset',util.save)
    assert(Presets.load('/tmp/ime_perpendicular.imeshpreset').settings.sideBandPerpendicular)
    project.regions[1].overrides.sideBandPerpendicular=nil;project.defaults.sideBandPerpendicular=nil
    IO.save(project,'/tmp/ime_perpendicular_legacy.imesh',util.save)
    loaded=IO.load('/tmp/ime_perpendicular_legacy.imesh')
    assert(Model.options(loaded,loaded.regions[1]).sideBandPerpendicular==false)
    local userProject=os.getenv('MBM_IMAGE_MESH_TEST_PROJECT')
    if userProject then
        local stone=IO.load(userProject);local o=Model.options(stone,stone.regions[1])
        o.sideBandPerpendicular=true;o.sideInset=100
        local asset,report=mbm.generateImageMesh(stone.image.path,o);assert(asset,report);data(asset)
        local simple,why=asset:simplify(o.simplifyRatio,nil,1,o.simplifyDetails,o.simplifyBoundary)
        assert(simple,why);data(asset)
        print('PERPENDICULAR STONE / 100 PX / SIMPLIFICATION / CLOSED OK')
    end
    options.shape='rectangle';options.followImage=false;options.sideInset=50
    local asset,report=mbm.generateImageMesh(path,options);assert(asset,report)
    expected=Asset.geometry(asset)
    job=assert(mbm.startImageMesh(path,options))
    started=mbm.getTimeRun()
end
function onInitScene()
    local ok,err=xpcall(run,debug.traceback)
    if not ok then print('PERPENDICULAR FAIL '..tostring(err));mbm.quit() end
end
function onLoop()
    if not job then return end
    local ok,err=xpcall(function()
        assert(mbm.getTimeRun()-started<20,'async timeout')
        local status=job:getStatus()
        if status.state=='running' then return end
        assert(status.state=='completed',status.error)
        local asset,report=job:takeResult();assert(asset,report);data(asset)
        local actual=Asset.geometry(asset)
        assert(#actual==#expected)
        for i,v in ipairs(actual) do close(v.u,expected[i].u);close(v.v,expected[i].v) end
        print('PERPENDICULAR UV / INVERSION / SHAPES / PERSISTENCE / ROUNDTRIP / ASYNC OK')
        job=nil;mbm.quit()
    end,debug.traceback)
    if not ok then print('PERPENDICULAR FAIL '..tostring(err));mbm.quit() end
end
