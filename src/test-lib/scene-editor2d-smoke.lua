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

   2DW world editor regression: projection, placement, lighting, persistence and export.
]]--
package.path = 'editor/?.lua;' .. package.path
dofile('editor/scene_editor2d.lua')
local projection=require('scene_editor_view')('2dw')
local initEditor, loopEditor = onInitScene, onLoop
local started, tested, failed
local function near(a,b) assert(math.abs(a-b)<0.002,tostring(a)..' ~= '..tostring(b)) end
local asset=(os.getenv('PWD') or '.')..'/src/test-lib/Crate.msh'
local exportPath='/tmp/mini-mbm-scene2d-test.lua'
local savePath='/tmp/mini-mbm-scene2d-test.scene2d-edit.lua'
local asyncScene
function onInitScene()
    initEditor()
    started=mbm.getTimeRun()
    mbm.addPath('src/test-lib')
    sMeshSetFolder=''
    tMapOptions.iGridCountX,tMapOptions.iGridCountZ=4,3
    tMapOptions.fGridCellWidthX,tMapOptions.fGridCellDepthZ=100,80
    tLayers[1].fY=20
    tLayers[1].offset={x=30,z=7}
    setActiveTab('layer')
    rebuildGridVisual()
end
local function tests()
    setActiveTab("layer")
    rebuildGridVisual()
    assert(tLayers[1].bWall)
    assert(#tGridLines==9)
    for _,mode in ipairs({'Orthogonal','Isometric'}) do
        tMapOptions.sMapType=mode
        for _,stagger in ipairs({{false,false},{true,false},{false,true},{true,true}}) do
            tLayers[1].bHalfOffsetX,tLayers[1].bHalfOffsetZ=table.unpack(stagger)
            for x=-2,2 do for y=-2,2 do
                local wx,wy,wz=gridCellToWorld(x,y,tLayers[1])
                local a,b=worldToGridCell(wx,wy,wz,tLayers[1])
                assert(a==x and b==y)
                near(wz,7)
            end end
        end
    end
    tLayers[1].bHalfOffsetX,tLayers[1].bHalfOffsetZ=false,false
    tMapOptions.sMapType='Orthogonal'
    local x,y,z=gridCellToWorld(1,1,tLayers[1])
    near(x,130);near(y,100);near(z,7)
    registerMeshSetEntry(asset)
    local placed=addPlacedMesh(asset,'mesh',1,1,1,nil,nil,true)
    assert(placed.tObj)
    near(placed.tObj.x,130);near(placed.tObj.y,100);near(placed.tObj.z,7)
    pushUndoSnapshot()
    placed.isSelected=true
    rotateSelectedMeshes(1)
    near(placed.rotationZ,math.pi/4);near(placed.rotationY,0)
    onUndoScene3d();near(tPlacedMeshes[1].rotationZ,0)
    onRedoScene3d();near(tPlacedMeshes[1].rotationZ,math.pi/4)
    cam3d.fx,cam3d.fy,cam3d.distance=130,100,400
    applyCam3d(cam3d)
    local sw,sh=mbm.getSizeScreen()
    local wx,wy=projection.screenToWorld(sw/2,sh/2)
    near(wx,130);near(wy,100)
    x,y,z=screenToWorldOnLayerPlane(sw/2,sh/2,tLayers[1])
    near(x,130);near(y,100);near(z,7)
    assert(isPointInScreenRectAtObjectDepth(130,100,7,sw/2-30,sh/2-30,sw/2+30,sh/2+30))
    updateHoverHighlight(sw/2,sh/2);assert(tHoveredPlaced==1)
    local capture=tImGui.GetWantCaptureMouse
    tImGui.GetWantCaptureMouse=function() return false end
    sMeshSelectedForPlacement=asset
    onTouchDown(0,sw/2,sh/2);onTouchUp(0,sw/2,sh/2)
    assert(#tPlacedMeshes==1 and tPlacedMeshes[1].isSelected)
    updateSelectionHighlights()
    tPlacedMeshes[1].isSelected=false
    onKeyDown(mbm.getKeyCode('shift'))
    onTouchDown(0,sw/2-40,sh/2-40)
    onTouchMove(0,sw/2+40,sh/2+40)
    onKeyUp(mbm.getKeyCode('shift'))
    onTouchUp(0,sw/2+40,sh/2+40)
    assert(tPlacedMeshes[1].isSelected and #tPlacedMeshes==1)
    updateSelectionHighlights()
    resetSceneAmbientLight()
    near(tLightConfig.ambientColor.r,230/255)
    mouseLastX,mouseLastY=sw/2+30,sh/2-10
    wx,wy=projection.screenToWorld(mouseLastX,mouseLastY)
    onTouchZoom(1)
    x,y=projection.screenToWorld(mouseLastX,mouseLastY);near(x,wx);near(y,wy)
    local oldX=cam3d.fx
    onTouchDown(1,sw/2,sh/2);onTouchMove(1,sw/2+40,sh/2);onTouchUp(1,sw/2+40,sh/2)
    assert(cam3d.fx<oldX)
    tImGui.GetWantCaptureMouse=capture
    tLightConfig.bEnabled=true
    tLightConfig.pointLights={{x=100,y=100,z=-30,radius=300,r=1,g=0.5,b=0.2,a=1}}
    setActiveTab('map');applyLightConfigToEngine();applyTabLighting()
    assert(mbm.getLightState('2dw').enabled)
    tSceneObjects={{type='rectangle',x=130,y=100,z=0,width=70,height=50,name='spawn'}}
    tMarkerEdit.dirty=true
    updateSceneObjectShapes()
    assert(tSceneObjectShapes[1].handle)
    tOptionsEditor.fSceneCamPos={x=130,y=100,z=0}
    tOptionsEditor.fSceneCamScale=2
    assert(writeScene3d(savePath,false,false))
    local saved=require('editor_asset_paths').loadScene(savePath,mbm)
    assert(saved.world=='2dw' and saved.tLayers[1].bWall)
    assert(applyLoadedScene3d({world='3d'})==false)
    assert(#tPlacedMeshes==1)
    applyLoadedScene3d(saved)
    near(tOptionsEditor.fSceneCamScale,2)
    near(tPlacedMeshes[1].rotationZ,math.pi/4)
    for _,async in ipairs({false,true}) do
        assert(writeScene3d(exportPath,async,true))
        local scene=assert(loadfile(exportPath))()
        assert(scene.world=='2dw' and not scene.tLayers)
        scene:load()
        if async then asyncScene=scene
        else
            assert(#scene.tMeshesLoaded==1)
            near(scene:get(1).z,7)
            near(mbm.getCamera('2d').sx,2)
            for _,object in ipairs(scene.tMeshesLoaded) do object:destroy() end
        end
    end
    setActiveTab('layer')
    tHoveredPlaced=1
    handleSelectClickAt()
    updateSelectionHighlights()
    local removed=tPlacedMeshes[1]
    local highlight=assert(removed.tHighlightShape)
    local live=removed.tObj
    removePlacedMesh(1)
    updateSelectionHighlights()
    assert(not live.visible and not highlight.visible and removed.tObj==nil)
    -- A completion after removal must not resurrect an untracked renderizable.
    local asyncLoader=placeMeshAsync
    local complete
    placeMeshAsync=function(_,_,_,callback) complete=callback end
    local pending=addPlacedMesh(asset,'mesh',1,0,0,nil,nil,false)
    placeMeshAsync=asyncLoader
    removePlacedMesh(#tPlacedMeshes)
    local late=assert(placeMeshSync(asset,'mesh','2dw'))
    complete(late)
    assert(not late.visible and pending.tObj==nil)
    updatePreviewMesh3d(tMeshSetEntries[1])
    assert(not tPreviewMesh3d.visible)
    local preview=tPreviewMesh3d
    iPreviewedMeshSetIndex=1
    local snapshot=captureScene3dSnapshot()
    restoreScene3dSnapshot(snapshot)
    assert(not preview.visible)
    assert(tPreviewMesh3d and not tPreviewMesh3d.visible)
    setActiveTab('mesh_set');updatePreviewMesh3d(tMeshSetEntries[1])
    assert(tPreviewMesh3d.visible)
    assert(not tPreviewMesh3d.tShape, 'Mesh Property created a GUI selection rectangle')
    local oldPreview=tPreviewMesh3d
    updatePreviewMesh3d(tMeshSetEntries[1])
    assert(not oldPreview.visible and not tPreviewMesh3d.tShape)
    setActiveTab('map')
    assert(not tPreviewMesh3d.visible)
    setActiveTab('layer')
    assert(not tPreviewMesh3d.visible)
    setActiveTab('mesh_set')
    assert(tPreviewMesh3d.visible)
    -- Non-mesh placements use the same utility: they must not create GUI overlays either.
    local utilityObject=placeMeshSync('src/test-lib/box.spt','sprite','2dw')
    assert(utilityObject and not utilityObject.tShape)
    utilityObject:destroy()
    -- GUI callers retain the default selection helper.
    local guiObject=tUtil.onAddMeshToEditor(asset,false,'2dw')
    assert(guiObject and guiObject.tShape)
    guiObject.tShape:destroy()
    guiObject:destroy()
    print('SCENE2D TESTS OK')
end
function onLoop(delta)
    if not tested and mbm.getTimeRun()-started>0.5 then
        tested=true
        local ok,err=pcall(tests)
        if not ok then failed=true; print('SCENE2D FAIL: '..tostring(err)) end
    end
    if not failed then loopEditor(delta) end
    if mbm.getTimeRun()-started>4 then
        if not failed and asyncScene and #asyncScene.tMeshesLoaded==1 then print('SCENE2D SMOKE OK')
        else print('SCENE2D FAIL: completion') end
        os.remove(exportPath);os.remove(savePath)
        mbm.quit()
    end
end
