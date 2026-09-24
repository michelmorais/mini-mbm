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

local function close(a,b,tolerance) assert(math.abs(a-b)<(tolerance or .0001),tostring(a)..' != '..tostring(b)) end
local function run()
 local path='/tmp/ime_curved_source.png';local pixels={}
 for y=0,64 do for x=0,64 do for c=1,3 do pixels[#pixels+1]=x*3 end end end
 assert(mbm.createTexture(pixels,65,65,3,'ime_curved_source',path))
 local o={heightSource='curved',width=100,height=100,curvedEdge=1,curvedTarget=8,
  curvedX=.5,curvedY=.5,curvedRadius=0,columns=8,rows=8,heightTolerance=.005}
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
 local span=generate();close(span(.5,.5),8);close(span(0,.5),1);close(span(.25,.5),4.5)
 local thickness,front,back=span(.5,.5);close(front,-4);close(back,4)
 o.curvedRadius=20;span=generate();close(span(.5,.5),8);close(span(.6,.5),8);close(span(.7,.5),8)
 close(span(.85,.5),4.5,.035)
 o.curvedEdge=8;o.curvedTarget=1;span=generate();close(span(.5,.5),1);close(span(0,.5),8);close(span(.85,.5),4.5,.035)
 o.curvedSymmetric=false;span=generate();thickness,front,back=span(.5,.5);close(thickness,1);close(back,.5)
 local _,_,outerBack=span(0,.5);close(outerBack,back)
 o.curvedEdge=3;o.curvedTarget=3;span=generate();close(span(.5,.5),3);close(span(0,.5),3)
 o.curvedEdge=1;o.curvedTarget=8;o.curvedSymmetric=true;o.curvedX=.4;o.height=60;o.curvedRadius=10
 span=generate();close(span(.4,.5),8);close(span(.4,.5+10/60),8,.04)
 -- Texture levels, manual edits, legacy back modes and border settings are inactive.
 o.heightEdits={{x=.4,y=.5,radius=.4,strength=1,height=0,mode='flatten'}}
 o.heightAreas={{{x=.1,y=.1},{x=.9,y=.1},{x=.9,y=.9},{x=.1,y=.9},height=0}}
 o.heightImage='/does/not/exist.png';o.invert=true;o.twoLevels=true;o.smoothPasses=4;o.backOpen=true
 span=generate();close(span(.4,.5),8)
 local output='/tmp/ime_curved_map.png';local ok,message=mbm.generateImageMeshMap(path,o,output);assert(ok,message)
 local bytes,w,h=mbm.readImagePixels(output);close(bytes:byte((32*w+26)*4+1)/255,1,.005)
 close(bytes:byte((32*w)*4+1)/255,1/8,.005)
 o.curvedX=0;local bad,err=mbm.generateImageMesh(path,o);assert(not bad and err:find('center'),err)
 o.curvedX=.5;o.curvedRadius=30;bad,err=mbm.generateImageMesh(path,o);assert(not bad and err:find('circle'),err)
 o.curvedRadius=0;o.holes={{{x=.1,y=.1},{x=.2,y=.1},{x=.2,y=.2},{x=.1,y=.2}}}
 bad,err=mbm.generateImageMesh(path,o);assert(bad,err);assert(bad:check());o.holes=nil
 o.maxVertices=10;bad,err=mbm.generateImageMesh(path,o);assert(not bad and err:find('budget'),err);o.maxVertices=65535
 o.curvedTarget=0;bad,err=mbm.generateImageMesh(path,o);assert(not bad,err);o.curvedTarget=8
 o.shape='ellipse';o.ellipseSegments=32;o.curvedRadius=10
 span=generate();close(span(.5,.5),8);close(span(1,.5),1,.001)
 -- A point inside the polygon may still be outside its visibility kernel.
 o.shape='polygon';o.curvedX=.15;o.curvedY=.5;o.curvedRadius=0
 o.contour={{x=0,y=0},{x=1,y=0},{x=1,y=1},{x=.7,y=1},{x=.7,y=.3},{x=.3,y=.3},{x=.3,y=1},{x=0,y=1}}
 bad,err=mbm.generateImageMesh(path,o);assert(not bad and err:find('center'),err)
 o.curvedX=.5;o.curvedY=.5
 -- Synthetic dented contour, independent of the user's local asset.
 o.width=100;o.height=100;o.curvedRadius=15;o.shape='polygon';o.contour={}
 for i=0,23 do local a=i*math.pi/12;local r=i%2==0 and .48 or .4;o.contour[#o.contour+1]={x=.5+r*math.cos(a),y=.5+r*math.sin(a)} end
 span=generate();close(span(.5,.5),8);close(span(.98,.5),1,.001)
 -- Regression: oblique rays through irregular contour vertices must not fall
 -- through a numerical gap between adjacent segments (the sharp.imesh contour).
 o.curvedRadius=25;o.columns=24;o.rows=24;o.heightTolerance=.03
 o.contour={
  {x=0x1.452dbap-7,y=0x1.8abf82p-2},
  {x=0x1.54cfeep-4,y=0x1.5bd8eap-2},
  {x=0x1.09c56p-4,y=0x1.e954dp-3},
  {x=0x1.533fb4p-3,y=0x1.9e4a42p-3},
  {x=0x1.660258p-3,y=0x1.c55fc2p-4},
  {x=0x1.1a2faep-2,y=0x1.eae508p-4},
  {x=0x1.3fb4f6p-2,y=0x1.326b18p-5},
  {x=0x1.a6e378p-2,y=0x1.09c56p-4},
  {x=0x1.e88cb4p-2,y=0x1.903848p-11},
  {x=0x1.19cbap-1,y=0x1.c88032p-5},
  {x=0x1.4d62e2p-1,y=0x1.38abf8p-6},
  {x=0x1.64d62ep-1,y=0x1.9fda7ap-4},
  {x=0x1.986d7p-1,y=0x1.9fda7ap-4},
  {x=0x1.a1cec2p-1,y=0x1.8b87ap-3},
  {x=0x1.d0b55ap-1,y=0x1.b10ce6p-3},
  {x=0x1.cc04bp-1,y=0x1.3fb4f6p-2},
  {x=0x1.f63aap-1,y=0x1.77fcep-2},
  {x=0x1.e377fcp-1,y=0x1.df2b62p-2},
  {x=0x1.ff9bf2p-1,y=0x1.151af8p-1},
  {x=0x1.dec754p-1,y=0x1.313eecp-1},
  {x=0x1.ecd94ep-1,y=0x1.64d62ep-1},
  {x=0x1.c2a35ep-1,y=0x1.7798d2p-1},
  {x=0x1.c2a35ep-1,y=0x1.ab3012p-1},
  {x=0x1.8f0c1ep-1,y=0x1.b49164p-1},
  {x=0x1.80fa24p-1,y=0x1.e377fcp-1},
  {x=0x1.4d62e2p-1,y=0x1.da16acp-1},
  {x=0x1.313eecp-1,y=0x1.ff9bf2p-1},
  {x=0x1.0bb9a6p-1,y=0x1.e828a6p-1},
  {x=0x1.c3076ep-2,y=0x1.ff9bf2p-1},
  {x=0x1.77fcep-2,y=0x1.dec754p-1},
  {x=0x1.10ce5ep-2,y=0x1.e828a6p-1},
  {x=0x1.e954dp-3,y=0x1.bdf2b6p-1},
  {x=0x1.2dba6ep-3,y=0x1.b9420ep-1},
  {x=0x1.083528p-3,y=0x1.85aaccp-1},
  {x=0x1.326b18p-5,y=0x1.72e828p-1},
  {x=0x1.09c56p-4,y=0x1.3f50e8p-1},
  {x=0x1.903848p-11,y=0x1.232cf2p-1},
  {x=0x1.903848p-11,y=0x1.151af8p-1},
  {x=0x1.7d75a4p-5,y=0x1.fb4f58p-2},
 }
 span=generate();close(span(.712534,.349555),1+7*23/24,.06)
 local job=assert(mbm.startImageMesh(path,o))
 return job,o,path
end
local task,started
function onInitScene()
 local ok,job=xpcall(run,debug.traceback)
 if not ok then print('CURVED NATIVE FAIL '..tostring(job));mbm.quit();return end
 task=job;started=mbm.getTimeRun()
end
function onLoop()
 if not task then mbm.quit();return end
 local s=task:getStatus()
 if s.state=='completed' then
  local mesh,report=task:takeResult();assert(mesh,report)
  assert(mesh:save('/tmp/ime_curved_native.msh',false,false,true))
  print('CURVED NATIVE GEOMETRY / PLATEAU / INVERT / METRIC / VALIDATION / MAP / ASYNC OK');mbm.quit()
 elseif s.state~='running' or mbm.getTimeRun()-started>15 then
  print('CURVED NATIVE FAIL '..tostring(s.error or s.state));task:cancel();mbm.quit()
 end
end
