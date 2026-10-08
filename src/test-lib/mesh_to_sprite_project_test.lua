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
local expected={{2,2,2,2,2,2,2},{2,3,4,4,4,4,4},{2,3,4,2,3,4,2},
    {4,3,2,2,2,2,2},{4,3,2,4,3,2,4},{2,3,4,3,2,2,2},{2,3,4,3,2,3,4}}
for mode=0,6 do for step=0,6 do
    assert(P.staticFrame({first=2,last=4,interval=0.25,mode=mode},step*0.25)==expected[mode+1][step+1])
end end
local tmp=os.tmpname(); P.save(p,tmp)
local q=P.load(tmp); assert(q.count==1 and q.start==2 and q.camera.distance==p.camera.distance)
os.remove(tmp)
local function rejects(change)
    local bad=P.copy(p); change(bad); assert(not pcall(P.validate,bad))
end
rejects(function(v) v.count=0 end); rejects(function(v) v.camera.distance=0/0 end)
rejects(function(v) v.width=4096;v.height=4096;v.count=4096 end)
rejects(function(v) v.version=2 end); rejects(function(v) v.unknown=true end)
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
