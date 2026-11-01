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

local PREVIOUS_ACTION_LINE_COLORS={
    brow_left={r=0.78,g=0.48,b=1,a=1},
    brow_right={r=1,g=0.73,b=0.20,a=1}
}
local ACTION_LINE_COLOR={r=1,g=0.46,b=0.08,a=1}
local CONTROLLER_COLOR={r=0.28,g=1,b=0.32,a=1}

local TEMPLATE_DEFINITIONS={
    {
        id='brow_left', nameKey='fre_facs_brow_left', actionUnits={'AU1','AU2','AU4'},
        color={r=0.15,g=0.85,b=1,a=1}, mirrorSign=1,
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
            {id='controller',role='controller',kind='point',closed=false,points={{-0.20,0.29,-0.12}}}
        }
    },
    {
        id='brow_right', nameKey='fre_facs_brow_right', actionUnits={'AU1','AU2','AU4'},
        color={r=1,g=0.35,b=0.75,a=1}, mirrorSign=-1,
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

-- Older projects saved each shape with the same group color. Give their
-- action lines and controller distinct display colors without rewriting the
-- project. A separately authored shape color still takes precedence.
function M.displayColor(group,control)
    local groupColor=group and group.color or {r=1,g=1,b=1,a=1}
    local color=control and control.color
    local role=control and control.role
    if role=='area' or not role then return color or groupColor end
    if role=='action_line' then
        local previous=PREVIOUS_ACTION_LINE_COLORS[group and group.templateId]
        if not color or sameRgb(color,groupColor) or sameRgb(color,previous) then
            return ACTION_LINE_COLOR
        end
        return color
    end
    if color and not sameRgb(color,groupColor) then return color end
    if role=='controller' then return CONTROLLER_COLOR end
    return color or groupColor
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
            points[#points+1]={index=index}
        end
    end
    control.points=points
    control.topologySignature=topology.signature or ''
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
        local controllerId=definition.id..'_controller'
        local controller
        for _,controlId in ipairs(group.controlIds) do
            if controlId==controllerId then controller=controlId; break end
        end
        if controller then
            local controllerControl
            for _,control in ipairs(project.surfaceControls) do
                if control.id==controller then controllerControl=control; break end
            end
            if controllerControl then
                for _,controlId in ipairs(group.controlIds) do
                    if controlId~=controller then
                        controllerControl.relationships[#controllerControl.relationships+1]={controlId=controlId,weight=1}
                    end
                end
            end
        end
        project.facialMasks[#project.facialMasks+1]=group
    end
    return true
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
