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
local Canvas=require 'image_mesh_canvas'
local IO=require 'image_mesh_io'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local started,baseline
local function run()
 init()
 local pixels={};for i=1,256*256 do pixels[#pixels+1]=80;pixels[#pixels+1]=150;pixels[#pixels+1]=220 end
 assert(mbm.createTexture(pixels,256,256,3,'freehand_fixture','/tmp/ime_freehand.png'))
 api.openImage('/tmp/ime_freehand.png')
 local E=api.state
 local function touch(fn,p)
  local t=Canvas.transform(E)
  fn(0,(t.x+p.x*t.scale)/E.camera2d.sx,(t.y+p.y*t.scaleY)/E.camera2d.sy)
 end
 local function stroke(points,tool)
  E.tool=tool;E.canvasDirty=true;Canvas.sync(E)
  touch(onTouchDown,points[1])
  for i=2,#points do touch(onTouchMove,points[i]) end
  touch(onTouchUp,points[#points]);assert(not E.drag)
 end
 local corners={{x=20,y=20},{x=230,y=20},{x=230,y=230},{x=145,y=230},{x=145,y=180},{x=110,y=180},{x=110,y=230},{x=20,y=230}}
 local points={}
 for i,p in ipairs(corners) do local q=corners[i%#corners+1]
  for j=0,30 do local t=j/30;points[#points+1]={x=p.x+(q.x-p.x)*t,y=p.y+(q.y-p.y)*t} end
 end
 local revision=E.revision
 stroke(points,'freehand');assert(api.freehand.ready(E));assert(#E.polygon==8)
 assert(#E.project.regions==0 and E.revision==revision,'stroke changed project before confirmation')
 api.finishPolygon();assert(#E.project.regions==1 and E.project.regions[1].shape=='polygon')
 local circle={};for i=0,180 do local a=i*math.pi/90;circle[#circle+1]={x=120+28*math.cos(a),y=100+28*math.sin(a)} end
 stroke(circle,'hole_freehand');assert(api.freehand.ready(E))
 local fine=#E.polygon;E.strokeTolerance=4;api.freehand.reduce(E);assert(#E.polygon<fine)
 E.strokeTolerance=1.5;api.freehand.reduce(E);assert(api.freehand.ready(E));assert(api.holes.finish(E,api.action))
 assert(#E.project.regions[1].holes==1)
 api.undo(false);assert(not E.project.regions[1].holes)
 api.undo(true);assert(#E.project.regions[1].holes==1)
 api.saveProject('/tmp/ime_freehand.imesh');api.openProject('/tmp/ime_freehand.imesh');api.select(1,false)
 assert(#IO.load('/tmp/ime_freehand.imesh').regions[1].holes==1)
 assert(api.exportOne('/tmp/ime_freehand.msh'))
 local asset=meshDebug:new();assert(asset:load('/tmp/ime_freehand.msh'));assert(asset:check())
 local revision=E.revision
 stroke({{x=30,y=30},{x=200,y=200},{x=30,y=200},{x=200,y=30}},'freehand')
 assert(E.stroke.error=='invalid' and not api.freehand.ready(E));assert(E.revision==revision)
 api.undo(false);assert(E.stroke==nil and #E.polygon==0);assert(E.revision==revision)
 -- Bounds and limits are checked on release; incomplete strokes never commit.
 local zigzag={};for i=0,300 do zigzag[#zigzag+1]={x=i,y=i%2==0 and 0 or 10} end
 E.stroke={points=zigzag};E.strokeTolerance=.1;api.freehand.reduce(E);assert(E.stroke.error=='limit')
 E.stroke.overflow=true;api.freehand.reduce(E);assert(E.stroke.error=='overflow')
 Canvas.cancel(E);E.strokeTolerance=1.5
 stroke(points,'freehand');assert(api.freehand.ready(E));Canvas.sync(E)
 started=mbm.getTimeRun()
 print('FREEHAND MODULE / HOLE / CONCAVE / REDUCTION / INVALID / LIMIT / CANCEL / HISTORY / SAVE / EXPORT OK')
end
function onInitScene() local ok,err=xpcall(run,debug.traceback);if not ok then print('FREEHAND FAIL '..tostring(err));mbm.quit() end end
function onLoop(delta)
 if not started then return end
 loop(delta)
 if not baseline and mbm.getTimeRun()-started>1 then baseline=api.state.canvasBuilds end
 if mbm.getTimeRun()-started>4 then
  assert(baseline==api.state.canvasBuilds,'idle freehand redraw')
  print('FREEHAND GUI / IDLE OK');mbm.quit()
 end
end
