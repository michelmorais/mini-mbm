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
| the Software.                                                                                                        |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Scene Editor 3D interaction regression fixture.

]]--
-- Run from repository root:
-- mini-mbm --scene src/test-lib/scene-editor3d-interaction-smoke.lua
--   --disable_select_monitor --nosplash -w 1000 -h 700 -ew 1000 -eh 700
-- Calls production input callbacks with the real renderer; no OS mouse injection.
package.path = 'editor/?.lua;' .. package.path
dofile('editor/scene_editor3d.lua')
local initEditor, loopEditor = onInitScene, onLoop
local started, tested, failure
local panStage=0
local realCapture = tImGui.GetWantCaptureMouse
local function near(a,b) assert(math.abs(a-b)<0.001, tostring(a)..' ~= '..tostring(b)) end
local function screenPoint(p)
    local w,h = mbm.getSizeScreen()
    local ox,oy,oz,fx,fy,fz = mbm.getPickRay(w/2,h/2)
    local o = {x=ox,y=oy,z=oz}
    local f = {x=fx,y=fy,z=fz}
    local len=math.sqrt(f.x*f.x+f.z*f.z)
    local r={x=f.z/len,y=0,z=-f.x/len}
    local u={x=f.y*r.z,y=f.z*r.x-f.x*r.z,z=-f.y*r.x}
    local vx,vy,vz = p.x-o.x,p.y-o.y,p.z-o.z
    local depth = vx*f.x+vy*f.y+vz*f.z
    local _,_,_,dx,dy,dz = mbm.getPickRay(w,h/2)
    local horizontal = (dx*r.x+dy*r.y+dz*r.z)/(dx*f.x+dy*f.y+dz*f.z)
    local _,_,_,ex,ey,ez = mbm.getPickRay(w/2,h)
    local vertical = (ex*u.x+ey*u.y+ez*u.z)/(ex*f.x+ey*f.y+ez*f.z)
    return w/2+(vx*r.x+vy*r.y+vz*r.z)/depth/horizontal*w/2,
        h/2+(vx*u.x+vy*u.y+vz*u.z)/depth/vertical*h/2
