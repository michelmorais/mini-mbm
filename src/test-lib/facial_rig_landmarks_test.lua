package.path='editor/?.lua;' .. package.path

local Landmarks=require 'facial_rig_landmarks'
local Project=require 'facial_rig_project'

local points={
    [1]={{x=-1,y=0,z=0},{x=1,y=0,z=0}},
    [2]={{x=0,y=2,z=0}}
}
local mesh={}
function mesh:getTotalSubset() return 2 end
function mesh:getTotalVertex(_,subset) return #points[subset] end
function mesh:getVertex(_,subset,vertex) return points[subset][vertex] end
function mesh:getIndex(_,subset) return {1,2} end

local topology=Landmarks.inspect(mesh)
assert(topology.vertexCount==3)
assert(topology.bounds.minX==-1 and topology.bounds.maxY==2)
assert(topology.signature~='')
local hit=Landmarks.nearestVertex(topology,{x=-1,y=0,z=-5},{x=0,y=0,z=1},0.1)
assert(hit and hit.index==1)
local project=Project.new()
assert(project.schemaVersion==4 and project.landmarkEditMode==1)
project.landmarkEditMode=2
project.language='pt_br'
assert(Project.normalize(project).landmarkEditMode==2)
assert(project.language=='pt_br')
assert(Project.normalize({schemaVersion=2}).landmarkEditMode==1,
    'legacy facial projects must default to independent side editing')
project.meshPath='fixture.msh'
Project.setTopology(project,topology)
assert(not project.faceOrigin.placed)
assert(Project.setFaceOrigin(project,{x=0,y=0,z=0},1,1))
assert(project.faceOrigin.placed and project.faceOrigin.vertexIndex==1)
assert(not project.landmarks[1].placed, 'placing the face origin must not place landmarks')
local mask=Project.addMask(project,'face',1)
mask.vertices={1,2}
project.landmarks[1].placed=true
project.landmarks[1].position={x=-1,y=0,z=0}
project.landmarks[1].vertexIndex=1
for _,landmark in ipairs(project.landmarks) do
    if landmark.required then landmark.placed=true end
end
local report=Project.validate(project,true,topology)
assert(report.ok and report.maskCount==1 and report.placedLandmarkCount>0)
local binding=Project.bindingDescription(project,topology)
assert(binding.faceOrigin.placed and #binding.roles==1 and #binding.masks==1 and #binding.landmarks==report.placedLandmarkCount)
project.faceOrigin.vertexIndex=99
assert(not Project.validate(project,true,topology).ok)
project.faceOrigin.vertexIndex=1
local stale={signature='stale',vertexCount=3}
assert(not Project.validate(project,true,stale).ok)
mask.topologySignature='stale'
assert(not Project.validate(project,true,topology).ok)
mask.topologySignature=topology.signature
mask.vertices={4}
assert(not Project.validate(project,true,topology).ok)
print('FACIAL RIG LANDMARKS OK')
