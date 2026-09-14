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

-- Editor-owned serializable state. No engine objects are stored in a project.
local M = {}
function M.copy(value)
    if type(value)~='table' then return value end
    local out={}
    for k,v in pairs(value) do out[k]=M.copy(v) end
    return out
end
function M.new()
    return {version=1,images={},frames={{parts={}}},clips={},nextId=1,
        options={copyImages=true,showSource=true,preserveHoles=true,splitRegions=true,
            autoKey=false,onion=false}}
end
function M.part(project,frame,id)
    if not project.frames[frame] then return nil end
    for i,p in ipairs(project.frames[frame].parts) do if p.id==id then return p,i end end
end
function M.addPart(project,frame,image,rings,vertices,indices,recipe)
    local p={id=project.nextId,name='Part '..project.nextId,image=image,parent=0,
        rings=M.copy(rings),vertices=M.copy(vertices),indices=M.copy(indices),recipe=M.copy(recipe),
        x=0,y=0,z=0,angle=0,sx=1,sy=1,pivot={x=0,y=0,z=0}}
    project.nextId=project.nextId+1
    local minx,miny,maxx,maxy=math.huge,math.huge,-math.huge,-math.huge
    for _,v in ipairs(vertices) do
        minx,miny=math.min(minx,v.x),math.min(miny,v.y)
        maxx,maxy=math.max(maxx,v.x),math.max(maxy,v.y)
    end
    p.origin={x=(minx+maxx)/2,y=(miny+maxy)/2}
    table.insert(project.frames[frame].parts,p)
    return p
end
function M.rotatePart(p,angle)
    -- The stored position is the geometry center. Rotate that center around
    -- the chosen world-space pivot as well as rotating the geometry itself.
    local delta=(angle-p.angle)*math.pi/180
    local c,s=math.cos(delta),math.sin(delta)
    local x,y=p.x-p.pivot.x,p.y-p.pivot.y
    p.x=p.pivot.x+x*c-y*s
    p.y=p.pivot.y+x*s+y*c
    p.angle=angle
end
function M.vertex(p,v,image)
    local a=p.angle*math.pi/180
    local x,y=(v.x-p.origin.x)*p.sx,(v.y-p.origin.y)*p.sy
    if not p.imported then y=-y end
    return {x=p.x+x*math.cos(a)-y*math.sin(a),y=p.y+x*math.sin(a)+y*math.cos(a),
        z=(v.z or 0)+p.z,u=p.imported and v.u or v.x/image.width,v=p.imported and v.v or v.y/image.height}
end
function M.reparent(project,frame,id,parent)
    local p=assert(M.part(project,frame,id),'missing_part')
    local cursor=parent
    local seen={}
    while cursor~=0 do
        assert(cursor~=id and not seen[cursor],'parent_cycle')
        seen[cursor]=true
        cursor=assert(M.part(project,frame,cursor),'missing_parent').parent
    end
    -- Rest geometry/pivots are in asset coordinates. Only animated transforms
    -- inherit the hierarchy, so changing this link leaves the rest pose intact.
    p.parent=parent
