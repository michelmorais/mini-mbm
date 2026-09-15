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

-- 2D adaptation of skeletal_animation_editor.lua's timeline interaction model.
local Model=require 'articulated_sprite_model'
local Pose=require 'articulated_sprite_pose'
local Ops=require 'articulated_sprite_timeline_model'
local M={}
local function L(key) return tLang.L('ase_tl_'..key) end
local function clamp(x,a,b) return math.max(a,math.min(b,x)) end
local function state(E)
    E.timeline=E.timeline or {selection={},count=0,snap=false,step=1/30,gap=0.1,removal=0.1,viewStart=0}
    return E.timeline
end
local function refreshSelection(s)
    s.count=0
    for key in pairs(s.selection) do
        if s.entries[key] then s.count=s.count+1 else s.selection[key]=nil end
    end
    s.batch=nil; s.drag=nil; s.box=nil
end
function M.sync(E)
    local s=state(E); local clip=E.project.clips[E.clip]
    local changed=s.clip~=clip or s.project~=E.project or s.frame~=E.frame
    if changed or s.revision~=E.geometryRevision then
        if changed then
            s.selection={}; s.viewStart=0; s.viewEnd=clip and clip.duration or 1
            s.removalPreview=false
        end
        s.project=E.project; s.clip=clip; s.frame=E.frame; s.revision=E.geometryRevision
        s.rows={}; s.entries={}; s.parts={}
        local tracks={}
        for _,track in ipairs(clip and clip.tracks or {}) do tracks[track.part]=track end
        for _,part in ipairs(E.project.frames[E.frame].parts) do
            local track=tracks[part.id]
            local row={part=part,track=track,keys=track and track.keys or {}}
            s.rows[#s.rows+1]=row; s.parts[part.id]=true
            for _,key in ipairs(row.keys) do s.entries[key]={key=key,part=part.id,row=#s.rows} end
        end
        refreshSelection(s)
    end
    return s,clip
end
function M.seek(E,time)
    if not E.playing and E.mode=='animate' and E.time==time then return end
    E.playing=false; E.mode='animate'; E.time=time; E.poseDirty=true; Pose.sync(E)
end
function M.selectKey(E,key,control)
    local s=M.sync(E); local entry=s.entries[key]
    if not entry then return end
    if control then s.selection[key]=not s.selection[key] or nil
    elseif not s.selection[key] then s.selection={[key]=true} end
    refreshSelection(s)
    E.selected=entry.part
    local p=Model.part(E.project,E.frame,entry.part); E.image=p.image
    M.seek(E,key.time)
    if E.drafts and E.drafts[E.selected] then Pose.recorded(E); E.dirty=true end
    E.pose=Pose.sample(s.clip,E.selected,E.time); E.keyOrigin=key.time
end
local function snapped(s,t,duration)
    if s.snap then t=math.floor(t/s.step+0.5)*s.step end
    return clamp(t,0,duration)
end
function M.execute(E,H,operation,value)
    local s,clip=M.sync(E)
    if not clip then return end
    if operation=='copy' then
        if s.count==0 then return end
        s.clipboard=Ops.selection(clip,s.selection)
        E.status=string.format(L('copied'),#s.clipboard.items)
        return true
    end
    local copied=(operation=='duplicate' or operation=='ripple') and Ops.selection(clip,s.selection) or s.clipboard
    local ok=H.action(function()
        if operation=='paste' or operation=='duplicate' or operation=='ripple' then
            Ops.paste(clip,copied,E.time,s.parts,operation=='ripple')
        elseif operation=='move' then Ops.move(clip,s.selection,value)
        elseif operation=='delete' then Ops.delete(clip,s.selection)
        elseif operation=='insert' then Ops.insertTime(clip,E.time,s.gap)
        elseif operation=='remove' then Ops.removeTime(clip,E.time,s.removal)
        else error('Unknown timeline operation') end
    end)
    if ok then
        Pose.clear(E); E.poseContext=nil; E.keyOrigin=nil
        E.time=math.min(E.time,clip.duration); E.poseDirty=true; E.playing=false
        E.status=L('applied'); s.selection={}; s.removalPreview=false
        M.sync(E); Pose.sync(E)
    end
    return ok
end
-- Resolve translations when shown so changing language also updates open help.
function M.tooltip(key)
    if tImGui.BeginTooltip() then
        local screenWidth=mbm.getRealSizeScreen()
        tImGui.PushTextWrapPos(math.min(420,math.max(80,screenWidth-32)))
        tImGui.Text(tLang.L(key))
        tImGui.PopTextWrapPos()
        tImGui.EndTooltip()
    end
end
local function itemTooltip(key)
    if tImGui.IsItemHovered(0) then M.tooltip(key) end
end
local function button(label,enabled,fn)
    tImGui.BeginDisabled(not enabled)
    if tImGui.Button(L(label)) then fn() end
    tImGui.EndDisabled()
    itemTooltip('ase_tl_'..label..'_tip')
end
local function nextControl(text,width)
    local needed=(width or tImGui.CalcTextSize(text).x)+24
    local right=tImGui.GetWindowPos().x+tImGui.GetWindowSize().x-16
    if tImGui.GetItemRectMax().x+needed<=right then tImGui.SameLine() end
end
function M.toolbar(E,H)
    local s,clip=M.sync(E)
    if not clip then return end
    tImGui.PushItemWidth(95)
    if tImGui.Button(tLang.L('ase_'..(E.playing and 'pause' or 'play'))) then
        Pose.clear(E)
        if not E.playing and E.time>=clip.duration then E.time=0 end
        E.playing=not E.playing; E.mode='animate'; E.poseDirty=true
    end
    itemTooltip('ase_play_tip')
    tImGui.SameLine()
    button('stop',true,function() Pose.clear(E); M.seek(E,0) end)
    tImGui.SameLine()
    local changed,time=tImGui.DragFloat(tLang.L('ase_time'),E.time,0.001,0,clip.duration,'%.3f s')
    itemTooltip('ase_time_tip')
    if changed then M.seek(E,snapped(s,time,clip.duration)) end
    nextControl(L('fit'))
    button('fit',true,function() s.viewStart=0; s.viewEnd=clip.duration; s.batch=nil end)
    nextControl(string.format(L('selected'),s.count)); tImGui.Text(string.format(L('selected'),s.count))
    button('copy',s.count>0,function() M.execute(E,H,'copy') end)
    nextControl(L('paste')); button('paste',s.clipboard~=nil,function() M.execute(E,H,'paste') end)
    nextControl(L('duplicate')); button('duplicate',s.count>0,function() M.execute(E,H,'duplicate') end)
    nextControl(L('delete')); button('delete',s.count>0,function() M.execute(E,H,'delete') end)
    if tImGui.CollapsingHeader(L('time_tools')) then
        button('ripple',s.count>1,function() M.execute(E,H,'ripple') end)
        local edit,gap=tImGui.DragFloat(L('gap'),s.gap,0.01,0.001,60,'%.3f s')
        itemTooltip('ase_tl_gap_tip')
        if edit then s.gap=clamp(gap,0.001,60) end
        nextControl(L('insert')); button('insert',true,function() M.execute(E,H,'insert') end)
        s.removalPreview=tImGui.Checkbox(L('preview_remove'),s.removalPreview or false)
        itemTooltip('ase_tl_preview_remove_tip')
        if s.removalPreview then
            local edit,span=tImGui.DragFloat(L('removal'),s.removal,0.01,0.001,clip.duration,'%.3f s')
            itemTooltip('ase_tl_removal_tip')
            if edit then s.removal=clamp(span,0.001,clip.duration) end
            local impact=s.impact
            if not impact or impact.clip~=clip or impact.revision~=E.geometryRevision or impact.time~=E.time or impact.duration~=s.removal then
                local count,stop=Ops.removalImpact(clip,E.time,s.removal)
                impact={clip=clip,revision=E.geometryRevision,time=E.time,duration=s.removal,count=count,stop=stop}
                s.impact=impact
            end
            local count,stop=impact.count,impact.stop
            tImGui.TextWrapped(string.format(L('remove_impact'),count,E.time,stop))
            button('remove',clip.duration-(stop-E.time)>Ops.epsilon and stop>E.time,
                function() M.execute(E,H,'remove') end)
        end
        s.snap=tImGui.Checkbox(L('snap'),s.snap)
        itemTooltip('ase_tl_snap_tip')
        nextControl('',95+tImGui.CalcTextSize(L('snap_step')).x)
        local edit,step=tImGui.DragFloat(L('snap_step'),s.step,0.001,0.0001,10,'%.6f s')
        itemTooltip('ase_tl_snap_step_tip')
        if edit then s.step=clamp(step,0.0001,10) end
        for i,fps in ipairs({24,25,30,50,60}) do
            if i>1 then tImGui.SameLine() end
            if tImGui.Button(fps..' FPS') then s.step=1/fps; s.snap=true end
        end
    end
    tImGui.PopItemWidth()
end
local function lower(keys,time)
    local lo,hi=1,#keys+1
    while lo<hi do local mid=math.floor((lo+hi)/2)
        if keys[mid].time<time then lo=mid+1 else hi=mid end
    end
    return lo
end
function M.draw(E,H)
    local s,clip=M.sync(E)
    if not clip then tImGui.TextWrapped(L('no_clip')); return end
    M.toolbar(E,H)
    local viewport=math.max(80,tImGui.GetContentRegionAvail().y)
    if not tImGui.BeginChild('##ase_tracks',{x=0,y=viewport},true) then tImGui.EndChild(); return end
    local width=math.max(220,tImGui.GetContentRegionAvail().x)
    local ruler,rowHeight,label=24,24,math.min(170,width*0.35)
    local height=ruler+math.max(1,#s.rows)*rowHeight+4
    tImGui.InvisibleButton('##ase_timeline',{x=width,y=math.max(height,viewport-8)})
    local origin=tImGui.GetItemRectMin(); local mouse=tImGui.GetMousePos()
    local hovered=tImGui.IsItemHovered(0)
    local x0,x1=origin.x+label,origin.x+width-6
    local duration=clip.duration
    local span=clamp((s.viewEnd or duration)-s.viewStart,math.min(0.001,duration),duration)
    s.viewStart=clamp(s.viewStart,0,duration-span); s.viewEnd=s.viewStart+span
    local function tx(time) return x0+(time-s.viewStart)/span*(x1-x0) end
    local function xt(x) return s.viewStart+(x-x0)/(x1-x0)*span end
    if hovered and E.control then
        local wheel=tImGui.GetZoom()
        if math.abs(wheel)>0.00001 then
            local anchor=clamp((mouse.x-x0)/(x1-x0),0,1)
            local t=s.viewStart+span*anchor
            span=clamp(span*math.exp(-wheel*0.2),math.min(0.001,duration),duration)
            s.viewStart=clamp(t-span*anchor,0,duration-span); s.viewEnd=s.viewStart+span
        end
    end
    if hovered and tImGui.IsMouseClicked(2,false) then s.pan={x=mouse.x,start=s.viewStart} end
    if s.pan and tImGui.IsMouseDown(2) then
        s.viewStart=clamp(s.pan.start-(mouse.x-s.pan.x)/(x1-x0)*span,0,duration-span); s.viewEnd=s.viewStart+span
    else s.pan=nil end
    local scroll=tImGui.GetScrollY()
    local first=math.max(1,math.floor((scroll-ruler)/rowHeight)+1)
    local last=math.min(#s.rows,math.ceil((scroll+viewport-ruler)/rowHeight)+1)
    local row=math.floor((mouse.y-origin.y-ruler)/rowHeight)+1
    if hovered and tImGui.IsMouseClicked(0,false) then
        E.playing=false
        local nearest,dist=nil,9
        if s.rows[row] and mouse.x>=x0 then
            local keys=s.rows[row].keys; local at=lower(keys,xt(mouse.x))
            for i=math.max(1,at-1),math.min(#keys,at+1) do
                local d=math.abs(tx(keys[i].time)-mouse.x)
                if d<dist then nearest,dist=keys[i],d end
            end
        end
        if nearest then
            M.selectKey(E,nearest,E.control)
            if not E.control then
                local members=Ops.selection(clip,s.selection)
                s.drag={x=mouse.x,anchor=nearest.time,min=members.first,max=members.last,delta=0,moved=false}
            end
        elseif mouse.x<x0 and s.rows[row] then
            E.selected=s.rows[row].part.id; E.image=s.rows[row].part.image; Pose.sync(E)
        elseif mouse.y<origin.y+ruler then
            s.scrub=true; M.seek(E,snapped(s,xt(mouse.x),duration))
        else s.box={x=mouse.x,y=mouse.y,cx=mouse.x,cy=mouse.y,control=E.control} end
    end
    if s.scrub then
        if tImGui.IsMouseDown(0) then M.seek(E,snapped(s,xt(mouse.x),duration)) else s.scrub=nil end
    end
    if s.drag and tImGui.IsMouseDown(0) then
        local d=s.drag
        d.moved=d.moved or math.abs(mouse.x-d.x)>3
        d.delta=clamp(snapped(s,xt(mouse.x),duration)-d.anchor,-d.min,duration-d.max)
    end
    if s.box and tImGui.IsMouseDown(0) then s.box.cx=mouse.x; s.box.cy=mouse.y end
    if tImGui.IsMouseReleased(0) then
        if s.drag and s.drag.moved and math.abs(s.drag.delta)>Ops.epsilon then
            local time=s.drag.anchor+s.drag.delta
            if M.execute(E,H,'move',s.drag.delta) then M.seek(E,time) end
        end
        s.drag=nil
        if s.box then
            local b=s.box; s.box=nil
            if not b.control then s.selection={} end
            if math.abs(b.cx-b.x)+math.abs(b.cy-b.y)>4 then
                local lo,hi=math.min(b.x,b.cx),math.max(b.x,b.cx)
                local top,bottom=math.min(b.y,b.cy),math.max(b.y,b.cy)
                for ri=math.max(1,math.floor((top-origin.y-ruler)/rowHeight)+1),math.min(#s.rows,math.ceil((bottom-origin.y-ruler)/rowHeight)) do
                    local y=origin.y+ruler+(ri-0.5)*rowHeight
                    if y>=top and y<=bottom then
                        local keys=s.rows[ri].keys
                        for ki=lower(keys,xt(lo)),#keys do
                            local key=keys[ki]; if key.time>xt(hi) then break end
                            s.selection[key]=true
                        end
                    end
                end
            else M.seek(E,snapped(s,xt(b.x),duration)) end
            refreshSelection(s)
        end
    end
    -- Only view/data/selection changes tessellate the static grid and markers.
    local signature=table.concat({origin.x,origin.y,width,viewport,scroll,s.viewStart,s.viewEnd,E.selected,s.drag and 1 or 0},':')
    if not s.batch or signature~=s.signature then
        s.signature=signature; s.labels={}
        s.batch=tImGui.CreateGeometryBatch(function()
            tImGui.AddRectFilled(origin,{x=origin.x+width,y=origin.y+math.max(height,viewport)}, {r=0.055,g=0.065,b=0.085,a=1})
            local raw=span/math.max(1,(x1-x0)/85)
            local power=10^math.floor(math.log(raw)/math.log(10)); local normal=raw/power
            local tick=(normal<=1 and 1 or normal<=2 and 2 or normal<=5 and 5 or 10)*power
            for i=math.ceil(s.viewStart/tick),math.floor(s.viewEnd/tick) do
                local x=tx(i*tick)
                tImGui.AddLine({x=x,y=origin.y},{x=x,y=origin.y+math.max(height,viewport)},{r=0.25,g=0.28,b=0.35,a=0.7},1)
                s.labels[#s.labels+1]={pos={x=x+2,y=origin.y+3},text=string.format('%.3f',i*tick)}
            end
            for ri=first,last do
                local r=s.rows[ri]; local y=origin.y+ruler+(ri-0.5)*rowHeight
                if r.part.id==E.selected then tImGui.AddRectFilled({x=origin.x,y=y-12},{x=origin.x+width,y=y+12},{r=0.15,g=0.3,b=0.45,a=0.6}) end
                local name=r.part.name; local cut=utf8.offset(name,20)
                if cut then name=name:sub(1,cut-1)..'...' end
                s.labels[#s.labels+1]={pos={x=origin.x+4,y=y-7},text=name}
                tImGui.AddLine({x=x0,y=y},{x=x1,y=y},{r=0.28,g=0.28,b=0.35,a=1},1)
                for ki=lower(r.keys,s.viewStart),#r.keys do
                    local key=r.keys[ki]; if key.time>s.viewEnd then break end
                    if not (s.drag and s.selection[key]) then
                        local selected=s.selection[key]
                        tImGui.AddCircleFilled({x=tx(key.time),y=y},selected and 6 or 4,
                            selected and {r=1,g=0.75,b=0.1,a=1} or {r=0.9,g=0.2,b=0.85,a=1},12)
                    end
                end
            end
        end)
    end
    tImGui.AddGeometryBatch(s.batch)
    for _,label in ipairs(s.labels) do tImGui.AddText(label.pos,{r=0.8,g=0.82,b=0.88,a=1},label.text) end
    if s.drag then for key in pairs(s.selection) do
        local entry=s.entries[key]
        if entry then
            local x=tx(key.time+s.drag.delta)
            if x>=x0 and x<=x1 then tImGui.AddCircleFilled({x=x,y=origin.y+ruler+(entry.row-0.5)*rowHeight},6,{r=1,g=0.75,b=0.1,a=1},12) end
        end
    end end
    if s.removalPreview then
        local stop=math.min(duration,E.time+s.removal)
        tImGui.AddRectFilled({x=clamp(tx(E.time),x0,x1),y=origin.y},
            {x=clamp(tx(stop),x0,x1),y=origin.y+math.max(height,viewport)},{r=1,g=0.2,b=0.1,a=0.2})
    end
    if s.box then
        local b=s.box
        tImGui.AddRect({x=math.min(b.x,b.cx),y=math.min(b.y,b.cy)},
            {x=math.max(b.x,b.cx),y=math.max(b.y,b.cy)},{r=0.3,g=0.7,b=1,a=1},0,0,1.5)
    end
    local playhead=tx(E.time)
    if playhead>=x0 and playhead<=x1 then tImGui.AddLine({x=playhead,y=origin.y},{x=playhead,y=origin.y+math.max(height,viewport)},{r=1,g=0.25,b=0.15,a=1},2) end
    if hovered and not s.drag and not s.box then M.tooltip('ase_tl_navigation') end
    s.focused=tImGui.IsWindowFocused(tImGui.Flags('ImGuiFocusedFlags_RootAndChildWindows'))
    tImGui.EndChild()
end
function M.keyDown(E,H,key)
    local s=state(E)
    if not E.showTimeline or not s.focused or tImGui.IsAnyItemActive() then return false end
    if E.control and key==mbm.getKeyCode('C') then M.execute(E,H,'copy'); return true end
    if E.control and key==mbm.getKeyCode('V') then M.execute(E,H,'paste'); return true end
    if key==mbm.getKeyCode('delete') and s.count>0 then M.execute(E,H,'delete'); return true end
    return false
end
return M
