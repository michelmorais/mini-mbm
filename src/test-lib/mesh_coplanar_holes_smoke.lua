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
local function create(v,idx)
 local d=meshDebug:new();d:setType('mesh');d:setModeFrontFace('CCW');d:addFrame(3);d:addSubSet(1)
 assert(d:addVertex(1,1,v));assert(d:addIndex(1,1,idx));d:addAnim('Static',1,1,1,0)
 return d
end
local function key(v) return string.format('%.9g/%.9g/%.9g',v.x,v.y,v.z) end
local function orient(a,b,c) return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x) end
local function edges(v,idx)
 local e={}
 for i=1,#idx,3 do for k=0,2 do
  local a,b=key(v[idx[i+k]]),key(v[idx[i+(k+1)%3]])
  local id=a<b and a..':'..b or b..':'..a
  local x=e[id] or {count=0,a=a,b=b};e[id]=x;x.count=x.count+1
  if x.count==2 then assert(a==x.b and b==x.a,'orientation mismatch') end
  assert(x.count<=2,'nonmanifold output')
 end end
 local boundary={};local n=0
 for _,x in pairs(e) do if x.count==1 then boundary[x.a..'->'..x.b]=true;n=n+1 end end
 return boundary,n
end
local function coverage(v,idx,p)
 local count=0
 for i=1,#idx,3 do
  local a,b,c=v[idx[i]],v[idx[i+1]],v[idx[i+2]]
  local x,y,z=orient(a,b,p),orient(b,c,p),orient(c,a,p)
  if (x>0 and y>0 and z>0) or (x<0 and y<0 and z<0) then count=count+1 end
 end
 return count
