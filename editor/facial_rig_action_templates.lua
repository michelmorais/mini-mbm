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

local TEMPLATE_DEFINITIONS={
    {
        id='brow_left', nameKey='fre_facs_brow_left', actionUnits={'AU1','AU2','AU4'},
        color={r=0.15,g=0.85,b=1,a=1}, mirrorSign=1,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {-0.10,0.31,-0.11},{-0.14,0.35,-0.11},{-0.20,0.37,-0.11},
                {-0.27,0.36,-0.11},{-0.34,0.32,-0.11},{-0.37,0.28,-0.11},
                {-0.31,0.25,-0.11},{-0.24,0.24,-0.11},{-0.17,0.25,-0.11},
                {-0.11,0.27,-0.11}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {-0.11,0.40,-0.09},{-0.15,0.43,-0.09},{-0.21,0.44,-0.09},
                {-0.28,0.43,-0.09},{-0.35,0.39,-0.09}
            }},
            {id='controller',role='controller',kind='point',closed=false,points={{-0.235,0.30,-0.12}}}
        }
    },
    {
        id='brow_right', nameKey='fre_facs_brow_right', actionUnits={'AU1','AU2','AU4'},
        color={r=1,g=0.35,b=0.75,a=1}, mirrorSign=-1,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {0.10,0.31,-0.11},{0.14,0.35,-0.11},{0.20,0.37,-0.11},
                {0.27,0.36,-0.11},{0.34,0.32,-0.11},{0.37,0.28,-0.11},
                {0.31,0.25,-0.11},{0.24,0.24,-0.11},{0.17,0.25,-0.11},
                {0.11,0.27,-0.11}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {0.11,0.40,-0.09},{0.15,0.43,-0.09},{0.21,0.44,-0.09},
                {0.28,0.43,-0.09},{0.35,0.39,-0.09}
            }},
            {id='controller',role='controller',kind='point',closed=false,points={{0.235,0.30,-0.12}}}
        }
    }
}

local function copyColor(color)
    return {r=color.r,g=color.g,b=color.b,a=color.a}
end

local function copyPoint(point)
    return {x=point[1],y=point[2],z=point[3]}
end

local function nearestVertex(topology,target,used)
    local bestIndex,bestDistance=nil,math.huge
    for index,vertex in ipairs(topology.vertices or {}) do
        if not used[index] then
            local dx,dy,dz=vertex.x-target.x,vertex.y-target.y,vertex.z-target.z
            local distance=dx*dx+dy*dy+dz*dz
            if distance<bestDistance then bestIndex,bestDistance=index,distance end
        end
    end
    return bestIndex
end

local function worldPoint(origin,scale,point)
    return {x=origin.x+point[1]*scale,y=origin.y+point[2]*scale,z=origin.z+point[3]*scale}
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
                color=copyColor(definition.color),relationships={},enabled=true}
            local used={}
            for _,templatePoint in ipairs(shape.points) do
                local target=worldPoint(faceOrigin.position,actualScale,templatePoint)
                local index=nearestVertex(topology,target,used)
                if index then
                    used[index]=true
                    control.points[#control.points+1]={index=index}
                end
            end
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
