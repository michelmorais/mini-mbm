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
local Scml=require 'articulated_sprite_scml'
local Parser=require 'spriter_scml_wrapper'
local Model=require 'articulated_sprite_model'
local function near(a,b) assert(math.abs(a-b)<0.0001,tostring(a)..' ~= '..tostring(b)) end
local path=os.tmpname()
local image=path..'.png'
local f=assert(io.open(image,'wb')); f:write('image fixture: pure importer checks readability only'); f:close()
f=assert(io.open(path,'wb'))
f:write(([=[<spriter_data>
<folder id="3"><file id="8" name="%s" width="20" height="40" pivot_x="0.5" pivot_y="0.5"/>
<file id="9" name="%s" width="20" height="40" pivot_x="0.5" pivot_y="0.5"/></folder>
<entity id="0" name="Hero"><animation id="0" name="Walk" length="1000" looping="false">
<mainline>
<key id="4"><bone_ref id="7" timeline="9" key="2"/><object_ref id="3" timeline="5" key="6" parent="7" z_index="0"/></key>
<key id="8" time="500"><bone_ref id="7" timeline="9" key="2"/><object_ref id="3" timeline="5" key="7" parent="7" z_index="1"/></key>
<key id="9" time="750"><bone_ref id="7" timeline="9" key="2"/></key>
</mainline>
<timeline id="9" name="Bone" object_type="bone">
<key id="2"><bone x="20" y="30" angle="0"/></key>
<key id="4" time="1000"><bone x="20" y="30" angle="90"/></key></timeline>
<timeline id="5" name="Hand">
<key id="6" spin="0"><object folder="3" file="8" x="10" angle="20"/></key>
<key id="7" time="500"><object folder="3" file="9" x="10" angle="20"/></key></timeline>
</animation></entity></spriter_data>]=]):format(image,image)); f:close()
local data=assert(Parser.parse(path)); local anim=data.entities[1].animations[1]
assert(anim.looping==false and anim.mainline[1].curve)
local pose=Scml.evaluate(data,anim,250)[1]
near(pose.x,20+10*math.cos(math.pi/8)); near(pose.y,30+10*math.sin(math.pi/8)); near(pose.angle,42.5)
assert(#Scml.evaluate(data,anim,800)==0)
near(Scml.curve({curve_type='quadratic',c1='0'},0.5),0.25)
near(Scml.curve({curve_type='cubic',c1='0',c2='1'},0.5),0.5)
near(Scml.curve({curve_type='bezier',c1='.25',c2='.25',c3='.75',c4='.75'},0.3),0.3)
anim.mainline[1].curve.curve_type='instant'
near(Scml.evaluate(data,anim,250)[1].angle,20)
anim.mainline[1].curve.curve_type='linear'
anim.timelines[9].keys[1].attrs.scale_x='-1'
anim.timelines[9].keys[2].attrs.scale_x='-1'
pose=Scml.evaluate(data,anim,250)[1]
near(pose.x,20-10*math.cos(math.pi/8)); near(pose.angle,2.5)
local project=Scml.import(path)
assert(#project.frames==1 and #project.clips==1 and #project.images==1)
assert(#project.frames[1].parts==2 and project.clips[1].loop==false)
local clip=project.clips[1]
local function keyAt(track,time)
    for _,k in ipairs(track.keys) do if math.abs(k.time-time)<0.000001 then return k end end
    error('Missing key: '..time)
end
near(keyAt(clip.tracks[1],0.25).angle,42.5)
near(keyAt(clip.tracks[1],0.5).sx,0)
near(keyAt(clip.tracks[2],0.5).sx,1)
near(keyAt(clip.tracks[2],0.75).sx,0)
for _,track in ipairs(clip.tracks) do
    for i=2,#track.keys do assert(track.keys[i].time-track.keys[i-1].time>0.00001) end
end
-- The same renderer matrix as SPT: translate from pivot, scale, rotate, translate back.
local p=project.frames[1].parts[1]; local k=keyAt(clip.tracks[1],0.25)
local vertex=Model.vertex(p,p.vertices[1],project.images[p.image])
local x,y=(vertex.x-p.pivot.x)*k.sx,(vertex.y-p.pivot.y)*k.sy
local a=k.angle*math.pi/180
near(p.pivot.x+k.x+x*math.cos(a)-y*math.sin(a),20+10*math.cos(math.pi/8)-10*math.cos(a)-20*math.sin(a))
-- Removing a preceding frame must keep the clip association valid.
table.insert(project.frames,1,{parts={}}); clip.frame=2
Model.removeFrame(project,1); assert(clip.frame==1)
os.remove(image)
local ok,err=pcall(Scml.import,path); assert(not ok and tostring(err):find('Missing SCML image',1,true))
os.remove(path)
print('SCML PARSER / CURVES / HIERARCHY / VISIBILITY / KEY SPACING OK')
-- Optional user fixture: it is deliberately not copied into the repository.
local fixture=arg[1]
if fixture then
    local project=Scml.import(fixture)
    assert(#project.frames==8 and #project.clips==8 and #project.images==27)
    assert(project.clips[1].name=='Walking' and project.clips[8].name=='Die' and not project.clips[8].loop)
    local static=Parser.import(fixture)
    assert(static.ok and #static.frames==62,'Legacy Sprite Maker import changed')
    print('FW HERO: 8 ARTICULATED CLIPS; LEGACY 62 STATIC FRAMES UNCHANGED')
end
