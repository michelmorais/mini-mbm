--[[---------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------]]

package.path='editor/?.lua;'..package.path
local Model=require 'image_mesh_model'
local IO=require 'image_mesh_io'
local Line=require 'image_mesh_height_line'
local task,started
local function close(a,b) assert(math.abs(a-b)<.012,tostring(a)..' != '..tostring(b)) end
local function test()
 local path='/tmp/ime-line-source.png';local pixels={}
 for i=1,65*33*3 do pixels[i]=128 end
 assert(mbm.createTexture(pixels,65,33,3,'ime_line',path))
 local line={{x=.25,y=.5},{x=.75,y=.5},shape='line',lineWidth=.25,height=.2,transition=0,name='groove',enabled=true}
 local o={heightSource='manual',baseHeight=.8,lockBorder=false,columns=16,rows=8,relief=10,followImage=true,heightAreas={line}}
 local function map()
  local ok,err=mbm.generateImageMeshMap(path,o,'/tmp/ime-line-map.png');assert(ok,err)
  local bytes=assert(mbm.readImagePixels('/tmp/ime-line-map.png'))
  return function(x,y) return bytes:byte((y*65+x)*4+1)/255 end
 end
 local m=map();close(m(32,16),.2);close(m(32,12),.2);close(m(32,11),.8)
 close(m(12,16),.2);close(m(11,16),.8);close(m(13,13),.8)
 assert(Line.contains({x=0,y=0,w=65,h=33},line,32,13))
 assert(not Line.contains({x=0,y=0,w=65,h=33},line,32,11))
 line.transition=.125;m=map();close(m(32,12),.8);close(m(32,14),.5);close(m(32,16),.2)
 line.transition=0
 local mesh,report=mbm.generateImageMesh(path,o);assert(mesh,report);assert(mesh:check())
 local job=assert(mbm.startImageMesh(path,o));line.lineWidth=.01;line[1].x=0
 while job:getStatus().state=='running' do coroutine.yield() end
 local copy,r=job:takeResult();assert(copy,r);assert(r.vertices==report.vertices)
 local a,b=mesh:getVertex(1,1,1,report.vertices),copy:getVertex(1,1,1,r.vertices)
 for i,p in ipairs(a) do assert(p.z==b[i].z) end
 line.lineWidth=.25;line[1].x=.25
 o.heightAreas={{{x=.25,y=.25},{x=.75,y=.75},{x=.75,y=.25},{x=.25,y=.75},shape='line',lineWidth=.2,height=.2,transition=0}}
 m=map();close(m(32,16),.2);close(m(2,2),.8)
 o.heightAreas={line};line.enabled=false;close(map()(32,16),.8);line.enabled=true
 o.heightSource='image';close(map()(32,16),128/255);o.heightSource='manual'
 line.lineWidth=0;local bad,err=mbm.generateImageMesh(path,o);assert(not bad and err:find('width'));line.lineWidth=.25
 o.maxVertices=10;bad,err=mbm.generateImageMesh(path,o);assert(not bad and err:find('budget'));o.maxVertices=nil
 local p=Model.new(path,65,33);local region=Model.add(p,'rectangle',0,0,65,33);region.heightAreas={Model.copy(line)}
 Model.validate(p);local history=Model.history();Model.commit(history,p);region.heightAreas[1].lineWidth=.4
 local old=Model.undo(history,p);assert(old.regions[1].heightAreas[1].lineWidth==.25)
 tImGui=require 'ImGui';local util=require 'editor_utils';IO.save(p,'/tmp/ime-line.imesh',util.save)
 local loaded=IO.load('/tmp/ime-line.imesh');local saved=loaded.regions[1].heightAreas[1]
 assert(saved.shape=='line' and saved.lineWidth==.4 and #saved==2)
 print('IMAGE MESH HEIGHT LINE WIDTH / ROUND CAPS / TRANSITION / OUTSIDE RELIEF / CROSSINGS / ASYNC / MODES / LIMITS / SAVE / UNDO OK')
end
function onInitScene() started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop()
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH HEIGHT LINE FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>40 then print('IMAGE MESH HEIGHT LINE FAIL timeout');mbm.quit() end
end
