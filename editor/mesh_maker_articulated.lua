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

local testApi=...
tImGui=require 'ImGui'
tUtil=require 'editor_utils'
local Model=require 'articulated_sprite_model'
local Timeline=require 'articulated_sprite_timeline'
local IO=require 'articulated_mesh_io'
local Pose=require 'articulated_mesh_pose'
local View=require 'articulated_mesh_view'
local E={project={frames={{parts={}}},clips={},options={autoKey=false}},frame=1,clip=1,selected=0,time=0,
    mode='animate',playing=false,geometryRevision=0,poseController=Pose,poseRevision=0,
    showTimeline=true,showParts=true,showView=true,showPivot=true,showOutline=true,dirty=false,poseDirty=false,
    cam={azimuth=0.5,elevation=0.25,distance=100,fx=0,fy=0,fz=0},status='',mask=7}
local function L(key) return tLang.L('ame_'..key) end
local function A(key) return tLang.L('ase_'..key) end
local function dpCall(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then
        local key=tostring(result[2]):match('(tl_[%w_]+)$')
        E.status=key and A(key) or tostring(result[2])
        print('[mesh_maker_articulated] '..E.status); tUtil.showMessageWarn(E.status,6)
    end
    return table.unpack(result,1,result.n)
end
local function help(key)
    if tImGui.IsItemHovered(0) then Timeline.tooltip(key) end
end
local function button(key,fn)
    if tImGui.Button(L(key)) then fn() end; help('ame_'..key..'_tip')
end
local function part() return Model.part(E.project,E.frame,E.selected) end
local function changed()
    E.geometryRevision=E.geometryRevision+1; E.dirty=true; E.poseDirty=true; E.modified=true
    E.partDraft=nil; E.clipDraft=nil
end
local function action(fn)
    if not E.base then return false end
    local previous=Model.copy(E.project)
    local ok,data=dpCall(function() fn(); return IO.build(E.base,E.project) end)
    if not ok then E.project=previous; return false end
    E.data=data; Model.commit(E.history,E.project); changed(); return true
end
local function releasePreview(obj)
    if obj then meshDebug:loadMeshPreview(obj,nil); obj:destroy() end
end
local function clear()
    releasePreview(E.preview); E.preview=nil; View.destroy(E)
    if E.previewPath then os.remove(E.previewPath); E.previewPath=nil end
    if E.base then os.remove(E.base); E.base=nil end
end
local function rebuild()
    if not E.data then return end
    local project=E.project
    if E.transient and E.mode=='animate' and project.clips[E.clip] then
        project=Model.copy(project)
        for id,pose in pairs(E.drafts) do IO.record(project,E.clip,id,E.time,pose,E.mask) end
    end
    local path=os.tmpname()..'.msh'; local obj
    local ok=dpCall(function()
        local d=IO.build(E.base,project)
        for f=1,#project.frames do d:addAnim('__ame_frame_'..f,f,f,1,mbm.PAUSED) end
        assert(d:save(path,false,false),'preview_save_failed')
        obj=mesh:new('3d'); assert(meshDebug:loadMeshPreview(obj,path),'preview_load_failed')
    end)
    if not ok then releasePreview(obj); os.remove(path); E.dirty=false; return false end
    local old,oldPath=E.preview,E.previewPath
    E.preview,E.previewPath=obj,path; E.previewRevision=E.geometryRevision; E.playerState=nil; E.poseDirty=true; E.dirty=false
    releasePreview(old); if oldPath then os.remove(oldPath) end
    return true
end
local function applyPlayer()
    if not E.preview or E.dirty or E.previewRevision~=E.geometryRevision then return end
    local clip=E.project.clips[E.clip]
    local signature=table.concat({E.geometryRevision,E.clip,E.frame,E.mode,E.playing and 1 or 0,E.playing and 0 or E.time},':')
    if E.playerState==signature and not E.poseDirty then return end
    E.playerState=signature; E.poseDirty=false
    E.preview:setAnim('__ame_frame_'..E.frame)
    for _,c in ipairs(E.project.clips) do E.preview:disableArticulatedAnimation(c.name) end
    if clip and E.mode=='animate' then
        assert(E.preview:playArticulatedAnimation(clip.name,clip.priority,0,1))
        assert(E.preview:seekArticulatedAnimation(clip.name,E.time))
        if not E.playing then E.preview:pauseArticulatedAnimation(clip.name) end
    end
end
local function load(path)
    local directory=path:gsub('\\','/'):match('^(.*)/[^/]*$') or '.'
    mbm.addPath(directory)
    local d=meshDebug:new(); assert(d:load(path),'load_mesh_failed'); assert(d:getType()=='mesh',L('expected_mesh'))
    -- Resolve references from other machines by basename beside the mesh.
    -- Geometry, materials and non-articulated sections remain in the native asset.
    for f=1,d:getTotalFrame() do for s=1,d:getTotalSubset(f) do
        local texture=d:getTexture(f,s)
        if texture and texture~='' and texture:sub(1,1)~='#' then
            local baseName=texture:gsub('\\','/'):match('[^/]+$')
            local nearby=directory..'/'..baseName; local file=io.open(nearby,'rb')
            if file then file:close(); d:setTexture(f,s,nearby) end
        end
    end end
    local project=IO.read(d); local subsets,bounds=IO.bounds(d)
    local base=os.tmpname()..'.msh'; assert(d:save(base,false,false),'snapshot_failed')
    clear(); E.base=base; E.path=path; E.project=project; E.data=d; E.subsetBounds=subsets; E.bounds=bounds
    E.frame=1; E.clip=1; E.time=0; E.playing=false; E.mode='animate'
    E.selected=project.frames[1].parts[1] and project.frames[1].parts[1].id or 0
    E.timeline=nil; E.poseContext=nil; Pose.clear(E); E.history=Model.history(project)
    changed(); E.modified=false; E.status=L('loaded'); tUtil.sMessageOverlay=E.status; View.fit(E); Pose.sync(E); rebuild(); applyPlayer()
    return true
end
local function save(path)
    assert(E.data,'missing_mesh')
    local d=IO.build(E.base,E.project); assert(d:save(path,false,false),'save_mesh_failed')
    E.path=path; E.modified=false; E.status=L('saved'); return true
end
local function history(redo)
    if not E.history then return end
    local candidate
    if redo then candidate=Model.redo(E.history) else candidate=Model.undo(E.history) end
    if candidate then
        E.project=candidate; E.data=IO.build(E.base,candidate); Pose.clear(E); E.playing=false
        E.clip=math.max(1,math.min(E.clip,#candidate.clips)); E.time=math.min(E.time,candidate.clips[E.clip] and candidate.clips[E.clip].duration or 0)
        changed(); Pose.sync(E)
    end
end
local function selectClip(index)
    E.clip=index; E.time=0; E.playing=false; E.mode='animate'; E.poseDirty=true; Pose.sync(E)
end
local function record()
    local p=part(); if not p or not E.project.clips[E.clip] then return end
    E.playing=false
    if action(function() IO.record(E.project,E.clip,p.id,E.time,E.pose,E.mask) end) then
        Pose.recorded(E); E.status=string.format(A('key_recorded'),p.name,E.time)
    end
end
local function titles()
    E.titles={parts=L('parts')..'###ame_parts',timeline=L('timeline')..'###ame_timeline',view=L('view')..'###ame_view'}
end
local function menu()
    if not tImGui.BeginMainMenuBar() then return end
    if tImGui.BeginMenu(tLang.L('menu_file')) then
        if tImGui.MenuItem(L('open')) then dpCall(function()
            local path=mbm.openFile(E.path,'*.msh')
            if path then if E.modified then E.pendingOpen=path else load(path) end end
        end) end
        help('ame_open_tip')
        tImGui.BeginDisabled(not E.data)
        if tImGui.MenuItem(L('save')) then dpCall(function() if E.path then save(E.path) end end) end
        help('ame_save_tip')
        if tImGui.MenuItem(L('save_as')) then dpCall(function() local path=mbm.saveFile(E.path,'*.msh'); if path then save(path) end end) end
        help('ame_save_tip'); tImGui.EndDisabled(); tImGui.EndMenu()
    end
    if tImGui.BeginMenu(A('edit')) then
        if tImGui.MenuItem(A('undo'),'Ctrl+Z') then dpCall(history,false) end
        if tImGui.MenuItem(A('redo'),'Ctrl+Y') then dpCall(history,true) end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu(tLang.L('menu_options')) then
        if tImGui.MenuItem(L('parts'),nil,E.showParts) then E.showParts=not E.showParts end
        if tImGui.MenuItem(L('timeline'),nil,E.showTimeline) then E.showTimeline=not E.showTimeline end
        if tImGui.MenuItem(L('view'),nil,E.showView) then E.showView=not E.showView end
        local lang=tLang.current; tLang.renderLanguageSubmenu(); if lang~=tLang.current then titles() end
        tImGui.EndMenu()
    end
    tImGui.EndMainMenuBar()
    if E.pendingOpen then
        tImGui.OpenPopup(A('discard'))
        if tImGui.BeginPopupModal(A('discard'),false,E.modalFlags) then
            tImGui.TextWrapped(A('discard_help'))
            if tImGui.Button(L('open')) then local path=E.pendingOpen; E.pendingOpen=nil; tImGui.CloseCurrentPopup(); dpCall(load,path) end
            tImGui.SameLine(); if tImGui.Button(A('cancel')) then E.pendingOpen=nil; tImGui.CloseCurrentPopup() end
            tImGui.EndPopup()
        end
    end
end
local function window(key,pos,size,fn)
    tImGui.SetNextWindowPos(pos,E.first); tImGui.SetNextWindowSize(size,E.first)
    local opened,closed=tImGui.Begin(E.titles[key],true,0)
    if closed then if key=='parts' then E.showParts=false elseif key=='timeline' then E.showTimeline=false else E.showView=false end end
    if opened then fn() end
    tImGui.End()
end
local function partsPanel()
    if not E.data then tImGui.TextWrapped(L('start')); return end
    tImGui.PushItemWidth(190)
    local edit,f=tImGui.DragInt(A('frame'),E.frame,1,1,#E.project.frames)
    if edit then E.frame=math.max(1,math.min(#E.project.frames,f)); E.selected=E.project.frames[E.frame].parts[1] and E.project.frames[E.frame].parts[1].id or 0; E.poseDirty=true; E.partDraft=nil; Pose.sync(E) end
    button('initialize',function() action(function()
        local d=IO.build(E.base,E.project); d:initializeArticulatedParts(); local options=E.project.options
        E.project=IO.read(d); E.project.options=options
        E.selected=E.project.frames[E.frame].parts[1] and E.project.frames[E.frame].parts[1].id or 0
    end) end)
    if tImGui.BeginChild('##ame_part_list',{x=0,y=145},true) then
        for _,p in ipairs(E.project.frames[E.frame].parts) do
            if tImGui.Selectable(string.format('%d - %s##part%s',p.subset,p.name,p.id),E.selected==p.id) then E.selected=p.id; E.partDraft=nil; Pose.sync(E) end
        end
    end
    tImGui.EndChild()
    local p=part()
    if p then
        if E.partDraftSource~=p or not E.partDraft then E.partDraft=Model.copy(p); E.partDraftSource=p; E.pivotEuler=Pose.euler(p.q) end
        local draft=E.partDraft
        local _,name=tImGui.InputText(A('name'),draft.name); draft.name=name
        local edit,position=tImGui.DragFloat3(L('pivot_position'),draft.pivot,0.01,-math.huge,math.huge,'%.3f')
        if edit then draft.pivot=position end
        help('ame_pivot_tip')
        local edit,euler=tImGui.DragFloat3(L('pivot_rotation'),E.pivotEuler,0.5,-360,360,'%.2f')
        if edit then E.pivotEuler=euler; draft.q=Pose.quaternion(euler) end
        help('ame_pivot_tip')
        local parent=Model.part(E.project,E.frame,draft.parent)
        if tImGui.BeginCombo(A('parent'),parent and parent.name or A('none')) then
            if tImGui.Selectable(A('none'),draft.parent==0) then draft.parent=0 end
            for _,other in ipairs(E.project.frames[E.frame].parts) do
                if other.id~=p.id and tImGui.Selectable(other.name..'##parent'..other.id,other.id==draft.parent) then draft.parent=other.id end
            end
            tImGui.EndCombo()
        end
        help('ase_parent_tip')
        button('apply_part',function() action(function()
            Model.reparent(E.project,E.frame,p.id,draft.parent)
            p.name=draft.name; p.pivot=Model.copy(draft.pivot); p.q=Model.copy(draft.q)
        end) end)
        button('remove_part',function() action(function()
            for _,frame in ipairs(E.project.frames) do
                for i=#frame.parts,1,-1 do local item=frame.parts[i]
                    if item.id==p.id then table.remove(frame.parts,i) elseif item.parent==p.id then item.parent=p.parent end
                end
            end
            for _,c in ipairs(E.project.clips) do for i=#c.tracks,1,-1 do if c.tracks[i].part==p.id then table.remove(c.tracks,i) end end end
            E.selected=0
        end) end)
    end
    tImGui.PopItemWidth()
end
local function clipProperties(clip)
    if not tImGui.CollapsingHeader(A('clip_properties')) then return end
    if E.clipDraftSource~=clip or not E.clipDraft then E.clipDraft=Model.copy(clip); E.clipDraftSource=clip end
    local d=E.clipDraft
    local _,name=tImGui.InputText(A('clip_name'),d.name); d.name=name
    local edit,value=tImGui.DragFloat(A('duration'),d.duration,0.01,0.001,36000,'%.3f s'); if edit then d.duration=value end
    edit,value=tImGui.DragFloat(A('speed'),d.speed,0.01,0.001,100,'%.2f'); if edit then d.speed=value end
    edit,value=tImGui.DragInt(A('priority'),d.priority,1,-1000,1000); if edit then d.priority=value end
    d.loop=tImGui.Checkbox(A('loop'),d.loop)
    local additive=tImGui.Checkbox(A('additive'),d.blend==1); d.blend=additive and 1 or 0
    button('apply_clip',function() action(function()
        local name=d.name:match('^%s*(.-)%s*$'); assert(name~='',L('empty_name'))
        for _,c in ipairs(E.project.clips) do assert(c==clip or c.name~=name,A('clip_name_exists')) end
        for _,track in ipairs(clip.tracks) do for _,k in ipairs(track.keys) do assert(k.time<=d.duration,'tl_out_of_range') end end
        clip.name=name; clip.duration=d.duration; clip.speed=d.speed; clip.priority=d.priority; clip.loop=d.loop; clip.blend=d.blend
        E.time=math.min(E.time,d.duration)
    end) end)
end
local function keyProperties(clip)
    local p=part(); tImGui.TextWrapped(A('selected_part')..': '..(p and p.name or A('none')))
    clipProperties(clip)
    if not p then return end
    if E.mode~='animate' then tImGui.TextWrapped(L('setup_help')); return end
    Pose.sync(E)
    if E.trackClip~=clip or E.trackPart~=p.id or E.trackRevision~=E.geometryRevision then
        E.track=nil
        for _,t in ipairs(clip.tracks) do if t.part==p.id then E.track=t; break end end
        E.trackClip=clip; E.trackPart=p.id; E.trackRevision=E.geometryRevision
    end
    local track=E.track
    local mask=track and track.mask or E.mask
    local newMask=0
    for i,name in ipairs({'position','rotation','scale'}) do
        local bit=2^(i-1); if i>1 then tImGui.SameLine() end
        if tImGui.Checkbox(L(name)..'##channel',mask&bit~=0) then newMask=newMask+bit end
    end
    help('articulated_channel_tooltip')
    if newMask~=mask and newMask>0 then
        if track then action(function() track.mask=newMask end) else E.mask=newMask end
    end
    local modified=false; local pose=E.pose
    if mask&1~=0 then
        local edit,v=tImGui.DragFloat3(L('position'),{pose.x,pose.y,pose.z},0.01,-math.huge,math.huge,'%.3f')
        if edit then pose.x,pose.y,pose.z=table.unpack(v); modified=true end
        help('ame_key_tip')
    end
    if mask&2~=0 then
        local edit,v=tImGui.DragFloat3(L('rotation')..' (deg)',pose.euler,0.5,-math.huge,math.huge,'%.2f')
        if edit then pose.euler=v; pose.q=Pose.quaternion(v); modified=true end
        help('ame_key_tip')
    end
    if mask&4~=0 then
        local edit,v=tImGui.DragFloat3(L('scale'),{pose.sx,pose.sy,pose.sz},0.01,-math.huge,math.huge,'%.3f')
        if edit then pose.sx,pose.sy,pose.sz=table.unpack(v); modified=true end
        help('ame_key_tip')
    end
    if tImGui.BeginCombo(A('easing'),A('easing_'..pose.easing)) then
        for i=0,5 do if tImGui.Selectable(A('easing_'..i),pose.easing==i) then pose.easing=i; modified=true end end
        tImGui.EndCombo()
    end
    if pose.easing==5 then
        pose.bezier=pose.bezier or {0.25,0.25,0.75,0.75}
        for i=1,4 do
            local edit,v=tImGui.DragFloat(A('bezier_'..i),pose.bezier[i],0.01,i%2==1 and 0 or -math.huge,i%2==1 and 1 or math.huge,'%.3f')
            if edit then pose.bezier[i]=v; modified=true end
        end
    end
    if modified then
        E.mode='animate'; E.poseRevision=E.poseRevision+1
        if E.project.options.autoKey then record() else Pose.stage(E) end
    end
    if E.transient then tImGui.TextWrapped(A('pending_poses')) end
    if tImGui.Button(A('record')) then record() end
    help('ase_record_tip')
end
local function timelinePanel()
    if not E.data then tImGui.TextWrapped(L('start')); return end
    local mode=E.mode=='setup' and 0 or 1
    mode=tImGui.RadioButton(A('setup'),mode,0); tImGui.SameLine(); mode=tImGui.RadioButton(A('animate'),mode,1)
    if mode~=(E.mode=='setup' and 0 or 1) then E.mode=mode==0 and 'setup' or 'animate'; E.playing=false; E.poseDirty=true; Pose.sync(E) end
    tImGui.SameLine(); E.project.options.autoKey=tImGui.Checkbox(A('auto_key'),E.project.options.autoKey); help('ase_auto_key_tip')
    tImGui.PushItemWidth(170)
    local clip=E.project.clips[E.clip]
    if tImGui.BeginCombo(A('clip'),clip and clip.name or A('none')) then
        for i,c in ipairs(E.project.clips) do if tImGui.Selectable(c.name..'##clip'..i,E.clip==i) then selectClip(i) end end
        tImGui.EndCombo()
    end
    tImGui.PopItemWidth(); tImGui.SameLine()
    if tImGui.Button(A('add_clip')) then action(function()
        local n=#E.project.clips+1; local used={}; for _,c in ipairs(E.project.clips) do used[c.name]=true end
        while used[A('clip')..' '..n] do n=n+1 end
        E.project.clips[#E.project.clips+1]={name=A('clip')..' '..n,duration=1,speed=1,priority=0,loop=true,blend=0,tracks={}}
        selectClip(#E.project.clips)
    end) end
    tImGui.SameLine(); tImGui.BeginDisabled(not clip)
    if tImGui.Button(A('delete_clip')) then action(function() table.remove(E.project.clips,E.clip); selectClip(1) end) end
    tImGui.EndDisabled(); clip=E.project.clips[E.clip]
    if not clip then tImGui.TextWrapped(A('tl_no_clip')); return end
    local avail=tImGui.GetContentRegionAvail(); local side=avail.x>=760
    if tImGui.BeginChild('##ame_graph',{x=side and avail.x-340 or 0,y=side and 0 or 240},false) then Timeline.draw(E,{action=action}) end
    tImGui.EndChild(); if side then tImGui.SameLine() end
    if tImGui.BeginChild('##ame_keys',{x=0,y=0},false) then tImGui.PushItemWidth(200); keyProperties(clip); tImGui.TextWrapped(E.status); tImGui.PopItemWidth() end
    tImGui.EndChild()
end
function onInitScene()
    mbm.setColor(0.08,0.09,0.12)
    titles(); E.camera=mbm.getCamera('3d'); E.camera:setFar(9999999); View.camera(E)
    E.first=tImGui.Flags('ImGuiCond_FirstUseEver'); E.modalFlags=tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize')
    tUtil.sMessageOverlay=L('start'); Pose.sync(E)
end
function onLoop(delta)
    if E.playing and E.preview and E.project.clips[E.clip] then
        local c=E.project.clips[E.clip]; local time=E.preview:getArticulatedAnimationTime(c.name)
        if time then E.time=time; if not c.loop and time>=c.duration then E.playing=false end end
    end
    Pose.sync(E); menu()
    local w,h=mbm.getRealSizeScreen()
    if E.showParts then window('parts',{x=w-320,y=22},{x=320,y=h*0.6},partsPanel) end
    if E.showView then window('view',{x=0,y=22},{x=215,y=280},function()
        button('fit',function() View.fit(E) end)
        if tUtil.drawOrbitGizmo(E.cam,{size=90}) then View.camera(E) end
        local selected=part()~=nil
        E.showPivot=tImGui.Checkbox(L('show_pivot'),E.showPivot)
        if E.pivotMarker then E.pivotMarker.visible=E.showPivot and selected end
        E.showOutline=tImGui.Checkbox(L('show_outline'),E.showOutline)
        if E.outline then E.outline.visible=E.showOutline and selected end
        tImGui.TextWrapped(L('navigation'))
    end) end
    if E.showTimeline then window('timeline',{x=218,y=h*0.63},{x=w-222,y=h*0.36},timelinePanel) end
    local now=mbm.getTimeRun()
    if E.dirty and (not E.lastBuild or now-E.lastBuild>=0.08) then E.lastBuild=now; rebuild() end
    applyPlayer(); View.update(E); View.input(E); tUtil.showOverlayMessage()
end
function onKeyDown(key)
    if key==mbm.getKeyCode('control') then E.control=true end
    if tImGui.GetWantCaptureKeyboard() then return end
    if Timeline.keyDown(E,{action=action},key) then return end
    if E.control and key==mbm.getKeyCode('Z') then dpCall(history,false)
    elseif E.control and key==mbm.getKeyCode('Y') then dpCall(history,true)
    elseif E.control and key==mbm.getKeyCode('S') and E.path then dpCall(save,E.path) end
end
function onKeyUp(key) if key==mbm.getKeyCode('control') then E.control=false end end
function onEndScene() clear() end
if type(testApi)=='table' then
    testApi.state=E; testApi.load=load; testApi.save=save; testApi.rebuild=rebuild; testApi.action=action
    testApi.selectClip=selectClip; testApi.record=record; testApi.history=history
    testApi.timelineCommand=function(op,value) return Timeline.execute(E,{action=action},op,value) end
    testApi.selectTimelineKey=function(key,control) Timeline.selectKey(E,key,control) end
end
