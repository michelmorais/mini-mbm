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

local Model=require 'articulated_sprite_model'
local M={}
function M.directory(path) return path:gsub('\\','/'):match('^(.*)/[^/]*$') or '.' end
local function absolute(path) return path:sub(1,1)=='/' or path:match('^%a:') end
local function read(path)
    local f=assert(io.open(path,'rb')); local bytes=assert(f:read('a')); assert(f:close()); return bytes
end
local function write(path,bytes)
    local f=assert(io.open(path,'wb')); local ok,err=f:write(bytes)
    local closed,closeError=f:close(); assert(ok,err); assert(closed,closeError)
end
function M.save(project,path)
    local out=Model.copy(project)
    local dir=M.directory(path)
    if out.options.copyImages then
        local name=path:gsub('\\','/'):match('([^/]+)$')..'.assets'
        assert(mbm.createDirectories(dir..'/'..name))
        local reserved,used={},{}
        for _,img in ipairs(out.images) do
            local base=assert(img.path:gsub('\\','/'):match('([^/]+)$'),'invalid_image_path')
            reserved[base:lower()]=true
        end
        for _,img in ipairs(out.images) do
            local base=img.path:gsub('\\','/'):match('([^/]+)$')
            local stem,ext=base:match('^(.*)(%.[^.]*)$')
            if not stem or stem=='' then stem,ext=base,'' end
            local bytes=read(img.path)
            local candidate,suffix=base,1
            while true do
                local destination=dir..'/'..name..'/'..candidate
                local available=not used[candidate:lower()] and
                    (candidate==base or not reserved[candidate:lower()])
                if available then
                    local existing=io.open(destination,'rb')
                    local matches=false
                    if existing then
                        matches=existing:read('a')==bytes
                        assert(existing:close())
                        available=matches
                    end
                    if available then
                        -- Never overwrite another image (including resources from
                        -- an earlier save). Reuse an existing copy only if identical.
                        if not matches then write(destination,bytes) end
                        used[candidate:lower()]=true
                        img.path=name..'/'..candidate
                        break
                    end
                end
                suffix=suffix+1; candidate=stem..'_'..suffix..ext
            end
        end
    end
    local temp=path..'.writing'
    write(temp,Model.encode(out))
    -- POSIX rename replaces atomically; on Windows keep a recoverable backup.
    local backup=path..'.previous'
    local existing=io.open(path,'rb')
    if existing then existing:close(); os.remove(backup); assert(os.rename(path,backup)) end
    local ok,err=os.rename(temp,path)
    if not ok then if existing then os.rename(backup,path) end; error(err) end
    if existing then os.remove(backup) end
end
function M.load(path)
    local project=assert(loadfile(path,'t',{}))()
    Model.validate(project)
    for _,img in ipairs(project.images) do
        if not absolute(img.path) then img.path=M.directory(path)..'/'..img.path end
    end
    return project
