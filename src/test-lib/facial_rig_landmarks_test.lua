package.path='editor/?.lua;' .. package.path

local Landmarks=require 'facial_rig_landmarks'
local Project=require 'facial_rig_project'
local Bridge=require 'facial_rig_blender_bridge'
local Expressions=require 'facial_rig_expressions'
local Clips=require 'facial_rig_clips'
local Polygon=require 'facial_rig_polygon'
local ActionTemplates=require 'facial_rig_action_templates'

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
local surface={vertices={{x=0,y=0,z=0},{x=1,y=0,z=0},{x=0,y=1,z=0}},triangles={{1,2,3}}}
local surfaceHit=Landmarks.intersectRaySurface(surface,{x=0.2,y=0.2,z=2},{x=0,y=0,z=-1})
assert(surfaceHit and math.abs(surfaceHit.point.z)<0.0001,
    'surface snapping must intersect the selected triangle')
local offsetPoint=Polygon.positionWithDepth(surface,1,surface.vertices[1],0.25)
assert(math.abs(offsetPoint.z-0.25)<0.0001,
    'surface controls must expose a normal-based depth offset')
local project=Project.new()
assert(project.schemaVersion==13 and project.landmarkEditMode==1)
assert(project.morphTargetPreview.targetId=='' and project.morphTargetPreview.weight==1)
project.morphTargetPreview={targetId='0000000000000007',weight=0}
local previewPath='/tmp/facial-rig-preview-project-smoke.lua'
assert(Project.save(project,previewPath,function(root,value,lines)
    lines[#lines+1]=root..'.morphTargetPreview = {targetId='..string.format('%q',value.morphTargetPreview.targetId)..
        ',weight='..tostring(value.morphTargetPreview.weight)..'}'
end))
local restoredPreview=assert(Project.load(previewPath)).morphTargetPreview
assert(restoredPreview.targetId=='0000000000000007' and restoredPreview.weight==0)
os.remove(previewPath)
project.landmarkEditMode=2
project.language='pt_br'
assert(Project.normalize(project).landmarkEditMode==2)
assert(project.language=='pt_br')
assert(Project.normalize({schemaVersion=2}).landmarkEditMode==1,
    'legacy facial projects must default to independent side editing')
assert(Project.normalize({schemaVersion=2}).morphTargetPreview.weight==1,
    'legacy facial projects must retain a default preview weight')
local legacyProject=Project.normalize({schemaVersion=10,masks={{name='legacy',vertices={1}}}})
assert(legacyProject.schemaVersion==13 and legacyProject.masks[1].representation=='vertex',
    'legacy masks must normalize to the vertex representation')
local polygon=Project.addSurfaceControl(project,'brow_left','polygon')
assert(polygon.kind=='polygon' and not polygon.closed and #polygon.points==0)
local polyline=Project.addSurfaceControl(project,'brow_action','polyline')
assert(polyline.kind=='polyline' and not polyline.closed)
local point=Project.addSurfaceControl(project,'brow_controller','point')
assert(point.kind=='point' and not point.closed)
point.relationships={{controlId=polygon.id,weight=0.75},{controlId=polyline.id,weight=1}}
Project.normalize(project)
assert(#project.surfaceControls[3].relationships==2 and
    project.surfaceControls[3].relationships[1].weight==0.75)
assert(#project.surfaceControls==3 and project.surfaceControls[1].name=='brow_left')
local templateProject=Project.new()
assert(ActionTemplates.instantiate(templateProject,surface,
    {placed=true,position={x=0,y=0,z=0},scale=1},1))
assert(#templateProject.facialMasks==2 and #templateProject.surfaceControls==6)
assert(#templateProject.surfaceControls[3].relationships==2,
    'a facial template controller must link its area and action line')
project.expressions=Expressions.syncCatalog(project.expressions,
    {{name='eye.L',convention='ARKit'},{name='eye.R',convention='ARKit'},{name='jawOpen'}})
local expression=Expressions.addPose(project.expressions,'smile')
assert(expression and Expressions.validate(project.expressions).ok)
expression.weights['eye.L']=0.75
Expressions.mirrorPose(project.expressions,expression)
assert(expression.weights['eye.R']==0.75,
    'expression side mirroring must preserve paired weights')
local clipState=Clips.new()
local clip=assert(Clips.addClip(clipState,'blink_clip'))
Clips.insertKey(clip,0,{['eye.L']=0,['eye.R']=0})
Clips.insertKey(clip,1,{['eye.L']=1,['eye.R']=1})
assert(Clips.validate(clip).ok)
local sampled=Clips.sample(clip,0.5,{'eye.L','eye.R'})
assert(math.abs(sampled['eye.L']-0.5)<0.0001 and math.abs(sampled['eye.R']-0.5)<0.0001,
    'facial clips must interpolate target weights')
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
assert(mask.representation=='vertex' and Project.maskRepresentation(mask)=='vertex')
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

-- The hidden vertex lies exactly on the ray; visible nose vertices do not.
local occluded={vertices={{x=-1,y=-1,z=1},{x=1,y=-1,z=1},{x=0,y=1,z=1},
    {x=0,y=0,z=-1},{x=1,y=-1,z=-1},{x=-1,y=-1,z=-1}},triangles={{4,5,6},{1,2,3}}}
local visible=assert(Landmarks.nearestSurfaceVertex(occluded,{x=0,y=0,z=5},{x=0,y=0,z=-2}))
assert(visible.index<=3 and visible.point.z==1,'must pick the nose surface, not the hidden scalp vertex')
assert(not Landmarks.nearestSurfaceVertex(occluded,{x=10,y=0,z=5},{x=0,y=0,z=-1}))
occluded.triangles={{3,2,1},{6,5,4}}
assert(Landmarks.nearestSurfaceVertex(occluded,{x=0,y=0,z=5},{x=0,y=0,z=-1}).point.z==1,
    'surface picking must be independent of winding and triangle order')
