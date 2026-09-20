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
local Canvas=require 'image_mesh_canvas'
local IO=require 'image_mesh_io'
local Presets=require 'image_mesh_presets'
local api={}; assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local started,baseline
local function close(a,b) assert(math.abs(a-b)<.00001,tostring(a)..' != '..tostring(b)) end
local function vertices(asset,report)
    assert(asset:check())
    return asset:getVertex(1,1,1,report.vertices)
end
local function run()
    init()
    local path='/tmp/ime_back_uv.png'
    local pixels={}
    for y=0,95 do for x=0,127 do
        pixels[#pixels+1]=x*2; pixels[#pixels+1]=y*2; pixels[#pixels+1]=150
    end end
    assert(mbm.createTexture(pixels,128,96,3,'ime_back_uv',path))
    for _,adaptive in ipairs{false,true} do
        for _,shape in ipairs{'rectangle','ellipse','polygon'} do
            local o={x=8,y=8,cropWidth=33,cropHeight=33,width=50,height=40,depth=10,relief=4,
                shape=shape,columns=8,rows=8,ellipseSegments=16,followImage=adaptive,lockBorder=false,
                contour={{x=0,y=0},{x=1,y=0},{x=.6,y=.5},{x=1,y=1},{x=0,y=1}}}
            local flat,fr=mbm.generateImageMesh(path,o);assert(flat,fr)
            local fv=vertices(flat,fr);local n=0
            for _,v in ipairs(fv) do if v.z>0 then break end;n=n+1 end
            local nb=0
            for i=n+1,#fv do if fv[i].z<0 then break end;nb=nb+1 end
            o.backRemap=true;o.backX=70;o.backY=40;o.backCropWidth=40;o.backCropHeight=40
            local remap,rr=mbm.generateImageMesh(path,o);assert(remap,rr)
            local rv=vertices(remap,rr)
            assert(rr.vertices==fr.vertices and rr.triangles==fr.triangles)
            for i,a in ipairs(fv) do
                local b=rv[i]
                for _,key in ipairs{'x','y','z','nx','ny','nz'} do close(a[key],b[key]) end
                if i>n and i<=n+nb then
                    close(b.u,(70+(a.x/o.width+.5)*39+.5)/128)
                    close(b.v,(40+(.5-a.y/o.height)*39+.5)/96)
                else close(a.u,b.u);close(a.v,b.v) end
            end
            o.backMirror=true
            local mirrored,mr=mbm.generateImageMesh(path,o);assert(mirrored,mr)
            local mv=vertices(mirrored,mr)
            for i=n+1,n+nb do close(mv[i].u+rv[i].u,(140+40)/128) end
            o.backX=120
            assert(not mbm.generateImageMesh(path,o),'out-of-image crop accepted')
            o.backRemap=false;o.backOpen=true
            local opened,oreport=mbm.generateImageMesh(path,o);assert(opened,oreport)
            local ov=vertices(opened,oreport);local indices=opened:getIndex(1,1)
            assert(oreport.vertices==fr.vertices-nb,'back vertices retained')
            local backFaces=0
            local fi=flat:getIndex(1,1)
            for i=1,#fi,3 do if fv[fi[i]].z>0 and fv[fi[i+1]].z>0 and fv[fi[i+2]].z>0 then backFaces=backFaces+1 end end
            assert(oreport.triangles==fr.triangles-backFaces,'back triangles retained')
            for i=1,#indices,3 do
                assert(not (ov[indices[i]].z>0 and ov[indices[i+1]].z>0 and ov[indices[i+2]].z>0),'back face generated')
            end
            for i=1,n do
                for _,key in ipairs{'x','y','z','nx','ny','nz','u','v'} do close(ov[i][key],fv[i][key]) end
            end
            for i=n+1,#ov do
                for _,key in ipairs{'x','y','z','nx','ny','nz','u','v'} do close(ov[i][key],fv[i+nb][key]) end
            end
            o.maxVertices=oreport.vertices;o.maxTriangles=oreport.triangles
            assert(mbm.generateImageMesh(path,o),'open mesh wrongly budgeted as closed')
            o.maxVertices=oreport.vertices-1
            assert(not mbm.generateImageMesh(path,o),'open vertex budget ignored')
        end
    end
    api.openImage(path)
    assert(api.action(function(p)
        Model.add(p,'ellipse',8,8,33,33)
    end))
    api.select(1,false)
    local E=api.state
    E.values.backRemap=true;assert(api.applyProperties())
    local original=Model.copy(E.project.regions[1])
    E.tool='back_uv';Canvas.sync(E)
    local function touch(fn,x,y)
        local t=Canvas.transform(E)
        fn(0,(t.x+x*t.scale)/E.camera2d.sx,(t.y+y*t.scaleY)/E.camera2d.sy)
    end
    touch(onTouchDown,24,24);assert(E.drag and E.drag.mode=='back_move','overlapping front intercepted UV drag')
    touch(onTouchMove,64,44);touch(onTouchUp,64,44)
    local r=E.project.regions[1]
    assert(r.backCrop.x==48 and r.backCrop.y==28,'move failed')
    assert(r.x==original.x and r.y==original.y and r.w==original.w and r.h==original.h,'UV move changed front')
    local before=Model.copy(r.backCrop)
    touch(onTouchDown,80,60);assert(E.drag and E.drag.mode=='back_resize','resize not hit')
    touch(onTouchMove,92,66);touch(onTouchUp,92,66)
    assert(E.project.regions[1].backCrop.w==45 and E.project.regions[1].backCrop.h==39,'resize failed')
    api.undo(false);assert(E.project.regions[1].backCrop.w==before.w,'UV undo failed')
    api.undo(true);assert(E.project.regions[1].backCrop.w==45,'UV redo failed')
    E.tool='back_uv'
    touch(onTouchDown,60,40);touch(onTouchMove,70,45)
    Canvas.cancel(E);assert(E.project.regions[1].backCrop.x==48,'cancel failed')
    assert(api.action(function(p) Presets.store(p,'Remap',E.values) end))
    api.saveProject('/tmp/ime_back_uv.imesh')
    api.openProject('/tmp/ime_back_uv.imesh')
    api.select(1,false)
    local saved=E.project.regions[1].backCrop
    assert(saved.x==48 and saved.w==45,'UV persistence failed')
    assert(not E.project.presets[1].settings.backCrop,'preset captured image-specific UV crop')
    assert(api.action(function(p) Presets.apply(p,1,E.selection,false) end))
    assert(E.project.regions[1].backCrop.x==48,'preset replaced UV placement')
    assert(api.exportOne('/tmp/ime_back_uv_export.msh'))
    local source,report=mbm.generateImageMesh(path,Model.options(E.project,E.project.regions[1]));assert(source,report)
    local exported=meshDebug:new();assert(exported:load('/tmp/ime_back_uv_export.msh'))
    local a,b=vertices(source,report),vertices(exported,report)
    for i,v in ipairs(a) do close(v.u,b[i].u);close(v.v,b[i].v);close(v.z,-b[i].z) end
    E.tool='back_uv';Canvas.sync(E)
    touch(onTouchDown,110,80);assert(E.panDrag,'empty UV area did not pan')
    touch(onTouchUp,110,80)
    local sample={shape='polygon',x=1,y=2,w=30,h=40,contour={{x=0,y=0},{x=.2,y=.5},{x=1,y=1}}}
    local mirrored=Model.backRegion(sample,true)
    close(mirrored.contour[2].x,.8);close(sample.contour[2].x,.2)
    local modes=Model.new(path,128,96)
    local mr=Model.add(modes,'rectangle',0,0,20,20)
    modes.defaults.backOpen=true;mr.overrides.backRelief=true
    local options=Model.options(modes,mr)
    assert(options.backRelief and not options.backOpen,'default mixed incompatible back modes')
    print('BACK OPEN / UV / BUDGET / CANVAS MOVE RESIZE / HISTORY / PRESETS / EXPORT OK')
    started=mbm.getTimeRun()
end
function onInitScene()
    local ok,err=pcall(run)
    if not ok then print('BACK UV FAIL '..tostring(err));mbm.quit() end
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
    local E=api.state
    if mbm.getTimeRun()-started>1 and not baseline then baseline={canvas=E.canvasBuilds,mesh=E.builds} end
    if mbm.getTimeRun()-started>3 then
        assert(E.canvasBuilds==baseline.canvas and E.builds==baseline.mesh,'idle UV rebuild')
        print('BACK UV GUI / IDLE OK');mbm.quit()
    end
end
