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
 local path='/tmp/ime_curved_quality.png';local pixels={};for i=1,129*129*3 do pixels[i]=150 end
 assert(mbm.createTexture(pixels,129,129,3,'ime_quality',path))
 local contour,target,hole={},{},{}
 for i=0,47 do local a=i*math.pi/24;local r=i%2==0 and .49 or .39;contour[#contour+1]={x=.5+r*math.cos(a),y=.5+r*math.sin(a)} end
 for i=0,31 do local a=i*math.pi/16;target[#target+1]={x=.5+.2*math.cos(a),y=.5+.2*math.sin(a)} end
 for i=0,15 do local a=i*math.pi/8;hole[#hole+1]={x=.5+.025*math.cos(a),y=.5+.025*math.sin(a)} end
 target.parent=0;target.role='target';target.thickness=30;target.profile='bezier';target.bezierPoints=4
 target.bezier1=0;target.bezier2=0;target.bezier3=.1;target.bezier4=.4
 local inner=rect(.44,.44,.56,.56);inner.parent=1;inner.role='target';inner.thickness=10
 local o={shape='polygon',contour=contour,holes={hole},heightSource='curved',curvedEdge=1,curvedNodes={target,inner},
  width=100,height=100,columns=24,rows=24,heightTolerance=.03,curvedSimplify=false}
 local asset,report=mbm.generateImageMesh(path,o);assert(asset,report);assert(asset:check())
 local span,vs=inspect(asset,report,o);local _,ix=Asset.geometry(asset)
 for _,n in ipairs{target,inner} do
  for i,a in ipairs(n) do local b=n[i%#n+1];close(span(a.x,a.y),n.thickness,.003);close(span((a.x+b.x)/2,(a.y+b.y)/2),n.thickness,.003) end
 end
 local thin,front=0,0;local sums,plateaus={},{}
 for i=1,#ix,3 do
  local ids={ix[i],ix[i+1],ix[i+2]};local a,b,c=vs[ids[1]],vs[ids[2]],vs[ids[3]]
  if a.z<0 and b.z<0 and c.z<0 then
   front=front+1
   local longest=0
   for j=1,3 do local u,v=vs[ids[j]],vs[ids[j%3+1]];longest=math.max(longest,(u.x-v.x)^2+(u.y-v.y)^2) end
   local quality=math.abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))/2/longest
   if quality<.001 then thin=thin+1 end
   local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z;local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
   local nx,ny,nz=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx;local length=math.sqrt(nx*nx+ny*ny+nz*nz)
   for j=1,3 do
    local id=ids[j];local p,q,r=vs[id],vs[ids[j%3+1]],vs[ids[(j+1)%3+1]]
    local dot=(q.x-p.x)*(r.x-p.x)+(q.y-p.y)*(r.y-p.y)+(q.z-p.z)*(r.z-p.z)
    local weight=math.atan(length,dot);local sum=sums[id] or {0,0,0};sums[id]=sum
    sum[1]=sum[1]+weight*nx/length;sum[2]=sum[2]+weight*ny/length;sum[3]=sum[3]+weight*nz/length
    if math.max(a.z,b.z,c.z)-math.min(a.z,b.z,c.z)<.00002 or -2*p.z<1.003 or -2*p.z>29.997 then plateaus[id]=true end
   end
  end
 end
 print('QUALITY TRIANGLES '..front..' THIN '..thin)
 assert(thin/front<.01,'curved refinement retains too many thin triangles')
 local checked=0
 for id,sum in pairs(sums) do if not plateaus[id] then
  local length=math.sqrt(sum[1]^2+sum[2]^2+sum[3]^2);local v=vs[id]
  close(v.nx,sum[1]/length,.001);close(v.ny,sum[2]/length,.001);close(v.nz,sum[3]/length,.001);checked=checked+1
 end end
 assert(checked>100)
 assert(asset:save('/tmp/ime_curved_quality.msh',false,false,true))
 print('CURVED QUALITY / TARGET EDGE HEIGHTS / HOLE / MANIFOLD / ANGLE NORMALS / EXPORT OK')
end
function onInitScene()
 local ok,e=xpcall(run,debug.traceback)
 if not ok then print('CURVED QUALITY FAIL '..tostring(e)) end
 mbm.quit()
end
function onLoop() mbm.quit() end