end
function M.duplicate(project,frame,id,animation)
    local source,index=M.part(project,frame,id)
    assert(source,'missing_part')
    local p=M.copy(source)
    p.id=project.nextId; project.nextId=project.nextId+1
    p.name=p.name..' (copy)'
    table.insert(project.frames[frame].parts,index+1,p)
    if animation then
        for _,clip in ipairs(project.clips) do
            local copies={}
            for _,track in ipairs(clip.tracks) do
                if track.part==id then
                    local t=M.copy(track); t.part=p.id; copies[#copies+1]=t
                end
            end
            for _,t in ipairs(copies) do clip.tracks[#clip.tracks+1]=t end
        end
    end
    return p
end
function M.remove(project,frame,id,hierarchy)
    local p=assert(M.part(project,frame,id),'missing_part')
    local removed={[id]=true}
    local parts=project.frames[frame].parts
    if hierarchy then
        local changed=true
        while changed do
            changed=false
            for _,child in ipairs(parts) do
                if removed[child.parent] and not removed[child.id] then removed[child.id]=true; changed=true end
            end
        end
    else
        for _,child in ipairs(parts) do if child.parent==id then child.parent=p.parent end end
    end
    for i=#parts,1,-1 do if removed[parts[i].id] then table.remove(parts,i) end end
    for _,clip in ipairs(project.clips) do
        for i=#clip.tracks,1,-1 do if removed[clip.tracks[i].part] then table.remove(clip.tracks,i) end end
    end
end
function M.removeFrame(project,index)
    assert(#project.frames>1,'cannot_remove_last_frame')
    local frame=assert(project.frames[index],'missing_frame')
    local removed={}
    for _,part in ipairs(frame.parts) do removed[part.id]=true end
    table.remove(project.frames,index)
    for _,clip in ipairs(project.clips) do
        for i=#clip.tracks,1,-1 do
            if removed[clip.tracks[i].part] then table.remove(clip.tracks,i) end
        end
    end
    local animations=project.frameAnimations or {}
    for i=#animations,1,-1 do
        local anim=animations[i]
        local first,last=anim[2],anim[3]
        if anim[1]:match('^__frame_') or (first==index and last==index) then
            table.remove(animations,i)
        else
            local lo,hi=math.min(first,last),math.max(first,last)
            if lo>index then lo=lo-1 end
            if hi>=index then hi=hi-1 end
            if first<=last then anim[2],anim[3]=lo,hi
            else anim[2],anim[3]=hi,lo end
        end
    end
end
function M.key(project,clipIndex,id,time,pose)
    local clip=assert(project.clips[clipIndex],'missing_clip')
    local track
    for _,t in ipairs(clip.tracks) do if t.part==id then track=t; break end end
    if not track then track={part=id,mask=7,keys={}}; clip.tracks[#clip.tracks+1]=track end
    local key=M.copy(pose); key.time=time
    for i,k in ipairs(track.keys) do
        if math.abs(k.time-time)<1e-6 then track.keys[i]=key; return end
    end
    track.keys[#track.keys+1]=key
    table.sort(track.keys,function(a,b) return a.time<b.time end)
end
function M.history(project) return {past={},future={},current=M.copy(project)} end
function M.commit(history,project)
    history.past[#history.past+1]=history.current
    if #history.past>80 then table.remove(history.past,1) end
    history.current=M.copy(project); history.future={}
end
function M.undo(history)
    if #history.past==0 then return nil end
    history.future[#history.future+1]=history.current
    history.current=table.remove(history.past)
    return M.copy(history.current)
end
function M.redo(history)
    if #history.future==0 then return nil end
    history.past[#history.past+1]=history.current
    history.current=table.remove(history.future)
    return M.copy(history.current)
end
local function encode(value)
    local kind=type(value)
    if kind=='string' then return string.format('%q',value) end
    if kind=='number' then
        assert(value==value and math.abs(value)<math.huge,'invalid_number')
        return tostring(value)
    end
    if kind=='boolean' then return tostring(value) end
    assert(kind=='table','invalid_project_value')
    local keys={}
    for k in pairs(value) do keys[#keys+1]=k end
    table.sort(keys,function(a,b) return tostring(a)<tostring(b) end)
    local out={'{'}
    for _,k in ipairs(keys) do out[#out+1]='['..encode(k)..']='..encode(value[k])..',' end
    out[#out+1]='}'
    return table.concat(out)
end
function M.encode(project) return 'return '..encode(project)..'\n' end
function M.validate(project)
    assert(type(project)=='table' and project.version==1,'unsupported_project_version')
    assert(type(project.frames)=='table' and #project.frames>0,'missing_frames')
    assert(type(project.images)=='table' and type(project.clips)=='table','invalid_project')
    local ids,maxId={},0
    for f,frame in ipairs(project.frames) do
        assert(type(frame.parts)=='table','invalid_frame')
        for _,p in ipairs(frame.parts) do
            assert(math.type(p.id)=='integer' and p.id>0 and not ids[p.id],'invalid_part_id')
            ids[p.id]=true; maxId=math.max(maxId,p.id)
            assert(project.images[p.image],'missing_image')
            assert(#p.vertices>=3 and #p.indices>=3 and #p.indices%3==0,'invalid_geometry')
            for _,index in ipairs(p.indices) do
                assert(math.type(index)=='integer' and index>=1 and index<=#p.vertices,'invalid_index')
            end
            M.reparent(project,f,p.id,p.parent)
        end
    end
    for _,clip in ipairs(project.clips) do
        assert(clip.duration>0 and type(clip.name)=='string','invalid_clip')
        for _,track in ipairs(clip.tracks) do assert(ids[track.part],'missing_track_part') end
    end
    project.nextId=math.max(project.nextId or 1,maxId+1)
    return project
end
return M
