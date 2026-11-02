--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to     |
| permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Explicit and suggested facial surface-control relationships.
]]--

local M={}
local tPointGizmo=require 'facial_surface_point_gizmo'

function M.labelId(controlId,translate)
    if type(controlId)~='string' then return tostring(controlId or '') end
    local groupName,detail
    for split=#controlId-1,1,-1 do
        if controlId:sub(split,split)=='_' then
            local candidate=controlId:sub(1,split-1)
            local key='fre_facs_'..candidate
            local translated=translate(key)
            if translated~=key then
                groupName=translated
                detail=controlId:sub(split+1)
                break
            end
        end
    end
    if not groupName then return controlId end
    local detailKeys={
        area='fre_facs_control_area',
        action_line='fre_facs_control_action_line',
        line_controller='fre_facs_control_line_controller',
        controller='fre_facs_control_group_controller',
        lower_lid_line='fre_facs_control_lower_lid_line',
        bridge_wrinkle='fre_facs_control_bridge_wrinkle',
        left_alar_fold='fre_facs_control_left_alar_fold',
        right_alar_fold='fre_facs_control_right_alar_fold'
    }
    local detailKey=detailKeys[detail]
    if not detailKey then return controlId end
    return groupName..' - '..translate(detailKey)
end

function M.allEntries(project)
    local result,seen={},{}
    local indexed={}
    for index,control in ipairs(project.surfaceControls or {}) do
        indexed[control.id]={control=control,index=index}
    end
    for _,group in ipairs(project.facialMasks or {}) do
        if group.enabled~=false then
            for _,id in ipairs(group.controlIds or {}) do
                local entry=indexed[id]
                if entry and not seen[id] and entry.control.enabled~=false then
                    result[#result+1]={control=entry.control,index=entry.index,group=group}
                    seen[id]=true
                end
            end
        end
    end
    return result
end

function M.dragEntries(control,pointIndex,entries)
    if not control or not pointIndex then return {} end
    local result={{control=control,pointIndex=pointIndex,weight=1}}
    if control.kind~='point' or
            (control.role~='controller' and control.role~='line_controller') then
        return result
    end
    local byId={}
    for _,entry in ipairs(entries or {}) do byId[entry.control.id]=entry.control end
    local seen={[control.id]=true}
    for _,relationship in ipairs(control.relationships or {}) do
        local target=byId[relationship.controlId]
        local weight=math.max(0,math.min(1,tonumber(relationship.weight) or 1))
        if target and target.enabled~=false and target~=control and not seen[target.id] and weight>0 then
            result[#result+1]={control=target,weight=weight}
            seen[target.id]=true
        end
    end
    return result
end

local function side(groupId)
    if type(groupId)~='string' then return nil end
    return groupId:match('_left$') or groupId:match('_right$')
end

local function distanceToSegment(p,a,b)
    local dx,dy=b.x-a.x,b.y-a.y
    local lengthSquared=dx*dx+dy*dy
    if lengthSquared<1e-12 then
        return math.sqrt((p.x-a.x)^2+(p.y-a.y)^2)
    end
    local t=math.max(0,math.min(1,((p.x-a.x)*dx+(p.y-a.y)*dy)/lengthSquared))
    local x,y=a.x+t*dx,a.y+t*dy
    return math.sqrt((p.x-x)^2+(p.y-y)^2)
end

local function insidePolygon(p,points)
    local inside=false
    local previous=points[#points]
    for _,current in ipairs(points) do
        if previous and current and (current.y>p.y)~=(previous.y>p.y) then
            local crossing=current.x+(p.y-current.y)*(previous.x-current.x)/
                (previous.y-current.y)
            if p.x<crossing then inside=not inside end
        end
        previous=current
    end
    return inside
end

function M.candidates(controller,entries,topology,radius,includePoints)
    if not controller or not topology then return {} end
    local pivot=tPointGizmo.position(controller,1,topology)
    if not pivot then return {} end
    local linked={}
    for _,link in ipairs(controller.relationships or {}) do linked[link.controlId]=true end
    local result={}
    local sourceSide=side(controller.groupId)
    for _,entry in ipairs(entries or {}) do
        local target=entry.control
        local targetSide=side(target.groupId)
        if target~=controller and (target.kind~='point' or includePoints) and
                target.enabled~=false and
                not linked[target.id] and (not sourceSide or not targetSide or sourceSide==targetSide) then
            local nearest=math.huge
            local positions={}
            for index=1,#(target.points or {}) do
                local point=tPointGizmo.position(target,index,topology)
                if point then
                    positions[index]=point
                    local dx,dy=point.x-pivot.x,point.y-pivot.y
                    nearest=math.min(nearest,math.sqrt(dx*dx+dy*dy))
                end
            end
            for index=1,#positions-1 do
                if positions[index] and positions[index+1] then
                    nearest=math.min(nearest,distanceToSegment(pivot,positions[index],positions[index+1]))
                end
            end
            if target.closed and #positions>2 and positions[1] and positions[#positions] then
                nearest=math.min(nearest,distanceToSegment(pivot,positions[#positions],positions[1]))
                if insidePolygon(pivot,positions) then nearest=0 end
            end
            if nearest<=radius then
                result[#result+1]={controlId=target.id,name=target.name or target.id,
                    distance=nearest,selected=true,weight=1}
            end
        end
    end
    table.sort(result,function(a,b)
        if a.distance==b.distance then return a.controlId<b.controlId end
        return a.distance<b.distance
    end)
    return result
end

function M.accept(controller,candidates)
    controller.relationships=controller.relationships or {}
    local existing={}
    for _,link in ipairs(controller.relationships) do existing[link.controlId]=true end
    local changed=false
    for _,candidate in ipairs(candidates or {}) do
        if candidate.selected and not existing[candidate.controlId] then
            controller.relationships[#controller.relationships+1]={controlId=candidate.controlId,
                weight=math.max(0,math.min(1,tonumber(candidate.weight) or 1))}
            existing[candidate.controlId]=true
            changed=true
        end
    end
    return changed
end

local function counterpart(id,ids)
    if id:find('_left_',1,true) then
        local swapped=id:gsub('_left_','_right_',1)
        return ids[swapped] and swapped or nil
    end
    if id:find('_right_',1,true) then
        local swapped=id:gsub('_right_','_left_',1)
        return ids[swapped] and swapped or nil
    end
    return ids[id] and id or nil
end

function M.mirror(project,sourceGroup)
    if not sourceGroup or type(sourceGroup.templateId)~='string' then return false end
    local sourceId=sourceGroup.templateId
    local targetId
    if sourceId:sub(-5)=='_left' then targetId=sourceId:sub(1,-6)..'_right'
    elseif sourceId:sub(-6)=='_right' then targetId=sourceId:sub(1,-7)..'_left' end
    if not targetId then return false end
    local controls,ids={},{}
    for _,control in ipairs(project.surfaceControls or {}) do
        controls[control.id]=control
        ids[control.id]=true
    end
    local changed=false
    for _,sourceControlId in ipairs(sourceGroup.controlIds or {}) do
        local source=controls[sourceControlId]
        local oppositeId=targetId..sourceControlId:sub(#sourceId+1)
        local target=controls[oppositeId]
        if source and target and source.kind=='point' then
            local newLinks={}
            for _,link in ipairs(source.relationships or {}) do
                local id=counterpart(link.controlId,ids)
                if id and id~=oppositeId then
                    newLinks[#newLinks+1]={controlId=id,weight=link.weight}
                end
            end
            local old=target.relationships or {}
            local different=#old~=#newLinks
            if not different then
                for index,link in ipairs(newLinks) do
                    if old[index].controlId~=link.controlId or old[index].weight~=link.weight then
                        different=true
                        break
                    end
                end
            end
            if different then target.relationships=newLinks; changed=true end
        end
    end
    return changed
end

function M.mirrorDragged(project,entries,mirrorGroup)
    local seen={}
    for _,entry in ipairs(entries or {}) do
        local groupId=entry.control and entry.control.groupId
        if groupId and not seen[groupId] then
            seen[groupId]=true
            for _,group in ipairs(project.facialMasks or {}) do
                if group.id==groupId or group.templateId==groupId then
                    mirrorGroup(group)
                    break
                end
            end
        end
    end
end

return M
