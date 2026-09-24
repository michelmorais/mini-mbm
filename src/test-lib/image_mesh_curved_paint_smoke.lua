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

local function close(a,b,t) assert(math.abs(a-b)<(t or .04),tostring(a)..' != '..tostring(b)) end
local task,started
local function run()
 local path='/tmp/ime_curved_paint.png';local pixels={};for i=1,65*65*3 do pixels[i]=128 end
 assert(mbm.createTexture(pixels,65,65,3,'ime_curved_paint',path))
 local o={heightSource='curved',width=100,height=100,curvedEdge=1,curvedTarget=8,
  curvedRadius=20,columns=8,rows=8,heightTolerance=.01}
 local function generate()
  local mesh,report=mbm.generateImageMesh(path,o);assert(mesh,report);assert(mesh:check())
  local vertices,indices=mesh:getVertex(1,1,1,report.vertices),mesh:getIndex(1,1)
  local function span(u,v)
   local x,y=(u-.5)*o.width,(.5-v)*o.height;local low,high=math.huge,-math.huge
   for i=1,#indices,3 do
    local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
    local det=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y)
    if math.abs(det)>1e-10 then
     local wa=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/det
     local wb=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/det
     if wa>=-1e-6 and wb>=-1e-6 and wa+wb<=1.000001 then
      local z=wa*a.z+wb*b.z+(1-wa-wb)*c.z;low=math.min(low,z);high=math.max(high,z)
     end
    end
   end
   return high-low,low,high
  end
  -- Geometric edge accounting includes duplicated normal/UV seams.
  local edges={}
  local function key(p) return string.format('%.5f,%.5f,%.5f',p.x+0.,p.y+0.,p.z+0.) end
  for _,p in ipairs(vertices) do close(p.nx*p.nx+p.ny*p.ny+p.nz*p.nz,1,.001) end
  for i=1,#indices,3 do
   local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
   for _,pair in ipairs{{a,b},{b,c},{c,a}} do
    local ka,kb=key(pair[1]),key(pair[2]);assert(ka~=kb)
    local k=ka<kb and ka..'/'..kb or kb..'/'..ka
    local e=edges[k] or {n=0,b=0};edges[k]=e;e.n=e.n+1;e.b=e.b+(ka<kb and 1 or -1)
   end
  end
  for _,e in pairs(edges) do assert(e.n==2 and e.b==0,'open or inconsistent curved geometry') end
  return span,mesh,report
 end

 local function dab(mode,x,y,radius,strength,height)
  return {mode=mode,x=x or .5,y=y or .5,radius=radius or .1,strength=strength or 1,height=height or .5}
 end
 local clean,_,original=generate();close(clean(.5,.5),8)
 o.heightEdits={dab('lower')};local span=generate();close(span(.5,.5),8) -- opt-in compatibility
 o.curvedPainting=true;span=generate();close(span(.5,.5),1);close(span(.6,.5),8,.1)
 o.heightEdits={dab('raise',0,.5)};span=generate();close(span(0,.5),8);close(span(.5,.5),8)
 o.heightEdits={dab('flatten',.5,.5,.1,1,.3)};span=generate();close(span(.5,.5),3.1)
 o.curvedTarget=1;o.curvedEdge=8;span=generate();close(span(.5,.5),3.1) -- absolute normalized thickness
 o.curvedTarget=8;o.curvedEdge=1
 o.heightEdits={dab('lower',.5,.5,.02)};span=generate();local low=span(.5,.5)
 o.heightEdits[2]=dab('smooth',.5,.5,.1);span=generate();assert(span(.5,.5)>low+.2,'smoothing must use painted curve')
 o.heightEdits={dab('lower',.375,.5,.025)};span=generate();close(span(.375,.5),1,.15) -- narrow off-center dab
 close(span(.75,.5),clean(.75,.5),.08)
 o.curvedSimplify=true;o.curvedSimplifyRatio=.5;o.curvedSimplifyError=.01
 local simple,_,report=generate();close(simple(.375,.5),span(.375,.5),.15);assert(report.curvedSourceTriangles)
 o.curvedSimplify=false
 o.heightEdits={dab('flatten',.5,.5,.1,1,.4)}
 assert(mbm.generateImageMeshMap(path,o,'/tmp/ime_curved_paint_map.png'))
 local bytes,w=mbm.readImagePixels('/tmp/ime_curved_paint_map.png');close(bytes:byte((32*w+32)*4+1)/255,3.8/8,.005)
 o.curvedNodes={{{x=.3,y=.3},{x=.7,y=.3},{x=.7,y=.7},{x=.3,y=.7},role='target',parent=0,thickness=8}}
 o.curvedSymmetric=false;span=generate();local thick,front,back=span(.5,.5);close(thick,3.8);close(back,0,.001)
 o.holes={{{x=.1,y=.1},{x=.2,y=.1},{x=.2,y=.2},{x=.1,y=.2}}}
 o.heightEdits={dab('raise',.15,.1,.15)};span=generate();assert(span(.15,.15)<0,'hole filled');assert(span(.15,.1)>2)
 o.holes=nil;o.curvedNodes=nil;o.curvedSymmetric=true
 o.curvedFaceted=true;o.heightEdits={dab('lower')};span=generate();close(span(.5,.5),8) -- saved but inactive
 o.curvedFaceted=false;o.curvedInterior=true;o.curvedNodes={{{x=.5,y=.5},role='target',parent=0,thickness=8}}
 o.heightEdits={dab('lower')};span=generate();close(span(.5,.5),1)
 o.heightEdits=nil;span=generate();close(span(.5,.5),8)
 o.curvedInterior=false;o.curvedNodes=nil;o.curvedTarget=1;o.heightEdits={dab('raise')};span=generate();close(span(.5,.5),1)
 o.curvedTarget=8;o.heightEdits={dab('raise')};o.heightEdits[1].radius=0
 local bad,err=mbm.generateImageMesh(path,o);assert(not bad and err:find('heightEdits'),err)
 o.heightEdits=nil;local _,_,restored=generate();assert(restored.triangles==original.triangles,'clear changed original geometry')
 o.heightEdits={dab('lower')};o.maxVertices=100
 assert(not mbm.generateImageMesh(path,o),'painting bypassed geometry budget');o.maxVertices=65535
 local cancelled=assert(mbm.startImageMesh(path,o));cancelled:cancel();cancelled:close()
 o.heightEdits={dab('lower')};task=assert(mbm.startImageMesh(path,o));o.heightEdits[1].mode='raise'
 started=mbm.getTimeRun()
end
function onInitScene()
 local ok,err=xpcall(run,debug.traceback)
 if not ok then print('CURVED PAINT FAIL '..tostring(err));mbm.quit() end
end
function onLoop()
 if not task then return end
 local s=task:getStatus()
 if s.state=='completed' then
  local asset,report=task:takeResult();assert(asset,report)
  local found=false
  for _,v in ipairs(asset:getVertex(1,1,1,report.vertices)) do
   if math.abs(v.x)<.001 and math.abs(v.y)<.001 then close(math.abs(v.z),.5,.001);found=true end
  end
  assert(found);assert(asset:save('/tmp/ime_curved_paint.msh',false,false,true))
  print('CURVED PAINT BRUSHES / RANGE / BORDERS / TARGETS / HOLES / INTERIOR / SIMPLIFY / MAP / ASYNC OK');mbm.quit()
 elseif s.state~='running' or mbm.getTimeRun()-started>20 then
  print('CURVED PAINT FAIL '..tostring(s.error or s.state));task:cancel();mbm.quit()
 end
end
