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
local Detect=require 'image_mesh_detect'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local started,baseline
local function run()
 init()
 local pixels={}
 for y=0,255 do for x=0,255 do
  local inside=x>=20 and x<230 and y>=20 and y<230 and not (x>=120 and y>=180)
  local cavity=x>=70 and x<100 and y>=70 and y<100
  local a=inside and not cavity and 255 or 0
  pixels[#pixels+1]=inside and 220 or 0;pixels[#pixels+1]=80;pixels[#pixels+1]=40;pixels[#pixels+1]=a
 end end
 local source='/tmp/ime_auto.png'
 assert(mbm.createTexture(pixels,256,256,4,'auto_fixture',source))
 local bytes,w,h=mbm.readImagePixels(source);assert(bytes and #bytes==w*h*4 and w==256 and h==256)
 local missing,why=mbm.readImagePixels('/tmp/no-such-ime-image');assert(not missing and type(why)=='string')
 local options={mode=1,alpha=127,tolerance=24,color={r=0,g=0,b=0,a=1}}
 local points,step,count=Detect.trace(bytes,w,h,{x=0,y=0,w=w,h=h},40,40,options,function() end)
 assert(#points==6 and step==1 and count>10000,'outer contour or cavity detection')
 assert(not pcall(Detect.trace,bytes,w,h,{x=0,y=0,w=w,h=h},0,0,options,function() end))
 local diagonal=string.char(255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255)
 local _,_,connected=Detect.trace(diagonal,2,2,{x=0,y=0,w=2,h=2},0,0,options,function() end)
 assert(connected==1,'diagonal pixels were merged')
 local mask={1,1,1,1,0,1,1,1,0};local data={}
 for _,v in ipairs(mask) do data[#data+1]=string.char(255,255,255,v*255) end
 local valid,why=pcall(Detect.trace,table.concat(data),3,3,{x=0,y=0,w=3,h=3},0,0,options,function() end)
 assert(valid and require('image_mesh_holes_geometry').simple(why),'diagonal corner not separated')
 local ticks=0
 local large=string.rep(string.char(255,255,255,255),1024*1024)
 local outline,coarse,total=Detect.trace(large,1024,1024,{x=0,y=0,w=1024,h=1024},100,100,options,function() ticks=ticks+1 end)
 assert(coarse==2 and total==262144 and #outline==4 and ticks>0)
 api.openImage(source)
 local E=api.state;E.tool='auto_contour';Canvas.sync(E)
 local function click(x,y)
  local t=Canvas.transform(E)
  onTouchDown(0,(t.x+x*t.scale)/E.camera2d.sx,(t.y+y*t.scaleY)/E.camera2d.sy)
  onTouchUp(0,(t.x+x*t.scale)/E.camera2d.sx,(t.y+y*t.scaleY)/E.camera2d.sy)
 end
 local function complete()
  local turns=0
  while E.autoTask do api.auto.resume(E);turns=turns+1;assert(turns<1000) end
  return turns
 end
 click(40,40);assert(complete()>1);assert(E.stroke and #E.polygon==6 and #E.project.regions==0)
 assert(api.freehand.ready(E));api.finishPolygon();assert(#E.project.regions==1)
 -- Color mode ignores internal dark details until a hole is explicitly requested.
 local rgb={};for y=0,255 do for x=0,255 do
  local shape=x>=60 and x<110 and y>=60 and y<110
  rgb[#rgb+1]=shape and 255 or 0;rgb[#rgb+1]=shape and 255 or 0;rgb[#rgb+1]=shape and 255 or 0
 end end
 assert(mbm.createTexture(rgb,256,256,3,'auto_color','/tmp/ime_auto_color.png'))
 local colors,cw,ch=mbm.readImagePixels('/tmp/ime_auto_color.png')
 options.mode=2
 local shape=Detect.trace(colors,cw,ch,{x=0,y=0,w=cw,h=ch},80,80,options,function() end)
 assert(#shape==4)
 local a=api.auto.state(E);a.mode=2;E.tool='auto_background';click(0,0);complete()
 assert(E.tool=='auto_contour' and a.color.r==0 and math.abs(a.color.g-80/255)<.001)
 -- Detect a real opening from a white area, preserving the selected module.
 assert(api.action(function(p) p.image.path='/tmp/ime_auto_color.png' end))
 a.target=2;a.color={r=0,g=0,b=0,a=1};E.tool='auto_contour';click(80,80);complete()
 assert(api.freehand.ready(E));assert(api.holes.finish(E,api.action));assert(#E.project.regions[1].holes==1)
 api.undo(false);assert(not E.project.regions[1].holes);api.undo(true)
 api.saveProject('/tmp/ime_auto.imesh');assert(#IO.load('/tmp/ime_auto.imesh').regions[1].holes==1)
 assert(api.exportOne('/tmp/ime_auto.msh'))
 local asset=meshDebug:new();assert(asset:load('/tmp/ime_auto.msh'));assert(asset:check())
 -- Cancellation and changing the selected project must discard unfinished work.
 E.tool='auto_contour';click(80,80);assert(E.autoTask);Canvas.cancel(E);assert(not E.autoTask and not E.stroke)
 E.tool='auto_contour';click(80,80);assert(E.autoTask)
 E.project=Model.copy(E.project);api.auto.resume(E);assert(not E.autoTask)
 E.tool='auto_contour';click(80,80);complete();assert(E.stroke);Canvas.sync(E)
 started=mbm.getTimeRun()
 print('AUTO RGBA / ALPHA / COLOR / CAVITIES / PICK / MODULE / HOLE / CANCEL / HISTORY / SAVE / EXPORT OK')
end
function onInitScene() local ok,err=xpcall(run,debug.traceback);if not ok then print('AUTO FAIL '..tostring(err));mbm.quit() end end
function onLoop(delta)
 if not started then return end
 loop(delta)
 if not baseline and mbm.getTimeRun()-started>1 then baseline=api.state.canvasBuilds end
 if mbm.getTimeRun()-started>4 then assert(baseline==api.state.canvasBuilds,'idle auto redraw');print('AUTO GUI / IDLE OK');mbm.quit() end
end
