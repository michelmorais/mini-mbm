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
local api={}; assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local frame,started,baseline=0,nil,nil
local function runInit()
    init(); started=mbm.getTimeRun()
    local pixels={}; for y=0,95 do for x=0,127 do
        local v=((x%32)<3 or (y%32)<3) and 20 or 160+(x+y)%70
        pixels[#pixels+1]=v; pixels[#pixels+1]=v; pixels[#pixels+1]=v
    end end
    local source='/tmp/ime_editor_source.png'
    assert(mbm.createTexture(pixels,128,96,3,'ime_editor_source',source))
    assert(api.openImage(source))
    assert(api.action(function(p)
        p.defaults.columns=8; p.defaults.rows=8
        Model.grid(p,{columns=4,rows=3,marginX=0,marginY=0,gapX=0,gapY=0})
        Model.add(p,'ellipse',0,0,32,32)
        Model.fromPoints(p,{{x=32,y=32},{x=63,y=32},{x=63,y=43},{x=43,y=43},{x=43,y=63},{x=32,y=63}})
    end))
    api.setEditMode(false); api.select(13); api.rebuild(); assert(api.state.preview and api.state.report)
    api.select(14); api.rebuild(); assert(api.state.preview and api.state.report)
    local before=api.state.project.defaults.relief
    assert(api.action(function(p) p.defaults.relief=3 end)); api.undo(false)
    assert(api.state.project.defaults.relief==before); api.undo(true); assert(api.state.project.defaults.relief==3)
    assert(api.saveProject('/tmp/ime_editor.imesh')); assert(not api.state.modified)
    assert(tUtil.sMessageOverlay:find('ime_editor.imesh',1,true),'save confirmation missing')
    assert(api.openProject('/tmp/ime_editor.imesh')); assert(#api.state.project.regions==14)
    api.select(1); api.select(2,true)
    api.state.values.depth=40; api.applyProperties()
    assert(Model.options(api.state.project,api.state.project.regions[1]).depth==40)
    assert(Model.options(api.state.project,api.state.project.regions[2]).depth==40)
    assert(Model.options(api.state.project,api.state.project.regions[3]).depth==20)
    api.exportOne('/tmp/ime_editor_selected.msh')
    for _,r in ipairs(api.state.project.regions) do os.remove('/tmp/image-mesh-stage2-export/'..IO.exportName(r)) end
    api.beginBatch('/tmp/image-mesh-stage2-export')
    -- Relinking a missing source restores the saved project only after size validation.
    local missing=Model.copy(api.state.project); missing.image.path='/tmp/ime_missing_source.png'
    IO.save(missing,'/tmp/ime_missing.imesh',tUtil.save)
    assert(not api.openProject('/tmp/ime_missing.imesh')); assert(api.state.missing)
    api.relink(source); assert(not api.state.missing and api.state.modified)
    api.select(14); api.rebuild(); assert(api.state.preview)
    print('IMAGE MESH EDITOR PROJECT / HISTORY / MULTISELECT / RELINK OK')
end
function onInitScene()
    local ok,err=pcall(runInit); if not ok then print('IMAGE MESH EDITOR SMOKE FAIL: '..tostring(err)); mbm.quit() end
end
function onLoop(delta)
    frame=frame+1
    local e=api.state
    local mouse,click,down
    local commands={
        [90]={'rectangle',5,5,true,true},[91]={'rectangle',25,25,false,true},[92]={'rectangle',25,25,false,false},
        [94]={'ellipse',40,5,true,true},[95]={'ellipse',60,25,false,true},[96]={'ellipse',60,25,false,false},
        [98]={'polygon',70,40,true,true},[100]={'polygon',100,40,true,true},[102]={'polygon',100,55,true,true},
        [104]={'polygon',85,55,true,true},[106]={'polygon',85,75,true,true},[108]={'polygon',70,75,true,true},
        [112]={'select',10,10,true,true},[113]={'select',16,14,false,true},[114]={'select',16,14,false,false},
        [117]={'select',31,29,true,true},[118]={'select',35,33,false,true},[119]={'select',35,33,false,false},
        [120]={'select',70,40,true,true},[121]={'select',72,42,false,true},[122]={'select',72,42,false,false},
    }
    if frame==93 then assert(#e.project.regions==15 and e.project.regions[15].w==21) end
    if frame==97 then assert(#e.project.regions==16 and e.project.regions[16].shape=='ellipse') end
    if frame==110 then api.finishPolygon(); assert(#e.project.regions==17) end
    if frame==112 then api.select(15) end
    if frame==115 then assert(e.project.regions[15].x==11 and e.project.regions[15].y==9); api.undo(false); assert(e.project.regions[15].x==5) end
    if frame==116 then api.undo(true); assert(e.project.regions[15].x==11) end
    if frame==120 then assert(e.project.regions[15].w==25 and e.project.regions[15].h==25); api.select(17) end
    if frame==89 then e.tool='rectangle' elseif frame==93 then e.tool='ellipse' elseif frame==97 then e.tool='polygon' elseif frame==111 then e.tool='select' end
    local command=commands[frame]
    local originalCheckbox=tImGui.Checkbox
    if frame==85 then
        tImGui.Checkbox=function(label,value)
            if label==tLang.L('ime_edit_mode') then return true end
            return originalCheckbox(label,value)
        end
    end
    local originalHovered=tImGui.GetWantCaptureMouse
    tImGui.GetWantCaptureMouse=function() return false end
    if command then
        e.tool=command[1]; local o=e.canvasTransform
        local x,y=o.x+(command[2]+0.25)*o.scale,o.y+(command[3]+0.25)*o.scale
        if command[4] then onTouchDown(0,x,y)
        elseif command[5] then onTouchMove(0,x,y)
        else onTouchUp(0,x,y) end
    elseif frame>=98 and frame<=109 then
        onTouchUp(0,e.canvasTransform.x,e.canvasTransform.y)
    end
    tImGui.GetWantCaptureMouse=originalHovered
    local originalGizmo,originalColor=tUtil.drawOrbitGizmo,tImGui.ColorEdit4
    if frame==170 then
        e.diagnosticBuilds=e.builds
        tUtil.drawOrbitGizmo=function(c,options)
            c.azimuth=c.azimuth+0.2
            return true
        end
        tImGui.ColorEdit4=function(label,value,flags)
            if label==tLang.L('ambient') then return true,{r=0.12,g=0.13,b=0.14,a=1} end
            return originalColor(label,value,flags)
        end
    end
    local originalBegin=tImGui.Begin
    local lightWindows=0
    if frame==180 then
        api.setEditMode(true)
        tImGui.Begin=function(title,...)
            if title:find('###ime_light',1,true) then lightWindows=lightWindows+1 end
            return originalBegin(title,...)
        end
    end
    loop(delta)
    tImGui.Begin=originalBegin
    if frame==180 then
        assert(lightWindows==0 and not mbm.getLightState('2dw').enabled,'2D light/window enabled')
        for _,kind in ipairs({'rectangle','circle','ellipse','triangle','regular'}) do
            api.setEditMode(true)
            local count=#e.project.regions
            e.primitive={kind=kind,w=24,h=20,sides=6}
            assert(api.addPrimitive())
            assert(#e.project.regions==count+1 and e.tool=='select')
            local selected=e.selected
            api.undo(false); assert(#e.project.regions==count)
            api.undo(true); api.select(selected)
            api.setEditMode(false); api.rebuild()
            assert(e.preview and e.report,'primitive extrusion failed: '..kind)
        end
        api.saveProject('/tmp/ime-primitives.imesh')
        api.openProject('/tmp/ime-primitives.imesh')
        assert(#e.project.regions==22)
        print('IMAGE MESH EDITOR PRIMITIVES / EXTRUSION / NO 2D LIGHT OK')
    end
    tUtil.drawOrbitGizmo,tImGui.ColorEdit4=originalGizmo,originalColor
    if frame==170 then
        local light=mbm.getLightState('3d')
        assert(math.abs(light.ambientColor.r-0.12)<0.0001)
        local direction=tUtil.dirFromOrbit(e.lights['3d'].orbit)
        assert(math.abs(light.directionalDirection.x-direction.x)<0.0001)
        assert(e.builds==e.diagnosticBuilds,'camera/light rebuilt mesh')
        print('IMAGE MESH EDITOR CAMERA / LIGHT PANELS OK')
    end
    tImGui.Checkbox=originalCheckbox
    if frame==85 then assert(e.editMode and e.values.lockBorder==true,'checkbox state was not preserved') end
    if frame==130 then
        assert(e.project.regions[17].contour[1].x>0)
        api.setEditMode(false); api.rebuild()
        assert(e.preview and e.preview.visible and e.report,'edited contour did not generate')
        assert(not e.imageObject.visible,'image leaked into 3D mode')
        print('IMAGE MESH EDITOR DRAW / MOVE / RESIZE / POLYGON POINT INPUT OK')
    end
    if frame==30 then
        assert(not api.state.batch)
        for _,r in ipairs(api.state.project.regions) do
            local path='/tmp/image-mesh-stage2-export/'..IO.exportName(r)
            local mesh=meshDebug:new(); assert(mesh:load(path)); assert(mesh:check())
        end
        baseline=api.state.builds
    end
    if frame==80 then assert(api.state.builds==baseline,'idle editor rebuilt geometry'); assert(api.state.target==nil,'preview still uses render target'); print('IMAGE MESH EDITOR UI / BATCH / IDLE OK') end
    if frame==135 then
        api.setEditMode(true)
        assert(e.imageObject.visible and not e.preview.visible)
        e.idleCanvas=e.canvasBuilds; e.idleMeshes=e.builds
    end
    if frame==155 then
        assert(e.canvasBuilds==e.idleCanvas and e.builds==e.idleMeshes,'idle editing rebuilt geometry')
        api.setEditMode(false); api.rebuild()
        local meshBuilds=e.builds; local revision=e.revision
        api.setWireframe(true)
        assert(e.wireObject and e.wireObject.visible and not e.preview.visible)
        local wire=e.wireObject; local wireBuilds=e.wireBuilds
        api.camera(); api.rebuild()
        assert(e.wireObject==wire and e.wireBuilds==wireBuilds and e.builds==meshBuilds)
        api.setEditMode(true); assert(not wire.visible)
        api.setEditMode(false); assert(wire.visible)
        api.setWireframe(false); assert(not wire.visible and e.preview.visible)
        api.setWireframe(true); assert(e.wireObject==wire and e.wireBuilds==wireBuilds)
        assert(e.revision==revision and e.builds==meshBuilds,'wireframe changed project/mesh')
        api.select(e.project.regions[2].id); assert(not wire.visible)
        api.rebuild(); assert(e.wireObject~=wire and e.wireObject.visible and not e.preview.visible)
        api.setWireframe(false)
        print('IMAGE MESH EDITOR WIREFRAME / CACHE / VISIBILITY OK')
        local distance=e.orbit.distance; api.setEditMode(false)
        tImGui.GetWantCaptureMouse=function() return false end
        onTouchDown(0,900,400); onTouchMove(0,940,420); onTouchUp(0,940,420); onTouchZoom(1)
        tImGui.GetWantCaptureMouse=originalHovered
        assert(e.orbit.distance<distance)
        api.setEditMode(true)
        tImGui.GetWantCaptureMouse=function() return false end
        local oldPan=e.camera2d.x; local oldZoom=e.zoom; local lines=e.canvasBuilds; local cameraScale=e.camera2d.sx
        onTouchDown(1,900,400); onTouchMove(1,940,420); onTouchUp(1,940,420)
        require('image_mesh_canvas').sync(e)
        local sx,sy=mbm.to2ds(-e.project.image.width*e.zoom/2,e.project.image.height*e.zoom/2)
        assert(math.abs(sx-e.canvasTransform.x)<0.01 and math.abs(sy-e.canvasTransform.y)<0.01,'world/image coordinates disagree')
        assert(math.abs(e.camera2d.x-(oldPan-40/cameraScale))<0.001)
        local mouse=tImGui.GetMousePos
        tImGui.GetMousePos=function() return {x=e.screenW*0.6,y=e.screenH*0.6} end
        onTouchZoom(1)
        tImGui.GetMousePos=mouse
        assert(e.zoom>oldZoom)
        assert(e.canvasBuilds==lines,'camera pan rebuilt contour buffers')
        api.fit(e); require('image_mesh_canvas').sync(e)
        local before=#e.project.regions
        local o=e.canvasTransform; e.tool='rectangle'
        onTouchDown(0,o.x+10*o.scale,o.y+10*o.scale)
        onTouchMove(0,o.x+20*o.scale,o.y+20*o.scale)
        assert(e.drag); api.setEditMode(false)
        assert(not e.drag and #e.project.regions==before,'mode change committed unfinished drawing')
        tImGui.GetWantCaptureMouse=function() return true end
        local azimuth=e.orbit.azimuth
        onTouchDown(0,900,400); onTouchMove(0,940,420); onTouchUp(0,940,420)
        assert(e.orbit.azimuth==azimuth,'UI input reached scene')
        tImGui.GetWantCaptureMouse=originalHovered
        print('IMAGE MESH EDITOR SCENE MODES / ORBIT / IDLE OK')
    end
    if frame==190 then
        local Canvas=require 'image_mesh_canvas'
        api.setEditMode(true); api.select(1); e.tool='select'; Canvas.sync(e)
        local r=e.project.regions[1]; local width,height=r.w,r.h
        local o=e.canvasTransform
        local x,y=o.x+(r.x+r.w-1)*o.scale+15,o.y+(r.y+r.h-1)*o.scale+15
        local captured=tImGui.GetWantCaptureMouse; tImGui.GetWantCaptureMouse=function() return false end
        onTouchDown(0,x,y); assert(e.drag and e.drag.mode=='resize','expanded resize target missed')
        onTouchMove(0,x,y); assert(r.w==width and r.h==height,'resize jumped on initial click')
        onTouchMove(0,x+20,y+20); onTouchUp(0,x+20,y+20)
        assert(e.project.regions[1].w==math.floor(width+20/o.scale+0.5))
        api.undo(false); assert(e.project.regions[1].w==width)
        local zoom=e.zoom; e.zoom=0.25; local small=Canvas.handleRadius(e)
        e.zoom=4; assert(Canvas.handleRadius(e)>small); e.zoom=zoom
        tImGui.GetWantCaptureMouse=captured
        print('IMAGE MESH EDITOR LARGE RESIZE HANDLE / NO JUMP OK')
    end
    if frame==200 then
        api.setEditMode(false); api.select(13)
        e.values.columns=255; e.values.rows=255; e.values.ellipseSegments=128
        e.values.maxVertices=65535; e.values.maxTriangles=1
        api.applyProperties(); api.rebuild()
        assert(not e.preview and e.generationFailure==e.status)
        assert(e.status:find('65535',1,true) and e.status:find(Model.region(e.project,13).name,1,true))
        assert(not e.status:find('.lua:',1,true),'error leaked Lua stack location')
        assert(Model.options(e.project,Model.region(e.project,13)).maxTriangles==131070)
        api.undo(false); api.rebuild(); assert(e.preview and not e.generationFailure)
        print('IMAGE MESH EDITOR BUDGET DIAGNOSTIC / RECOVERY OK')
    end
    if frame==210 then
        assert(api.compactCount(999)=='999' and api.compactCount(4000)=='4K' and api.compactCount(4200)=='4.2K')
        local report=e.report
        api.setEditMode(true); api.updateStatistics()
        assert(e.report.triangles==report.triangles)
        api.select(1); api.updateStatistics(); local builds=e.statisticsBuilds
        local expected=e.report.triangles
        api.select(13); api.updateStatistics(); api.select(1); api.updateStatistics()
        assert(e.statisticsBuilds==builds and e.report.triangles==expected,'selection did not reuse counts')
        for i=1,10 do api.updateStatistics() end
        assert(e.statisticsBuilds==builds,'idle recounts geometry')
        api.action(function(p) Model.region(p,1).overrides.columns=4 end)
        api.updateStatistics(); assert(e.statisticsBuilds==builds+1,'changed geometry did not recount')
        local asset,actual=mbm.generateImageMesh(e.project.image.path,Model.options(e.project,Model.region(e.project,1)))
        assert(asset and e.report.triangles==actual.triangles)
        api.undo(false); api.updateStatistics()
        local restart=tUtil.tTimerOverlay.restart; local restarted=0
        tUtil.tTimerOverlay.restart=function(self) restarted=restarted+1; return restart(self) end
        api.saveProject('/tmp/ime_feedback.imesh'); api.saveProject('/tmp/ime_feedback.imesh')
        tUtil.tTimerOverlay.restart=restart
        assert(restarted==2 and tUtil.sMessageOverlay:find('ime_feedback.imesh',1,true))
        print('IMAGE MESH EDITOR SAVE FEEDBACK / FACE COUNTS / CACHE OK')
    end
    if started and mbm.getTimeRun()-started>8 then mbm.quit() end
end
function onEndScene() finish() end