end
function onInitScene()
    initEditor()
    started=mbm.getTimeRun()
    tShowGridByTab.map=true
    addLayer(); tLayers[2].fY=-100; tLayers[2].offset.x=40
    iSelectedLayer=0
    rebuildGridVisual()
    assert(#tGridLines==84,'all layer grids')
    addSceneObjectMarker()
    table.insert(tSceneObjects,{type='line',name='path',x=100,y=0,z=0,points={{x=100,y=0,z=0},{x=180,y=0,z=0}}})
    table.insert(tSceneObjects,{type='triangle',name='triangle',x=-120,y=0,z=0,points={{x=-160,y=0,z=-30},{x=-80,y=0,z=-30},{x=-120,y=0,z=40}}})
    tMarkerEdit.dirty=true
    updateSceneObjectShapes()
    resetUndoHistory()
end
local function testObjectSerialization()
    local previous = tSceneObjects
    -- No renderer/property-panel visit: Save/Export must materialize every default themselves.
    local fixtures = {
        {type='rectangle',name='edited',x=1,y=2,z=3,width=675,height=47},
        {type='rectangle',name='untouched',x=4,y=5,z=6},
        {type='cube',name='volume',width=75},
        {type='circle',name='radius'},
        {type='triangle',name='corners',x=10,y=20,z=30},
        {type='line',name='path',points={{x=1,y=2,z=3},{x=4,y=5,z=6}}},
        {type='point',name='anchor',x=-5000,y=6000,z=7000},
    }
    for _,mode in ipairs({{false,false},{true,false},{true,true}}) do
        tSceneObjects = tUtil.deepCopyTable(fixtures)
        local path=os.tmpname()
        local ok,reason=writeScene3d(path,mode[2],mode[1])
        assert(ok,reason)
        local scene=assert(loadfile(path))()
        os.remove(path)
        local objects=scene:getAllSceneObjects()
        assert(#objects==7)
        assert(objects[1].width==675 and objects[1].height==47,'edited dimensions preserved')
        assert(objects[2].width==100 and objects[2].height==100,'rectangle defaults serialized')
        assert(objects[3].width==75 and objects[3].height==100 and objects[3].depth==100,'cube defaults')
        assert(objects[4].ray==50,'circle default radius')
        assert(#objects[5].points==3 and objects[5].points[1].x==-40,'triangle geometry')
        assert(#objects[6].points==2 and objects[6].points[2].z==6,'line geometry preserved')
        assert(objects[7].x==-5000 and objects[7].y==6000 and objects[7].z==7000,'point coordinates')
    end
    tSceneObjects = previous
    print('SCENE3D OBJECT SERIALIZATION OK')
end

local function runTests()
    testObjectSerialization()
    tImGui.GetWantCaptureMouse=function() return false end
    local object=tSceneObjects[1]
    local x,y=screenPoint(object)
    assert(pickSceneObject(x,y)==object,'point pick')
    onTouchDown(0,x,y)
    assert(tMarkerEdit.drag,'free drag begins')
    local azimuth=cam3d.azimuth
    onTouchMove(0,x+30,y+10)
    assert(tMarkerEdit.drag.moved,'free drag moves')
    onTouchUp(0,x+30,y+10)
    near(cam3d.azimuth,azimuth)
    assert(iUndoIndex==2,'one drag snapshot')
    onUndoScene3d(); near(tSceneObjects[1].x,0)
    onRedoScene3d(); assert(math.abs(tSceneObjects[1].x)>0.01)
    updateSceneObjectShapes()
    object=tSceneObjects[1]
    tMarkerEdit.selected=object
    updateSceneObjectInteraction()
    for _,name in ipairs({'x','y','z'}) do
        local before={x=object.x,y=object.y,z=object.z}
        local target={x=object.x,y=object.y,z=object.z}; target[name]=target[name]+58
        x,y=screenPoint(target)
        onTouchDown(0,x,y)
        assert(tMarkerEdit.drag and tMarkerEdit.drag.axis,'axis pick '..name)
        onTouchMove(0,x+20,y+15)
        onTouchUp(0,x+20,y+15)
        assert(math.abs(object[name]-before[name])>0.01,'axis moved '..name)
        for _,other in ipairs({'x','y','z'}) do if other~=name then near(object[other],before[other]) end end
        updateSceneObjectShapes()
    end
    local path=tSceneObjects[2]
    tMarkerEdit.capture=path
    x,y=screenPoint({x=180,y=0,z=100})
    onTouchDown(0,x,y); onTouchMove(0,x+1,y); onTouchUp(0,x,y)
    assert(#path.points==3,'line point appended')
    near(path.points[3].x,180); near(path.points[3].y,0); near(path.points[3].z,100)
    near(cam3d.azimuth,azimuth)
    setActiveTab('map'); assert(tMarkerEdit.capture==path,'same tab preserves capture')
    setActiveTab('layer'); assert(not tMarkerEdit.capture,'tab switch ends capture')
    updateSceneObjectShapes(); assert(not tSceneObjectShapes[1].handle.visible,'markers hidden')
    setActiveTab('map'); updateSceneObjectShapes()
    assert(tSceneObjectShapes[1].handle.visible and tSceneObjectShapes[1].handle.alwaysOnTop)
    -- Real buffer methods must remain untouched in idle marker/grid updates.
    updateSceneObjectShapes(); updateSceneObjectInteraction()
    local mathTools=require 'scene_object_tools'
    for _,index in ipairs({2,3}) do
        local marker=tSceneObjects[index]
        local first={x=marker.points[1].x,y=marker.points[1].y,z=marker.points[1].z}
        mathTools.translate(marker,marker.x+10,marker.y+20,marker.z+30)
        near(marker.points[1].x,first.x+10)
        near(marker.points[1].y,first.y+20)
        near(marker.points[1].z,first.z+30)
    end
    tMarkerEdit.dirty=true
    updateSceneObjectShapes()
    assert(mathTools.pointRadius({fGridCellWidthX=50,fGridCellDepthZ=200})==4)
    bShowSceneObjectMarkers=false
    assert(not beginSceneObjectDrag(x,y),'hidden objects cannot be dragged')
    bShowSceneObjectMarkers=true
    local cube={type='cube',name='volume',x=-100,y=140,z=0,width=90,height=120,depth=60}
    table.insert(tSceneObjects,cube)
    tMarkerEdit.dirty=true
    updateSceneObjectShapes()
    local cubeHandle=tSceneObjectShapes[#tSceneObjects].handle
    local scale=cubeHandle:getScale()
    near(scale.x,90); near(scale.y,120); near(scale.z,60)
    x,y=screenPoint(cube)
    assert(pickSceneObject(x,y)==cube,'cube pick')
    onTouchDown(0,x,y); onTouchMove(0,x+15,y+5); onTouchUp(0,x+15,y+5)
    assert(cube.x~=-100 or cube.y~=140,'cube drag')
    local snap=captureScene3dSnapshot().tSceneObjects[#tSceneObjects]
    assert(snap.type=='cube' and snap.width==90 and snap.height==120 and snap.depth==60)
    local box={x=0,y=0,z=0,width=2,height=4,depth=6}
    near(mathTools.rayBoxDistance(0,0,-10,0,0,1,box),7)
    assert(not mathTools.rayBoxDistance(2,0,-10,0,0,1,box),'parallel ray outside cube')
    assert(not mathTools.rayBoxDistance(0,0,-10,0,0,-1,box),'cube behind ray')
    near(mathTools.rayBoxDistance(0,0,0,1,0,0,box),0)
    for _,enabled in ipairs({false,true}) do
        bSceneObjectsAlwaysOnTop=enabled
        tMarkerEdit.dirty=true
        updateSceneObjectShapes(); updateSceneObjectInteraction()
        for _,entry in ipairs(tSceneObjectShapes) do assert(entry.handle.alwaysOnTop==enabled) end
        for _,axis in pairs(tMarkerEdit.axes) do assert(axis.alwaysOnTop==enabled) end
    end
    assert(tSceneMarkerColor.a==0.25)
    local lineHandle=tSceneObjectShapes[2].handle
    local oldSet=lineHandle.set
    local gridHandle=tGridLines[1]
    local oldGridSet=gridHandle.set
    gridHandle.set=function() error('idle grid upload') end
    lineHandle.set=function() error('idle line upload') end
    for i=1,10 do setActiveTab('map'); updateSceneObjectShapes(); updateSceneObjectInteraction() end
    lineHandle.set=oldSet
    gridHandle.set=oldGridSet
    tImGui.GetWantCaptureMouse=realCapture
    print('SCENE3D INTERACTION TESTS OK')
end
local function runPanTest()
    if panStage >= 8 then return end
    tImGui.GetWantCaptureMouse=function() return false end
    if panStage % 2 == 0 then
        cam3d.azimuth=math.floor(panStage/2)*math.pi/2
        cam3d.elevation=0.5
        cam3d.fx,cam3d.fy,cam3d.fz=0,0,0
        applyCam3d(cam3d)
    else
        local normal=camera3d:getNormal('F')
        local origin={x=cam3d.fx,y=cam3d.fy,z=cam3d.fz}
        local mathTools=require 'scene_object_tools'
        local ax,ay,az=mathTools.planeHit(400,300,origin,normal)
        local bx,by,bz=mathTools.planeHit(440,320,origin,normal)
        onTouchDown(1,400,300); onTouchMove(1,440,320); onTouchUp(1,440,320)
        near(cam3d.fx,ax-bx); near(cam3d.fy,ay-by); near(cam3d.fz,az-bz)
    end
    panStage=panStage+1
    tImGui.GetWantCaptureMouse=realCapture
    if panStage==8 then
        cam3d.azimuth=0.6; cam3d.elevation=0.5
        cam3d.fx,cam3d.fy,cam3d.fz=0,0,0
        print('SCENE3D PAN TESTS OK')
    end
end
function onLoop(delta)
    if not tested and mbm.getTimeRun()-started>0.5 then
        tested=true
        local ok,err=pcall(runTests)
        if not ok then failure=err; print('SCENE3D FAIL: '..tostring(err)) end
        tImGui.GetWantCaptureMouse=realCapture
    elseif tested and not failure then
        local ok,err=pcall(runPanTest)
        if not ok then failure=err; print('SCENE3D FAIL: '..tostring(err)) end
        tImGui.GetWantCaptureMouse=realCapture
    end
    if not failure then loopEditor(delta) end
    if mbm.getTimeRun()-started>7 then
        if not failure then print('SCENE3D SMOKE OK') end
        mbm.quit()
    end
end
