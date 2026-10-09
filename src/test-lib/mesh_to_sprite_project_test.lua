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

package.path='src/test-lib/?.lua;editor/?.lua;'..package.path
local P=require 'mesh_to_sprite_project'
local function near(a,b) assert(math.abs(a-b)<0.00001,tostring(a)..' != '..tostring(b)) end
for _,case in ipairs({
    {'bite','bite'}, {'','animation'}, {string.rep('a',31),string.rep('a',31)},
    {string.rep('a',50),string.rep('a',31)},
    {string.rep('a',30)..'ção',string.rep('a',30)},
    {string.rep('a',29)..'ção',string.rep('a',29)..'ç'},
    {string.rep('歩',12),string.rep('歩',10)},
    {string.rep('😀',9),string.rep('😀',7)},
}) do
    local name=P.suggestAnimationName(case[1])
    assert(name==case[2] and utf8.len(name))
    local config=P.defaults();config.name=name;P.validate(config)
end
local p=P.defaults(); p.count=4; p.start=2; p.stop=6
near(P.sample(p,1),2); near(P.sample(p,4),5)
p.cycle=false; near(P.sample(p,4),6); p.count=1; near(P.sample(p,1),2)
local motion=P.defaults();motion.animateTransform=true;motion.start=2;motion.stop=6;motion.count=4
motion.position={x=-3,y=2,z=4}
motion.finalTransform.position={x=9,y=-4,z=10}
motion.finalTransform.rotation={x=-180,y=180,z=720}
motion.finalTransform.scale={x=2,y=0.5,z=3}
for _,cycle in ipairs({false,true}) do
    motion.cycle=cycle
    for i=1,4 do near(P.transformProgress(motion,P.sample(motion,i)),(i-1)/3) end
    local t=P.transformProgress(motion,(motion.start+P.sample(motion,4))/2)
    local x,y,z=P.transformComponents(motion,'position',t);near(x,3);near(y,-1);near(z,7)
    x,y,z=P.transformComponents(motion,'rotation',t);near(x,-90);near(y,90);near(z,360)
    x,y,z=P.transformComponents(motion,'scale',t);near(x,1.5);near(y,0.75);near(z,2)
end
near(P.transformProgress(motion,0),0);near(P.transformProgress(motion,99),1)
motion.count=1;near(P.transformProgress(motion,6),0)
motion.count=4;motion.stop=motion.start;near(P.transformProgress(motion,6),0)
motion.animateTransform=false;near(P.transformProgress(motion,6),0)
local sizing=P.defaults()
sizing.width=320;sizing.height=160
local fw,fh=P.frameSize(sizing);assert(fw==320 and fh==160)
sizing.followImage=false
P.setFrameDimension(sizing,'frameWidth',80);assert(sizing.frameHeight==40)
P.setFrameDimension(sizing,'frameHeight',90);assert(sizing.frameWidth==180)
sizing.keepAspect=false
P.setFrameDimension(sizing,'frameWidth',100);assert(sizing.frameHeight==90)
local fit=P.fitSize(200,100,300,400);assert(fit.x==300 and fit.y==150)
local expected={{2,2,2,2,2,2,2},{2,3,4,4,4,4,4},{2,3,4,2,3,4,2},
    {4,3,2,2,2,2,2},{4,3,2,4,3,2,4},{2,3,4,3,2,2,2},{2,3,4,3,2,3,4}}
for mode=0,6 do for step=0,6 do
    assert(P.staticFrame({first=2,last=4,interval=0.25,mode=mode},step*0.25)==expected[mode+1][step+1])
end end
local tmp=require('test_temp_path').new(); P.save(p,tmp)
local q=P.load(tmp); assert(q.count==1 and q.start==2 and q.camera.distance==p.camera.distance)
local legacyFile=assert(io.open(tmp,'r'));local legacy=legacyFile:read('*a');legacyFile:close()
legacy=legacy:gsub('%["version"%]=3,','["version"]=2,')
legacy=legacy:gsub('%["animateTransform"%]=[^,]+,',''):gsub('%["finalTransform"%]=%b{},','')
local v2=assert(io.open(tmp,'w'));v2:write(legacy);v2:close()
local fromV2=P.load(tmp)
assert(fromV2.version==3 and not fromV2.animateTransform and fromV2.finalTransform.scale.x==p.scale.x)
legacy=legacy:gsub('%["version"%]=2,','["version"]=1,')
for _,key in ipairs({'frameWidth','frameHeight','followImage','keepAspect','showPivot'}) do
    legacy=legacy:gsub('%["'..key..'"%]=[^,]+,','')
end
local lf=assert(io.open(tmp,'w'));lf:write(legacy);lf:close()
local migrated=P.load(tmp)
assert(migrated.version==3 and migrated.followImage and migrated.keepAspect and not migrated.showPivot)
assert(migrated.frameWidth==migrated.width and migrated.frameHeight==migrated.height)
os.remove(tmp)
local function rejects(change)
    local bad=P.copy(p); change(bad); assert(not pcall(P.validate,bad))
end
rejects(function(v) v.count=0 end); rejects(function(v) v.camera.distance=0/0 end)
rejects(function(v) v.width=4096;v.height=4096;v.count=4096 end)
rejects(function(v) v.version=4 end); rejects(function(v) v.unknown=true end)
local f=assert(io.open(tmp,'w'));f:write('while true do end');f:close()
assert(not pcall(P.load,tmp));os.remove(tmp)
local Pixels=require 'mesh_to_sprite_pixels'
local black=string.char(100,50,25,255,0,0,0,255)
local white=string.char(227,177,152,255,255,255,255,255)
local rgba=Pixels.compose(black,white,2,1,{r=0,g=0,b=0,a=0})
local r,g,b,a=rgba:byte(1,4)
assert(math.abs(r-199)<=1 and math.abs(g-100)<=1 and math.abs(b-50)<=1 and a==128)
assert(rgba:sub(5)==string.char(0,0,0,0))
print('MESH TO SPRITE PROJECT TEST OK')
