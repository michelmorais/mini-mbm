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
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   FACS-inspired facial action templates for the Facial Rig Editor.
]]--

local M={}
local tPointGizmo=require 'facial_surface_point_gizmo'

local PREVIOUS_ACTION_LINE_COLORS={
    brow_left={r=0.78,g=0.48,b=1,a=1},
    brow_right={r=1,g=0.73,b=0.20,a=1}
}
local AREA_COLOR={r=0.15,g=0.85,b=1,a=1}
local PREVIOUS_RIGHT_AREA_COLOR={r=1,g=0.35,b=0.75,a=1}
local ACTION_LINE_COLOR={r=1,g=0.46,b=0.08,a=1}
local CONTROLLER_COLOR={r=0.28,g=1,b=0.32,a=1}
local LINE_CONTROLLER_COLOR={r=1,g=0.9,b=0.16,a=1}

local TEMPLATE_DEFINITIONS={
    {
        id='brow_left', nameKey='fre_facs_brow_left', actionUnits={'AU1','AU2','AU4'},
        color=AREA_COLOR, mirrorSign=1,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {-0.105,0.275,-0.11},{-0.13,0.305,-0.11},{-0.17,0.322,-0.11},
                {-0.21,0.330,-0.11},{-0.25,0.321,-0.11},{-0.285,0.292,-0.11},
                {-0.30,0.282,-0.11},{-0.275,0.275,-0.11},{-0.235,0.275,-0.11},
                {-0.19,0.265,-0.11},{-0.145,0.260,-0.11},{-0.115,0.265,-0.11}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {-0.12,0.38,-0.09},{-0.16,0.40,-0.09},{-0.21,0.41,-0.09},
                {-0.26,0.40,-0.09},{-0.29,0.37,-0.09}
            }},
            {id='line_controller',role='line_controller',kind='point',closed=false,
                points={{-0.21,0.46,-0.09}}},
            {id='controller',role='controller',kind='point',closed=false,points={{-0.20,0.29,-0.12}}}
        }
    },
    {
        id='brow_right', nameKey='fre_facs_brow_right', actionUnits={'AU1','AU2','AU4'},
        color=AREA_COLOR, mirrorSign=-1,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {0.105,0.275,-0.11},{0.13,0.305,-0.11},{0.17,0.322,-0.11},
                {0.21,0.330,-0.11},{0.25,0.321,-0.11},{0.285,0.292,-0.11},
                {0.30,0.282,-0.11},{0.275,0.275,-0.11},{0.235,0.275,-0.11},
                {0.19,0.265,-0.11},{0.145,0.260,-0.11},{0.115,0.265,-0.11}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {0.12,0.38,-0.09},{0.16,0.40,-0.09},{0.21,0.41,-0.09},
                {0.26,0.40,-0.09},{0.29,0.37,-0.09}
            }},
            {id='line_controller',role='line_controller',kind='point',closed=false,
                points={{0.21,0.46,-0.09}}},
            {id='controller',role='controller',kind='point',closed=false,points={{0.20,0.29,-0.12}}}
        }
    }
}

local function copyColor(color)
    return {r=color.r,g=color.g,b=color.b,a=color.a}
end

local function sameRgb(left,right)
    return left and right and left.r==right.r and left.g==right.g and left.b==right.b
end

local function stockGroupColor(group)
    if not group then return false end
    return sameRgb(group.color,AREA_COLOR) or
        group.templateId=='brow_right' and sameRgb(group.color,PREVIOUS_RIGHT_AREA_COLOR)
end

-- Older projects saved each shape with the same group color. Give their
-- action lines and controller distinct display colors without rewriting the
-- project. A separately authored shape color still takes precedence.
function M.displayColor(group,control)
    local groupColor=group and group.color or AREA_COLOR
    local color=control and control.color
    local role=control and control.role
    if role=='area' or not role then
        local area=color or groupColor
        if group and group.templateId=='brow_right' and
                sameRgb(area,PREVIOUS_RIGHT_AREA_COLOR) then return AREA_COLOR end
        return area
    end
    if role=='action_line' then
        local previous=PREVIOUS_ACTION_LINE_COLORS[group and group.templateId]
        if not color or sameRgb(color,previous) or
                stockGroupColor(group) and sameRgb(color,groupColor) then
            return ACTION_LINE_COLOR
        end
        return color
    end
    if role=='controller' then return CONTROLLER_COLOR end
    if color and not sameRgb(color,groupColor) then return color end
    if role=='line_controller' then return LINE_CONTROLLER_COLOR end
    return color or groupColor