end
local function validate(d,source,indices,expected,holes)
 local v=d:getVertex(1,1,1,d:getTotalVertex(1,1));local idx=d:getIndex(1,1)
 assert(#idx==expected*3,'unexpected count '..#idx/3)
 local before,b=edges(source,indices);local after,n=edges(v,idx)
 assert(b==n and expected==b+2*holes-2,'Euler count')
 for e in pairs(before) do assert(after[e],'lost boundary '..e) end
 local originals={};for _,p in ipairs(source) do originals[key(p)]=p end
 for _,p in ipairs(v) do local q=assert(originals[key(p)],'new vertex')
  for _,f in ipairs{'u','v','nx','ny','nz'} do assert(p[f]==q[f],'attribute changed') end
 end
 local area0,area1=0,0
 for i=1,#indices,3 do area0=area0+orient(source[indices[i]],source[indices[i+1]],source[indices[i+2]]) end
 for i=1,#idx,3 do local a,b,c=v[idx[i]],v[idx[i+1]],v[idx[i+2]]
  local area=orient(a,b,c);assert(area*area0>0,'degenerate/flipped');area1=area1+area
  -- No retained vertex may lie inside an unrelated output edge (T-junction).
  for _,p in ipairs(v) do for _,e in ipairs{{a,b},{b,c},{c,a}} do
   if orient(e[1],e[2],p)==0 then
    local dot=(p.x-e[1].x)*(p.x-e[2].x)+(p.y-e[1].y)*(p.y-e[2].y)
    assert(dot>=0,'T-junction')
   end
  end end
 end
 local segments,seen={},{}
 for i=1,#idx,3 do for k=0,2 do local a,b=idx[i+k],idx[i+(k+1)%3]
  local id=math.min(a,b)..'/'..math.max(a,b)
  if not seen[id] then seen[id]=true;segments[#segments+1]={a,b} end
 end end
 for i,a in ipairs(segments) do for j=i+1,#segments do local b=segments[j]
  if a[1]~=b[1] and a[1]~=b[2] and a[2]~=b[1] and a[2]~=b[2] then
   local p,q,r,t=v[a[1]],v[a[2]],v[b[1]],v[b[2]]
   assert(not (orient(p,q,r)*orient(p,q,t)<0 and orient(r,t,p)*orient(r,t,q)<0),'crossing output edges')
  end
 end end
 assert(math.abs(area1-area0)<1e-7,'area changed')
 -- Independent dense footprint samples include every hole and the exterior notch.
 local minX,maxX,minY,maxY=math.huge,-math.huge,math.huge,-math.huge
 for _,p in ipairs(source) do minX=math.min(minX,p.x);maxX=math.max(maxX,p.x);minY=math.min(minY,p.y);maxY=math.max(maxY,p.y) end
 for y=0,31 do for x=0,31 do
  local p={x=minX+(maxX-minX)*(x+.173)/32,y=minY+(maxY-minY)*(y+.319)/32}
  local a,b=coverage(source,indices,p),coverage(v,idx,p)
  assert(a==b and b<=1,'coverage/overlap '..p.x..'/'..p.y)
 end end
 assert(d:check())
end
local function run()
 for _,fixture in ipairs{{'hole',1,40},{'multiple',2,42},{'inner_concave',1,40},{'outer_concave',1,40}} do
  for _,slope in ipairs{0,.5} do
   local kind,h,count=table.unpack(fixture)
   local d,v,idx=helper.grid(kind,slope)
   local r=assert(d:simplify(nil,nil,nil,true,0,'coplanar'))
   assert(r.planarRegions==1 and not r.qemRan,kind..' rejected '..r.planarHoles..' topology '..r.planarTopology)
   validate(d,v,idx,count,h)
   local signature=helper.signature(d)
   local repeatMesh=helper.grid(kind,slope);assert(repeatMesh:simplify(nil,nil,nil,true,0,'coplanar'))
   assert(signature==helper.signature(repeatMesh),'nondeterministic')
   assert(assert(d:simplify(nil,nil,nil,true,0,'coplanar')).unchanged)
   assert(signature==helper.signature(d),'no-op changed buffers')
   assert(d:save('/tmp/coplanar-hole.msh',false,false,true))
   local restored=meshDebug:new();assert(restored:load('/tmp/coplanar-hole.msh'))
   assert(signature==helper.signature(restored),'save/reload changed result')
   print('COPLANAR HOLE '..kind..' slope='..slope..' '..#idx/3 ..' -> '..count)
  end
 end
 for _,scale in ipairs{1/1024,1,1024} do
  local d,v,idx=helper.grid('hole',.5)
  for _,p in ipairs(v) do p.x=(p.x+16)*scale;p.y=(p.y-16)*scale;p.z=(p.z+2)*scale end
  for i=1,#idx,3 do idx[i+1],idx[i+2]=idx[i+2],idx[i+1] end
  d=create(v,idx)
  assert(assert(d:simplify(nil,nil,nil,true,0,'coplanar')).resultTriangleCount==40)
  validate(d,v,idx,40,1)
 end
 -- An unselected wall on the inner rim must preserve every original segment.
 local d,v,idx=helper.grid('hole');d:addSubSet(1)
 local wall,wi={},{}
 for y=3,5 do wall[#wall+1]={x=3,y=y,z=0,nx=1,ny=0,nz=0,u=0,v=y/8}
  wall[#wall+1]={x=3,y=y,z=-1,nx=1,ny=0,nz=0,u=1,v=y/8} end
 for y=0,1 do local a=y*2+1;for _,i in ipairs{a,a+1,a+3,a,a+3,a+2} do wi[#wi+1]=i end end
 assert(d:addVertex(1,2,wall));assert(d:addIndex(1,2,wi));d:setTexture(1,2,'#FF0000FF')
 assert(d:simplify(nil,1,1,true,0,'coplanar'));validate(d,v,idx,40,1)
 local after=d:getVertex(1,2,1,#wall);for i,p in ipairs(after) do for k,value in pairs(wall[i]) do assert(p[k]==value) end end
 local afterIndices=d:getIndex(1,2);for i,value in ipairs(wi) do assert(afterIndices[i]==value) end
 -- A separate sheet strictly inside the hole is certified disjoint.
 local obstacle=helper.grid('hole');obstacle:addSubSet(1)
 assert(obstacle:addVertex(1,2,{{x=3.5,y=3.5,z=0},{x=4.5,y=3.5,z=0},{x=4,y=4.5,z=0}}))
 assert(obstacle:addIndex(1,2,{1,2,3}))
 local accepted=assert(obstacle:simplify(nil,1,1,true,0,'coplanar'))
 assert(not accepted.unchanged and #obstacle:getIndex(1,1)==120)
 local _,nearVertices,nearIndices=helper.grid('hole')
 for _,p in ipairs(nearVertices) do p.z=1e-8*p.x*p.x end
 local near=create(nearVertices,nearIndices);local nearReport=assert(near:simplify(nil,nil,nil,true,0,'coplanar'))
 assert(nearReport.resultTriangleCount==40 and nearReport.planarMaximumError<=2e-6)
 for _,number in ipairs{16,17} do
  local v,idx,holes={},{},{}
  for h=0,number-1 do holes[(2+(h%4)*4)..'/'..(2+math.floor(h/4)*3)]=true end
  for y=0,20 do for x=0,20 do v[#v+1]={x=x,y=y,z=0,u=x/32,v=y/32,nx=0,ny=0,nz=2} end end
  for y=0,19 do for x=0,19 do if not holes[x..'/'..y] then
   local a=y*21+x+1;for _,i in ipairs{a,a+1,a+22,a,a+22,a+21} do idx[#idx+1]=i end
  end end end
  local d=create(v,idx);local before=helper.signature(d);local r=assert(d:simplify(nil,nil,nil,true,0,'coplanar'))
  if number==16 then validate(d,v,idx,174,16)
  else assert(r.unchanged and r.planarWorkLimit>0 and before==helper.signature(d)) end
 end
 local combined=helper.grid('hole');local combinedReport=assert(combined:simplify(.5,nil,nil,true,0,'coplanar_qem'))
 assert(combinedReport.resultTriangleCount==40 and not combinedReport.qemRan)
 local failed=helper.grid('hole');local before=helper.signature(failed)
 local failedReport,message=failed:simplify(.1,nil,nil,true,0,'coplanar_qem')
 assert(not failedReport and message and helper.signature(failed)==before,'combined failure published planar stage')
 local touching=helper.grid('touching');local original=helper.signature(touching)
 assert(assert(touching:simplify(nil,nil,nil,true,0,'coplanar')).unchanged)
 assert(original==helper.signature(touching),'touching rings changed')
 for _,attribute in ipairs{'u','nx'} do
  local d,v,idx=helper.grid('hole');v[11][attribute]=.3;d=create(v,idx)
  local original=helper.signature(d);local r=assert(d:simplify(nil,nil,nil,true,0,'coplanar'))
  assert(r.unchanged and r.planarAttributes>0 and original==helper.signature(d))
 end
 print('COPLANAR HOLES SMOKE OK')
end
function onInitScene()
 local ok,e=xpcall(run,debug.traceback);if not ok then print('COPLANAR HOLES FAIL '..tostring(e)) end
end
function onLoop() mbm.quit() end
