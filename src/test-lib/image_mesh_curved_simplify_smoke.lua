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


local pending,started,cancelling
local function rect(parent,role,x0,y0,x1,y1,h)
 return {{x=x0,y=y0},{x=x1,y=y0},{x=x1,y=y1},{x=x0,y=y1},parent=parent,role=role,thickness=h}
end
local function close(a,b,e) assert(math.abs(a-b)<(e or .001),tostring(a)..' != '..tostring(b)) end
local function inspect(mesh,report,o)
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
  return span,vertices
end

local function run()
 local path='/tmp/ime_curved_simplify.png';local pixels={};for i=1,65*65*3 do pixels[i]=150 end
 assert(mbm.createTexture(pixels,65,65,3,'ime_curved_simplify',path))
 local o={heightSource='curved',width=100,height=100,curvedEdge=1,columns=12,rows=12,heightTolerance=.015,
   curvedSimplifyRatio=.5,curvedSimplifyError=.01}
 local function verify(name)
  o.curvedSimplify=false
  local dense,dr=mbm.generateImageMesh(path,o);assert(dense,dr)
  local before,dv=inspect(dense,dr,o)
  o.curvedSimplify=true
  local mesh,r=mbm.generateImageMesh(path,o);assert(mesh,r);assert(mesh:check())
  local after,sv=inspect(mesh,r,o)
  assert(r.curvedSourceTriangles>=r.curvedResultTriangles)
  assert(r.curvedMaximumError<=o.curvedSimplifyError+1e-7)
  local points={}
  for _,p in ipairs(sv) do points[string.format('%.6f,%.6f,%.6f',p.x,p.y,p.z)]=true end
  -- Every boundary vertex and every authored target endpoint survives exactly.
  for _,p in ipairs(dv) do
   local boundary=math.abs(p.x)==50 or math.abs(p.y)==50
   local u,v=p.x/o.width+.5,.5-p.y/o.height
   local protected=boundary
   if not o.curvedNodes and o.curvedRadius>0 then
    protected=protected or math.abs(math.sqrt(p.x*p.x+p.y*p.y)-o.curvedRadius)<.0001
   end
   for _,n in ipairs(o.curvedNodes or {}) do
    for i=1,(#n>2 and #n or 1) do
     local a,b=n[i],n[i%#n+1];local dx,dy=b.x-a.x,b.y-a.y;local d=dx*dx+dy*dy
     local t=d>0 and math.max(0,math.min(1,((u-a.x)*dx+(v-a.y)*dy)/d)) or 0
     protected=protected or math.sqrt((u-a.x-t*dx)^2+(v-a.y-t*dy)^2)<2e-7
    end
   end
   if protected then assert(points[string.format('%.6f,%.6f,%.6f',p.x,p.y,p.z)],'lost protected vertex') end
  end
  for _,n in ipairs(o.curvedNodes or {}) do for _,p in ipairs(n) do
   close(after(p.x,p.y),before(p.x,p.y),.0001)
  end end
  -- Independent mesh interpolation, including flat backs and wave profiles.
  for y=1,19 do for x=1,19 do close(after(x/20,y/20),before(x/20,y/20),7*o.curvedSimplifyError+.0001) end end
  print(name..' '..r.curvedSourceTriangles..' -> '..r.curvedResultTriangles..' bound '..r.curvedMaximumError)
  return r,mesh
 end
 o.curvedRadius=15
 local r=verify('LEGACY PLATEAU');assert(r.curvedResultTriangles<r.curvedSourceTriangles)
 o.curvedRadius=0
 r=verify('PROTECTED LEGACY PEAK');assert(not r.curvedTargetReached)
 o.curvedNodes={rect(0,'target',.3,.3,.7,.7,8),rect(1,'target',.4,.4,.6,.6,5)}
 r=verify('NESTED PLATEAUS');assert(r.curvedResultTriangles<r.curvedSourceTriangles)
 o.curvedNodes={{{x=.5,y=.15},{x=.5,y=.85},parent=0,role='target',thickness=8,
  profile='bezier',bezierPoints=4,bezier1=1,bezier2=1,bezier3=0,bezier4=0}}
 verify('BEZIER WAVE LINE')
 o.curvedSymmetric=false;verify('FLAT BACK')
 o.curvedSymmetric=true;o.curvedNodes={rect(0,'region',.1,.2,.4,.8,1),
  {{x=.25,y=.5},parent=1,role='target',thickness=8},rect(0,'region',.6,.2,.9,.8,1),
  {{x=.75,y=.5},parent=3,role='target',thickness=5}}
 verify('INDEPENDENT REGIONS')
 o.curvedSimplifyRatio=.01;o.curvedSimplifyError=.0001
 r=verify('CONSERVATIVE STOP');assert(not r.curvedTargetReached)
 o.curvedSimplifyRatio=1;r=verify('NO REDUCTION');assert(r.curvedSourceTriangles==r.curvedResultTriangles and r.curvedTargetReached)
 o.curvedSimplifyRatio=.5;o.curvedSimplifyError=.01
 for _,bad in ipairs{0,-1,1.1,math.huge} do
  o.curvedSimplifyRatio=bad;local a,e=mbm.generateImageMesh(path,o);assert(not a and e:find('simplification'),e)
 end
 o.curvedSimplifyRatio=.5
 for _,bad in ipairs{0,-1,.3,math.huge} do
  o.curvedSimplifyError=bad;local a,e=mbm.generateImageMesh(path,o);assert(not a and e:find('simplification'),e)
 end
 o.curvedSimplifyError=.01
 local function map(enabled,name)
  o.curvedSimplify=enabled;local file='/tmp/ime_simplify_map_'..name..'.png'
  local ok,e=mbm.generateImageMeshMap(path,o,file);assert(ok,e);return mbm.readImagePixels(file)
 end
 assert(map(false,'before')==map(true,'after'),'simplification changed the analytic map')
 print('CURVED SIMPLIFY MAP UNCHANGED OK')
 pending=assert(mbm.startImageMesh(path,o));o.curvedSimplify=false;o.curvedSimplifyRatio=1
 started=mbm.getTimeRun()
 print('CURVED SIMPLIFY SYNC OK')
end
function onInitScene()
 local ok,err=xpcall(run,debug.traceback)
 if not ok then print('CURVED SIMPLIFY FAIL '..tostring(err));mbm.quit() end
end
function onLoop()
 if not pending then mbm.quit();return end
 local s=pending:getStatus()
 if cancelling then
  if s.state=='running' then assert(mbm.getTimeRun()-started<20);return end
  assert(s.state=='cancelled',s.error or s.state);assert(not pending:takeResult())
  print('CURVED SIMPLIFY CANCEL / NO PARTIAL RESULT OK');mbm.quit();return
 end
 if s.state=='completed' then
  local a,r=pending:takeResult();assert(a,r);assert(r.curvedResultTriangles<r.curvedSourceTriangles)
  assert(a:save('/tmp/ime_curved_simplified.msh',false,false,true))
  print('CURVED SIMPLIFY ASYNC SNAPSHOT / EXPORT OK')
  pending=assert(mbm.startImageMesh('/tmp/ime_curved_simplify.png',{
   heightSource='curved',curvedRadius=20,columns=64,rows=64,curvedSimplify=true,
   curvedSimplifyRatio=.01,curvedSimplifyError=.01}))
  pending:cancel();cancelling=true;started=mbm.getTimeRun()
 elseif s.state~='running' or mbm.getTimeRun()-started>20 then
  print('CURVED SIMPLIFY FAIL '..tostring(s.error or s.state));pending:cancel();mbm.quit()
 end
end
