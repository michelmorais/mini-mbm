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

package.path='editor/?.lua;'..package.path
local G=require 'articulated_sprite_geometry'
local M=require 'articulated_sprite_model'
local function area(vertices,index)
    local total=0
    for i=1,#index,3 do total=total+math.abs(G.area({vertices[index[i]],vertices[index[i+1]],vertices[index[i+2]]})) end
    return total
end
local function near(a,b) assert(math.abs(a-b)<1e-4,tostring(a)..' ~= '..tostring(b)) end
-- Adjacent float32 Y levels have no representable midpoint. The old
-- scanline test rejected this valid polygon with open_contour ... ... 1.
do
    local lo,hi=268.590087890625,268.590118408203125
    local ring={{x=0,y=250},{x=4,y=250},{x=6,y=lo},
        {x=10,y=hi},{x=4,y=280},{x=0,y=280}}
    local vertices,indices=G.triangulate({ring},2)
    near(area(vertices,indices),math.abs(G.area(ring)))
    for i=1,32 do
        local rings=G.form('circle',{x=225,y=220,w=96,h=96},32)
        local v=rings[1][i]
        v.x=273+(v.x-273)*0.6; v.y=268+(v.y-268)*0.6
        local vertices,indices=G.triangulate(rings,2)
        assert(math.abs(area(vertices,indices)-math.abs(G.area(rings[1])))<0.1)
    end
end
local rect={x=0,y=0,w=10,h=10}
local rings=G.form('rectangle',rect,2)
local v,ix=G.triangulate(rings,2)
assert(#ix==6); near(area(v,ix),100)
local hole={{x=2,y=2},{x=8,y=2},{x=8,y=8},{x=2,y=8}}
v,ix=G.triangulate({rings[1],hole},30)
near(area(v,ix),64)
for i=1,#ix,3 do
    local a,b,c=v[ix[i]],v[ix[i+1]],v[ix[i+2]]
    assert(not G.contains(hole,(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3))
end
for _,kind in ipairs({'circle','capsule','ring'}) do
    local shape=G.form(kind,rect,12,{inner=0.3,dx=1,dy=0.5})
    local a,b=G.triangulate(shape,12)
    assert(#b>0 and area(a,b)>0)
end
local alpha=string.char(255,255,255,255,0,255,255,255,255)
local contours=G.alphaContours(alpha,3,3,{x=0,y=0,w=3,h=3},nil,0,true)
assert(#contours==2 and #G.components(contours)==1)
v,ix=G.triangulate(contours,2); near(area(v,ix),8)
contours=G.alphaContours(alpha,3,3,{x=0,y=0,w=3,h=3},nil,0,false)
v,ix=G.triangulate(contours,2); near(area(v,ix),9)
contours=G.alphaContours(string.char(255,0,0,255),2,2,{x=0,y=0,w=2,h=2},nil,0,true)
assert(#G.components(contours)==2)
v,ix=G.triangulate(contours,2); near(area(v,ix),2)
assert(not pcall(G.triangulate,{{{x=0,y=0},{x=2,y=2},{x=0,y=2},{x=2,y=0}}},2))
local project=M.new(); project.images={{path='fixture.png',width=10,height=10}}
v,ix=G.triangulate(rings,2)
local p=M.addPart(project,1,1,rings,v,ix,{kind='rectangle'})
p.x=12; p.pivot.x=15
-- An endpoint pivot stays fixed through setup rotation, including after moving
-- the pivot on an already rotated part. Moving only the pivot leaves geometry intact.
do
    for _,imported in ipairs({false,true}) do
        local part=M.copy(p)
        part.imported=imported
        part.sx=1.5; part.sy=0.75
        local image=project.images[1]
        local endpoint=part.vertices[1]
        local before=M.vertex(part,endpoint,image)
        part.pivot.x,part.pivot.y=before.x,before.y
        M.rotatePart(part,90)
        local after=M.vertex(part,endpoint,image)
        near(after.x,before.x); near(after.y,before.y)
        local other=part.vertices[2]
        local otherBefore=M.vertex(part,other,image)
        part.pivot.x,part.pivot.y=otherBefore.x,otherBefore.y
        local unchanged=M.vertex(part,other,image)
        near(unchanged.x,otherBefore.x); near(unchanged.y,otherBefore.y)
        M.rotatePart(part,-45)
        local otherAfter=M.vertex(part,other,image)
        near(otherAfter.x,otherBefore.x); near(otherAfter.y,otherBefore.y)
        near(part.pivot.x,otherBefore.x); near(part.pivot.y,otherBefore.y)
    end
end
local child=M.duplicate(project,1,p.id,false)
M.reparent(project,1,child.id,p.id)
assert(child.x==12 and child.pivot.x==15)
assert(not pcall(M.reparent,project,1,p.id,child.id))
project.clips={{name='walk',duration=1,tracks={}}}
M.key(project,1,p.id,0,{x=1}); M.key(project,1,p.id,0,{x=2})
assert(#project.clips[1].tracks[1].keys==1)
local animated=M.duplicate(project,1,p.id,true)
assert(project.clips[1].tracks[2].part==animated.id)
M.remove(project,1,p.id,false)
assert(child.parent==0 and child.x==12 and child.pivot.x==15)
assert(#project.clips[1].tracks==1)
local history=M.history(project)
M.remove(project,1,child.id,true); M.commit(history,project)
project=M.undo(history); assert(M.part(project,1,child.id))
project=M.redo(history); assert(not M.part(project,1,child.id))
local restored=assert(load(M.encode(project),'project','t',{}))()
M.validate(restored)
assert(restored.frames[1].parts[1].pivot.x==15)
math.randomseed(123)
for sample=1,100 do
    local bytes,total={},0
    for i=1,36 do
        local opaque=math.random(0,1)==1
        bytes[i]=string.char(opaque and 255 or 0)
        if opaque then total=total+1 end
    end
    if total>0 then
        local r=G.alphaContours(table.concat(bytes),6,6,{x=0,y=0,w=6,h=6},nil,0,true)
        local vertices,index=G.triangulate(r,2)
        near(area(vertices,index),total)
    end
end
print('ARTICULATED SPRITE MODEL/GEOMETRY OK')

-- Frame deletion preserves unrelated tracks, remaps frame ranges and is undoable.
do
    local project=M.new()
    project.frames={{parts={{id=1}}},{parts={{id=2}}},{parts={{id=3}}}}
    project.clips={{tracks={{part=1},{part=2},{part=3}}}}
    project.frameAnimations={{'all',1,3},{'deleted',2,2},{'after',3,3},{'reverse',3,1},{'__frame_2',2,2}}
    local history=M.history(project)
    M.removeFrame(project,2); M.commit(history,project)
    assert(#project.frames==2 and project.frames[2].parts[1].id==3)
    assert(#project.clips[1].tracks==2 and project.clips[1].tracks[2].part==3)
    assert(#project.frameAnimations==3)
    assert(project.frameAnimations[1][3]==2)
    assert(project.frameAnimations[2][2]==2 and project.frameAnimations[2][3]==2)
    assert(project.frameAnimations[3][2]==2 and project.frameAnimations[3][3]==1)
    project=M.undo(history)
    assert(#project.frames==3 and #project.clips[1].tracks==3 and #project.frameAnimations==5)
    project=M.redo(history)
    M.removeFrame(project,2)
    assert(#project.frames==1)
    assert(not pcall(M.removeFrame,project,1))
end
print('ARTICULATED SPRITE FRAME DELETE OK')
