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
local O=require 'articulated_sprite_timeline_model'
local M=require 'articulated_sprite_model'
local function fixture()
    return {duration=2,tracks={
        {part=1,mask=7,keys={{time=0,angle=10},{time=0.5,angle=70,easing=5,bezier={0.2,0.1,0.8,0.9},euler={0,0,430}},{time=2,angle=0}}},
        {part=2,mask=7,keys={{time=0,sx=0,sy=0},{time=0.5,sx=2,sy=2},{time=2,sx=0,sy=0}}}}}
end
local function near(a,b) assert(math.abs(a-b)<0.000001,tostring(a)..' ~= '..tostring(b)) end
local function failure(clip,fn,reason)
    local before=M.encode(clip)
    local ok,err=pcall(fn)
    assert(not ok and tostring(err):find(reason,1,true),tostring(err))
    assert(M.encode(clip)==before,'failed operation mutated keys')
end
local parts={[1]=true,[2]=true}
local c=fixture(); local selection={[c.tracks[1].keys[1]]=true,[c.tracks[1].keys[2]]=true,[c.tracks[2].keys[2]]=true}
local copy=O.selection(c,selection)
assert(#copy.items==3 and copy.first==0 and copy.last==0.5)
O.paste(c,copy,1,parts)
assert(#c.tracks[1].keys==5 and #c.tracks[2].keys==4)
near(c.tracks[1].keys[4].time,1.5); assert(c.tracks[1].keys[4].euler[3]==430)
assert(c.tracks[1].keys[4].bezier[1]==0.2 and c.tracks[1].keys[4].easing==5)
failure(c,function() O.paste(c,copy,1,parts) end,'tl_collision')
failure(c,function() O.paste(c,copy,1.75,parts) end,'tl_out_of_range')
failure(c,function() O.paste(c,copy,0.75,{[1]=true}) end,'tl_missing_part')
-- Relative spacing is preserved by group dragging, and collisions reject atomically.
c=fixture(); selection={[c.tracks[1].keys[1]]=true,[c.tracks[2].keys[1]]=true}
failure(c,function() O.move(c,selection,0.5) end,'tl_collision')
O.move(c,selection,0.25); near(c.tracks[1].keys[1].time,0.25); near(c.tracks[2].keys[1].time,0.25)
O.insertTime(c,0.5,0.2); near(c.duration,2.2); near(c.tracks[1].keys[2].time,0.7)
local count,stop=O.removalImpact(c,0.6,0.4); assert(count==2); near(stop,1)
O.removeTime(c,0.6,0.4); near(c.duration,1.8); assert(#c.tracks[1].keys==2)
near(c.tracks[1].keys[2].time,1.8)
-- Inserting a copied interval shifts every track; endpoint padding avoids collisions.
c=fixture(); O.paste(c,copy,0,parts,true)
assert(c.duration>2.5 and #c.tracks[1].keys==5)
near(c.tracks[1].keys[1].angle,10); near(c.tracks[1].keys[2].euler[3],430)
assert(c.tracks[1].keys[3].time>0.5+O.epsilon)
failure(c,function() O.removeTime(c,0,10) end,'tl_invalid_time')
selection={}; for _,t in ipairs(c.tracks) do for _,key in ipairs(t.keys) do selection[key]=true end end
O.delete(c,selection); assert(#c.tracks[1].keys==0 and #c.tracks[2].keys==0)
-- Clipboard payload is independent of later source edits.
assert(copy.items[2].key.euler[3]==430)
print('TIMELINE COPY / PASTE / MOVE / INSERT / REMOVE / COLLISIONS OK')
