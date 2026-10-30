--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  |
| OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                                                          |
|------------------------------------------------------------------------------------------------------------------------|
]]--

package.path='editor/?.lua;'..package.path
local TargetProject=require 'facial_rig_target_project'
local Targets=require 'facial_rig_targets'
local Project=require 'facial_rig_project'
local Gizmo=require 'facial_rig_target_gizmo'

local function vec(x,y,z) return {x=x,y=y,z=z} end
local function target(id,name,positions)
    return {id=id,name=name,convention='ARKit',defaultWeight=0,
        positionDeltas=positions,normalDeltas={}}
end
local function mockMesh(targets,vertexCount)
    local mesh={targets=targets,vertexCount=vertexCount,nextId=20}
    function mesh:getFacialExportData()
        return {targets=self.targets,vertexCount=#self.targets>0 and self.vertexCount or 0,
            hasNormals=false}
    end
    function mesh:removeFacialTarget(index)
        if self.hasClips and #self.targets<=1 then return false end
        table.remove(self.targets,index)
        return true
    end
    function mesh:updateFacialTarget(index,name,convention,defaultWeight,positions,normals)
        for other,entry in ipairs(self.targets) do
            if other~=index and entry.name==name then return false end
        end
        local entry=self.targets[index]
        entry.name=name
        entry.convention=convention
        entry.defaultWeight=defaultWeight
        entry.positionDeltas=positions
        entry.normalDeltas=normals or {}
        return true
    end
    function mesh:addFacialTarget(name,convention,defaultWeight,positions,normals)
        for _,entry in ipairs(self.targets) do
            if entry.name==name then return nil end
        end
        local id=string.format('%016x',self.nextId)
        self.nextId=self.nextId+1
        self.targets[#self.targets+1]={id=id,name=name,convention=convention,
            defaultWeight=defaultWeight,positionDeltas=positions,normalDeltas=normals or {}}
        return #self.targets
    end
    function mesh:getFacialTargetInfo(index) return self.targets[index] end
    return mesh
end