end
function M.build(project)
    Model.validate(project)
    local d=meshDebug:new()
    d:setStride(3); d:enableNormal(false)
    for fi,frame in ipairs(project.frames) do
        assert(#frame.parts>0,'empty_frame')
        d:addFrame(3)
        for si,p in ipairs(frame.parts) do
            d:addSubSet(fi)
            local vertices={}
            for _,v in ipairs(p.vertices) do vertices[#vertices+1]=Model.vertex(p,v,project.images[p.image]) end
            assert(d:addVertex(fi,si,vertices),'add_vertices_failed')
        end
    end
    for fi,frame in ipairs(project.frames) do
        for si,p in ipairs(frame.parts) do
            assert(d:addIndex(fi,si,p.indices),'add_indices_failed')
            assert(d:setTexture(fi,si,project.images[p.image].path),'set_texture_failed')
        end
    end
    d:removeNormals(); d:setType('sprite')
    d:setModeDraw('TRIANGLES'); d:setModeCullFace('BACK'); d:setModeFrontFace('CW')
    local clipFrames={}
    for _,clip in ipairs(project.clips) do if clip.frame then clipFrames[clip.name]=clip.frame end end
    for _,anim in ipairs(project.frameAnimations or {}) do
        if not anim[1]:match('^__frame_') and not clipFrames[anim[1]] then d:addAnim(table.unpack(anim)) end
    end
    for _,clip in ipairs(project.clips) do
        if clip.frame then d:addAnim(clip.name,clip.frame,clip.frame,1,mbm.PAUSED) end
    end
    if project.physics and #project.physics>0 then d:setPhysics(project.physics) end
    for fi in ipairs(project.frames) do d:addAnim('__frame_'..fi,fi,fi,1,mbm.PAUSED) end
    for fi,frame in ipairs(project.frames) do
        for si,p in ipairs(frame.parts) do
            local q=p.pivotQ or {0,0,0,1}
            d:addArticulatedPart(p.id,fi,si,p.name,p.pivot.x,p.pivot.y,p.pivot.z or 0,
                q[1],q[2],q[3],q[4],0)
        end
    end
    -- Parents are assigned only after every part exists (ordering is independent).
    local partIndex=0
    for _,frame in ipairs(project.frames) do
        for _,p in ipairs(frame.parts) do
            partIndex=partIndex+1
            local q=p.pivotQ or {0,0,0,1}
            d:updateArticulatedPart(partIndex,p.name,p.pivot.x,p.pivot.y,p.pivot.z or 0,
                q[1],q[2],q[3],q[4],p.parent)
        end
    end
    for _,clip in ipairs(project.clips) do
        local ci=d:addArticulatedAnimation(clip.name,clip.duration,clip.speed or 1,
            clip.priority or 0,clip.loop~=false,clip.blend or 0)
        for _,track in ipairs(clip.tracks) do
            local ti=d:addArticulatedTrack(ci,track.part,track.mask or 7)
            for ki,k in ipairs(track.keys) do
                local q=k.q or {0,0,math.sin((k.angle or 0)*math.pi/360),math.cos((k.angle or 0)*math.pi/360)}
                d:addArticulatedKey(ci,ti,k.time,k.x or 0,k.y or 0,k.z or 0,
                    q[1],q[2],q[3],q[4],k.sx or 1,k.sy or 1,k.sz or 1)
                if k.euler then d:setArticulatedKeyEuler(ci,ti,k.time,table.unpack(k.euler))
                elseif not k.q then d:setArticulatedKeyEuler(ci,ti,k.time,0,0,k.angle or 0) end
                d:setArticulatedKeyEasing(ci,ti,ki,k.easing or 0)
                if k.bezier then d:setArticulatedKeyBezier(ci,ti,ki,table.unpack(k.bezier)) end
            end
        end
    end
    return d
end
function M.export(project,path)
    local d=M.build(project)
    assert(d:save(path,false,false),'save_sprite_failed')
    return d
end
function M.import(path)
    mbm.addPath(M.directory(path))
    local d=meshDebug:new(); assert(d:load(path),'load_sprite_failed')
    assert(d:getType()=='sprite','expected_sprite')
    local project=Model.new(); project.frames={}
    local info=meshDebug:getInfo(path)
    project.frameAnimations={}
    for i=1,(info and info.animation or 0) do project.frameAnimations[i]={d:getAnim(i)} end
    project.physics=d:getPhysics()
    local images={}
    local mode=d:getModeDraw()
    assert(mode=='TRIANGLES' or mode=='TRIANGLE_STRIP' or mode=='TRIANGLE_FAN','unsupported_sprite_primitive')
    local reverse=d:getModeFrontFace()=='CCW'
    for fi=1,d:getTotalFrame() do
        local frame={parts={}}; project.frames[fi]=frame
        for si=1,d:getTotalSubset(fi) do
            local pathTexture=d:getTexture(fi,si)
            local full=mbm.getFullPath(pathTexture) or pathTexture
            local image=images[full]
            if not image then
                local tex=assert(mbm.loadTexture(full),'missing_image')
                image=#project.images+1; images[full]=image
                project.images[image]={path=full,width=tex:getWidth(),height=tex:getHeight()}
            end
            local vertices=d:getVertex(fi,si,1,d:getTotalVertex(fi,si))
            local indices=d:getIndex(fi,si)
            if not indices or #indices==0 then indices={}; for i=1,#vertices do indices[i]=i end end
            if mode~='TRIANGLES' then
                local triangles={}
                for i=3,#indices do
                    local a=mode=='TRIANGLE_FAN' and indices[1] or indices[i-2]
                    local b,c=indices[i-1],indices[i]
                    if mode=='TRIANGLE_STRIP' and i%2==0 then a,b=b,a end
                    if a~=b and b~=c and a~=c then
                        for _,index in ipairs({a,b,c}) do triangles[#triangles+1]=index end
                    end
                end
                indices=triangles
            end
            if reverse then
                for i=1,#indices,3 do indices[i+1],indices[i+2]=indices[i+2],indices[i+1] end
            end
            local p=Model.addPart(project,fi,image,{},vertices,indices,{kind='imported'})
            p.imported=true; p.origin={x=0,y=0}
        end
    end
    -- Imported parts may use IDs colliding with provisional IDs. Assign unbound
    -- subsets only after reserving every original identity.
    local assigned,maxId={},0
    for i=1,d:getTotalArticulatedParts() do
        local id,fi,si,name,x,y,z,qx,qy,qz,qw,parent=d:getArticulatedPart(i)
        local p=project.frames[fi].parts[si]
        p.id=id; p.name=name; p.pivot={x=x,y=y,z=z}; p.pivotQ={qx,qy,qz,qw}; p.parent=parent
        assigned[p]=true; maxId=math.max(maxId,id)
    end
    for _,frame in ipairs(project.frames) do
        for _,p in ipairs(frame.parts) do
            if not assigned[p] then maxId=maxId+1; p.id=maxId end
        end
    end
    project.nextId=maxId+1
    for ci=1,d:getTotalArticulatedAnimations() do
        local name,duration,speed,priority,loop,blend=d:getArticulatedAnimation(ci)
        local clip={name=name,duration=duration,speed=speed,priority=priority,loop=loop,blend=blend,tracks={}}
        for _,anim in ipairs(project.frameAnimations) do
            if anim[1]==name and anim[2]==anim[3] then clip.frame=anim[2]; break end
        end
        project.clips[#project.clips+1]=clip
        for ti=1,d:getTotalArticulatedTracks(ci) do
            local id,mask,count=d:getArticulatedTrack(ci,ti)
            local track={part=id,mask=mask,keys={}}; clip.tracks[#clip.tracks+1]=track
            for ki=1,count do
                local time,x,y,z,qx,qy,qz,qw,sx,sy,sz,easing,bx1,by1,bx2,by2,ex,ey,ez,has=d:getArticulatedKey(ci,ti,ki)
                track.keys[#track.keys+1]={time=time,x=x,y=y,z=z,q={qx,qy,qz,qw},sx=sx,sy=sy,sz=sz,
                    easing=easing,bezier={bx1,by1,bx2,by2},euler=has and {ex,ey,ez} or nil}
            end
        end
    end
    return Model.validate(project)
end
return M