end

-- Only replace colors that came from older stock templates. Explicitly
-- authored colors remain untouched when a saved project is opened.
function M.standardizeColors(project)
    local changed=false
    for _,group in ipairs(project and project.facialMasks or {}) do
        local previousGroupColor=group.color
        if group.templateId=='brow_right' and sameRgb(group.color,PREVIOUS_RIGHT_AREA_COLOR) then
            group.color=copyColor(AREA_COLOR)
            changed=true
        end
        for _,controlId in ipairs(group.controlIds or {}) do
            local control=M.controlById(project,controlId)
            if control and control.role=='controller' and
                    (not sameRgb(control.color,CONTROLLER_COLOR) or control.color.a~=CONTROLLER_COLOR.a) then
                control.color=copyColor(CONTROLLER_COLOR)
                changed=true
            elseif control and control.role=='area' and group.templateId=='brow_right' and
                    sameRgb(control.color,PREVIOUS_RIGHT_AREA_COLOR) then
                control.color=copyColor(AREA_COLOR)
                changed=true
            elseif control and control.role=='action_line' then
                local oldAccent=PREVIOUS_ACTION_LINE_COLORS[group.templateId]
                if sameRgb(control.color,oldAccent) or
                        (sameRgb(previousGroupColor,AREA_COLOR) or
                            group.templateId=='brow_right' and
                            sameRgb(previousGroupColor,PREVIOUS_RIGHT_AREA_COLOR)) and
                        sameRgb(control.color,previousGroupColor) then
                    control.color=copyColor(ACTION_LINE_COLOR)
                    changed=true
                end
            end
        end
    end
    return changed
end

local function oppositeTemplateId(templateId)
    if type(templateId)~='string' then return nil end
    if templateId:sub(-5)=='_left' then return templateId:sub(1,-6)..'_right' end
    if templateId:sub(-6)=='_right' then return templateId:sub(1,-7)..'_left' end
    return nil
end

