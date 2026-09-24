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
local task,started,baseline,heightBaseline,buildBaseline
local function close(a,b) assert(math.abs(a-b)<.0001,tostring(a)..' != '..tostring(b)) end
local function run()
 init()
 local path='/tmp/ime_graph_editor.png';local pixels={};for i=1,129*129*3 do pixels[i]=150 end
 assert(mbm.createTexture(pixels,129,129,3,'ime_graph_editor',path));api.openImage(path)
 assert(api.action(function(p)
  local r=Model.add(p,'rectangle',0,0,129,129)
  r.heightEdits={{x=.5,y=.5,radius=.1,strength=1,height=0,mode='flatten'}}
  r.overrides.simplify=true
 end))
 api.select(1,false);local E=api.state
 E.values.heightSource='curved';E.values.curvedRadius=15;E.values.columns=8;E.values.rows=8
 assert(api.applyProperties())
 assert(api.action(function(p) local r=p.regions[1];Model.curved.convert(r,Model.options(p,r)) end))
 local function nodes() return E.project.regions[1].curvedNodes end
 local function touch(fn,x,y)
  local t=Canvas.transform(E);fn(0,(t.x+x*128*t.scale)/E.camera2d.sx,(t.y+y*128*t.scaleY)/E.camera2d.sy)
 end
 E.tool='curved';E.curvedNode=1;Canvas.sync(E)
 -- Empty-space dragging pans the camera without changing any target.
 local beforePan=Model.copy(nodes());local camera=E.camera2d
 local cx,cy=camera.x,camera.y;local t=Canvas.transform(E)
 local px,py=t.x+.1*128*t.scale,t.y+.1*128*t.scaleY
 onTouchDown(0,px/camera.sx,py/camera.sy)
 onTouchMove(0,(px+24)/camera.sx,(py+16)/camera.sy)
 onTouchUp(0,(px+24)/camera.sx,(py+16)/camera.sy)
 close(camera.x,cx-24/camera.sx);close(camera.y,cy+16/camera.sy)
 assert(Model.curved.same(beforePan,nodes()) and not E.drag and not E.panDrag)
 camera:setPos(cx,cy);Canvas.sync(E)
 print('GRAPH EDITOR EMPTY SPACE PAN OK')
 touch(onTouchDown,.5,.5);touch(onTouchMove,.55,.52);touch(onTouchUp,.55,.52)
 close(Model.curved.bounds(nodes()[1]).x,.55)
 api.undo(false);close(Model.curved.bounds(nodes()[1]).x,.5)
 api.undo(true);close(Model.curved.bounds(nodes()[1]).x,.55)
 E.tool='curved';Canvas.sync(E)
 touch(onTouchDown,.7,.67);touch(onTouchMove,.75,.72);touch(onTouchUp,.75,.72)
 close(Model.curved.bounds(nodes()[1]).rx,.2)
 assert(api.action(function(p)
  local r=p.regions[1];Model.curved.add(r,1,'target','line',3)
 end))
 E.tool='curved';E.curvedNode=1;Canvas.sync(E)
 local old=nodes()[2][1].x
 touch(onTouchDown,.55,.52);touch(onTouchMove,.5,.5);touch(onTouchUp,.5,.5)
 close(nodes()[2][1].x,old-.05)
 E.tool='curved';E.curvedNode=2;Canvas.sync(E)
 local py=nodes()[2][1].y
 touch(onTouchDown,.5,py);touch(onTouchMove,.48,py);touch(onTouchUp,.48,py)
 close(nodes()[2][1].x,.48)
 E.tool='curved';Canvas.sync(E)
 touch(onTouchDown,.49,.5);touch(onTouchMove,.5,.52);Canvas.cancel(E)
 close(nodes()[2][1].x,.48)
 assert(api.action(function(p)
  local r=p.regions[1];local a=Model.curved.add(r,0,'region','rectangle',8)
  for _,v in ipairs(r.curvedNodes[a]) do v.x=v.x*.2+.025;v.y=v.y*.4+.2 end
  Model.curved.add(r,a,'target','point',2)
 end))
 local count=#nodes();assert(count==4)
 assert(api.action(function(p) p.regions[1].curvedNodes=Model.curved.remove(p.regions[1].curvedNodes,3) end))
 assert(#nodes()==2);api.undo(false);assert(#nodes()==4)
 E.draft.curvedNodes[2].thickness=4;assert(api.applyProperties());close(nodes()[2].thickness,4)
 local graph=api.curved.hierarchy
 local savedNodes=Model.copy(E.draft.curvedNodes)
 assert(not graph.edit(E,function(n) return Model.curved.transform(n,1,'rx',-1) end,'shape_rx',-1))
 assert(tUtil.sMessageOverlay==E.graphEditError and tUtil.bWarnMessage and not tUtil.bFocusMsgOnce)
 assert(E.graphEditError:find(tLang.L('ime_curved_shape_rx'),1,true) and E.graphEditError:find('-1',1,true))
 assert(Model.curved.same(savedNodes,E.draft.curvedNodes))

 assert(not graph.edit(E,function(n) n[1][1].x=-.1 end))
 assert(Model.curved.same(savedNodes,E.draft.curvedNodes))
 assert(E.graphEditError)
 assert(not graph.edit(E,function(n) n[1][1]=Model.copy(n[1][2]) end))
 assert(Model.curved.same(savedNodes,E.draft.curvedNodes))
 assert(not graph.edit(E,function(n) n[1][1]={x=.5,y=.5} end))
 assert(not graph.edit(E,function(n) n[2][1]=Model.copy(n[2][2]) end))
 assert(not graph.edit(E,function(n) n[#n+1]=Model.copy(n[3]) end))
 assert(not graph.edit(E,function(n) n[1][1].x=0/0 end))
 assert(not graph.edit(E,function(n) return Model.curved.transform(n,1,'x',.99) end))
 assert(Model.curved.same(savedNodes,E.draft.curvedNodes))
 assert(graph.edit(E,function(n) return Model.curved.transform(n,1,'x',.51) end))
 close(Model.curved.bounds(E.draft.curvedNodes[1]).x,.51)
 close(E.draft.curvedNodes[2][1].x,savedNodes[2][1].x+.01)
 assert(E.draft.curvedNodes[1].shape=='ellipse' and not E.graphEditError)
 assert(api.applyProperties());api.undo(false)
 assert(Model.curved.same(savedNodes,E.draft.curvedNodes))
 E.tool='curved';E.curvedNode=1;Canvas.sync(E)
 local beforeDrag=Model.copy(nodes())
 touch(onTouchDown,.5,.5);touch(onTouchMove,.99,.99);touch(onTouchUp,.99,.99)
 assert(Model.curved.same(beforeDrag,nodes()),'invalid drag changed geometry')
 print('GRAPH EDITOR REJECT INVALID / MOVE CENTER WITH CHILDREN OK')

 api.saveProject('/tmp/ime_graph_editor.imesh')
 local saved=IO.load('/tmp/ime_graph_editor.imesh');local o=Model.options(saved,saved.regions[1]);assert(#o.curvedNodes==4);close(o.curvedNodes[2].thickness,4)
 api.openProject('/tmp/ime_graph_editor.imesh');api.select(1,false)
 api.exportOne('/tmp/ime_graph_editor.msh');while E.meshTask do coroutine.yield() end
 assert(IO.exists('/tmp/ime_graph_editor.msh'),E.status)
 E.heightView=2;E.heightRequested=true
 repeat api.updateHeightPreview();coroutine.yield() until E.heightObject or E.heightError
 assert(E.heightObject,E.heightError)
 E.tool='curved';E.curvedPanelOpen=true;E.canvasDirty=true;Canvas.sync(E)
 local beforeClose=Model.copy(E.project)
 assert(not api.curved.hierarchy.close(E,function() return false end))
 assert(E.curvedPanelOpen and E.tool=='curved')
 assert(api.curved.hierarchy.close(E,function() return true end))
 assert(not E.curvedPanelOpen and E.tool=='select')
 assert(Model.curved.same(beforeClose,E.project),'closing the panel changed the project')
 assert(not api.curved.hierarchy.open(E,function() return false end))
 assert(not E.curvedPanelOpen and E.tool=='select')
 assert(api.curved.hierarchy.open(E,function() return true end))
 assert(E.curvedPanelOpen and E.tool=='curved')
 assert(Model.curved.same(beforeClose,E.project),'opening the panel changed the project')

 started=mbm.getTimeRun()
 print('GRAPH EDITOR MOVE / RESIZE / HISTORY / PERSISTENCE / EXPORT / MAP OK')
end
function onInitScene()
 task=coroutine.create(run);local ok,err=coroutine.resume(task)
 if not ok then print('GRAPH EDITOR FAIL '..tostring(err));mbm.quit() end
end
function onLoop(delta)
 local header=tImGui.CollapsingHeader
 tImGui.CollapsingHeader=function(label,...)
  if label==tLang.L('ime_grooves_group') then tImGui.SetNextItemOpen(true,0) end
  return header(label,...)
 end
 loop(delta);tImGui.CollapsingHeader=header
 if coroutine.status(task)~='dead' then
  local ok,err=coroutine.resume(task)
  if not ok then print('GRAPH EDITOR FAIL '..tostring(err));mbm.quit() end
  return
 end
 if not started then return end
 local E=api.state
 if not baseline and mbm.getTimeRun()-started>1 then baseline=E.canvasBuilds;heightBaseline=E.heightBuilds;buildBaseline=E.builds end
 if baseline and mbm.getTimeRun()-started>3 then
  assert(E.canvasBuilds==baseline and E.heightBuilds==heightBaseline and E.builds==buildBaseline,'idle editor rebuilds curved geometry')
  print('GRAPH EDITOR IDLE / REAL IMGUI PANEL OK');mbm.quit()
 end
end
