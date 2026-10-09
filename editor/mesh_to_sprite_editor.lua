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

tImGui=require 'ImGui'
tUtil=require 'editor_utils'
local P=require 'mesh_to_sprite_project'
local A=require 'mesh_to_sprite_animation'
local C=require 'mesh_to_sprite_capture'
local E={project=P.defaults(),images={},time=0,dirty=false,stale=true}
local UI={}
local function L(key) return tLang.L('m2s_'..key) end
local function title(key) return L(key)..'###'..E.titles[key] end
local function dpCall(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then print('[mesh_to_sprite] '..tostring(result[2])); E.message=tostring(result[2]) end
    return table.unpack(result,1,result.n)
end
local function change(render)
    E.dirty=true
    if render~=false then E.stale=true; E.refresh=true end
end
local function action(fn,...)
    local ok,value=dpCall(fn,...)
    if not ok then C.exportFailed(E); if E.job then C.cancel(E) end end
    return ok,value
end
local function selectAnimation(resetStatic)
    local p=E.project; local a=E.animation
    p.staticName=a.static[p.static].name
    if resetStatic then
        p.staticMode=a.static[p.static].mode; p.staticInterval=a.static[p.static].interval; p.baseFrame=a.static[p.static].first
    end
    local duration=a.static[p.static].duration
    p.cycle=p.staticMode==2 or p.staticMode==4 or p.staticMode==6
    if p.kind~='none' then
        for _,clip in ipairs(a[p.kind]) do
            if clip.name==p.clip then
                duration=clip.duration
                p.cycle=clip.loop==true
                if p.kind=='articulated' then duration=duration/math.max(0.0001,math.abs(clip.speed)) end
            end
        end
    end
    p.name=P.suggestAnimationName(p.kind=='none' and p.staticName or p.clip)
    p.start=0; p.stop=duration; p.frameTime=duration/p.count
    E.time=0; change()
end
function E.loadSource(path,restore)
    path=P.absolute(path,mbm.getPathEngine()..'/')
    local signature=P.signature(path)
    local a=A.load(path,E.project.skinning)
    C.release(E)
    E.animation=a
    local p=E.project; p.source=path
    if restore and p.signature~='' and p.signature~=signature then E.message=L('source_changed')
    else E.message=L('loaded') end
    p.signature=signature
    if not restore then
        p.output=''
        p.static=1; p.staticName=a.static[1].name; p.kind='none'; p.clip=''
        if #a.skeletal>0 then p.kind='skeletal'; p.clip=a.skeletal[1].name
        elseif #a.articulated>0 then p.kind='articulated'; p.clip=a.articulated[1].name end
        local w,h,d=a.object:getSize()
        p.camera.distance=math.max(w,h,d,1)*2.5
        p.camera.fx,p.camera.fy,p.camera.fz=a.object:getAABBCenter(true)
        selectAnimation(true)
    end
    E.time=p.start; E.refresh=true; E.stale=true
end
function E.open(path)
    path=P.absolute(path,mbm.getPathEngine()..'/')
    local p=P.load(path)
    C.release(E); E.project=p; E.path=path; E.dirty=false; E.time=p.start; E.stale=true
    if p.source~='' then action(E.loadSource,p.source,true) end
end
function E.save(path)
    path=P.absolute(path,mbm.getPathEngine()..'/')
    P.save(E.project,path); E.path=path; E.dirty=false; E.message=L('saved')
end
function E.capture()
    assert(E.animation,L('load_first'))
    C.begin(E); E.stale=false; E.message=L('capturing')
end
function E.export(path)
    path=P.absolute(path,mbm.getPathEngine()..'/')
    C.export(E,path); E.project.output=path; E.dirty=true; E.message=L('exported')
end
local function newProject()
    C.release(E); E.project=P.defaults(); E.path=nil; E.dirty=false; E.stale=true
    E.time=0; E.playing=false; E.refresh=false; E.message=L('welcome')
end
local function saveDialog()
    local path=mbm.saveFile(E.path or 'capture.mesh2sprite','*.mesh2sprite')
    if path and path~='' then return action(E.save,path) end
    return false
end
local function request(fn)
    if E.dirty then E.pending=fn; E.openConfirm=true else action(fn) end
end
local function openDialog()
    local path=mbm.openFile(E.path or '','*.mesh2sprite')
    if path and path~='' then E.open(path) end
end
local function sourceDialog()
    local path=mbm.openFile(E.project.source,'*.msh','*.mbm')
    if path and path~='' then action(E.loadSource,path,false) end
end
local function outputDialog()
    local path=mbm.saveFile(E.project.output~='' and E.project.output or P.basename(E.project.source):gsub('%.[^%.]+$','')..'.spt','*.spt')
    if path and path~='' then action(E.export,path) end
end
function UI.menu()
    if tImGui.BeginMainMenuBar() then
        if tImGui.BeginMenu(L('project')) then
            if not E.job then
                if tImGui.MenuItem(L('new'),'Ctrl+N') then request(newProject) end
                if tImGui.MenuItem(L('open'),'Ctrl+O') then request(openDialog) end
                if tImGui.MenuItem(L('save'),'Ctrl+S') then
                    if E.path then action(E.save,E.path) else saveDialog() end
                end
                if tImGui.MenuItem(L('save_as')) then saveDialog() end
                if tImGui.MenuItem(L('quit')) then request(mbm.quit) end
            end
            tImGui.EndMenu()
        end
        if tImGui.BeginMenu(L('language')) then
            if tImGui.MenuItem('English') then tLang.setLanguage('en') end
            if tImGui.MenuItem('Portugues') then tLang.setLanguage('pt_br') end
            tImGui.EndMenu()
        end
        if tImGui.BeginMenu(tLang.L('menu_about')) then
            if tImGui.MenuItem(tLang.L('mbm_engine')) then tLang.openDocumentation() end
            if tImGui.BeginMenu(tLang.L('menu_version')) then
                tImGui.TextDisabled(E.versions)
                tImGui.EndMenu()
            end
            tImGui.EndMenu()
        end
        tImGui.EndMainMenuBar()
    end
    if E.openConfirm then tImGui.OpenPopup(title('unsaved')); E.openConfirm=false end
    local opened=tImGui.BeginPopupModal(title('unsaved'),false,E.flags.auto)
    if opened then
        tImGui.Text(L('unsaved_help'))
        if tImGui.Button(L('save')) then
            local ok=E.path and action(E.save,E.path) or saveDialog()
            if ok then local fn=E.pending; E.pending=nil; tImGui.CloseCurrentPopup(); action(fn) end
        end
        tImGui.SameLine()
        if tImGui.Button(L('discard')) then local fn=E.pending; E.pending=nil; tImGui.CloseCurrentPopup(); action(fn) end
        tImGui.SameLine()
        if tImGui.Button(L('cancel')) then E.pending=nil; tImGui.CloseCurrentPopup() end
        tImGui.EndPopup()
    end
end
local function number(key,obj,field,integer,min,max,render)
    local changed,value
    local label=L(key)..'##'..field
    if key~='x' and key~='y' and key~='z' and key~='distance' and key~='near' and key~='far' then
        tImGui.Text(L(key)); label='##'..field
    end
    tImGui.SetNextItemWidth(140)
    if integer then changed,value=tImGui.InputInt(label,obj[field],1,10)
    else changed,value=tImGui.DragFloat(label,obj[field],0.1,min or 0,max or 0,'%.3f') end
    if changed and value and value==value then
        if min then value=math.max(min,value) end
        if max then value=math.min(max,value) end
        obj[field]=value; change(render)
        if obj==E.project and (field=='width' or field=='height') and obj.keepAspect and not obj.followImage then
            P.setFrameDimension(obj,'frameWidth',obj.frameWidth)
        end
        if obj==E.project and (field=='count' or field=='start' or field=='stop') and obj.stop>obj.start then
            obj.frameTime=(obj.stop-obj.start)/obj.count
        end
    end
end
local function vector(key,obj)
    tImGui.Text(L(key)); tImGui.PushID(key)
    for _,k in ipairs({'x','y','z'}) do number(k,obj,k,false) end
    tImGui.PopID()
end
local function checkbox(key,obj,field)
    local value=tImGui.Checkbox(L(key),obj[field])
    if value~=obj[field] then obj[field]=value; change() end
end
local function combo(key,index,names)
    tImGui.Text(L(key)); tImGui.SetNextItemWidth(260)
    return tImGui.Combo('##'..key,index,names)
end
local function color(key,obj,field)
    tImGui.Text(L(key)); tImGui.SetNextItemWidth(250)
    local changed,value=tImGui.ColorEdit4('##'..key,obj[field])
    if changed then obj[field]=value; change() end
end
function UI.frameSettings()
    local p=E.project
    local follow=tImGui.Checkbox(L('follow_image'),p.followImage)
    if follow~=p.followImage then
        p.frameWidth,p.frameHeight=P.frameSize(p)
        p.followImage=follow; change(false)
    end
    local keep=tImGui.Checkbox(L('keep_aspect'),p.keepAspect)
    if keep~=p.keepAspect then
        p.keepAspect=keep
        if keep then P.setFrameDimension(p,'frameWidth',p.frameWidth) end
        change(false)
    end
    local fw,fh=P.frameSize(p)
    if p.followImage then
        tImGui.Text(string.format('%s: %.1f x %.1f',L('frame_size'),fw,fh))
    else
        for _,entry in ipairs({{'frame_width','frameWidth'},{'frame_height','frameHeight'}}) do
            local old=p[entry[2]]
            number(entry[1],p,entry[2],false,0.01,100000,false)
            if old~=p[entry[2]] then P.setFrameDimension(p,entry[2],p[entry[2]]) end
        end
    end
    fw,fh=P.frameSize(p)
    if math.abs(fw/fh-p.width/p.height)>0.0001 then
        tImGui.TextWrapped(L('aspect_warning'))
    end
    number('pivot_x',p,'pivotX',false,0,1,false); number('pivot_y',p,'pivotY',false,0,1,false)
    local show=tImGui.Checkbox(L('show_pivot'),p.showPivot)
    if show~=p.showPivot then p.showPivot=show; E.dirty=true end
end
function UI.options()
    local p=E.project
    tUtil.setInitialWindowPositionLeft(title('options'),0,25,310)
    local opened=tImGui.Begin(title('options'),false,0)
    if opened then
        tImGui.TextWrapped(L('project')..': '..(E.path or L('untitled'))..(E.dirty and ' *' or ''))
        if E.job then
            tImGui.Text(L('capturing'))
            tImGui.ProgressBar((E.job.index-1)/p.count,{x=260,y=20})
            if tImGui.Button(L('cancel')) then C.cancel(E); E.stale=true; E.refresh=true end
        else
            if tImGui.Button(L('source')) then sourceDialog() end
            tImGui.Text(p.source~='' and P.basename(p.source) or L('load_first'))
            if E.animation then
                local names={}; for i,c in ipairs(E.animation.static) do names[i]=c.name end
                local selected=p.static
                if not E.animation.static[selected] or E.animation.static[selected].name~=p.staticName then selected=0 end
                local changed,index=combo('static',selected,names)
                if changed then p.static=index; selectAnimation(true) end
                local modes={L('paused'),L('growing'),L('growing_loop'),L('decreasing'),L('decreasing_loop'),L('recursive'),L('recursive_loop')}
                local mc,mi=combo('static_mode',p.staticMode+1,modes)
                if mc then
                    p.staticMode=mi-1
                    if p.kind=='none' then p.cycle=p.staticMode==2 or p.staticMode==4 or p.staticMode==6 end
                    change()
                end
                number('static_interval',p,'staticInterval',false,0.001,3600)
                local base=E.animation.static[p.static]
                if p.staticMode==0 and base then number('base_frame',p,'baseFrame',true,base.first,base.last) end
                local kinds={'none','articulated','skeletal'}; local index=1
                for i,k in ipairs(kinds) do if p.kind==k then index=i end end
                changed,index=combo('combine',index,{L('none'),L('articulated'),L('skeletal')})
                if changed then
                    local kind=kinds[index]
                    if kind=='none' or #E.animation[kind]>0 then
                        p.kind=kind; p.clip=kind=='none' and '' or E.animation[kind][1].name; selectAnimation()
                    else E.message=L('no_clips') end
                end
                if p.kind~='none' then
                    names={}; index=0
                    for i,c in ipairs(E.animation[p.kind]) do names[i]=c.name; if c.name==p.clip then index=i end end
                    changed,index=combo('clip',index,names)
                    if changed then p.clip=names[index]; selectAnimation() end
                    if p.kind=='skeletal' then
                        local methods={'auto','lbs','dqs'}; local si=1
                        for i,k in ipairs(methods) do if k==p.skinning then si=i end end
                        local sc,sn=combo('skinning',si,methods)
                        if sc then p.skinning=methods[sn]; change() end
                    end
                end
                number('start',p,'start',false,0,36000)
                number('stop',p,'stop',false,p.start,36000)
                number('count',p,'count',true,1,4096)
                checkbox('cycle',p,'cycle')
                number('frame_time',p,'frameTime',false,0.001,3600,false)
                if tImGui.CollapsingHeader(L('mesh')) then
                    checkbox('animate_transform',p,'animateTransform')
                    if p.animateTransform then tImGui.Text(L('initial_transform')) end
                    vector('position',p.position); vector('rotation',p.rotation); vector('scale',p.scale)
                    if p.animateTransform then
                        tImGui.Separator();tImGui.Text(L('final_transform'))
                        tImGui.PushID('finalTransform')
                        vector('position',p.finalTransform.position)
                        vector('rotation',p.finalTransform.rotation)
                        vector('scale',p.finalTransform.scale)
                        tImGui.PopID()
                        tImGui.TextWrapped(L('transform_hint'))
                    end
                end
                if tImGui.CollapsingHeader(L('capture_settings')) then
                    number('width',p,'width',true,8,4096); number('height',p,'height',true,8,4096)
                    UI.frameSettings()
                    color('background',p,'background')
                end
                tImGui.Text(string.format('%.1f MiB RGBA',4.0*p.width*p.height*p.count/1024^2))
                tImGui.Text(L('name'));tImGui.SetNextItemWidth(250)
                local changed,name=tImGui.InputText('##m2sName',p.name,32)
                if changed then p.name=name; change(false) end
                if tImGui.Button(L('capture')) then action(E.capture) end
                if E.captured then
                    tImGui.Text(E.stale and L('stale') or L('ready'))
                    if not E.stale and tImGui.Button(L('export')) then outputDialog() end
                end
            end
        end
        if E.message then tImGui.TextWrapped(E.message) end
    end
    tImGui.End()
end
function UI.camera()
    local p=E.project; local c=p.camera; local w,h=mbm.getRealSizeScreen()
    tImGui.SetNextWindowPos({x=math.max(315,w-290),y=25},E.flags.always)
    tImGui.SetNextWindowSize({x=280,y=math.max(240,(h-40)*0.55)},E.flags.always)
    local opened=tImGui.Begin(title('camera'),false,0)
    if opened and not E.job then
        if tUtil.drawOrbitGizmo(c,{size=110}) then change() end
        local ce=math.cos(c.elevation)
        local pos={x=c.fx+c.distance*ce*math.sin(c.azimuth),y=c.fy+c.distance*math.sin(c.elevation),z=c.fz+c.distance*ce*math.cos(c.azimuth)}
        local old=P.copy(pos); vector('position',pos)
        if pos.x~=old.x or pos.y~=old.y or pos.z~=old.z then
            local dx,dy,dz=pos.x-c.fx,pos.y-c.fy,pos.z-c.fz
            local d=math.sqrt(dx*dx+dy*dy+dz*dz)
            if d>0.001 then c.distance=d; c.azimuth=math.atan(dx,dz); c.elevation=math.asin(math.max(-0.9999,math.min(0.9999,dy/d))) end
        end
        tImGui.Text(L('focus'))
        number('x',c,'fx',false); number('y',c,'fy',false); number('z',c,'fz',false)
        number('distance',c,'distance',false,0.01,100000)
        local degrees=math.deg(c.roll);local roll={value=degrees}
        number('roll',roll,'value',false,-180,180)
        if roll.value~=degrees then c.roll=math.rad(roll.value) end
        if tImGui.CollapsingHeader(L('clipping')) then
            number('near',c,'near',false,0.001,c.far-0.001); number('far',c,'far',false,c.near+0.001,1000000)
        end
        if E.animation and tImGui.Button(L('fit')) then
            local w,h,d=E.animation.object:getAABB(true)
            c.fx,c.fy,c.fz=E.animation.object:getAABBCenter()
            c.distance=math.max(w,h,d,1)*2.5/math.min(1,p.width/p.height); change()
        end
        if tImGui.Button(L('reset')) then p.camera=P.defaults().camera; change() end
    end
    tImGui.End()
end
function UI.light()
    local l=E.project.light; local w,h=mbm.getRealSizeScreen()
    tImGui.SetNextWindowPos({x=math.max(315,w-290),y=35+math.max(240,(h-40)*0.55)},E.flags.always)
    tImGui.SetNextWindowSize({x=280,y=math.max(180,(h-40)*0.45)},E.flags.always)
    local opened=tImGui.Begin(title('light'),false,0)
    if opened and not E.job then
        checkbox('enabled',l,'enabled')
        if tUtil.drawOrbitGizmo(l.orbit,{size=110}) then change() end
        color('ambient',l,'ambient'); color('directional',l,'color')
        if tImGui.CollapsingHeader(L('direction')) then
            local d=tUtil.dirFromOrbit(l.orbit); local before=P.copy(d)
            vector('direction',d)
            if d.x~=before.x or d.y~=before.y or d.z~=before.z then l.orbit=tUtil.orbitFromDir(d) end
        end
        if tImGui.CollapsingHeader(L('point')) then
            vector('position',l.position); number('radius',l,'radius',false,0.01,100000)
            color('color',l,'point')
        end
        if tImGui.Button(L('reset')) then E.project.light=P.defaults().light; change() end
    end
    tImGui.End()
end
local function imagePreview(info,size,renderTarget,pivot)
    local origin=tImGui.GetCursorScreenPos()
    if E.checker then
        local cursor=tImGui.GetCursorScreenPos()
        local side=math.max(size.x,size.y)
        tImGui.Image(E.checker,size,{x=0,y=0},{x=size.x/side,y=size.y/side})
        tImGui.SetCursorScreenPos(cursor)
    end
    if renderTarget then tImGui.Image(info,size,E.renderUv0,E.renderUv1)
    else tImGui.Image(info,size) end
    if E.project.showPivot and pivot then
        local point={x=origin.x+size.x*pivot.pivotX,y=origin.y+size.y*pivot.pivotY}
        tImGui.AddCircleFilled(point,6,{r=0,g=0,b=0,a=1},16)
        tImGui.AddCircleFilled(point,4,{r=1,g=0.8,b=0,a=1},16)
    end
end
function UI.preview()
    local w,h=mbm.getRealSizeScreen()
    tImGui.SetNextWindowPos({x=315,y=25},E.flags.always)
    tImGui.SetNextWindowSize({x=math.max(240,w-615),y=math.max(300,h-30)},E.flags.always)
    local opened=tImGui.Begin(title('preview'),false,0)
    if opened then
        if E.texture then
            local p=E.project; local available=tImGui.GetContentRegionAvail()
            local reserved=E.captured and math.max(220,available.y*0.4) or 0
            local size=P.fitSize(p.width,p.height,available.x,available.y-100-reserved)
            imagePreview(E.texture,size,true,p)
            if E.animation and not E.job then
                if tImGui.Button(E.playing and L('pause') or L('play')) then
                    if not E.playing and E.time>=p.stop then E.time=p.start; E.poseDirty=true end
                    E.playing=not E.playing
                end
                local changed,time=tImGui.SliderFloat(L('time'),E.time,p.start,math.max(p.start+0.001,p.stop),'%.3f s')
                if changed then E.time=time; E.poseDirty=true; E.playing=false end
                if tImGui.Button(L('previous')) then E.time=math.max(p.start,E.time-(p.stop-p.start)/p.count); E.poseDirty=true end
                tImGui.SameLine()
                if tImGui.Button(L('next')) then E.time=math.min(p.stop,E.time+(p.stop-p.start)/p.count); E.poseDirty=true end
            end
        end
        if E.captured then
            tImGui.Text(L('frame'))
            tImGui.SetNextItemWidth(-1)
            local changed,index=tImGui.SliderInt('##capturedFrame',E.selected or 1,1,#E.images)
            if changed then E.selected=index end
            index=E.selected or 1
            if E.thumbnailIndex~=index then
                -- Reuse one preview texture. Reload keeps its alpha metadata and avoids
                -- fetching released GPU images from the shared per-filename cache.
                if E.thumbnail then assert(E.thumbnail:reload(E.images[index]))
                else E.thumbnail=assert(mbm.loadTexture(E.images[index])) end
                E.thumbnailIndex=index
            end
            local available=tImGui.GetContentRegionAvail()
            local fw,fh=P.frameSize(E.project)
            local size=P.fitSize(fw,fh,E.exported and (available.x-12)/2 or available.x,available.y-60)
            local row=tImGui.GetCursorScreenPos()
            if E.thumbnail then imagePreview(E.thumbnail,size,false,E.project) end
            if E.exported then
                tImGui.SetCursorScreenPos({x=row.x+(available.x+12)/2,y=row.y})
                if E.spriteTexture then
                    imagePreview(E.spriteTexture,P.fitSize(E.spriteWidth,E.spriteHeight,(available.x-12)/2,available.y-60),true,E.exportConfig)
                end
                tImGui.Text(L('exported'))
                if tImGui.Button(E.spritePlaying and L('pause_sprite') or L('play_sprite')) then E.spritePlaying=not E.spritePlaying end
            end
        end
    end
    tImGui.End()
end
function onInitScene()
    E.originalLight=mbm.getLightState('3d')
    E.versions=string.format('%s\nIMGUI: %s',mbm.get('version'),tImGui.GetVersion())
    -- Same render-target orientation as Sprite Maker; file textures keep standard UVs.
    local topOrigin=mbm.get('USE_DIRECTX9') or mbm.get('USE_DIRECTX11') or mbm.get('USE_METAL')
    E.renderUv0={x=0,y=topOrigin and 0 or 1}
    E.renderUv1={x=1,y=topOrigin and 1 or 0}
    E.flags={always=tImGui.Flags('ImGuiCond_Always'),once=tImGui.Flags('ImGuiCond_Once'),auto=tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize')}
    E.titles={options='m2sOptions',camera='m2sCamera',light='m2sLight',preview='m2sPreview',unsaved='m2sUnsaved'}
    E.checkerPath=tUtil.createAlphaPattern(128,128,16,{r=170,g=170,b=170},{r=100,g=100,b=100})
    if E.checkerPath then E.checker=mbm.loadTexture(E.checkerPath) end
    tUtil.sMessageOverlay=''
    E.message=L('welcome'); mbm.setColor(0.12,0.12,0.14)
end
function onLoop(delta)
    action(function()
        C.tickSprite(E,delta)
        if C.tick(E) then E.selected=1; E.message=L('ready') end
        if E.animation and not E.job then
            if E.refresh then E.refresh=false; E.poseDirty=false; C.preview(E,E.time)
            elseif E.playing or E.poseDirty then
                local p=E.project
                if E.playing then
                    E.time=E.time+delta
                    if E.time>p.stop then
                        if p.cycle and p.stop>p.start then E.time=p.start+(E.time-p.start)%(p.stop-p.start)
                        else E.time=p.stop; E.playing=false end
                    end
                end
                A.pose(E.animation,E.time); E.target.visible=true; E.pendingPreview=true; E.poseDirty=false
            end
        end
    end)
    if E.shortcut then local fn=E.shortcut; E.shortcut=nil; request(fn) end
    UI.menu(); UI.options(); UI.camera(); UI.light(); UI.preview()
    tUtil.showOverlayMessage()
end
function onKeyDown(key)
    if key==mbm.getKeyCode('control') then E.control=true end
    if E.control and not E.job then
        if key==mbm.getKeyCode('S') then if E.path then action(E.save,E.path) else saveDialog() end end
        -- New/Open confirmations are emitted in the next ImGui frame.
        if key==mbm.getKeyCode('N') then E.shortcut=newProject end
        if key==mbm.getKeyCode('O') then E.shortcut=openDialog end
    end
end
function onKeyUp(key) if key==mbm.getKeyCode('control') then E.control=false end end
function onTouchZoom(zoom)
    if not E.job and not tImGui.IsAnyWindowHovered() then
        E.project.camera.distance=math.max(0.01,E.project.camera.distance*(zoom>0 and 0.9 or 1.1)); change()
    end
end
function onEndScene()
    C.release(E)
    if E.checker then E.checker:release(); E.checker=nil end
    if E.checkerPath then os.remove(E.checkerPath); E.checkerPath=nil end
    if E.originalLight then C.restoreLight(E.originalLight) end
end
-- Narrow automation surface for integration tests, using the production pipeline.
MeshToSpriteEditor=E
