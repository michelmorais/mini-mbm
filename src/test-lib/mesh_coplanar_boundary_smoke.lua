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


local helper={};assert(loadfile('src/test-lib/mesh_coplanar_smoke.lua'))(helper)
local function simplify(d,subset,mode,ratio)
 local r,e=d:simplify(ratio,subset,1,true,0,mode or 'coplanar',1e-7,.05,true)
 assert(r,e);return r
end
local function noTJunction(d)
 local vertices,edges={},{}
 for s=1,d:getTotalSubset(1) do
  local v=d:getVertex(1,s,1,d:getTotalVertex(1,s));local idx=d:getIndex(1,s)
  for _,p in ipairs(v) do vertices[#vertices+1]=p end
  for i=1,#idx,3 do for k=0,2 do edges[#edges+1]={v[idx[i+k]],v[idx[i+(k+1)%3]]} end end
 end
 for _,e in ipairs(edges) do local a,b=e[1],e[2];local x,y,z=b.x-a.x,b.y-a.y,b.z-a.z
  for _,p in ipairs(vertices) do local u,v,w=p.x-a.x,p.y-a.y,p.z-a.z
   if y*w-z*v==0 and z*u-x*w==0 and x*v-y*u==0 then
    assert(u*(p.x-b.x)+v*(p.y-b.y)+w*(p.z-b.z)>=0,'geometric T-junction')
   end
  end
 end
end
local function coverage(v,idx,p)
 local n=0
 for i=1,#idx,3 do local a,b,c=v[idx[i]],v[idx[i+1]],v[idx[i+2]]
  local function cross(a,b) return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x) end
  local x,y,z=cross(a,b),cross(b,c),cross(c,a)
  if (x>0 and y>0 and z>0) or (x<0 and y<0 and z<0) then n=n+1 end
 end
 return n