-- Mirror only positions. The target keeps its own mesh anchors and authored
-- colors; per-point offsets represent the mirrored world-space positions.
function M.mirrorGroup(project,topology,sourceGroup,centerX)
    if not project or not topology or not sourceGroup then return false end
    centerX=tonumber(centerX) or 0
    local sourceId=sourceGroup.templateId
    local targetId=oppositeTemplateId(sourceId)
    if not targetId then return false end
    local targetGroup
    for _,group in ipairs(project.facialMasks or {}) do
        if group.templateId==targetId then targetGroup=group; break end
    end
    if not targetGroup then return false end
    local targetIds={}
    for _,id in ipairs(targetGroup.controlIds or {}) do targetIds[id]=true end
    local prepared,changed={},false
    local prefix=sourceId..'_'
    for _,sourceControlId in ipairs(sourceGroup.controlIds or {}) do
        if sourceControlId:sub(1,#prefix)==prefix then
            local sourceControl=M.controlById(project,sourceControlId)
            local oppositeId=targetId..sourceControlId:sub(#sourceId+1)
            local targetControl=targetIds[oppositeId] and M.controlById(project,oppositeId) or nil
            if sourceControl and targetControl then
                if #sourceControl.points~=#targetControl.points then return false end
                for index=1,#sourceControl.points do
                    local source=tPointGizmo.position(sourceControl,index,topology)
                    local targetAnchor=targetControl.points[index]
                    local base=targetAnchor and topology.vertices[targetAnchor.index]
                    if not source or not base then return false end
                    local offset={x=2*centerX-source.x-base.x,y=source.y-base.y,
                        z=source.z-base.z-(targetControl.depthOffset or 0)}
                    local old=targetAnchor.offset or {}
                    if math.abs(offset.x-(old.x or 0))>1e-6 or
                            math.abs(offset.y-(old.y or 0))>1e-6 or
                            math.abs(offset.z-(old.z or 0))>1e-6 then changed=true end
                    prepared[#prepared+1]={control=targetControl,index=index,offset=offset}
                end
            end
        end
    end
    if changed then
        for _,item in ipairs(prepared) do
            item.control.points[item.index].offset=item.offset
            item.control.topologySignature=topology.signature or ''
        end
    end
    return changed
end

local function nearestVertex(topology,target,used,faceSubset)
    local bestIndex,bestDistance=nil,math.huge
    for index,vertex in ipairs(topology.vertices or {}) do
        local ref=topology.refs and topology.refs[index]
        if not used[index] and (not faceSubset or ref and ref.subset==faceSubset) then
            local dx,dy,dz=vertex.x-target.x,vertex.y-target.y,vertex.z-target.z
            -- The template defines a front-facing outline. Prioritize its X/Y
            -- shape over guessed depth, so cheek/eyelid vertices cannot pull
            -- the lower brow contour down when the mesh surface curves.
            local distance=dx*dx+dy*dy+dz*dz*0.04
            if distance<bestDistance then bestIndex,bestDistance=index,distance end
        end
    end
    return bestIndex
end

local function worldPoint(origin,scale,point)
    return {x=origin.x+point[1]*scale,y=origin.y+point[2]*scale,z=origin.z+point[3]*scale}
end

local function fitPoints(control,shape,topology,faceOrigin,scale)
    local originRef=topology.refs and topology.refs[faceOrigin.vertexIndex]
    local faceSubset=originRef and originRef.subset
    local used={}
    local points={}
    for _,templatePoint in ipairs(shape.points) do
        local target=worldPoint(faceOrigin.position,scale,templatePoint)
        local index=nearestVertex(topology,target,used,faceSubset)
        if index then
            used[index]=true
            local anchor={index=index}
            if shape.role=='line_controller' then
                local vertex=topology.vertices[index]
                anchor.offset={x=target.x-vertex.x,y=target.y-vertex.y,z=0}
            end
            points[#points+1]=anchor
        end
    end
    control.points=points
    control.topologySignature=topology.signature or ''
end

local function addRelationship(controller,targetId)
    controller.relationships=controller.relationships or {}
    for _,relation in ipairs(controller.relationships) do
        if relation.controlId==targetId then return false end
    end
    controller.relationships[#controller.relationships+1]={controlId=targetId,weight=1}
    return true
end

local function linkControllers(project,group,onlyNew)
    local controls={}
    local changed=false
    for _,controlId in ipairs(group.controlIds or {}) do
        controls[controlId]=M.controlById(project,controlId)
    end
    local prefix=group.templateId and group.templateId~='' and group.templateId or group.id
    local groupController=controls[prefix..'_controller']
    local lineController=controls[prefix..'_line_controller']
    for _,controlId in ipairs(group.controlIds or {}) do
        local target=controls[controlId]
        if groupController and target and target~=groupController and
                (not onlyNew or target==lineController) then
            if addRelationship(groupController,controlId) then changed=true end
        end
        if lineController and target and target.role=='action_line' then
            if addRelationship(lineController,controlId) then changed=true end
        end
    end
    return changed
end

local function fitLineControllerToExistingAction(control,project,group,topology,scale)
    local x,y,z,count,anchorIndex=0,0,0,0,nil
    for _,controlId in ipairs(group.controlIds or {}) do
        local action=M.controlById(project,controlId)
        if action and action.role=='action_line' and #(action.points or {})>0 then
            local raw=action.points[math.ceil(#action.points*0.5)]
            local index=type(raw)=='table' and raw.index or raw
            local vertex=topology.vertices[index]
            if vertex then
                local offset=type(raw)=='table' and raw.offset or nil
                x=x+vertex.x+(offset and offset.x or 0)
                y=y+vertex.y+(offset and offset.y or 0)
                z=z+vertex.z+(offset and offset.z or 0)+(action.depthOffset or 0)
                count=count+1
                anchorIndex=anchorIndex or index
            end
        end
    end
    if count==0 then return false end
    local vertex=topology.vertices[anchorIndex]
    control.points={{index=anchorIndex,offset={x=x/count-vertex.x,
        y=y/count+scale*0.05-vertex.y,z=z/count-vertex.z}}}
    control.topologySignature=topology.signature or ''
    return true
end

function M.templates()
    return TEMPLATE_DEFINITIONS
end

function M.names(groups)
    local result={}
    for _,group in ipairs(groups or {}) do result[#result+1]=group.name or group.id or '' end
    return result
end

function M.instantiate(project,topology,faceOrigin,scale)
    if type(project)~='table' or type(topology)~='table' or type(faceOrigin)~='table' or
            not faceOrigin.placed or #(project.facialMasks or {})>0 then return false end
    if #(topology.vertices or {})==0 then return false end
    project.facialMasks=project.facialMasks or {}
    project.surfaceControls=project.surfaceControls or {}
    local actualScale=math.max(tonumber(scale) or tonumber(faceOrigin.scale) or 1,1e-6)
    for _,definition in ipairs(TEMPLATE_DEFINITIONS) do
        local group={id=definition.id,name=definition.id,nameKey=definition.nameKey,
            templateId=definition.id,actionUnits={},color=copyColor(definition.color),
            controlIds={},enabled=true}
        for _,actionUnit in ipairs(definition.actionUnits) do group.actionUnits[#group.actionUnits+1]=actionUnit end
        for _,shape in ipairs(definition.shapes) do
            local control={id=definition.id..'_'..shape.id,name=definition.id..'_'..shape.id,
                groupId=definition.id,role=shape.role,kind=shape.kind,closed=shape.closed==true,
                points={},topologySignature=topology.signature or '',depthOffset=0,
                color=copyColor(M.displayColor(group,{role=shape.role})),relationships={},enabled=true}
            fitPoints(control,shape,topology,faceOrigin,actualScale)
            project.surfaceControls[#project.surfaceControls+1]=control
            group.controlIds[#group.controlIds+1]=control.id
        end
        linkControllers(project,group)
        project.facialMasks[#project.facialMasks+1]=group
    end
    return true
end

-- Add only the missing line controller to saved brow projects. Existing
-- contours, offsets, and group-controller placement remain authored data.
function M.ensureLineControllers(project,topology,faceOrigin,scale)
    if type(project)~='table' or type(topology)~='table' or type(faceOrigin)~='table' or
            not faceOrigin.placed or #(topology.vertices or {})==0 then return false end
    local changed=false
    local actualScale=math.max(tonumber(scale) or tonumber(faceOrigin.scale) or 1,1e-6)
    for _,group in ipairs(project.facialMasks or {}) do
        local definition
        for _,candidate in ipairs(TEMPLATE_DEFINITIONS) do
            if candidate.id==group.templateId then definition=candidate; break end
        end
        if definition then
            local hasAction=false
            for _,controlId in ipairs(group.controlIds or {}) do
                local control=M.controlById(project,controlId)
                if control and control.role=='action_line' then hasAction=true; break end
            end
            local id=definition.id..'_line_controller'
            local controller=M.controlById(project,id)
            if hasAction and not controller then
                local shape
                for _,candidate in ipairs(definition.shapes) do
                    if candidate.role=='line_controller' then shape=candidate; break end
                end
                if shape then
                    controller={id=id,name=id,groupId=group.id,role='line_controller',kind='point',
                        closed=false,points={},topologySignature=topology.signature or '',depthOffset=0,
                        color=copyColor(LINE_CONTROLLER_COLOR),relationships={},enabled=true}
                    if not fitLineControllerToExistingAction(controller,project,group,topology,actualScale) then
                        fitPoints(controller,shape,topology,faceOrigin,actualScale)
                    end
                    project.surfaceControls[#project.surfaceControls+1]=controller
                    changed=true
                end
            end
            if hasAction and controller then
                local listed=false
                for _,controlId in ipairs(group.controlIds or {}) do
                    if controlId==id then listed=true; break end
                end
                if not listed then
                    group.controlIds[#group.controlIds+1]=id
                    changed=true
                end
                if linkControllers(project,group,true) then changed=true end
            end
        end
    end
    return changed
end

-- Refit only an explicitly selected stock template. Callers should take an
-- undo snapshot before using this on an already authored project.
function M.refit(project,topology,faceOrigin,scale,group)
    if type(project)~='table' or type(topology)~='table' or type(faceOrigin)~='table' or
            not faceOrigin.placed or type(group)~='table' or #(topology.vertices or {})==0 then return false end
    local definition
    for _,candidate in ipairs(TEMPLATE_DEFINITIONS) do
        if candidate.id==group.templateId then definition=candidate break end
    end
    if not definition then return false end
    local controls={}
    for _,shape in ipairs(definition.shapes) do
        local id=definition.id..'_'..shape.id
        local control=M.controlById(project,id)
        if not control or control.groupId~=group.id then return false end
        controls[#controls+1]={control=control,shape=shape}
    end
    local actualScale=math.max(tonumber(scale) or tonumber(faceOrigin.scale) or 1,1e-6)
    for _,entry in ipairs(controls) do
        fitPoints(entry.control,entry.shape,topology,faceOrigin,actualScale)
    end
    return true
end

function M.controlById(project,controlId)
    for index,control in ipairs(project and project.surfaceControls or {}) do
        if control.id==controlId then return control,index end
    end
    return nil
end

function M.groupControls(project,group)
    local result={}
    for _,controlId in ipairs(group and group.controlIds or {}) do
        local control,index=M.controlById(project,controlId)
        if control then result[#result+1]={control=control,index=index} end
    end
    return result
end

return M