-- Use a complete Lua table serializer here so Project.save/load exercises the
-- nested numeric sparse-delta keys, not just a hand-written preview field.
local function serialize(name,value,lines)
    local function encode(item)
        if type(item)=='string' then return string.format('%q',item) end
        if type(item)~='table' then return tostring(item) end
        local parts={'{'}
        for key,entry in pairs(item) do
            parts[#parts+1]='['..encode(key)..']='..encode(entry)..','
        end
        parts[#parts+1]='}'
        return table.concat(parts)
    end
    lines[#lines+1]=name..'='..encode(value)
end

local base={target('0000000000000001','jawOpen',{vec(0,0,0),vec(0,0,0),vec(0,0,0)})}
local current={target('0000000000000001','jawOpen',{vec(0,0,0),vec(0,-0.2,0),vec(0,0,0)}),
    target('0000000000000002','jawOpen_test',{vec(0,0,0),vec(0,0,0),vec(0,0,0)})}
local patch=assert(TargetProject.capture(base,current,3,'same-topology'))
assert(#patch.targets==2 and patch.targets[1].positions[2].y==-0.2)
assert(next(patch.targets[2].positions)==nil,'neutral target should not serialize dense zero vectors')

local project=Project.new()
project.morphTargetEdits=patch
project.morphTargetPreview={targetId='0000000000000002',weight=0}
assert(Gizmo.setPivot(project,'0000000000000002',vec(1,2,3)))
local path=os.tmpname()
assert(Project.save(project,path,serialize))
local restoredProject=assert(Project.load(path))
os.remove(path)
assert(restoredProject.morphTargetPreview.weight==0)
assert(restoredProject.morphTargetPivots['0000000000000002'].y==2)
local origin,direction,pivot=vec(1,0,2),vec(0,0,-1),vec(0,0,0)
local ring=assert(Gizmo.hitRing(origin,direction,pivot,1))
assert(ring.name=='z' and math.abs(Gizmo.signedAngle(ring.axis,ring.direction,vec(0,1,0))-90)<1e-6)
assert(Gizmo.hitAxis(vec(0.5,0,2),direction,pivot,1,0.1)=='x')
assert(math.abs(Gizmo.axisParameter(vec(0.5,0,2),direction,pivot,Gizmo.axes.x)-0.5)<1e-6)
local basePoint,baseDelta,rotationPivot=vec(2,0,0),vec(0.2,0,0),vec(1,0,0)
local basis=Targets.rotationBasis(0,0,math.pi*0.5)
local previewX,previewY,previewZ=Targets.rotatedDelta(basePoint,baseDelta,rotationPivot,basis)
local edited={positionDeltas={vec(0.2,0,0)},normalDeltas={}}
Targets.applyEdit(edited,{basePoint},{1},{mode='rotate',pivot=rotationPivot,rz=math.pi*0.5})
assert(math.abs(edited.positionDeltas[1].x-previewX)<1e-6)
assert(math.abs(edited.positionDeltas[1].y-previewY)<1e-6)
assert(math.abs(edited.positionDeltas[1].z-previewZ)<1e-6)
local mesh=mockMesh({target('0000000000000001','jawOpen',Targets.zeroDeltas(3))},3)
local ok,idMap=TargetProject.apply(mesh,restoredProject.morphTargetEdits,'same-topology')
assert(ok and idMap['0000000000000002']==mesh.targets[2].id)
local pivots=Gizmo.remapPivots(restoredProject.morphTargetPivots,idMap)
assert(pivots[mesh.targets[2].id].y==2)
assert(mesh.targets[1].positionDeltas[2].y==-0.2)
assert(mesh.targets[2].name=='jawOpen_test' and #mesh.targets[2].positionDeltas==3)
assert(not TargetProject.apply(mockMesh(base,3),patch,'wrong-topology'))

local emptyPatch=assert(TargetProject.capture({},
    {target('0000000000000001','newNeutral',Targets.zeroDeltas(3))},3,'same-topology'))
local emptyMesh=mockMesh({},3)
assert(TargetProject.apply(emptyMesh,emptyPatch,'same-topology'))
assert(emptyMesh.targets[1].name=='newNeutral')

local twoBase={target('0000000000000001','left',Targets.zeroDeltas(3)),
    target('0000000000000002','right',Targets.zeroDeltas(3))}
local swapped={target('0000000000000001','right',Targets.zeroDeltas(3)),
    target('0000000000000002','left',Targets.zeroDeltas(3))}
local swapPatch=assert(TargetProject.capture(twoBase,swapped,3,'same-topology'))
local swapMesh=mockMesh({target('0000000000000001','left',Targets.zeroDeltas(3)),
    target('0000000000000002','right',Targets.zeroDeltas(3))},3)
assert(TargetProject.apply(swapMesh,swapPatch,'same-topology'))
assert(swapMesh.targets[1].name=='right' and swapMesh.targets[2].name=='left')
local deletePatch=assert(TargetProject.capture(twoBase,{twoBase[2]},3,'same-topology'))
local deleteMesh=mockMesh({target('0000000000000001','left',Targets.zeroDeltas(3)),
    target('0000000000000002','right',Targets.zeroDeltas(3))},3)
assert(TargetProject.apply(deleteMesh,deletePatch,'same-topology'))
assert(#deleteMesh.targets==1 and deleteMesh.targets[1].id=='0000000000000002')

-- Deleting a source target and then creating another may reuse its numeric ID.
-- Provenance keeps this as a delete + add, preserving zeroed clip weights.
local replacement={target('0000000000000001','left',Targets.zeroDeltas(3)),
    target('0000000000000002','newJaw',Targets.zeroDeltas(3))}
local replacementPatch=assert(TargetProject.capture(twoBase,replacement,3,'same-topology',
    {['0000000000000002']=true},{['0000000000000002']=true}))
assert(#replacementPatch.removedIds==1 and #replacementPatch.targets==1)
local replacementMesh=mockMesh({target('0000000000000001','left',Targets.zeroDeltas(3)),
    target('0000000000000002','right',Targets.zeroDeltas(3))},3)
assert(TargetProject.apply(replacementMesh,replacementPatch,'same-topology'))
assert(replacementMesh.targets[2].name=='newJaw')

local onlyBase={target('0000000000000001','jawOpen',Targets.zeroDeltas(3))}
local onlyReplacement={target('0000000000000002','jawOpen',Targets.zeroDeltas(3))}
local clipPatch=assert(TargetProject.capture(onlyBase,onlyReplacement,3,'same-topology',
    {['0000000000000002']=true},{['0000000000000001']=true}))
local clipMesh=mockMesh({target('0000000000000001','jawOpen',Targets.zeroDeltas(3))},3)
clipMesh.hasClips=true
assert(TargetProject.apply(clipMesh,clipPatch,'same-topology'))
assert(#clipMesh.targets==1 and clipMesh.targets[1].name=='jawOpen')

print('FACIAL TARGET PROJECT OK')