end
local function wall(d,varying)
 d:addSubSet(1);local v,idx={},{}
 for y=0,8 do
  v[#v+1]={x=0,y=y,z=0,u=0,v=y/8,nx=-1,ny=varying and y/100 or 0,nz=0}
  v[#v+1]={x=0,y=y,z=-2,u=1,v=y/8,nx=-1,ny=varying and y/100 or 0,nz=0}
 end
 for y=0,7 do local a=y*2+1;for _,i in ipairs{a,a+2,a+3,a,a+3,a+1} do idx[#idx+1]=i end end
 assert(d:addVertex(1,2,v));assert(d:addIndex(1,2,idx));d:setTexture(1,2,'#FF6060FF')
 return v,idx
end
local function run()
 for _,fixture in ipairs{{'flat',2},{'hole',8},{'multiple',14},{'inner_concave',10},{'outer_concave',10}} do
  local kind,count=table.unpack(fixture)
  for _,slope in ipairs{0,.5} do
   local d,source,sourceIndex=helper.grid(kind,slope)
   local r=simplify(d);assert(r.resultTriangleCount==count,kind..' count '..r.resultTriangleCount)
   assert(r.planarBoundaryRemovedVertices>0 and not r.planarBoundaryFallback and not r.qemRan)
   noTJunction(d)
   local v=d:getVertex(1,1,1,d:getTotalVertex(1,1));local idx=d:getIndex(1,1)
   for _,p in ipairs(v) do assert(p.nz==2 and p.nx==0 and p.ny==0 and p.u==p.x/8 and p.v==p.y/8) end
   for y=0,23 do for x=0,23 do local p={x=(x+.137)/3,y=(y+.291)/3}
    local n=coverage(v,idx,p);assert(n<=1 and n==coverage(source,sourceIndex,p),'coverage')
   end end
   local before=helper.signature(d);assert(simplify(d).unchanged and helper.signature(d)==before,kind)
   assert(d:save('/tmp/coplanar-boundary.msh',false,false,true))
   local loaded=meshDebug:new();assert(loaded:load('/tmp/coplanar-boundary.msh'));assert(helper.signature(loaded)==before)
   print('COPLANAR BOUNDARY '..kind..' '..#sourceIndex/3 ..' -> '..count)
  end
 end
 -- Legacy results remain exactly available with the option disabled.
 local old=helper.grid('flat');assert(assert(old:simplify(nil,nil,1,true,0,'coplanar')).resultTriangleCount==30)
 assert(simplify(old).resultTriangleCount==2,'already interior-reduced mesh did not reduce its border')
 local near=helper.grid('near');local r=simplify(near);assert(r.resultTriangleCount==30 and r.planarBoundaryRemovedVertices==0)
 for _,kind in ipairs{'uv','normal','touching'} do
  local d=helper.grid(kind);local before=helper.signature(d);assert(simplify(d).unchanged and helper.signature(d)==before,kind)
 end
 local bent=helper.grid('bent');r=simplify(bent);assert(r.resultTriangleCount>=16 and r.planarMaximumError==0);noTJunction(bent)
 -- Opposite attribute charts and material IDs retain physical seam values.
 local d=helper.grid('flat');d:addSubSet(1);local _,v,idx=helper.grid('flat')
 for _,p in ipairs(v) do p.x=p.x+8;p.u=p.u+2 end
 assert(d:addVertex(1,2,v));assert(d:addIndex(1,2,idx));d:setTexture(1,2,'#FF6060FF')
 r=simplify(d);assert(r.resultTriangleCount==4 and r.planarRegions==2);noTJunction(d)
 local seam=d:getVertex(1,2,1,d:getTotalVertex(1,2))
 for _,p in ipairs(seam) do assert(p.u==(p.x-8)/8+2 and p.v==p.y/8 and p.nz==2) end
 -- Coincident UV aliases inside the same subset must coordinate without welding.
 local _,left,leftIndex=helper.grid('flat');local _,right,rightIndex=helper.grid('flat')
 local offset=#left;for _,p in ipairs(right) do p.x=p.x+8;p.u=p.u+2;left[#left+1]=p end
 for _,i in ipairs(rightIndex) do leftIndex[#leftIndex+1]=offset+i end
 local charts=meshDebug:new();charts:setType('mesh');charts:setModeFrontFace('CCW');charts:addFrame(3);charts:addSubSet(1)
 assert(charts:addVertex(1,1,left));assert(charts:addIndex(1,1,leftIndex));charts:addAnim('Static',1,1,1,0)
 r=simplify(charts);assert(r.resultTriangleCount==4 and r.resultVertexCount==8);noTJunction(charts)
 local aliases=charts:getVertex(1,1,1,8);local seamA,seamB=0,0
 for _,p in ipairs(aliases) do if p.x==8 then
  if p.u==1 then seamA=seamA+1 elseif p.u==2 then seamB=seamB+1 else error('UV seam welded') end
 end end
 assert(seamA==2 and seamB==2)
 -- A perpendicular planar wall participates in the same geometric decision.
 local joined=helper.grid('flat');wall(joined)
 r=simplify(joined);assert(r.resultTriangleCount==4,'cap/wall '..r.resultTriangleCount);noTJunction(joined)
 -- Unselected or attribute-ineligible walls lock all shared samples.
 for _,varying in ipairs{false,true} do
  local protected=helper.grid('flat');local source,sourceIndex=wall(protected,varying)
  local original=protected:getVertex(1,2,1,#source)
  local subset;if not varying then subset=1 end
  r=simplify(protected,subset)
  assert(r.resultTriangleCount==25,'protected wall '..r.resultTriangleCount);noTJunction(protected)
  local after=protected:getVertex(1,2,1,#source)
  local function key(p) return p.x..'/'..p.y..'/'..p.z end
  local originals={};for _,p in ipairs(original) do originals[key(p)]=p end
  for _,p in ipairs(after) do local q=assert(originals[key(p)]);for k,value in pairs(q) do assert(p[k]==value) end end
  local function faces(v,idx)
   local set={};for i=1,#idx,3 do local a,b,c=key(v[idx[i]]),key(v[idx[i+1]]),key(v[idx[i+2]])
    local keys={a..':'..b..':'..c,b..':'..c..':'..a,c..':'..a..':'..b};table.sort(keys);set[keys[1]]=true
   end;return set
  end
  local afterIndex=protected:getIndex(1,2);local actual=faces(after,afterIndex)
  for f in pairs(faces(original,sourceIndex)) do assert(actual[f],'protected face changed') end
  if not varying then for i,value in ipairs(sourceIndex) do assert(afterIndex[i]==value) end end
 end
 local combined=helper.grid('flat');r=simplify(combined,nil,'coplanar_qem',.1);assert(r.resultTriangleCount==2 and not r.qemRan)
 local failed=helper.grid('flat');local before=helper.signature(failed)
 local result,message=failed:simplify(.001,nil,1,true,0,'coplanar_qem',1e-7,.05,true)
 assert(not result and message and helper.signature(failed)==before,'failed QEM published boundary reduction')
 print('COPLANAR BOUNDARY SMOKE OK')
end
function onInitScene()
 local ok,e=xpcall(run,debug.traceback);if not ok then print('COPLANAR BOUNDARY FAIL '..tostring(e)) end
end
function onLoop() mbm.quit() end
