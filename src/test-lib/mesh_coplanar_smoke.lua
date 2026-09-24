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


local function grid(kind,slope)
 local d=meshDebug:new();d:setType('mesh');d:setModeFrontFace('CCW');d:addFrame(3);d:addSubSet(1)
 local v,idx={},{}
 for y=0,8 do for x=0,8 do
  local z=(slope or 0)*x
  if kind=='angular' then z=z+.0005*x*x end
  if kind=='shallow' then z=z+1e-5*x*x end
  if kind=='near' then z=z+1e-8*x*x end
  if kind=='bent' then z=z+.01*x*x end
  v[#v+1]={x=x,y=y,z=z,u=x/8,v=y/8,nx=0,ny=0,nz=2}
 end end
 if kind=='uv' then v[41].u=v[41].u+.1 end
 if kind=='affine_normal' then for _,p in ipairs(v) do p.nx=p.x/8;p.ny=p.y/16 end end
 if kind=='normal' then v[41].nx=.2 end
 for y=0,7 do for x=0,7 do
  local cut=(kind=='hole' and x>=3 and x<=4 and y>=3 and y<=4)
   or (kind=='multiple' and ((x==2 and y==2) or (x==5 and y==5)))
   or (kind=='inner_concave' and ((y==3 and (x==3 or x==4)) or (x==3 and y==4)))
   or (kind=='touching' and ((x==3 and y==3) or (x==4 and y==4)))
   or (kind=='outer_concave' and x>=1 and x<=2 and y>=1 and y<=2)
  if not cut and not((kind=='concave' or kind=='outer_concave') and x>=4 and y>=4) then
   local a=y*9+x+1
   for _,i in ipairs{a,a+1,a+10,a,a+10,a+9} do idx[#idx+1]=i end
  end
 end end
 assert(d:addVertex(1,1,v));assert(d:addIndex(1,1,idx));d:addAnim('Static',1,1,1,0);assert(d:check())
 return d,v,idx
end
local function signature(d)
 local a={}
 for s=1,d:getTotalSubset(1) do
  for _,p in ipairs(d:getVertex(1,s,1,d:getTotalVertex(1,s))) do
   for _,k in ipairs{'x','y','z','u','v','nx','ny','nz'} do a[#a+1]=string.format('%.9g',p[k]) end
  end
  for _,i in ipairs(d:getIndex(1,s)) do a[#a+1]=tostring(i) end
 end
 return table.concat(a,',')
end
local function validate(d,expected)
 local v=d:getVertex(1,1,1,d:getTotalVertex(1,1));local idx=d:getIndex(1,1)
 assert(#idx/3==expected,'count '..(#idx/3)..' expected '..expected)
 local area=0;local edges={}
 for i=1,#idx,3 do
  local a,b,c=v[idx[i]],v[idx[i+1]],v[idx[i+2]]
  local cross=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)
  assert(cross>0,'degenerate/flipped');area=area+cross/2
  for k=0,2 do local x,y=idx[i+k],idx[i+(k+1)%3]
   local key=math.min(x,y)..'/'..math.max(x,y);edges[key]=(edges[key] or 0)+1
  end
 end
 local boundary=0;for _,n in pairs(edges) do assert(n<=2);if n==1 then boundary=boundary+1 end end
 assert(boundary==32,'boundary '..boundary)
 for _,p in ipairs(v) do assert(p.nz==2 and p.nx==0 and p.ny==0,'normal was changed')
  assert(math.abs(p.u-p.x/8)<1e-7 and math.abs(p.v-p.y/8)<1e-7,'UV changed')
 end
 return area
end
local function run()
 for _,slope in ipairs{0,.5} do
  local d=grid('flat',slope)
  local r,e=d:simplify(nil,nil,nil,true,0,'coplanar');assert(r,e)
  assert(r.planarRegions==1 and not r.qemRan and r.planarRemovedTriangles==98)
  assert(validate(d,30)==64);assert(d:check())
  local after=signature(d);r,e=d:simplify(nil,nil,nil,true,0,'coplanar');assert(r,e)
  assert(r.unchanged and signature(d)==after,'no-op changed source')
  assert(d:save('/tmp/mbm-coplanar-result.msh',false,false,true))
  local loaded=meshDebug:new();assert(loaded:load('/tmp/mbm-coplanar-result.msh'));assert(signature(loaded)==after)
 end
 for _,kind in ipairs{'uv','normal','bent'} do
  local d=grid(kind);local before=signature(d)
  local r,e=d:simplify(nil,nil,nil,true,0,'coplanar');assert(r,e)
  assert(r.unchanged and signature(d)==before,kind..' was changed')
 end
 local concave=grid('concave');local r,e=concave:simplify(nil,nil,nil,true,0,'coplanar');assert(r,e)
 assert(validate(concave,30)==48)
 local near=grid('near');r,e=near:simplify(nil,nil,nil,true,0,'coplanar');assert(r,e)
 assert(r.planarMaximumError<=2e-6)
 local strict=grid('shallow');local strictReport=assert(strict:simplify(nil,nil,nil,true,0,'coplanar'))
 local relaxed=grid('shallow');local relaxedReport=assert(relaxed:simplify(nil,nil,nil,true,0,'coplanar',1e-4))
 assert(relaxedReport.resultTriangleCount==30 and relaxedReport.resultTriangleCount<strictReport.resultTriangleCount)
 assert(relaxedReport.planarMaximumError>0 and relaxedReport.planarMaximumError<.002)
 assert(validate(relaxed,30)==64)
 local exact=grid('flat');assert(assert(exact:simplify(nil,nil,nil,true,0,'coplanar',0)).resultTriangleCount==30)
 for _,value in ipairs{-1,.011,math.huge,0/0} do
  local invalid=grid('flat');local signatureBefore=signature(invalid)
  local result,message=invalid:simplify(nil,nil,nil,true,0,'coplanar',value)
  assert(not result and message and signatureBefore==signature(invalid),'invalid tolerance changed mesh')
 end
 for _,kind in ipairs{'uv','normal','bent'} do
  local protected=grid(kind);local signatureBefore=signature(protected)
  assert(protected:simplify(nil,nil,nil,true,0,'coplanar',.01))
  assert(signatureBefore==signature(protected),'relaxed tolerance bypassed '..kind)
 end
 local angularStrict=grid('angular');local strictAngle=assert(angularStrict:simplify(nil,nil,nil,true,0,'coplanar',.01))
 local angularOutputGuard=grid('angular');assert(assert(angularOutputGuard:simplify(nil,nil,nil,true,0,'coplanar',.01,1)).unchanged,'replacement exceeded angular guard')
 local angularRelaxed=grid('angular');local relaxedAngle=assert(angularRelaxed:simplify(nil,nil,nil,true,0,'coplanar',.01,5))
 assert(relaxedAngle.resultTriangleCount==30 and strictAngle.resultTriangleCount>30,strictAngle.resultTriangleCount.." / "..relaxedAngle.resultTriangleCount)
 assert(relaxedAngle.planarMaximumError>0 and relaxedAngle.planarMaximumError<.06)
 assert(validate(angularRelaxed,30)==64)
 local distanceGuard=grid('angular');assert(assert(distanceGuard:simplify(nil,nil,nil,true,0,'coplanar',0,1)).resultTriangleCount>30)
 local exactAngle=grid('flat');assert(assert(exactAngle:simplify(nil,nil,nil,true,0,'coplanar',0,0)).resultTriangleCount==30)
 for _,value in ipairs{-1,5.1,math.huge,0/0} do
  local invalid=grid('flat');local original=signature(invalid)
  local result,message=invalid:simplify(nil,nil,nil,true,0,'coplanar',.01,value)
  assert(not result and message and signature(invalid)==original)
 end
 print('COPLANAR ANGLE OK '..strictAngle.resultTriangleCount..' -> '..relaxedAngle.resultTriangleCount)
 print('COPLANAR TOLERANCE OK '..strictReport.resultTriangleCount..' -> '..relaxedReport.resultTriangleCount)
 local combined=grid('flat');r,e=combined:simplify(.5,nil,nil,true,0,'coplanar_qem');assert(r,e)
 assert(r.resultTriangleCount==30 and not r.qemRan)
 local failed=grid('flat');local before=signature(failed)
 r,e=failed:simplify(.1,nil,nil,true,0,'coplanar_qem');assert(not r and e)
 assert(signature(failed)==before,'partial planar result committed')
 local legacy=grid('flat');r,e=legacy:simplify(.25);assert(r,e);assert(r.resultTriangleCount==32 and r.qemRan)
 local animated=grid('flat');assert(animated:copyFrameFrom(animated,1)==2)
 before=signature(animated);r,e=animated:simplify(nil,nil,nil,true,0,'coplanar');assert(r,e)
 assert(r.unchanged and r.planarSkipped and signature(animated)==before)

 -- Non-indexed exact attribute joining, both front-face windings.
 local ni,verts,indices=grid('flat');local flat={}
 for _,i in ipairs(indices) do flat[#flat+1]=verts[i] end
 local d=meshDebug:new();d:setType('mesh');d:setModeFrontFace('CW');d:addFrame(3);d:addSubSet(1)
 -- Reverse actual winding, independently of the culling flag.
 for i=1,#flat,3 do flat[i+1],flat[i+2]=flat[i+2],flat[i+1] end
 assert(d:addVertex(1,1,flat));d:addAnim('Static',1,1,1,0)
 r,e=d:simplify(nil,nil,nil,true,0,'coplanar');assert(r,e);assert(r.resultTriangleCount==30)
 -- Two adjacent material charts must retain their seam segmentation.
 local material,v0,i0=grid('flat');material:addSubSet(1)
 local v1={};for i,p in ipairs(v0) do v1[i]={x=p.x+8,y=p.y,z=p.z,u=p.u+2,v=p.v,nx=0,ny=0,nz=2} end
 assert(material:addVertex(1,2,v1));assert(material:addIndex(1,2,i0))
 material:setTexture(1,1,'#FF0000FF');material:setTexture(1,2,'#00FF00FF')
 r,e=material:simplify(nil,nil,nil,true,0,'coplanar');assert(r,e)
 assert(r.planarRegions==2 and r.resultTriangleCount==60 and material:getTotalSubset(1)==2)
 -- A wall sharing the original edge segments stays unchanged during subset-only work.
 local wall=grid('flat');wall:addSubSet(1);local w,wi={},{}
 for y=0,8 do w[#w+1]={x=0,y=y,z=0};w[#w+1]={x=0,y=y,z=1} end
 for y=0,7 do local a=2*y+1;for _,i in ipairs{a,a+2,a+3,a,a+3,a+1} do wi[#wi+1]=i end end
 assert(wall:addVertex(1,2,w));assert(wall:addIndex(1,2,wi))
 local beforeWall=wall:getVertex(1,2,1,#w)
 r,e=wall:simplify(nil,1,nil,true,0,'coplanar');assert(r,e);assert(r.planarRegions==1)
 assert(wall:getTotalIndex(1,2)==#wi)
 for i,p in ipairs(wall:getVertex(1,2,1,#w)) do
  for _,k in ipairs{'x','y','z','u','v','nx','ny','nz'} do assert(p[k]==beforeWall[i][k],'wall changed') end
 end
 -- An external contact at an otherwise removable interior vertex prevents reduction.
 local contact=grid('flat');contact:addSubSet(1)
 assert(contact:addVertex(1,2,{{x=4,y=4,z=0},{x=4,y=4,z=1},{x=5,y=4,z=1}}))
 assert(contact:addIndex(1,2,{1,2,3}));before=signature(contact)
 r,e=contact:simplify(nil,1,nil,true,0,'coplanar');assert(r,e)
 assert(r.unchanged and r.planarSurroundings>0 and signature(contact)==before)
 -- UV discontinuity inside one subset creates two separate charts.
 local seam=meshDebug:new();seam:setType('mesh');seam:setModeFrontFace('CCW');seam:addFrame(3);seam:addSubSet(1)
 local allv,alli={},{}
 for _,list in ipairs{v0,v1} do for _,p in ipairs(list) do allv[#allv+1]=p end end
 for _,i in ipairs(i0) do alli[#alli+1]=i end
 for _,i in ipairs(i0) do alli[#alli+1]=i+#v0 end
 assert(seam:addVertex(1,1,allv));assert(seam:addIndex(1,1,alli));seam:addAnim('Static',1,1,1,0)
 r,e=seam:simplify(nil,nil,nil,true,0,'coplanar');assert(r,e);assert(r.resultTriangleCount==60 and r.planarRegions==2)

 local mixed=grid('flat');mixed:addSubSet(1)
 local oct={{x=21,y=0,z=0},{x=19,y=0,z=0},{x=20,y=1,z=0},{x=20,y=-1,z=0},{x=20,y=0,z=1},{x=20,y=0,z=-1}}
 assert(mixed:addVertex(1,2,oct));assert(mixed:addIndex(1,2,{3,1,5,3,5,2,3,2,6,3,6,1,4,5,1,4,2,5,4,6,2,4,1,6}))
 r,e=mixed:simplify(.25,nil,nil,true,0,'coplanar_qem');assert(r,e)
 assert(r.qemRan and r.planarRemovedTriangles==98 and r.resultTriangleCount==34,'combined target is not original ratio')

 for _,scale in ipairs{1/1024,1024} do
  local _,v,idx=grid('flat')
  for _,p in ipairs(v) do p.x=p.x*scale+1024*scale;p.y=p.y*scale-1024*scale;p.z=p.x/4+p.y/2 end
  local transformed=meshDebug:new();transformed:setType('mesh');transformed:setModeFrontFace('CCW');transformed:addFrame(3);transformed:addSubSet(1)
  assert(transformed:addVertex(1,1,v));assert(transformed:addIndex(1,1,idx));transformed:addAnim('Static',1,1,1,0)
  r,e=transformed:simplify(nil,nil,nil,true,0,'coplanar');assert(r,e);assert(r.resultTriangleCount==30)
 end
 local fan=meshDebug:new();fan:setType('mesh');fan:setModeFrontFace('CCW');fan:addFrame(3);fan:addSubSet(1)
 local v={{x=0,y=0,z=0,u=0,v=0},{x=8,y=0,z=0,u=1,v=0},{x=8,y=8,z=0,u=1,v=1},{x=0,y=8,z=0,u=0,v=1},{x=4,y=4,z=0,u=.5,v=.5}}
 for _,p in ipairs(v) do p.nx=0;p.ny=0;p.nz=1 end
 assert(fan:addVertex(1,1,v));assert(fan:addIndex(1,1,{1,2,5,2,3,5,3,4,5,4,1,5}));fan:addAnim('Static',1,1,1,0)
 r,e=fan:simplify(nil,nil,nil,true,0,'coplanar');assert(r,e);assert(r.resultTriangleCount==2)
 local bad,v,idx=grid('flat');idx[#idx+1]=1;idx[#idx+1]=2;idx[#idx+1]=11
 assert(bad:addIndex(1,1,idx));before=signature(bad)
 r,e=bad:simplify(nil,nil,nil,true,0,'coplanar');assert(r,e);assert(r.unchanged and signature(bad)==before)
 -- Unused source storage can exceed the physical output contract; check exact compacted count.
 local _,largeVertices,smallIndices=grid('flat')
 for i=#largeVertices+1,65536 do largeVertices[i]={x=i,y=0,z=0,nx=0,ny=0,nz=2,u=0,v=0} end
 local large=meshDebug:new();large:setType('mesh');large:setModeFrontFace('CCW');large:addFrame(3);large:addSubSet(1)
 assert(large:addVertex(1,1,largeVertices));assert(large:addIndex(1,1,smallIndices));large:addAnim('Static',1,1,1,0)
 r,e=large:simplify(nil,nil,nil,true,0,'coplanar');assert(r,e)
 assert(r.sourceVertexCount==65536 and r.resultVertexCount==32 and large:check())
 print('COPLANAR SMOKE OK: grid inclined concave holes UV normals near bent target rollback noop frames save/reload')
end
local api=...
if type(api)=='table' then api.grid=grid;api.signature=signature;return end
function onInitScene()
 local ok,e=pcall(run);if not ok then print('COPLANAR SMOKE FAIL '..tostring(e)) end
end
function onLoop() mbm.quit() end
