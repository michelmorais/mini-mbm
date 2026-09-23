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
   for _,p in ipairs{a,b,c} do
    close(p.nx,nx/length,.0001);close(p.ny,ny/length,.0001);close(p.nz,nz/length,.0001)
   end
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
 local path='/tmp/ime_facets.png';local pixels={};for i=1,65*65*3 do pixels[i]=150 end
 assert(mbm.createTexture(pixels,65,65,3,'ime_facets',path))
 local o={shape='ellipse',heightSource='curved',width=100,height=100,curvedEdge=1,curvedTarget=8,
  curvedFaceted=true,curvedFacetSectors=8,curvedFacetRings=1,columns=8,rows=8}
 local function generate()
  local a,r=mbm.generateImageMesh(path,o);assert(a,r);assert(a:check())
  assert(r.vertices==r.triangles*3,'facet seams missing from vertex budget')
  assert(not r.curvedSourceTriangles,'facets were simplified')
  local span,v=inspect(a,r,o);return span,a,r,v
 end
 local span,asset,report=generate();assert(report.triangles==32);close(span(.5,.5),8)
 close(span(1,.5),1)
 o.curvedRadius=15;span,asset,report=generate();assert(report.triangles==64)
 close(span(.5,.5),8);close(span(.55,.5),8);close(span(.65,.5),8)
 o.curvedFacetRings=3;span,asset,report=generate();assert(report.triangles==128)
 local oldVertices,oldIndices=Asset.geometry(asset)
 o.columns=64;o.rows=64;o.heightTolerance=.001
 local _,same=generate();local sv,si=Asset.geometry(same);assert(#sv==#oldVertices and #si==#oldIndices)
 for i,v in ipairs(sv) do close(v.x,oldVertices[i].x);close(v.y,oldVertices[i].y);close(v.z,oldVertices[i].z) end
 o.columns=8;o.rows=8
 local ok,e=mbm.generateImageMeshMap(path,o,'/tmp/ime_facets_outline.png');assert(ok,e)
 local alpha,w=mbm.readImagePixels('/tmp/ime_facets_outline.png','alpha')
 assert(alpha:byte(46*w+60+1)==0,'map still uses the smooth ellipse outline')
 local baseline=span
 -- Hole clipping must preserve every remaining plane, not return to radial evaluation.
 o.holes={rect(.55,.42,.75,.58)};span=generate();assert(span(.65,.5)==-math.huge)
 for _,p in ipairs{{.2,.4},{.55,.65},{.77,.51},{.4,.2},{.51,.6}} do close(span(p[1],p[2]),baseline(p[1],p[2]),.0001) end
 o.curvedSimplify=true;generate()
 o.curvedSymmetric=false;span=generate();local _,_,back=span(.4,.4);close(back,.5)
 o.curvedSymmetric=true;o.holes=nil
 print('FACETS SECTORS / RINGS / TABLE / PLANES / HOLES / FLAT BACK / NORMALS OK')
 for _,side in ipairs{'edge','color','repeat','band'} do o.sideMode=side;o.sideInset=1;generate() end
 o.sideMode='edge'
 o.shape='rectangle';generate()
 o.shape='polygon';o.contour={{x=0,y=0},{x=.7,y=0},{x=1,y=.5},{x=.7,y=1},{x=0,y=1}};generate()
 o.curvedX=.45;o.curvedY=.47;span,asset,report=generate()
 assert(asset:save('/tmp/ime_facets_native.msh',false,false,true))
 local reloaded=meshDebug:new();assert(reloaded:load('/tmp/ime_facets_native.msh'));inspect(reloaded,report,o)
 o.curvedEdge=8;o.curvedTarget=1;span=generate();close(span(.45,.47),1)
 o.curvedEdge=4;o.curvedTarget=4;span=generate();close(span(.45,.47),4)
 o.curvedEdge=1;o.curvedTarget=8
 print('FACETS MATERIALS / CONVEX POLYGON / OFFSET CENTER / INVERT / EQUAL / EXPORT OK')
 local map='/tmp/ime_facets_map.png';local ok,e=mbm.generateImageMeshMap(path,o,map);assert(ok,e)
 span=generate();local rgba,w,h=mbm.readImagePixels(map)
 for y=20,44,4 do for x=20,44,4 do close(rgba:byte((y*w+x)*4+1)/255,span(x/64,y/64)/8,.0041) end end
 local old=o.curvedFacetSectors
 for _,n in ipairs{0,7,129} do o.curvedFacetSectors=n;local a,e=mbm.generateImageMesh(path,o);assert(not a and e:find('Faceting'),e) end
 o.curvedFacetSectors=old
 for _,n in ipairs{0,17} do o.curvedFacetRings=n;local a,e=mbm.generateImageMesh(path,o);assert(not a and e:find('Faceting'),e) end
 o.curvedFacetRings=3
 o.curvedNodes={};span=generate();close(span(.5,.5),1)
 o.curvedNodes=nil;local a,e
 local contour=o.contour;o.contour={{x=0,y=0},{x=1,y=0},{x=1,y=1},{x=.5,y=.8},{x=0,y=1}}
 a,e=mbm.generateImageMesh(path,o);assert(not a and e:find('convex'),e);o.contour=contour
 local shape,radius,rings=o.shape,o.curvedRadius,o.curvedFacetRings
 o.shape='ellipse';o.curvedRadius=0;o.curvedFacetRings=1;o.curvedX=.5;o.curvedY=.5
 o.maxVertices=80;a,e=mbm.generateImageMesh(path,o);assert(not a and e:find('side/facet seams'),e)
 o.maxVertices=nil;o.shape=shape;o.curvedRadius=radius;o.curvedFacetRings=rings;o.curvedX=.45;o.curvedY=.47
 print('FACETS MAP / VALIDATION / HARD EDGE BUDGET OK')
 -- Conforming rings must preserve every authored target height, even when the
 -- targets have different corner counts, centers and stored curve profiles.
 o.shape='rectangle';o.contour=nil;o.curvedFacetRings=2
 local first=rect(.2,.2,.8,.8);first.parent=0;first.role='target';first.thickness=5
 local second={{x=.43,y=.35},{x=.65,y=.4},{x=.55,y=.65},parent=1,role='target',thickness=8}
 o.curvedNodes={first,second};span=generate()
 for _,n in ipairs(o.curvedNodes) do for _,p in ipairs(n) do close(span(p.x,p.y),n.thickness,.001) end end
 close(span(.53,.46),8,.001)
 local baseline=span
 second.profile='bezier';second.bezier1=1;second.bezier2=0
 span=generate()
 for _,p in ipairs{{.1,.3},{.3,.45},{.7,.7},{.53,.46}} do close(span(p[1],p[2]),baseline(p[1],p[2]),.001) end
 o.holes={rect(.15,.42,.48,.55)};span=generate();assert(span(.3,.5)==-math.huge)
 for _,p in ipairs{{.1,.3},{.3,.65},{.7,.7},{.53,.46}} do close(span(p[1],p[2]),baseline(p[1],p[2]),.001) end
 o.holes=nil;o.curvedSymmetric=false;span=generate();local _,_,back=span(.53,.46);close(back,0,.001)
 o.curvedSymmetric=true
 o.curvedNodes[3]={{x=.54,y=.46},parent=2,role='target',thickness=3};span=generate();close(span(.54,.46),3,.001)
 local ok,e=mbm.generateImageMeshMap(path,o,map);assert(ok,e)
 local rgba,w,h=mbm.readImagePixels(map)
 for y=12,52,4 do for x=12,52,4 do close(rgba:byte((y*w+x)*4+1)/255,span(x/64,y/64)/8,.0041) end end
 local asset=select(2,generate());assert(asset:save('/tmp/ime_facet_chain.msh',false,false,true))
 o.curvedNodes[3]=nil
 second.role='region';a,e=mbm.generateImageMesh(path,o);assert(not a and e:find('local regions'),e);second.role='target'
 local saved=o.curvedNodes[2];o.curvedNodes[2]={{x=.4,y=.4},{x=.6,y=.6},parent=1,role='target',thickness=8}
 a,e=mbm.generateImageMesh(path,o);assert(not a and e:find('lines'),e);o.curvedNodes[2]=saved
 print('FACETS CHAIN / HEIGHTS / SHARED RINGS / HOLES / MAP / PEAK / FLAT BACK / UNSUPPORTED INPUT OK')
 pending=assert(mbm.startImageMesh(path,o));o.curvedFaceted=false;o.curvedFacetSectors=64
 started=mbm.getTimeRun()
end
function onInitScene()
 local ok,e=xpcall(run,debug.traceback)
 if not ok then print('FACETS FAIL '..tostring(e));mbm.quit() end
end
function onLoop()
 if not pending then mbm.quit();return end
 local s=pending:getStatus()
 if cancelling then
  if s.state=='running' then assert(mbm.getTimeRun()-started<20);return end
  assert(s.state=='cancelled',s.error or s.state);assert(not pending:takeResult())
  print('FACETS CANCEL / NO PARTIAL RESULT OK');mbm.quit();return
 end
 if s.state=='completed' then
  local a,r=pending:takeResult();assert(a,r);assert(r.vertices==r.triangles*3);assert(a:check())
  print('FACETS ASYNC SNAPSHOT OK')
  pending=assert(mbm.startImageMesh('/tmp/ime_facets.png',{
   heightSource='curved',shape='ellipse',curvedFaceted=true,curvedFacetSectors=64,curvedFacetRings=8}))
  pending:cancel();cancelling=true;started=mbm.getTimeRun()
 elseif s.state~='running' or mbm.getTimeRun()-started>20 then
  print('FACETS FAIL '..tostring(s.error or s.state));pending:cancel();mbm.quit()
 end
end
