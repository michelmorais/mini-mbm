package.path='editor/?.lua;' .. package.path

local Landmarks=require 'facial_rig_landmarks'
local Project=require 'facial_rig_project'
local Bridge=require 'facial_rig_blender_bridge'

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
assert(project.schemaVersion==5 and project.landmarkEditMode==1)
project.landmarkEditMode=2
project.language='pt_br'
assert(Project.normalize(project).landmarkEditMode==2)
assert(project.language=='pt_br')
assert(Project.normalize({schemaVersion=2}).landmarkEditMode==1,
    'legacy facial projects must default to independent side editing')
assert(Project.addSourceObject(project,'Head')==1)
assert(Project.addSourceObject(project,'Head')==1,
    'source object catalog must remain unique')
assert(Project.normalize(project).sourceObjects[1]=='Head')
assert(Project.sourceObjectSuggestions('main_face')[1]=='Head')
local tongueSuggestions=Project.sourceObjectSuggestions('tongue')
assert(#tongueSuggestions==1 and tongueSuggestions[1]=='Tongue')
local hairSuggestions=Project.sourceObjectSuggestions('hair')
assert(#hairSuggestions==1 and hairSuggestions[1]=='Hair')
local accessorySuggestions=Project.sourceObjectSuggestions('accessory')
assert(#accessorySuggestions==1 and accessorySuggestions[1]=='Accessory')
project.roles[1].object='UpperTeeth'
Project.normalize(project)
assert(project.sourceObjects[2]=='UpperTeeth',
    'legacy role assignments must be included in the source object catalog')
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
local rig=Project.rigDescription(project,topology)
assert(rig.valid and #rig.bones>=3, 'required landmarks must produce a reviewable rig preview')
assert(#rig.roleBindings==1 and rig.roleBindings[1].binding=='facial_skeleton')
assert(#rig.warnings>=2, 'missing optional nose and jaw landmarks should be reported as warnings')
local bridgeReport=Bridge.bindingReport(project,rig,binding)
assert(bridgeReport.valid and #bridgeReport.entries==1)
local payload,payloadError=Bridge.buildPayload(project,topology,rig,binding)
assert(payload and payload.schema=='mini-mbm-facial-rig-bridge-1',payloadError)
project.landmarks[5].placed=false
assert(not Project.rigDescription(project,topology).valid,
    'missing eye center must block the canonical rig preview')
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
