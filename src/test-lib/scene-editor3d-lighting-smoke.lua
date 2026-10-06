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

   Scene Editor 3D per-asset lighting regression fixture.

]]--
-- Run from repo root with --disable_select_monitor --nosplash -w 1000 -h 700 -ew 1000 -eh 700.
-- The async exported scene stays visible for comparison: left unlit, right lit.
package.path = 'editor/?.lua;' .. package.path
dofile('editor/scene_editor3d.lua')
local init=onInitScene
local start,phase=0,0
local scenes={}
function onInitScene()
    init()
    mbm.addPath('src/test-lib')
    start=mbm.getTimeRun()
    tLightConfig.bEnabled=true
    tLightConfig.ambientColor={r=0.05,g=0.05,b=0.05,a=1}
    tLightConfig.directionalColor={r=0,g=0,b=0,a=1}
    applyLightConfigToEngine()
    cam3d.azimuth=0; cam3d.elevation=0.2; cam3d.distance=400
    local lit='src/test-lib/Crate.msh'
    local unlit='./src/test-lib/Crate.msh'
    tMeshOffsets[lit]={receiveLight=true}
    tMeshOffsets[unlit]={receiveLight=false}
    tMapOptions.sMapType='Free'
    addPlacedMesh(lit,'mesh',1,0,0,-80,0,true)
    addPlacedMesh(unlit,'mesh',1,0,0,80,0,true)
    tOptionsEditor.fSceneCamPos={x=0,y=80,z=400}
    tOptionsEditor.fSceneCamFocus={x=0,y=0,z=0}
    for _,async in ipairs({false,true}) do
        local path=os.tmpname()
        assert(writeScene3d(path,async,true))
        local scene=assert(loadfile(path))()
        os.remove(path)
        assert(scene.tMeshOffsets[lit].receiveLight==true)
        assert(scene.tMeshOffsets[unlit].receiveLight==false)
        scenes[#scenes+1]=scene
    end
    assert(getMeshOffset('legacy').receiveLight==true)
    resetUndoHistory()
    tMeshOffsets[unlit].receiveLight=true
    pushUndoSnapshot()
    onUndoScene3d()
    assert(getMeshOffset(unlit).receiveLight==false)
    onRedoScene3d()
    assert(getMeshOffset(unlit).receiveLight==true)
    for _,placed in ipairs(tPlacedMeshes) do placed.tObj.visible=false end
    scenes[1]:load()
    assert(scenes[1].iLoadedCount==2)
    for _,object in ipairs(scenes[1].tMeshesLoaded) do object.visible=false end
    scenes[2]:load()
end
function onLoop(delta)
    local scene=scenes[2]
    if scene and scene.iLoadedCount==2 and phase==0 then
        assert(mbm.getLightState('3d').enabled,'global light must be restored')
        print('PER ASSET LIGHT SAVE EXPORT SYNC ASYNC UNDO OK')
        phase=1
    end
    if tImGui.Begin('Lighting test - left unlit, right lit',false,0) then
        tImGui.Text('Left: ignores light. Right: receives light. Ambient = 0.05.')
    end
    tImGui.End()
    if mbm.getTimeRun()-start>7 then
        assert(phase==1,'async scene did not finish')
        print('LIGHTING SMOKE OK')
        mbm.quit()
    end
end
