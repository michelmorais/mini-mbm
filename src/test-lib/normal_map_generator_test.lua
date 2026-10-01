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
local Height=require 'height_map_source'
local Generator=require 'normal_map_generator'
local Preview=require 'normal_map_preview'
local Project=require 'normal_map_project'
local function image(w,h,fn)
    local pixels={}
    for y=1,h do for x=1,w do pixels[#pixels+1]=string.char(fn(x,y)) end end
    return Height.image(table.concat(pixels),w,h)
end
local function pixel(result,x,y) return result.bytes:byte(((y-1)*result.width+x-1)*4+1,((y-1)*result.width+x)*4) end
local flat=image(9,7,function() return 90,90,90,255 end)
for _,edge in ipairs{'clamp','repeat'} do
    local result=Generator.generate(flat,{edge=edge,blur=4})
    for y=1,7 do for x=1,9 do
        local r,g,b,a=pixel(result,x,y);assert(r==128 and g==128 and b==255 and a==255)
    end end
end
local ramp=image(33,33,function(x,y) return (x-1)*7,(y-1)*7,0,255 end)
local xmap=Generator.generate(ramp,{channel='r'})
local r,g,b=pixel(xmap,17,17);assert(r<128 and g==128 and b>=254,'X direction')
local positive=Generator.generate(ramp,{channel='g'})
local negative=Generator.generate(ramp,{channel='g',convention='-Y'})
local _,gp=pixel(positive,17,17);local _,gn=pixel(negative,17,17)
assert(gp>128 and gn<128 and gp+gn==255,'Y convention')
local inverted=Generator.generate(ramp,{channel='g',invert=true})
assert(select(2,pixel(inverted,17,17))==gn,'Height inversion')
local zero=Generator.generate(ramp,{strength=0})
assert(pixel(zero,17,17)==128 and select(2,pixel(zero,17,17))==128,'Zero strength')
-- Hidden RGB must not introduce relief beside a constant opaque region, even after blur.
local cutout=image(17,17,function(x,y)
    if x<4 or y<4 then return 255,0,200,0 end
    return 80,80,80,255
end)
for _,blur in ipairs{0,2,8} do
    local result=Generator.generate(cutout,{blur=blur})
    for y=1,17 do for x=1,17 do
        local nr,ng,nb=pixel(result,x,y);assert(nr==128 and ng==128 and nb==255,'Transparent boundary')
    end end
end
-- Repeat edges sample the opposite side; clamp does not.
local repeatMap=Generator.generate(ramp,{channel='r',edge='repeat'})
assert(pixel(repeatMap,1,17)>128 and pixel(xmap,1,17)<128,'Repeat boundary')
local one=Generator.generate(image(1,1,function() return 50,60,70,255 end),{blur=32})
assert(one.bytes==string.char(128,128,255,255))
local function finish(job)
    local steps=0
    while job.state=='running' do job:step(.0001);steps=steps+1 end
    assert(job.state=='completed',job.error);return job.result,steps
end
local settings={channel='r'}
local first=Generator.start(ramp,settings);settings.channel='g'
local second=Generator.start(flat,{})
local result,steps=finish(first);assert(result.bytes==xmap.bytes and steps>1,'Snapshot/cooperative processing')
assert(finish(second).width==9,'Independent jobs')
local cancel=Generator.start(ramp,{});cancel:step(0);cancel:cancel();cancel:step()
assert(cancel.state=='cancelled' and not cancel.result and not cancel.thread)
local failure=Generator.job(function() error('expected failure') end);failure:step();assert(failure.state=='failed')
local lp,ln=Preview.light(positive,70,40,true),Preview.light(negative,70,40,true)
for i=1,#lp do assert(math.abs(lp:byte(i)-ln:byte(i))<=1,'Convention in preview') end
local temporary=(os.getenv('TEMP') or os.getenv('TMPDIR') or '/tmp'):gsub('\\','/')
local project=temporary..'/normal-generator-test.normalmap'
local source=temporary..'/a b "file".png'
Project.save(project,source,{blur=3,channel='a',invert=true})
local path,options=Project.load(project)
assert(path==source and options.blur==3 and options.channel=='a' and options.invert)
local f=assert(io.open(project,'wb'));f:write('normal-map-project 1\nsource=61\nstrength=nan\n');f:close()
assert(not pcall(Project.load,project),'Malformed project accepted')
os.remove(project)
assert(not pcall(Project.save,project,'relative.png',{}),'Ambiguous source path accepted')
assert(not pcall(Height.settings,{white=0}))
assert(not pcall(Height.settings,{strength=0/0}))
assert(not pcall(Height.image,'bad',1,1))
assert(not pcall(Height.image,string.rep('a',16385*4),16385,1),'Unbounded row accepted')
for _,name in ipairs{'normal_map_editor','normal_map_panel','normal_map_project','normal_map_preview','normal_map_generator','height_map_source'} do
    assert(loadfile('editor/'..name..'.lua'))
end
print('NORMAL MAP GENERATOR TESTS PASS')
