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
local function close(a,b) assert(math.abs(a-b)<.012, tostring(a)..' != '..tostring(b)) end
local function rectangle(x0,y0,x1,y1,h,t)
 return {{x=x0,y=y0},{x=x1,y=y0},{x=x1,y=y1},{x=x0,y=y1},height=h,transition=t or 0}
end
function onInitScene()
 local ok,err=xpcall(function()
  local path='/tmp/ime_areas_source.png';local pixels={}
  for y=0,64 do for x=0,64 do for c=1,3 do pixels[#pixels+1]=math.floor(x*255/64) end end end
  assert(mbm.createTexture(pixels,65,65,3,'ime_areas_source',path))
  local o={heightSource='manual',baseHeight=.5,lockBorder=false,columns=8,rows=8,
   followImage=true,heightTolerance=.08,width=100,height=100,depth=20,relief=8}
  local serial=0
  local function map()
   serial=serial+1;local output='/tmp/ime_areas_map_'..serial..'.png'
   local ok,message=mbm.generateImageMeshMap(path,o,output);assert(ok,message)
   local bytes,w,h=mbm.readImagePixels(output);assert(bytes and w==65 and h==65)
   return function(x,y) return bytes:byte((y*w+x)*4+1)/255 end
  end
  local m=map();close(m(10,10),.5);close(m(55,55),.5)
  o.twoLevels=true;o.invert=true;o.smoothPasses=4;m=map();close(m(10,10),.5)
  local a=rectangle(.25,.25,.75,.75,.8)
  local b=rectangle(.5,.5,.9,.9,.2)
  o.heightAreas={a,b};m=map();close(m(20,20),.8);close(m(40,40),.2);close(m(5,5),.5)
  o.heightAreas={b,a};m=map();close(m(40,40),.8)
  a.enabled=false;m=map();close(m(20,20),.5);close(m(40,40),.2);a.enabled=true
  o.heightAreas={a};a.transition=.125;m=map();close(m(16,24),.5);close(m(20,24),.65);close(m(24,24),.8)
  a.transition=0;o.heightEdits={{x=.5,y=.5,radius=.1,strength=1,height=.1,mode='flatten'}}
  m=map();close(m(32,32),.1);o.heightEdits=nil
  o.heightSource='image';o.twoLevels=false;o.invert=false;o.smoothPasses=0;m=map();close(m(20,20),.8) -- Image now shares final areas
  o.heightSource='mixed';m=map();close(m(20,20),.8);close(m(5,5),5/64)
  o.heightSource='manual';o.heightAreas={a,b}
  local mesh,report=mbm.generateImageMesh(path,o);assert(mesh,report)
  local vertices,indices=mesh:getVertex(1,1,1,report.vertices),mesh:getIndex(1,1)
  local function height(u,v)
   local x,y=(u-.5)*100,(.5-v)*100;local z=math.huge
   for i=1,#indices,3 do
    local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
    local det=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y)
    if math.abs(det)>1e-10 then
     local wa=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/det
     local wb=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/det
     if wa>=-1e-7 and wb>=-1e-7 and wa+wb<=1.0000001 then z=math.min(z,wa*a.z+wb*b.z+(1-wa-wb)*c.z) end
    end
   end
   return (-z-10)/8
  end
  close(height(.35,.35),.8);close(height(.6,.6),.2);close(height(.1,.1),.5)
  local function checkMesh(asset,stats)
   assert(asset:check());local vv=asset:getVertex(1,1,1,stats.vertices);local ii=asset:getIndex(1,1)
   local edges={}
   local function key(p) return string.format('%.5f,%.5f,%.5f',p.x+0.,p.y+0.,p.z+0.) end
   for _,p in ipairs(vv) do assert(math.abs(p.nx*p.nx+p.ny*p.ny+p.nz*p.nz-1)<.0001) end
   for i=1,#ii,3 do
    local a,b,c=vv[ii[i]],vv[ii[i+1]],vv[ii[i+2]]
    for _,pair in ipairs{{a,b},{b,c},{c,a}} do
     local ka,kb=key(pair[1]),key(pair[2]);assert(ka~=kb)
     local k=ka<kb and ka..'/'..kb or kb..'/'..ka
     local e=edges[k] or {n=0,b=0};edges[k]=e;e.n=e.n+1;e.b=e.b+(ka<kb and 1 or -1)
    end
   end
   for _,e in pairs(edges) do assert(e.n==2 and e.b==0,'open or inconsistent area geometry') end
  end
  checkMesh(mesh,report)
  o.twoLevels=true;o.grooveThreshold=.8;o.grooveTransition=.2
  local other,stats=mbm.generateImageMesh(path,o);assert(other,stats)
  assert(stats.vertices==report.vertices and stats.triangles==report.triangles,'hidden detection affects manual topology')
  for i,p in ipairs(other:getVertex(1,1,1,stats.vertices)) do
   local q=vertices[i];close(p.x,q.x);close(p.y,q.y);close(p.z,q.z);close(p.nx,q.nx);close(p.ny,q.ny);close(p.nz,q.nz)
  end
  assert(mesh:save('/tmp/ime_areas_native.msh',false,false,true))
  local hole=rectangle(.42,.42,.48,.48,0);hole.height=nil;hole.transition=nil;o.holes={hole}
  mesh,report=mbm.generateImageMesh(path,o);assert(mesh,report)
  checkMesh(mesh,report)
  o.lockBorder=true;o.borderWidth=.03;m=map();close(m(0,32),0);close(m(20,20),.8)
  local simplified,reason=other:simplify(.7,nil,1,true,0);assert(simplified,reason)
  assert(other:save('/tmp/ime_areas_simplified.msh',false,false,true))
  o.heightAreas={{{x=0,y=0},{x=1,y=1},{x=1,y=0},{x=0,y=1},height=.5,transition=0}}
  local bad,message=mbm.generateImageMeshMap(path,o,'/tmp/ime_area_invalid.png');assert(not bad and message:find('itself'))
  o.heightAreas={rectangle(0,0,1,1,2)};bad,message=mbm.generateImageMeshMap(path,o,'/tmp/ime_area_invalid.png');assert(not bad and message:find('height'))
  print('HEIGHT AREAS NATIVE MAP / ORDER / TRANSITION / PAINT / MODES / GEOMETRY / HOLES / VALIDATION OK')
 end,debug.traceback)
 if not ok then print('HEIGHT AREAS NATIVE FAIL '..tostring(err)) end
 mbm.quit()
end
function onLoop() mbm.quit() end
