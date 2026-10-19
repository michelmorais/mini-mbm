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

-- Run from the repository root with the bundled Lua interpreter.
local file = assert(io.open('editor/scene_editor_world.lua'))
local source = file:read('*a'); file:close()
local env = setmetatable({}, {__index=_G})
local function import(name)
    local start = assert(source:find('function '..name..'(', 1, true))
    local finish = source:find('\nfunction ', start+1, true) or (#source+1)
    assert(load(source:sub(start,finish-1), '@'..name, 't', env))()
end
local function near(a,b) assert(math.abs(a-b)<0.00001, tostring(a)..' ~= '..tostring(b)) end
env.view={loader=function(code) return code end}
local calls, undo = 0, 0
local object = {
    setPos=function() end,
    setAngle=function(self,x,y,z) self.angles={x,y,z}; calls=calls+1 end,
    setScale=function(self,x,y,z) self.scales={x,y,z} end,
}
local placed = {fileName='test.msh',type='mesh',rotationX=0.2,rotationY=0.3,rotationZ=-0.4,
    scale={x=2,y=3,z=4},tObj=object,isSelected=true,layerIndex=1}
env.getMeshOffset=function() return {x=0,y=0,z=0,rx=0.1,ry=0.2,rz=0.3,sx=2,sy=1,sz=0.5} end
env.resolvePlacedMeshWorldPos=function() return 10,20,30 end
env.pushUndoSnapshot=function() undo=undo+1 end
import('syncPlacedMeshTransform')
env.syncPlacedMeshTransform(placed)
near(object.angles[1],0.3); near(object.angles[2],0.5); near(object.angles[3],-0.1)
assert(object.scales[1]==4 and object.scales[2]==3 and object.scales[3]==2)
-- Exercise the real exported loader, with a render object standing in for a GPU mesh.
import('getSceneLoaderCode3d')
local game = assert(load('local tScene3d={tMeshOffsets={}}\n'..env.getSceneLoaderCode3d(false), '@export', 't', env))()
game.tMeshesLoaded={};game.tMeshesLoadedDictionary={};game.iLoadedCount=0
game._createMeshWithLighting=function() return object end
local function add(info)
    info.fileName='test.msh';info.type='mesh';info.x=0;info.y=0;info.z=0
    game:_addMesh(info,function() end)
end
add({rotationX=0.2,rotationY=0.3,rotationZ=-0.4,sx=2,sy=3,sz=4})
near(object.angles[1],0.2); near(object.angles[3],-0.4)
assert(object.scales[1]==2 and object.scales[3]==4)
add({rotationY=0.7}) -- Older files have no X/Z fields.
assert(object.angles[1]==0 and object.angles[3]==0)
near(object.angles[2],0.7)
-- Widget behavior: idle frames do not synchronize transforms or push history.
local edits, last = {}, nil
env.placedTransformAxes={rotation={'X','Z'},scale={'x','y','z'}}
env.tLang={L=function(key) return key end}
env.L=env.tLang.L
env.primaryRotation='rotationY'
env.tUtil={getShortName=function(name) return name end, pushResponsiveItemWidth=function() end}
env.tMapOptions={sMapType='Orthogonal'}
env.registerMeshSetEntry=function() end
env.tImGui=setmetatable({
    CalcTextSize=function() return {x=100,y=16} end,
    GetCursorStartPos=function() return {x=8,y=8} end,
    Button=function() return false end,
    Checkbox=function(_,value) return value end,
    DragFloat=function(label,value)
        last=edits[label]
        return last~=nil,last or value
    end,
    IsItemDeactivatedAfterEdit=function() return last~=nil end,
}, {__index=function() return function() end end})
import('drawPlacedMeshRow')
calls=0;undo=0
env.drawPlacedMeshRow(1,placed,{},true)
assert(calls==0 and undo==0)
edits={['placed_rotation_x##placed_rotX1']=45,['scale_z##placed_scalez1']=1.5}
env.drawPlacedMeshRow(1,placed,{},true)
near(placed.rotationX,math.pi/4);assert(placed.scale.z==1.5)
assert(calls==2 and undo==2)
assert(placed.rotationY==0.3 and placed.rotationZ==-0.4)
-- Clipboard and undo state must preserve all per-instance axes.
import('copyStringList')
import('getArticulatedAnimationNames')
env.tPlacedMeshes={placed};env.iSelectedLayer=1;env.tLayers={{}}
env.tUtil.showMessage=function() end
import('copySelectedMeshes')
env.copySelectedMeshes()
near(env.tCopyBuffer[1].rotationX,placed.rotationX)
near(env.tCopyBuffer[1].rotationZ,placed.rotationZ)
local function newPlaced(name,kind,layer)
    local result={fileName=name,type=kind,layerIndex=layer,tObj=object}
    table.insert(env.tPlacedMeshes,result)
    return result
end
env.addPlacedMesh=newPlaced
env.applyPlacedMeshAnimations=function() end
env.tMapOptions.sMapType='Free'
env.isWorldPosWithinGridBounds=function() return true end
import('pasteCopiedMeshes')
env.pasteCopiedMeshes()
local pasted=env.tPlacedMeshes[2]
near(pasted.rotationX,placed.rotationX);near(pasted.rotationZ,placed.rotationZ)
assert(pasted.scale.z==placed.scale.z)
env.deepCopyPlainTable=function(value) return value end
env.tSceneObjects={};env.tMeshOffsets={}
import('captureScene3dSnapshot')
local snapshot=env.captureScene3dSnapshot()
env.removePlacedMesh=function(i) table.remove(env.tPlacedMeshes,i) end
env.clearSceneObjectVisuals=function() end
env.destroyPreviewMesh3d=function() end
env.rebuildGridVisual=function() end
env.getFilteredMeshSetEntries=function() return {} end
import('restoreScene3dSnapshot')
env.restoreScene3dSnapshot(snapshot)
local restored=env.tPlacedMeshes[1]
near(restored.rotationX,placed.rotationX);near(restored.rotationZ,placed.rotationZ)
assert(restored.scale.z==placed.scale.z)
print('Scene 3D transforms: PASS (offsets, export, legacy files, controls, idle, clipboard, undo restore)')
