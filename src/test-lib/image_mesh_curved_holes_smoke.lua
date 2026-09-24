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
local function circle(x,y,r)
 local points={};for i=0,15 do local a=i*math.pi/8;points[#points+1]={x=x+r*math.cos(a),y=y+r*math.sin(a)} end
 return points
end
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
  local area,volume,areaCorrection=0,0,0
  local function key(p) return string.format('%.5f,%.5f,%.5f',p.x+0.,p.y+0.,p.z+0.) end
  for _,p in ipairs(vertices) do close(p.nx*p.nx+p.ny*p.ny+p.nz*p.nz,1,.001) end
  for i=1,#indices,3 do
   local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
   if a.z<0 and b.z<0 and c.z<0 then
    -- This Lua build uses float32. Compensated accumulation avoids losing area
    -- when a regularized triangulation contributes thousands of small faces.
    local term=math.abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))/2-areaCorrection
    local sum=area+term;areaCorrection=(sum-area)-term;area=sum
   end
   volume=volume+(a.x*(b.y*c.z-b.z*c.y)+a.y*(b.z*c.x-b.x*c.z)+a.z*(b.x*c.y-b.y*c.x))/6
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
 local path='/tmp/ime_curved_holes.png';local pixels={};for i=1,65*65*3 do pixels[i]=150 end
 assert(mbm.createTexture(pixels,65,65,3,'ime_curved_holes',path))
 local o={heightSource='curved',width=100,height=100,curvedEdge=1,columns=8,rows=8,heightTolerance=.02}
 local function verify(name,holes,probes)
  for _,n in ipairs(o.curvedNodes or {}) do n.parent=n.parent or 0;n.role=n.role or "target" end
  o.holes=nil;o.curvedSimplify=false
  local original,originalReport=mbm.generateImageMesh(path,o);assert(original,originalReport)
  local before=inspect(original,originalReport,o)
  o.holes=holes
  for _,simplify in ipairs{false,true} do
   o.curvedSimplify=simplify
   local asset,report=mbm.generateImageMesh(path,o);assert(asset,name..': '..tostring(report));assert(asset:check())
   local after=inspect(asset,report,o)
   for _,p in ipairs(probes) do assert(after(p[1],p[2])==-math.huge,name..': face covers hole') end
   for _,p in ipairs{{.04,.2},{.95,.7},{.25,.95},{.75,.03}} do
    local expected=before(p[1],p[2]);if expected~=-math.huge then close(after(p[1],p[2]),expected,.4) end
   end
   if not o.curvedSymmetric and not o.shape then local _,_,back=after(.04,.2);close(back,o.curvedNodes and 0 or .5) end
   assert(asset:save('/tmp/ime_curved_holes_native.msh',false,false,true))
  end
  print('CURVED HOLES '..name..' / WATERTIGHT / SIMPLIFY OK')
 end
 o.curvedSymmetric=true;o.curvedRadius=15
 verify('LEGACY PLATEAU',{circle(.5,.5,.08)},{{.5,.5}})
 o.curvedRadius=0
 verify('REMOVED LEGACY PEAK',{rect(.42,.42,.58,.58)},{{.5,.5}})
 o.curvedNodes={rect(.3,.3,.7,.7)};o.curvedNodes[1].thickness=8
 verify('CROSSING TARGET',{rect(.22,.4,.45,.6)},{{.35,.5}})
 o.curvedNodes[2]=rect(.4,.4,.6,.6);o.curvedNodes[2].parent=1;o.curvedNodes[2].thickness=3
 verify('NESTED TARGETS',{circle(.5,.5,.08),rect(.1,.3,.2,.6)},{{.5,.5},{.15,.45}})
 o.curvedNodes={{{x=.5,y=.1},{x=.5,y=.9},thickness=8,profile='bezier',bezierPoints=4,bezier1=1,bezier2=1,bezier3=0,bezier4=0}}
 verify('BEZIER LINE',{rect(.4,.4,.6,.6)},{{.5,.5}})
 o.curvedSymmetric=false
 verify('FLAT BACK',{rect(.4,.4,.6,.6)},{{.5,.5}})
 o.curvedEdge=8;o.curvedNodes={rect(.2,.2,.8,.8),{{x=.5,y=.5},parent=1,role='target',thickness=1}}
 o.curvedNodes[1].role='region'
 verify('CROSSING LOCAL REGION / DESCENDING',{rect(.12,.4,.25,.6),circle(.5,.5,.08)},{{.18,.5},{.5,.5}})
 o.curvedNodes={};o.curvedEdge=4
 verify('FLAT EMPTY HIERARCHY',{circle(.3,.5,.1),circle(.7,.5,.1)},{{.3,.5},{.7,.5}})
 local validHoles=o.holes
 local many={};for y=0,3 do for x=0,3 do local a,b=.09+x*.23,.09+y*.23;many[#many+1]=rect(a,b,a+.08,b+.08) end end
 verify('SIXTEEN HOLES',many,{{.13,.13},{.82,.82}})
 verify('CONCAVE HOLE',{{{x=.2,y=.2},{x=.6,y=.2},{x=.6,y=.3},{x=.3,y=.3},{x=.3,y=.6},{x=.2,y=.6}}},{{.25,.25},{.25,.5}})
 o.shape='ellipse';verify('ELLIPSE OUTLINE',{circle(.5,.5,.1)},{{.5,.5}})
 o.shape='polygon';o.contour={{x=0,y=0},{x=1,y=0},{x=1,y=1},{x=.65,y=1},{x=.5,y=.85},{x=.35,y=1},{x=0,y=1}}
 verify('CONCAVE OUTLINE',{circle(.5,.5,.1)},{{.5,.5}})
 o.shape=nil;o.contour=nil;o.holes=validHoles
 for _,side in ipairs{'edge','color','repeat','band'} do
  o.sideMode=side;o.sideInset=1
  local a,r=mbm.generateImageMesh(path,o);assert(a,r);inspect(a,r,o)
 end
 o.sideMode='edge'
 print('CURVED HOLES SIDE MODES / EULER / AREA / VOLUME OK')
 for _,holes in ipairs{{rect(0,.2,.3,.4)},{rect(.2,.2,.5,.5),rect(.3,.3,.6,.6)},
  {{{x=.3,y=.3},{x=.6,y=.6},{x=.3,y=.6},{x=.6,y=.3}}}} do
  o.holes=holes;local a,e=mbm.generateImageMesh(path,o);assert(not a and e:lower():find('hole'),e)
 end
 o.holes=validHoles
 local map='/tmp/ime_curved_holes_map.png';local ok,e=mbm.generateImageMeshMap(path,o,map);assert(ok,e)
 local data,w,h=mbm.readImagePixels(map);assert(w==65 and h==65)
 -- RGBA map is transparent inside holes, opaque over the remaining surface.
 assert(data:byte((32*w+19)*4+4)==0 and data:byte((32*w+45)*4+4)==0)
 assert(data:byte((32*w+32)*4+4)==255)
 o.maxVertices=50
 local limited,reason=mbm.generateImageMesh(path,o);assert(not limited and reason:find('budget'),reason)
 o.maxVertices=nil
 print('CURVED HOLES VALIDATION / MAP / BUDGET OK')
 pending=assert(mbm.startImageMesh(path,o));o.holes[1][1].x=0;o.curvedEdge=999
 started=mbm.getTimeRun()
end
function onInitScene()
 local ok,e=xpcall(run,debug.traceback)
 if not ok then print('CURVED HOLES FAIL '..tostring(e));mbm.quit() end
end
function onLoop()
 if not pending then mbm.quit();return end
 local s=pending:getStatus()
 if cancelling then
  if s.state=='running' then assert(mbm.getTimeRun()-started<20);return end
  assert(s.state=='cancelled',s.error or s.state);assert(not pending:takeResult())
  print('CURVED HOLES CANCEL / NO PARTIAL RESULT OK');mbm.quit();return
 end
 if s.state=='completed' then
  local a,r=pending:takeResult();assert(a,r);assert(a:check());close(r.maxHeight,0)
  inspect(a,r,{width=100,height=100,holes={circle(.3,.5,.1),circle(.7,.5,.1)}})
  print('CURVED HOLES ASYNC SNAPSHOT OK')
  pending=assert(mbm.startImageMesh('/tmp/ime_curved_holes.png',{
   heightSource='curved',curvedRadius=15,columns=32,rows=32,holes={circle(.5,.5,.08)}}))
  pending:cancel();cancelling=true;started=mbm.getTimeRun()
 elseif s.state~='running' or mbm.getTimeRun()-started>20 then
  print('CURVED HOLES FAIL '..tostring(s.error or s.state));pending:cancel();mbm.quit()
 end
end
