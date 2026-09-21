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
local function cross(a,b,c) return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x) end
local function check(asset,open,holeCount)
 assert(asset:check())
 local vertices,indices=Asset.geometry(asset);local edges={};local volume=0;local unique={}
 local function key(v) return string.format('%.5f/%.5f/%.5f',v.x+0.,v.y+0.,v.z+0.) end
 for i=1,#indices,3 do
  local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
  local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
  local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
  assert((uy*vz-uz*vy)^2+(uz*vx-ux*vz)^2+(ux*vy-uy*vx)^2>1e-12,'degenerate triangle')
  volume=volume+(a.x*(b.y*c.z-b.z*c.y)+a.y*(b.z*c.x-b.x*c.z)+a.z*(b.x*c.y-b.y*c.x))/6
  for _,e in ipairs{{a,b},{b,c},{c,a}} do
   local aa,bb=key(e[1]),key(e[2]);assert(aa~=bb,'degenerate edge');unique[aa]=true;unique[bb]=true
   local k=aa<bb and aa..':'..bb or bb..':'..aa
   local n=edges[k] or {0,0};edges[k]=n;n[1]=n[1]+1;n[2]=n[2]+(aa<bb and 1 or -1)
  end
 end
 for _,n in pairs(edges) do assert((open and n[1]==1) or (n[1]==2 and n[2]==0),'nonmanifold hole mesh '..n[1]) end
 local nv,ne=0,0;for _ in pairs(unique) do nv=nv+1 end;for _ in pairs(edges) do ne=ne+1 end
 if not open then assert(nv-ne+#indices/3==2-2*(holeCount or 2),'hole topology changed') end
 assert(open or volume>0)
 for _,v in ipairs(vertices) do assert(math.abs(v.nx*v.nx+v.ny*v.ny+v.nz*v.nz-1)<1e-4) end
 return vertices,indices
end
local function run()
 local pixels={};for y=0,63 do for x=0,63 do pixels[#pixels+1]=x*4;pixels[#pixels+1]=y*4;pixels[#pixels+1]=128 end end
 local image='/tmp/ime_holes.png';assert(mbm.createTexture(pixels,64,64,3,'holes_fixture',image))
 local circle={};for i=0,15 do local a=-i*math.pi/8;circle[#circle+1]={x=.68+.09*math.cos(a),y=.35+.09*math.sin(a)} end
 local holes={{{x=.2,y=.2},{x=.36,y=.2},{x=.36,y=.5},{x=.2,y=.5}},circle}
 for _,shape in ipairs{'rectangle','ellipse','polygon'} do for _,adaptive in ipairs{false,true} do
  for _,side in ipairs{'edge','color','repeat','band'} do
   for _,back in ipairs{'flat','open','relief','solid','remap','external'} do
    local o={shape=shape,holes=holes,columns=6,rows=6,relief=3,sideMode=side,sideInset=2,sideRepeatU=2.5,sideRepeatV=1.5,
     followImage=adaptive,contour={{x=0,y=0},{x=1,y=0},{x=1,y=1},{x=.6,y=1},{x=.6,y=.8},{x=.4,y=.8},{x=.4,y=1},{x=0,y=1}},
     backOpen=back=='open',backRelief=back=='relief',backSolid=back=='solid',backRemap=back=='remap',backExternal=back=='external',backTexture=image}
    local asset,report=mbm.generateImageMesh(image,o)
    assert(asset,shape..' '..side..' '..back..' '..tostring(adaptive)..': '..tostring(report))
    check(asset,o.backOpen)
    if back=='flat' and side=='edge' then
     local ok,why=asset:simplify(.95,nil,1,true,0);assert(ok,why);check(asset,false)
     local file='/tmp/ime_holes_'..shape..'_'..tostring(adaptive)..'.msh';assert(asset:save(file,false,false,true))
     local loaded=meshDebug:new();assert(loaded:load(file));check(loaded,false)
    end
   end
  end
  print('HOLES '..shape..' '..tostring(adaptive)..' CLOSED / MODES / NORMALS / SIMPLIFY OK')
 end end
 local areaOptions={holes={holes[1]},columns=4,rows=4,relief=0}
 local asset,report=mbm.generateImageMesh(image,areaOptions);assert(asset,report)
 local v,idx=check(asset,false,1);local area=0
 for i=1,#idx,3 do local a,b,c=v[idx[i]],v[idx[i+1]],v[idx[i+2]]
  if a.z<0 and b.z<0 and c.z<0 then area=area+math.abs(cross(a,b,c))/2 end
 end
 assert(math.abs(area-10000*(1-.16*.3))<.02,'front area filled hole: '..area)
 for _,bad in ipairs{
  {{{x=0,y=.2},{x=.2,y=.2},{x=.2,y=.4}}},
  {{{x=.2,y=.2},{x=.5,y=.5},{x=.5,y=.2},{x=.2,y=.5}}},
  {holes[1],holes[1]},
  {holes[1],{{x=.22,y=.22},{x=.3,y=.22},{x=.3,y=.3},{x=.22,y=.3}}}
 } do local result=mbm.generateImageMesh(image,{holes=bad});assert(not result,'invalid holes accepted') end
 local many={}
 for y=0,3 do for x=0,3 do local a,b=.08+x*.23,.08+y*.23
  many[#many+1]={{x=a,y=b},{x=a+.1,y=b},{x=a+.1,y=b+.1},{x=a,y=b+.1}}
 end end
 local complex,cr=mbm.generateImageMesh(image,{holes=many,columns=4,rows=4});assert(complex,cr);check(complex,false,16)
 local concave={{x=.2,y=.2},{x=.5,y=.2},{x=.5,y=.3},{x=.3,y=.3},{x=.3,y=.5},{x=.2,y=.5}}
 local ca,ce=mbm.generateImageMesh(image,{holes={concave},columns=4,rows=4});assert(ca,ce);check(ca,false,1)
 assert(mbm.generateImageMeshMap(image,areaOptions,'/tmp/ime_holes_map.png',false))
 local alpha,w,h=mbm.readPngAlpha('/tmp/ime_holes_map.png');assert(alpha and alpha:byte(22*w+18+1)==0,'height map filled hole')
 -- A cut must not taper the surrounding height field towards the hole edges.
 local white={};for i=1,64*64*3 do white[i]=255 end
 local whiteImage='/tmp/ime_holes_white.png'
 assert(mbm.createTexture(white,64,64,3,'holes_white',whiteImage))
 for _,adaptive in ipairs{false,true} do for _,borderWidth in ipairs{0,.4} do
  local o={holes={holes[1]},width=100,height=100,depth=20,relief=10,
   columns=6,rows=6,followImage=adaptive,lockBorder=true,borderWidth=borderWidth}
  local cut,why=mbm.generateImageMesh(whiteImage,o);assert(cut,why)
  local vertices=check(cut,false,1);local checked=0
  for _,v in ipairs(vertices) do if v.z<=-10 then
   local u,y=v.x/100+.5,.5-v.y/100
   local distance=math.min(u,1-u,y,1-y)
   local expected=10
   if distance<1e-6 then expected=0
   elseif borderWidth>0 then expected=10*math.min(1,distance/borderWidth) end
   assert(math.abs(-v.z-10-expected)<.001,'hole flattened neighboring relief')
   checked=checked+1
  end end
  assert(checked>0)
 end end
 print('HOLES PRESERVE RELIEF / OUTER BORDER ONLY / ADAPTIVE AND UNIFORM OK')
 -- Unordered holes must bridge through the correct local sector, including
 -- interior holes initially hidden from all outer vertices by other holes.
 math.randomseed(147)
 for trial=1,500 do
  local rings={}
  for y=0,2 do for x=0,2 do if math.random()<.75 then
   local a,b=.1+x*.27,.1+y*.27;local size=trial%2==0 and .269 or .12
   rings[#rings+1]={{x=a,y=b},{x=a+size,y=b},{x=a+size,y=b+size},{x=a,y=b+size}}
  end end end
  for i=#rings,2,-1 do local j=math.random(i);rings[i],rings[j]=rings[j],rings[i] end
  local cut,why=mbm.generateImageMesh(image,{holes=rings,columns=1,rows=1,relief=0})
  assert(cut,'hole order '..trial..': '..tostring(why))
  local vertices,indices=check(cut,false,#rings);local area=0
  for i=1,#indices,3 do local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
   if a.z<0 and b.z<0 and c.z<0 then area=area+math.abs(cross(a,b,c))/2 end
  end
  local size=trial%2==0 and .269 or .12
  assert(math.abs(area-10000*(1-#rings*size*size))<.03,'incorrect area in close holes')
 end
 for _,gap in ipairs{.01,.001,.0001} do
  local narrow={{x=.1,y=.1},{x=.9,y=.1},{x=.9,y=.9},{x=.5+gap,y=.9},
   {x=.5+gap,y=.2},{x=.5,y=.2},{x=.5,y=.9},{x=.1,y=.9}}
  for _,asHole in ipairs{false,true} do
   local o={columns=1,rows=1,relief=0,shape=asHole and 'rectangle' or 'polygon'}
   if asHole then o.holes={narrow} else o.contour=narrow end
   local cut,why=mbm.generateImageMesh(image,o);assert(cut,why);check(cut,false,asHole and 1 or 0)
  end
 end
 print('HOLES 500 ORDERS / CLOSE CONTOURS / NARROW STROKES / CLOSED / EULER OK')
 local budget,error=mbm.generateImageMesh(image,{holes=many,maxVertices=100});assert(not budget and error:find('budget'))
 print('HOLES AREA / EULER / MAX HOLES / CONCAVE / VALIDATION / MAP / BUDGET / ROUNDTRIP OK')
end
function onInitScene() local ok,err=pcall(run);if not ok then print('HOLES FAIL '..tostring(err)) end;mbm.quit() end
function onLoop() end
