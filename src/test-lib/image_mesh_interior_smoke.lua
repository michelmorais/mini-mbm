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
local Asset=require 'image_mesh_asset'
local pending,started,cancelling
local function close(a,b,e) assert(math.abs(a-b)<(e or .001),tostring(a)..' != '..tostring(b)) end
local function rect(x0,y0,x1,y1) return {{x=x0,y=y0},{x=x1,y=y0},{x=x1,y=y1},{x=x0,y=y1}} end
local function inspect(mesh,report,o)
  local vertices,indices=Asset.geometry(mesh)
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
  local edges,unique={},{}
  local area,volume=0,0
  local function key(p) return string.format('%.5f,%.5f,%.5f',p.x+0.,p.y+0.,p.z+0.) end
  for _,p in ipairs(vertices) do close(p.nx*p.nx+p.ny*p.ny+p.nz*p.nz,1,.001) end
  for i=1,#indices,3 do
   local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
   if a.z<0 and b.z<0 and c.z<0 then area=area+math.abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))/2 end
   volume=volume+(a.x*(b.y*c.z-b.z*c.y)+a.y*(b.z*c.x-b.x*c.z)+a.z*(b.x*c.y-b.y*c.x))/6
   local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
   local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
   local nx,ny,nz=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx
   local length=math.sqrt(nx*nx+ny*ny+nz*nz)
   for _,pair in ipairs{{a,b},{b,c},{c,a}} do
    local ka,kb=key(pair[1]),key(pair[2]);assert(ka~=kb);unique[ka]=true;unique[kb]=true
    local k=ka<kb and ka..'/'..kb or kb..'/'..ka
    local e=edges[k] or {n=0,b=0};edges[k]=e;e.n=e.n+1;e.b=e.b+(ka<kb and 1 or -1)
   end
  end
  for _,e in pairs(edges) do assert(e.n==2 and e.b==0,'open or inconsistent curved geometry') end
  local nv,ne=0,0;for _ in pairs(unique) do nv=nv+1 end;for _ in pairs(edges) do ne=ne+1 end
  assert(nv-ne+#indices/3==2-2*#(o.holes or {}),'incorrect hole topology')
  assert(volume>0,'reversed volume')
  if not o.shape or o.shape=='rectangle' then
   local expected=1
   for _,hole in ipairs(o.holes or {}) do
    local cut=0;for i,a in ipairs(hole) do local b=hole[i%#hole+1];cut=cut+a.x*b.y-a.y*b.x end
    expected=expected-math.abs(cut)/2
   end
   close(area,expected*o.width*o.height,.03)
  end
  return span,vertices
end

local function run()
 local path='/tmp/ime_interior.png';local pixels={};for i=1,65*65*3 do pixels[i]=150 end
 assert(mbm.createTexture(pixels,65,65,3,'ime_interior',path))
 local u={{x=0,y=0},{x=1,y=0},{x=1,y=1},{x=.7,y=1},{x=.7,y=.3},{x=.3,y=.3},{x=.3,y=1},{x=0,y=1}}
 local target={{x=.15,y=.85},{x=.15,y=.15},{x=.85,y=.15},{x=.85,y=.85},shape='polyline',parent=0,role='target',thickness=8}
 local o={heightSource='curved',curvedInterior=true,shape='polygon',contour=u,width=100,height=100,
  curvedEdge=1,curvedNodes={target},columns=24,rows=24}
 local function generate()
  local a,r=mbm.generateImageMesh(path,o);assert(a,r);assert(a:check());return inspect(a,r,o),a,r
 end
 local span,asset=generate()
 for _,p in ipairs(target) do close(span(p.x,p.y),8,.001) end
 for _,p in ipairs(u) do close(span(p.x,p.y),1,.001) end
 assert(span(.5,.7)==-math.huge,'surface bridges the U gap')
 for _,p in ipairs{{.1,.5},{.9,.5},{.5,.1}} do local h=span(p[1],p[2]);assert(h>1 and h<8) end
 local map='/tmp/ime_interior_map.png';local ok,e=mbm.generateImageMeshMap(path,o,map);assert(ok,e)
 local rgba,w,h=mbm.readImagePixels(map)
 assert(rgba:byte((45*w+32)*4+4)==0,'gap not transparent')
 for y=8,56,4 do for x=4,60,4 do
  local value=span(x/64,y/64)
  if value~=-math.huge then close(rgba:byte((y*w+x)*4+1)/255,value/8,.0041) end
 end end
 assert(asset:save('/tmp/ime_interior.msh',false,false,true))
 print('INTERIOR U / POLYLINE / EDGE HEIGHTS / NO GAP BRIDGE / MAP / EXPORT OK')
 o.curvedEdge=8;target.thickness=1;span=generate();close(span(.15,.85),1,.001)
 o.curvedEdge=4;target.thickness=4;span=generate();close(span(.1,.5),4,.001)
 o.curvedEdge=1;target.thickness=8;o.curvedSymmetric=false;span=generate();local _,_,back=span(.1,.5);close(back,0,.001)
 o.curvedSymmetric=true;o.holes={rect(.03,.4,.08,.6)};span=generate();assert(span(.05,.5)==-math.huge)
 close(span(.03,.5),1,.001);close(span(.15,.5),8,.001)
 print('INTERIOR INVERT / EQUAL / FLAT BACK / HOLE BOUNDARY OK')
 local function rejected(fragment)
  local a,e=mbm.generateImageMesh(path,o);assert(not a and e:find(fragment),tostring(e))
 end
 target[2]={x=.85,y=.85};rejected('boundary');target[2]={x=.15,y=.15}
 o.curvedFaceted=true;rejected('faceting');o.curvedFaceted=false
 o.curvedInterior=false;rejected('Polyline');o.curvedInterior=true
 target[2]={x=.05,y=.5};rejected('inside');target[2]={x=.15,y=.15}
 o.holes=nil;o.curvedNodes={{{x=.15,y=.5},parent=0,role='target',thickness=8}};generate()
 o.curvedNodes={{{x=.15,y=.4},{x=.15,y=.6},parent=0,role='target',thickness=8}};generate()
 o.maxVertices=30;rejected('budget');o.maxVertices=nil
 print('INTERIOR POINT / LINE / INVALID SEGMENTS / MODE / BUDGET OK')
 o.curvedNodes={{{x=.1,y=.1},{x=.25,y=.2},{x=.1,y=.2},{x=.25,y=.1},parent=0,role='target',shape='polyline',thickness=8}}
 rejected('cross itself')
 o.curvedNodes={{{x=.15,y=.1},{x=.25,y=.1},{x=.1,y=.1},parent=0,role='target',shape='polyline',thickness=8}}
 rejected('backtrack')
 o.curvedNodes={};span=generate();close(span(.15,.5),1,.001)
 o.contour={{x=.1,y=.05},{x=.9,y=.05},{x=.9,y=.6},{x=.3,y=.6},{x=.3,y=.75},{x=.9,y=.75},
  {x=.9,y=.95},{x=.1,y=.95},{x=.1,y=.4},{x=.7,y=.4},{x=.7,y=.25},{x=.1,y=.25}}
 o.width=140;o.height=70
 o.curvedNodes={{{x=.2,y=.15},{x=.8,y=.15},{x=.8,y=.5},{x=.2,y=.5},{x=.2,y=.85},{x=.8,y=.85},
  parent=0,role='target',shape='polyline',thickness=8}}
 span=generate();assert(span(.5,.32)==-math.huge and span(.5,.67)==-math.huge)
 close(span(.2,.5),8,.001);close(span(.8,.5),8,.001)
 print('INTERIOR EMPTY / S SHAPE / ANISOTROPIC METRIC OK')
 pending=assert(mbm.startImageMesh(path,o));o.curvedNodes[1][1].x=.5;started=mbm.getTimeRun()
end
function onInitScene()
 local ok,e=xpcall(run,debug.traceback)
 if not ok then print('INTERIOR FAIL '..tostring(e));mbm.quit() end
end
function onLoop()
 if not pending then mbm.quit();return end
 local s=pending:getStatus()
 if cancelling then
  if s.state=='running' then assert(mbm.getTimeRun()-started<30);return end
  assert(s.state=='cancelled',s.error or s.state);assert(not pending:takeResult())
  print('INTERIOR CANCEL / NO PARTIAL RESULT OK');mbm.quit();return
 end
 if s.state=='completed' then
  local a,r=pending:takeResult();assert(a,r);assert(a:check());print('INTERIOR ASYNC SNAPSHOT OK')
  pending=assert(mbm.startImageMesh('/tmp/ime_interior.png',{heightSource='curved',curvedInterior=true,
   columns=64,rows=64,curvedNodes={{{x=.5,y=.5},parent=0,role='target',thickness=8}}}))
  pending:cancel();cancelling=true;started=mbm.getTimeRun()
 elseif s.state~='running' or mbm.getTimeRun()-started>30 then
  print('INTERIOR FAIL '..tostring(s.error or s.state));pending:cancel();mbm.quit()
 end
end
