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

local function close(a,b,tolerance) assert(math.abs(a-b)<(tolerance or .06),tostring(a)..' != '..tostring(b)) end
local function rect(parent,role,x0,y0,x1,y1,h)
 return {{x=x0,y=y0},{x=x1,y=y0},{x=x1,y=y1},{x=x0,y=y1},parent=parent,role=role,thickness=h or 8}
end
local function line(parent,x,y0,y1,h)
 return {{x=x,y=y0},{x=x,y=y1},parent=parent,role='target',thickness=h}
end
local function circle(parent,r,h)
 local n={parent=parent,role='target',thickness=h}
 for i=0,31 do local a=i*math.pi/16;n[#n+1]={x=.5+r*math.cos(a),y=.5+r*math.sin(a)} end
 return n
end
local pending,started,sampleResult
local function run()
 local path='/tmp/ime_profiles.png';local pixels={};for i=1,65*65*3 do pixels[i]=150 end
 assert(mbm.createTexture(pixels,65,65,3,'ime_hierarchy_source',path))
 local o={heightSource='curved',width=100,height=100,curvedEdge=1,columns=8,rows=8,heightTolerance=.005,
  curvedNodes={line(0,.5,.125,.875,8)}}
 local function generate(mesh,report)
  if not mesh then mesh,report=mbm.generateImageMesh(path,o) end;assert(mesh,report);assert(mesh:check())
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

 local span=generate();close(span(.5,.5),8);close(span(.25,.5),4.5);close(span(.5,.0625),4.5)
 print('HIERARCHY LINE / WATERTIGHT OK')
 o.curvedNodes[1].profile='smooth'
 span=generate();close(span(.125,.5),2.09375);close(span(.375,.5),6.90625)
 o.curvedNodes[1].profile='bezier';o.curvedNodes[1].bezier1=0;o.curvedNodes[1].bezier2=0
 span=generate();close(span(.25,.5),1.875);close(span(.375,.5),3.953125)
 o.curvedEdge=8;o.curvedNodes[1].thickness=1
 span=generate();close(span(.25,.5),7.125);close(span(.5,.5),1)
 o.curvedEdge=1;o.curvedNodes[1].thickness=8
 o.curvedNodes[1].bezier1=.8;o.curvedNodes[1].bezier2=.2
 span=generate();close(span(.125,.5),3.66875);close(span(.375,.5),5.33125)
 o.curvedNodes[1].bezier1=1;o.curvedNodes[1].bezier2=0
 span=generate();close(span(.125,.5),4.0625);close(span(.375,.5),4.9375)
 local previous=1
 for i=0,16 do local height=span(i/32,.5)
  assert(height>=previous-.001 and height>=.999 and height<=8.001,'crossed controls invert or overshoot')
  previous=height
 end
 for _,pair in ipairs{{-.1,.2},{1.1,.2},{.8,-.1},{.8,1.1}} do
  o.curvedNodes[1].bezier1=pair[1];o.curvedNodes[1].bezier2=pair[2]
  local rejected,reason=mbm.generateImageMesh(path,o);assert(not rejected and reason:find('Bezier'),reason)
 end
 print('PROFILE CROSSED CONTROLS / MONOTONIC / INDEPENDENT LIMITS OK')
 print('PROFILE SMOOTH / BEZIER / DESCENDING / VALIDATION OK')
 local n=o.curvedNodes[1];n.bezier1=0;n.bezier2=0;n.bezier3=0;n.bezier4=0
 for count=3,4 do
  n.bezierPoints=count;span=generate();close(span(.25,.5),1+7*.5^(count+1))
  close(span(.375,.5),1+7*.75^(count+1))
 end
 n.bezier1=1;n.bezier2=1;n.bezier3=0;n.bezier4=0
 span=generate()
 local before=span(.1875,.5);local after=span(.3125,.5)
 assert(before>after,'four-control profile lost its middle wave')
 for i=0,16 do local height=span(i/32,.5);assert(height>=.999 and height<=8.001,'multi-control overshoot') end
 for _,key in ipairs{'bezier3','bezier4'} do
  n[key]=1.1;local rejected,reason=mbm.generateImageMesh(path,o);assert(not rejected and reason:find('Bezier'),reason);n[key]=0
 end
 print('PROFILE 3 / 4 CONTROLS / WAVE / BOUNDS OK')


 o.curvedNodes={circle(0,.25,8),circle(1,.1,3)}
 span=generate();close(span(.5,.5),3);close(span(.75,.5),8);close(span(.675,.5),5.5)
 print('HIERARCHY CHAIN / PLATEAU OK')
 o.curvedNodes[2].profile='bezier';o.curvedNodes[2].bezier1=0;o.curvedNodes[2].bezier2=0
 span=generate();close(span(.675,.5),7.375);close(span(.5,.5),3);close(span(.875,.5),4.5)
 print('PROFILE INDEPENDENT CHAIN OK')

 o.curvedNodes={line(0,.5,.125,.875,8),rect(0,'region',.125,.25,.375,.75),line(2,.25,.375,.625,2),
  rect(0,'region',.625,.25,.875,.75),line(4,.75,.375,.625,3)}
 span=generate();close(span(.5,.5),8);close(span(.25,.5),2);close(span(.75,.5),3)
 close(span(.125,.5),2.75);close(span(.375,.5),6.25);close(span(.1875,.5),2.375);close(span(.3125,.5),4.125)
 close(span(.0625,.5),1.875)
 print('HIERARCHY INHERITED BORDER / TWO GROOVES OK')
 o.curvedNodes[3].profile='bezier';o.curvedNodes[3].bezier1=0;o.curvedNodes[3].bezier2=0
 o.curvedNodes[5].profile='smooth'
 span=generate();close(span(.125,.5),2.75);close(span(.375,.5),6.25)
 close(span(.1875,.5),2.65625);close(span(.8125,.5),2.875)
 print('PROFILE LOCAL BORDERS / INDEPENDENT REGIONS OK')

 local function map(name)
  local file='/tmp/ime_profiles_'..name..'.png';local ok,msg=mbm.generateImageMeshMap(path,o,file);assert(ok,msg)
  return mbm.readImagePixels(file)
 end
 local first=map('a');local nodes=o.curvedNodes
 o.curvedNodes={nodes[1],nodes[4],nodes[5],nodes[2],nodes[3]};o.curvedNodes[3].parent=2;o.curvedNodes[5].parent=4
 local second=map('b');assert(first==second,'sibling ordering affects the height field')
 o.curvedSymmetric=false;span=generate();local thickness,front,back=span(.25,.5);close(thickness,2);close(back,0)
 local _,_,otherBack=span(.5,.5);close(otherBack,0)
 print('HIERARCHY ORDER / FLAT BACK OK')
 o.curvedSymmetric=true
 local valid=o.curvedNodes
 o.curvedNodes={line(0,.5,.125,.875,8),rect(0,'region',.1,.2,.4,.8),rect(0,'region',.3,.3,.6,.9)}
 local bad,msg=mbm.generateImageMesh(path,o);assert(not bad and msg:find('overlap'),msg)
 o.curvedNodes={line(0,.5,.125,.875,8),line(0,.6,.2,.8,4)}
 bad,msg=mbm.generateImageMesh(path,o);assert(not bad and msg:find('already has'),msg)
 o.curvedNodes={line(0,.5,.125,.875,8),rect(1,'region',.1,.2,.4,.8)}
 bad,msg=mbm.generateImageMesh(path,o);assert(not bad and msg:find('terminal'),msg)
 o.curvedNodes={};span=generate();close(span(.5,.5),1)
 o.curvedNodes=valid
 o.curvedNodes[1].profile='bezier';o.curvedNodes[1].bezier1=0;o.curvedNodes[1].bezier2=0
 o.curvedNodes[1].bezierPoints=4;o.curvedNodes[1].bezier3=0;o.curvedNodes[1].bezier4=0
 sampleResult=generate
 pending=assert(mbm.startImageMesh(path,o));o.curvedNodes[1].thickness=500
 o.curvedNodes[1].profile='smooth';o.curvedNodes[1].bezier2=1;o.curvedNodes[1].bezierPoints=2;o.curvedNodes[1].bezier3=1;o.curvedNodes[1].bezier4=1
 for _,p in ipairs(o.curvedNodes[2]) do p.x=0 end
 started=mbm.getTimeRun()
 print('HIERARCHY VALIDATION / EMPTY ROOT OK')
end
function onInitScene()
 local ok,err=xpcall(run,debug.traceback)
 if not ok then print('PROFILE FAIL '..tostring(err));mbm.quit() end
end
function onLoop()
 if not pending then mbm.quit();return end
 local status=pending:getStatus()
 if status.state=='completed' then
  local asset,report=pending:takeResult();assert(asset,report);close(report.maxHeight,3.5)
  local span=sampleResult(asset,report);close(span(.25,.2),1.21875)
  assert(asset:save('/tmp/ime_profiles_native.msh',false,false,true))
  print('HIERARCHY ASYNC SNAPSHOT / EXPORT OK');mbm.quit()
 elseif status.state~='running' or mbm.getTimeRun()-started>15 then
  print('PROFILE FAIL '..tostring(status.error or status.state));pending:cancel();mbm.quit()
 end
end
