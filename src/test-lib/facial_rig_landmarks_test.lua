package.path='editor/?.lua;' .. package.path

local Landmarks=require 'facial_rig_landmarks'
local Project=require 'facial_rig_project'
local Bridge=require 'facial_rig_blender_bridge'
local Expressions=require 'facial_rig_expressions'
local Clips=require 'facial_rig_clips'
local Polygon=require 'facial_rig_polygon'
local ActionTemplates=require 'facial_rig_action_templates'
local PointGizmo=require 'facial_surface_point_gizmo'
local SurfaceEditor=require 'facial_surface_control_editor'

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
local browPointA={x=1,y=2,z=3}
local browPointB={x=4,y=5,z=1}
local shiftedA=Polygon.positionWithFaceDepth(browPointA,3)
local shiftedB=Polygon.positionWithFaceDepth(browPointB,3)
assert(shiftedA.x==browPointA.x and shiftedA.y==browPointA.y and shiftedA.z==6 and
    shiftedB.x==browPointB.x and shiftedB.y==browPointB.y and shiftedB.z==4,
    'face depth must preserve the front-view contour at large offsets')
local project=Project.new()
assert(project.schemaVersion==15 and project.landmarkEditMode==1)
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
assert(legacyProject.schemaVersion==15 and legacyProject.masks[1].representation=='vertex',
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
local gizmoTopology={vertices={{x=0,y=0,z=0},{x=1,y=0,z=0},{x=0,y=1,z=0}},
    triangles={{1,2,3}},bounds={minZ=0,maxZ=0}}
local gizmoArea={kind='polyline',points={{index=1},{index=2}},depthOffset=0}
local gizmoController={kind='point',role='controller',points={{index=3}},depthOffset=0}
local entries={{control=gizmoArea,index=1},{control=gizmoController,index=2}}
local rayOrigin={x=0.7,y=0,z=5}
local rayDirection={x=0,y=0,z=-1}
local lineHit=PointGizmo.pick(entries,gizmoTopology,rayOrigin,rayDirection,0.05,0.05)
assert(lineHit and lineHit.entry.index==1 and lineHit.pointIndex==2,
    'clicking an outline must select the nearer editable endpoint')
local pointHit=PointGizmo.pick(entries,gizmoTopology,{x=0,y=1,z=5},rayDirection,0.05,0.05)
assert(pointHit and pointHit.entry.index==2 and pointHit.pointIndex==1,
    'clicking the controller must select its own anchor')
local axis=PointGizmo.pickAxis({x=0.7,y=1,z=5},rayDirection,{x=0,y=1,z=0},1,0.05)
assert(axis=='x' and math.abs(PointGizmo.axisParameter({x=0.7,y=1,z=5},rayDirection,
    {x=0,y=1,z=0},PointGizmo.axes.x)-0.7)<1e-8,
    'the X gizmo must be selectable and measure displacement along X')
local areaDrag=PointGizmo.snapshot(PointGizmo.dragEntries(gizmoArea,2,entries),gizmoTopology)
assert(#areaDrag==1 and #areaDrag[1].points==1 and areaDrag[1].points[1].index==2,
    'a brow contour drag must capture only the clicked anchor')
PointGizmo.translate(areaDrag,gizmoTopology,{x=0.3,y=0,z=0.4})
assert(PointGizmo.position(gizmoArea,1,gizmoTopology).x==0 and
    math.abs(PointGizmo.position(gizmoArea,2,gizmoTopology).x-1.3)<1e-8 and
    PointGizmo.position(gizmoController,1,gizmoTopology).x==0,
    'moving a brow contour anchor must leave its neighbors and controller still')
local actionLine={kind='polyline',role='action_line',points={{index=1},{index=2}},depthOffset=0}
local lineDrag=PointGizmo.snapshot(PointGizmo.dragEntries(actionLine,1,entries),gizmoTopology)
PointGizmo.translate(lineDrag,gizmoTopology,{x=0,y=0.25,z=0})
assert(PointGizmo.position(actionLine,1,gizmoTopology).y==0.25 and
    PointGizmo.position(actionLine,2,gizmoTopology).y==0,
    'moving an action-line anchor must leave its other anchor still')
local lineController={kind='point',role='line_controller',points={{index=3}},depthOffset=0}
local secondActionLine={kind='polyline',role='action_line',points={{index=2}},depthOffset=0}
local lineGroup={{control=gizmoArea},{control=actionLine},{control=secondActionLine},
    {control=lineController},{control=gizmoController}}
local lineGroupDrag=PointGizmo.snapshot(PointGizmo.dragEntries(lineController,1,lineGroup),gizmoTopology)
assert(#lineGroupDrag==3 and lineGroupDrag[1].control==actionLine and
    lineGroupDrag[2].control==secondActionLine and lineGroupDrag[3].control==lineController,
    'the line controller must capture action lines and itself, but not the brow area')
PointGizmo.translate(lineGroupDrag,gizmoTopology,{x=0.2,y=0,z=0})
assert(PointGizmo.position(actionLine,1,gizmoTopology).x==0.2 and
    math.abs(PointGizmo.position(secondActionLine,1,gizmoTopology).x-1.2)<1e-5 and
    PointGizmo.position(lineController,1,gizmoTopology).x==0.2 and
    PointGizmo.position(gizmoArea,1,gizmoTopology).x==0 and
    PointGizmo.position(gizmoController,1,gizmoTopology).x==0,
    'moving the line controller must leave the area and green controller still')
local snapTopology={vertices={{x=0,y=-1,z=0},{x=0,y=0,z=0},{x=0,y=1,z=0},
    {x=1,y=0,z=0},{x=1,y=1,z=0},{x=1,y=-1,z=0}}}
local snapArea={kind='polygon',role='area',points={{index=1}}}
local snapLine={kind='polyline',role='action_line',points={{index=2}}}
local snapLineController={kind='point',role='line_controller',points={{index=3,
    offset={x=0,y=0.2,z=0}}}}
local snapEntries={{control=snapArea,index=1},{control=snapLine,index=2},
    {control=snapLineController,index=3}}
local snapState={topology=snapTopology,undoStack={}}
local surfaceVertex=3
local snapApi={surfaceHit=function() return {point={x=0,y=1.2,z=0}} end,
    controlEntries=function() return snapEntries end,radius=function() return 0.1 end,
    pickRay=function() return {x=0,y=0,z=2},{x=0,y=0,z=-1} end,
    nearestVertex=function() return {index=surfaceVertex,point=snapTopology.vertices[surfaceVertex]} end,
    pushUndo=function() snapState.undoStack[#snapState.undoStack+1]=true end,
    refresh=function() end,commit=function() return true end}
assert(SurfaceEditor.begin(snapState,0,0,snapApi))
surfaceVertex=5
assert(SurfaceEditor.update(snapState,1,0,snapApi))
assert(snapLine.points[1].index==4 and snapArea.points[1].index==1 and
    snapLineController.points[1].index==5 and
    math.abs(snapLineController.points[1].offset.y-0.2)<1e-5,
    'right dragging the line controller must move only lines and keep its offset above them')
assert(SurfaceEditor.finish(snapState,snapApi))
local snapshot=PointGizmo.snapshot(PointGizmo.dragEntries(gizmoController,1,entries),gizmoTopology)
assert(#snapshot==2 and #snapshot[1].points==2,
    'only the green controller may capture every point in the mask group')
PointGizmo.translate(snapshot,gizmoTopology,{x=0.3,y=0,z=0.4})
assert(math.abs(PointGizmo.position(gizmoArea,1,gizmoTopology).x-0.3)<1e-8 and
    math.abs(PointGizmo.position(gizmoController,1,gizmoTopology).z-0.4)<1e-8,
    'controller translation must preserve the group shape using per-point offsets')
local offsetProject=Project.new()
offsetProject.surfaceControls={gizmoArea,gizmoController}
Project.normalize(offsetProject)
assert(offsetProject.surfaceControls[1].points[1].offset.x==0.3 and
    math.abs(Polygon.resolvePoints(offsetProject.surfaceControls[1],gizmoTopology)[1].position.z-0.4)<1e-8,
    'normalization and point resolution must preserve authored 3D offsets')
local snapControl={points={{index=1,offset={x=0.25,y=0.25,z=0.5}}},depthOffset=0.2}
local snapPlan=PointGizmo.planSurfaceSnap(snapControl,gizmoTopology,gizmoTopology.bounds,
    Landmarks.intersectRaySurface)
assert(snapPlan and snapPlan.changed,'surface snap must find the face at the edited X/Y')
PointGizmo.applySurfaceSnap(snapControl,snapPlan)
local snapped=PointGizmo.position(snapControl,1,gizmoTopology)
assert(snapped.x==0.25 and snapped.y==0.25 and math.abs(snapped.z)<1e-8 and
    snapControl.depthOffset==0,
    'surface snap must adjust depth without losing the authored X/Y placement')
local missedControl={points={{index=1,offset={x=2,y=2,z=0.5}}},depthOffset=0}
assert(not PointGizmo.planSurfaceSnap(missedControl,gizmoTopology,gizmoTopology.bounds,
    Landmarks.intersectRaySurface) and missedControl.points[1].offset.z==0.5,
    'a failed surface projection must leave the shape unchanged')
local curvedTopology={vertices={{x=0,y=0,z=0},{x=1,y=0,z=0.5},{x=0,y=1,z=1}},
    triangles={{1,2,3}},bounds={minZ=0,maxZ=1}}
local curvedLine={points={{index=1,offset={x=0.2,y=0.2,z=0.8}},
    {index=2,offset={x=-0.4,y=0.2,z=0.8}}},depthOffset=0.3}
local curvedPlan=assert(PointGizmo.planSurfaceSnap(curvedLine,curvedTopology,
    curvedTopology.bounds,Landmarks.intersectRaySurface))
PointGizmo.applySurfaceSnap(curvedLine,curvedPlan)
local curvedFirst=PointGizmo.position(curvedLine,1,curvedTopology)
local curvedSecond=PointGizmo.position(curvedLine,2,curvedTopology)
assert(math.abs(curvedFirst.z-0.3)<1e-5 and math.abs(curvedSecond.z-0.5)<1e-5 and
    math.abs(curvedFirst.x-0.2)<1e-5 and math.abs(curvedSecond.x-0.6)<1e-5,
    'surface snap must solve a different Z at each point of a curved action line')
offsetProject.surfaceControls[1].points[1].offset={x=0.125,y=-0.25,z=0.5}
offsetProject.landmarkEditMode=2
mbm={setColor=function() end}
tImGui={}
timer={new=function() return {} end}
local editorUtils=require 'editor_utils'
local offsetPath='/tmp/facial-rig-point-offset-smoke.lua'
assert(Project.save(offsetProject,offsetPath,editorUtils.save))
local restoredProject=assert(Project.load(offsetPath))
local restoredOffset=restoredProject.surfaceControls[1].points[1].offset
assert(restoredOffset.x==0.125 and restoredOffset.y==-0.25 and restoredOffset.z==0.5 and
    restoredProject.landmarkEditMode==2,
    'the editor serializer must preserve point displacement and synchronized-side mode')
os.remove(offsetPath)
local templateProject=Project.new()
assert(ActionTemplates.instantiate(templateProject,surface,
    {placed=true,position={x=0,y=0,z=0},scale=1},1))
assert(#templateProject.facialMasks==8 and #templateProject.surfaceControls==32,
    'a new project must receive both sides of the brow, eye, cheek and mouth-corner templates')
local expectedNewGroups={eye_left='AU5',eye_right='AU5',cheek_left='AU6',
    cheek_right='AU6',mouth_corner_left='AU12',mouth_corner_right='AU12'}
for _,group in ipairs(templateProject.facialMasks) do
    if expectedNewGroups[group.templateId] then
        assert(group.actionUnits[1]==expectedNewGroups[group.templateId] and #group.controlIds==4,
            'new facial masks must expose AU metadata and four editable controls')
        local entries=ActionTemplates.groupControls(templateProject,group)
        assert(entries[1].control.kind=='polygon' and entries[2].control.kind=='polyline' and
            entries[3].control.role=='line_controller' and entries[4].control.role=='controller',
            'new masks must expose area, action line, line controller and group controller')
        local area=ActionTemplates.displayColor(group,entries[1].control)
        local action=ActionTemplates.displayColor(group,entries[2].control)
        local controller=ActionTemplates.displayColor(group,entries[4].control)
        assert(area.r==0.15 and area.g==0.85 and area.b==1 and
            action.r==1 and action.g==0.46 and action.b==0.08 and
            controller.r==0.28 and controller.g==1 and controller.b==0.32,
            'all new masks must use the same cyan, orange and green palette')
    end
end
for _,pairIndex in ipairs({3,5,7}) do
    local left=ActionTemplates.templates()[pairIndex]
    local right=ActionTemplates.templates()[pairIndex+1]
    assert(right.id==left.id:gsub('_left$','_right'))
    for shapeIndex,shape in ipairs(left.shapes) do
        local mirrored=right.shapes[shapeIndex]
        assert(mirrored.role==shape.role and #mirrored.points==#shape.points)
        for pointIndex,point in ipairs(shape.points) do
            local opposite=mirrored.points[pointIndex]
            assert(opposite[1]==-point[1] and opposite[2]==point[2] and opposite[3]==point[3],
                'paired facial templates must start as exact X mirrors')
        end
    end
end
assert(#templateProject.surfaceControls[4].relationships==3 and
    #templateProject.surfaceControls[3].relationships==1 and
    templateProject.surfaceControls[3].relationships[1].controlId=='brow_left_action_line',
    'group and line controllers must link only to their intended controls')
local templateGroup=templateProject.facialMasks[1]
local rightGroup=templateProject.facialMasks[2]
local areaColor=ActionTemplates.displayColor(templateGroup,templateProject.surfaceControls[1])
local actionColor=ActionTemplates.displayColor(templateGroup,templateProject.surfaceControls[2])
local controllerColor=ActionTemplates.displayColor(templateGroup,templateProject.surfaceControls[4])
local lineControllerColor=ActionTemplates.displayColor(templateGroup,templateProject.surfaceControls[3])
assert(actionColor.r~=areaColor.r and controllerColor.g~=areaColor.g and
    lineControllerColor.g~=controllerColor.g,
    'area, action line, line controller and group controller must be visually distinct')
local rightAreaColor=ActionTemplates.displayColor(rightGroup,templateProject.surfaceControls[5])
local rightActionColor=ActionTemplates.displayColor(rightGroup,templateProject.surfaceControls[6])
local rightControllerColor=ActionTemplates.displayColor(rightGroup,templateProject.surfaceControls[8])
assert(areaColor.r==rightAreaColor.r and areaColor.g==rightAreaColor.g and
    areaColor.b==rightAreaColor.b and actionColor.r==rightActionColor.r and
    actionColor.g==rightActionColor.g and actionColor.b==rightActionColor.b and
    controllerColor.r==rightControllerColor.r and controllerColor.g==rightControllerColor.g and
    controllerColor.b==rightControllerColor.b,
    'new left and right brows must share the same area, action-line and controller palette')
templateProject.surfaceControls[2].color=templateGroup.color
assert(ActionTemplates.displayColor(templateGroup,templateProject.surfaceControls[2]).r==actionColor.r and
    actionColor.r~=templateGroup.color.r,
    'an older project with one color per group must show the new action-line accent')
local customActionColor={r=0.1,g=0.2,b=0.3,a=1}
templateProject.surfaceControls[2].color=customActionColor
assert(ActionTemplates.displayColor(templateGroup,templateProject.surfaceControls[2])==customActionColor,
    'an authored action-line color must remain intact')
templateProject.surfaceControls[2].color={r=0.78,g=0.48,b=1,a=1}
local leftOrange=ActionTemplates.displayColor(templateGroup,templateProject.surfaceControls[2])
templateProject.surfaceControls[6].color={r=1,g=0.73,b=0.20,a=1}
local rightOrange=ActionTemplates.displayColor(rightGroup,templateProject.surfaceControls[6])
assert(leftOrange.r==1 and leftOrange.g==rightOrange.g and leftOrange.b==rightOrange.b and
    leftOrange.g>leftOrange.b,
    'previously saved left/right action-line accents must both display as orange')
rightGroup.color={r=1,g=0.35,b=0.75,a=1}
templateProject.surfaceControls[5].color={r=1,g=0.35,b=0.75,a=1}
templateProject.surfaceControls[8].color={r=1,g=0.35,b=0.75,a=1}
rightControllerColor=ActionTemplates.displayColor(rightGroup,templateProject.surfaceControls[8])
assert(rightControllerColor.r==controllerColor.r and rightControllerColor.g==controllerColor.g and
    rightControllerColor.b==controllerColor.b,
    'saved right-brow controller pink must display as green before migration')
assert(ActionTemplates.standardizeColors(templateProject))
assert(not ActionTemplates.standardizeColors(templateProject) and
    templateProject.surfaceControls[5].color.r==areaColor.r and
    templateProject.surfaceControls[6].color.g==actionColor.g and
    templateProject.surfaceControls[8].color.g==controllerColor.g and
    rightGroup.color.b==areaColor.b,
    'old stock colors must migrate once to the shared facial palette')
local customAreaColor={r=0.37,g=0.12,b=0.64,a=1}
templateProject.surfaceControls[5].color=customAreaColor
assert(not ActionTemplates.standardizeColors(templateProject) and
    ActionTemplates.displayColor(rightGroup,templateProject.surfaceControls[5])==customAreaColor,
    'an authored area color must survive the stock-color migration')
rightGroup.color=customAreaColor
templateProject.surfaceControls[6].color=customAreaColor
assert(not ActionTemplates.standardizeColors(templateProject) and
    ActionTemplates.displayColor(rightGroup,templateProject.surfaceControls[6])==customAreaColor,
    'a custom group and action-line color must not be mistaken for stock colors')
local savedBrowProject=Project.new()
assert(ActionTemplates.instantiate(savedBrowProject,surface,
    {placed=true,position={x=0,y=0,z=0},scale=1},1))
for index=#savedBrowProject.facialMasks,3,-1 do table.remove(savedBrowProject.facialMasks,index) end
for index=#savedBrowProject.surfaceControls,9,-1 do table.remove(savedBrowProject.surfaceControls,index) end
savedBrowProject.surfaceControls[1].points[1].offset={x=0.037,y=0,z=0}
assert(not ActionTemplates.instantiate(savedBrowProject,surface,
    {placed=true,position={x=0,y=0,z=0},scale=1},1) and
    #savedBrowProject.facialMasks==2 and savedBrowProject.surfaceControls[1].points[1].offset.x==0.037,
    'opening an authored brow project must not inject new templates or overwrite edits')
local catalogPath='/tmp/facial-rig-catalog-smoke.lua'
assert(Project.save(templateProject,catalogPath,editorUtils.save))
local restoredCatalog=assert(Project.load(catalogPath))
assert(#restoredCatalog.facialMasks==8 and #restoredCatalog.surfaceControls==32,
    'the full new-project template catalog must survive save and reload')
os.remove(catalogPath)
local faceTopology={vertices={},refs={},signature='template-face'}
for _,definition in ipairs(ActionTemplates.templates()) do
    for _,shape in ipairs(definition.shapes) do
        for _,coords in ipairs(shape.points) do
            faceTopology.vertices[#faceTopology.vertices+1]={x=coords[1],y=coords[2],z=coords[3]+0.2}
            faceTopology.refs[#faceTopology.refs+1]={subset=1,vertex=#faceTopology.vertices}
        end
    end
end
local faceVertexCount=#faceTopology.vertices
for index=1,faceVertexCount do
    local vertex=faceTopology.vertices[index]
    faceTopology.vertices[#faceTopology.vertices+1]={x=vertex.x,y=vertex.y,z=vertex.z-0.2}
    faceTopology.refs[#faceTopology.refs+1]={subset=2,vertex=index}
end
local fittedProject=Project.new()
local fittedOrigin={placed=true,vertexIndex=1,position={x=0,y=0,z=0},scale=1}
assert(ActionTemplates.instantiate(fittedProject,faceTopology,fittedOrigin,1))
local fittedArea=fittedProject.surfaceControls[1]
assert(#fittedArea.points==12,'brow template should have a tapered 12-point outline')
for _,control in ipairs(fittedProject.surfaceControls) do
    for _,anchor in ipairs(control.points) do
        assert(anchor.index<=faceVertexCount,'template anchor must stay on the face subset')
    end
end
local otherBrowIndex=fittedProject.surfaceControls[5].points[1].index
fittedArea.points[1].index=faceVertexCount+1
fittedArea.depthOffset=0.3
assert(ActionTemplates.refit(fittedProject,faceTopology,fittedOrigin,1,fittedProject.facialMasks[1]))
assert(fittedArea.points[1].index<=faceVertexCount and fittedArea.depthOffset==0.3,
    'refitting must restore the brow contour while preserving its depth setting')
assert(fittedProject.surfaceControls[5].points[1].index==otherBrowIndex,
    'refitting one brow must preserve the other brow')
local leftArea=fittedProject.surfaceControls[1]
local rightArea=fittedProject.surfaceControls[5]
local savedRightAnchor=rightArea.points[1].index
leftArea.points[1].offset={x=0.04,y=0.03,z=0.02}
assert(ActionTemplates.mirrorGroup(fittedProject,faceTopology,fittedProject.facialMasks[1],0))
local leftWorld=PointGizmo.position(leftArea,1,faceTopology)
local rightWorld=PointGizmo.position(rightArea,1,faceTopology)
assert(math.abs(leftWorld.x+rightWorld.x)<1e-5 and
    math.abs(leftWorld.y-rightWorld.y)<1e-5 and
    math.abs(leftWorld.z-rightWorld.z)<1e-5 and rightArea.points[1].index==savedRightAnchor,
    'synchronized FACS shapes must mirror positions without changing target mesh anchors')
rightArea.points[1].offset.x=rightArea.points[1].offset.x+0.02
assert(ActionTemplates.mirrorGroup(fittedProject,faceTopology,fittedProject.facialMasks[2],0))
leftWorld=PointGizmo.position(leftArea,1,faceTopology)
rightWorld=PointGizmo.position(rightArea,1,faceTopology)
assert(math.abs(leftWorld.x+rightWorld.x)<1e-5,
    'editing the right brow in synchronized mode must update the left brow')
local eyeLeft=fittedProject.surfaceControls[9]
local eyeRight=fittedProject.surfaceControls[13]
eyeLeft.points[1].offset={x=-0.02,y=0.01,z=0.015}
assert(ActionTemplates.mirrorGroup(fittedProject,faceTopology,fittedProject.facialMasks[3],0))
local eyeLeftWorld=PointGizmo.position(eyeLeft,1,faceTopology)
local eyeRightWorld=PointGizmo.position(eyeRight,1,faceTopology)
assert(math.abs(eyeLeftWorld.x+eyeRightWorld.x)<1e-5 and
    math.abs(eyeLeftWorld.y-eyeRightWorld.y)<1e-5 and
    math.abs(eyeLeftWorld.z-eyeRightWorld.z)<1e-5,
    'new paired templates must use the same synchronized-side editing')
local savedAreaIndex=fittedArea.points[2].index
fittedArea.points[2].offset={x=0.123,y=0.045,z=0.02}
local savedActionLine=ActionTemplates.controlById(fittedProject,'brow_left_action_line')
savedActionLine.points[3].offset={x=0,y=0.12,z=0.03}
local expectedLinePosition=PointGizmo.position(savedActionLine,3,faceTopology)
local groupController=ActionTemplates.controlById(fittedProject,'brow_left_controller')
local savedGroupIndex=groupController.points[1].index
local oldLineController=ActionTemplates.controlById(fittedProject,'brow_left_line_controller')
for index,control in ipairs(fittedProject.surfaceControls) do
    if control==oldLineController then table.remove(fittedProject.surfaceControls,index); break end
end
for index,id in ipairs(fittedProject.facialMasks[1].controlIds) do
    if id=='brow_left_line_controller' then
        table.remove(fittedProject.facialMasks[1].controlIds,index)
        break
    end
end
fittedProject.schemaVersion=14
Project.normalize(fittedProject)
assert(ActionTemplates.ensureLineControllers(fittedProject,faceTopology,fittedOrigin,1))
assert(not ActionTemplates.ensureLineControllers(fittedProject,faceTopology,fittedOrigin,1),
    'line-controller migration must be idempotent')
local addedLineController=ActionTemplates.controlById(fittedProject,'brow_left_line_controller')
assert(addedLineController and #addedLineController.points==1 and
    fittedArea.points[2].index==savedAreaIndex and fittedArea.points[2].offset.x==0.123 and
    groupController.points[1].index==savedGroupIndex,
    'adding a line controller to an old project must preserve authored shapes')
local migratedLinePosition=PointGizmo.position(addedLineController,1,faceTopology)
assert(math.abs(migratedLinePosition.y-expectedLinePosition.y-0.05)<1e-5 and
    math.abs(migratedLinePosition.x-expectedLinePosition.x)<1e-5,
    'the migrated controller must be placed near the already edited action line')
local migratedPath='/tmp/facial-rig-line-controller-smoke.lua'
assert(Project.save(fittedProject,migratedPath,editorUtils.save))
local reopened=assert(Project.load(migratedPath))
os.remove(migratedPath)
local reopenedLineController=ActionTemplates.controlById(reopened,'brow_left_line_controller')
assert(reopenedLineController and reopenedLineController.role=='line_controller' and
    reopenedLineController.relationships[1].controlId=='brow_left_action_line' and
    not ActionTemplates.ensureLineControllers(reopened,faceTopology,fittedOrigin,1),
    'saved line controllers must reopen without duplicating or losing their action-line links')
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
