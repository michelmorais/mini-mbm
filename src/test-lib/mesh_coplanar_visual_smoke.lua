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
local targets,objects={},{}
local frames=0
function onInitScene()
 local ok,e=xpcall(function()
  local pixels={};for y=0,63 do for x=0,63 do local c=(math.floor(x/8)+math.floor(y/8))%2==0 and 220 or 50
   for _,v in ipairs{c,c,255,255} do pixels[#pixels+1]=v end
  end end
  local texture='/tmp/coplanar-checker.png';assert(mbm.createTexture(pixels,64,64,4,'coplanar-checker',texture))
  mbm.setLightEnabled('3d',true);mbm.setAmbientLight('3d',.3,.3,.3)
  mbm.setDirectionalLight('3d',0,0,-1,.6,.6,.6)
  mbm.setPointLight('3d',2,3,10,30,.3,.2,.1)
  for _,case in ipairs{{'island',0},{'affine_normal',0},{'flat',0},{'inclined',.5},{'concave',0},{'materials',0},{'wall',0},{'shallow',0},{'angular',0},{'hole',0},{'multiple',.5},{'inner_concave',0},{'outer_concave',0}} do
   local d,vertices,indices=helper.grid(case[1]=='island' and 'hole' or case[1],case[2]);d:setTexture(1,1,texture)
   if case[1]=='island' then
    d:addSubSet(1)
    assert(d:addVertex(1,2,{{x=3.5,y=3.5,z=0,nx=0,ny=0,nz=2,u=0,v=0},{x=4.5,y=3.5,z=0,nx=0,ny=0,nz=2,u=1,v=0},{x=4,y=4.5,z=0,nx=0,ny=0,nz=2,u=.5,v=1}}))
    assert(d:addIndex(1,2,{1,2,3}));d:setTexture(1,2,'#FF6060FF')
   elseif case[1]=='materials' then
    d:addSubSet(1)
    for _,v in ipairs(vertices) do v.x=v.x+8 end
    assert(d:addVertex(1,2,vertices));assert(d:addIndex(1,2,indices));d:setTexture(1,2,'#FF6060FF')
   elseif case[1]=='wall' then
    d:addSubSet(1);local v,idx={},{}
    for y=0,8 do
     v[#v+1]={x=0,y=y,z=0,nx=-1,ny=0,nz=0,u=0,v=y/8}
     v[#v+1]={x=0,y=y,z=-2,nx=-1,ny=0,nz=0,u=1,v=y/8}
    end
    for y=0,7 do local a=y*2+1;for _,i in ipairs{a,a+2,a+3,a,a+3,a+1} do idx[#idx+1]=i end end
    assert(d:addVertex(1,2,v));assert(d:addIndex(1,2,idx));d:setTexture(1,2,texture)
   end
   for _,stage in ipairs{'before','after'} do
    if stage=='after' then local r,err=d:simplify(nil,nil,nil,true,0,'coplanar',case[1]=='angular' and .01 or (case[1]=='shallow' and 1e-4 or nil),case[1]=='angular' and 5 or nil,true);assert(r,err);assert(r.planarRegions>=1) end
    local path='/tmp/coplanar-'..case[1]..'-'..stage
    assert(d:save(path..'.msh',false,false,true))
    local object=mesh:new('3d');assert(object:load(path..'.msh'));objects[#objects+1]=object
    local rt=render2texture:new('2ds');assert(rt:create(256,256,true,path));assert(rt:add(object))
    local camera=rt:getCamera('3d');camera:setPos(-6,6,-24);camera:setFocus(4,4,case[2]*4);
    if case[1]=='materials' then camera:setPos(8,6,-28);camera:setFocus(8,4,0) end;camera:setNear(.1);camera:setFar(100)
    targets[#targets+1]={rt=rt,path=path,case=case[1],stage=stage}
   end
  end
 end,debug.traceback)
 if not ok then print('COPLANAR VISUAL FAIL '..tostring(e));mbm.quit() end
end
function onLoop()
 frames=frames+1;if frames<5 then return end
 local ok,e=xpcall(function()
  for _,t in ipairs(targets) do assert(t.rt:save(t.path..'.png')) end
  for i=1,#targets,2 do
   local a=assert(mbm.readImagePixels(targets[i].path..'.png'))
   local b=assert(mbm.readImagePixels(targets[i+1].path..'.png'));assert(#a==#b)
   local sum,max,colors=0,0,{}
   for p=1,#a do local delta=math.abs(a:byte(p)-b:byte(p));sum=sum+delta;max=math.max(max,delta) end
   for p=1,#a,4 do colors[a:sub(p,p+3)]=true end
   local count=0;for _ in pairs(colors) do count=count+1 end
   assert(count>8,'blank render')
   print(string.format('COPLANAR VISUAL %s mean=%.6g max=%d colors=%d',targets[i].case,sum/#a,max,count))
   assert(sum/#a<1,'render difference exceeded one intensity level on average')
  end
  print('COPLANAR VISUAL CHECKER / DIRECTIONAL + POINT LIGHT / OBLIQUE OK')
 end,debug.traceback)
 if not ok then print('COPLANAR VISUAL FAIL '..tostring(e)) end
 mbm.quit()
end
