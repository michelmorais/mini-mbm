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
local function vertexKey(p)
 local a={};for _,k in ipairs{'x','y','z','u','v','nx','ny','nz'} do a[#a+1]=string.format('%.9g',p[k]) end
 return table.concat(a,',')
end
local function attributes(d)
 local result={}
 for s=1,d:getTotalSubset(1) do
  local set={};for _,p in ipairs(d:getVertex(1,s,1,d:getTotalVertex(1,s))) do set[vertexKey(p)]=true end
  result[s]={vertices=set,texture=d:getTexture(1,s)}
 end
 return result
end
local function exercise(path,name,expected)
 for _,reduce in ipairs{false,true} do
  local d=meshDebug:new();assert(d:load(path));assert(d:check())
  local before=helper.signature(d);local attrs=attributes(d)
  local r,e=d:simplify(nil,nil,1,true,0,'coplanar',nil,nil,reduce);assert(r,e)
  assert(not r.qemRan and r.resultTriangleCount<=r.sourceTriangleCount)
  assert(d:getTotalSubset(1)==#attrs)
  if name=='Lorekeeper-walk' then assert(r.planarSkipped and r.unchanged) end
  if expected then assert(r.resultTriangleCount==(reduce and expected[2] or expected[1])) end
  if r.unchanged then assert(helper.signature(d)==before) end
  for s,a in ipairs(attrs) do
   assert(d:getTexture(1,s)==a.texture,'texture changed')
   for _,p in ipairs(d:getVertex(1,s,1,d:getTotalVertex(1,s))) do assert(a.vertices[vertexKey(p)],'new corner attributes') end
  end
  assert(d:check());local signature=helper.signature(d)
  local output='/tmp/coplanar-imported-'..name..'-'..tostring(reduce)..'.msh'
  assert(d:save(output,false,false,true))
  local restored=meshDebug:new();assert(restored:load(output));assert(restored:check())
  assert(signature==helper.signature(restored),'save/reload changed buffers')
  print(string.format('COPLANAR ASSET %s boundaries=%s %d -> %d skipped=%s',name,tostring(reduce),r.sourceTriangleCount,r.resultTriangleCount,tostring(r.planarSkipped)))
 end
end
function onInitScene()
 local ok,e=xpcall(function()
  mbm.addPath('src/test-lib')
  for _,name in ipairs{'Crate','base','building_A','Lorekeeper-walk'} do exercise('src/test-lib/'..name..'.msh',name) end
  -- Actual OBJ parser, with matching position/UV/normal indices. Two adjacent charts.
  local obj=assert(io.open('/tmp/coplanar-import.obj','w'))
  obj:write('mtllib coplanar-import.mtl\n')
  for side=0,1 do for y=0,8 do for x=0,8 do
   obj:write(string.format('v %g %g 0\nvt %g %g\nvn 0 0 1\n',x+side*8,y,x/8+side*2,y/8))
  end end end
  for side=0,1 do
   obj:write('o chart'..side..'\nusemtl plain\n')
   for y=0,7 do for x=0,7 do local a=side*81+y*9+x+1
    for _,face in ipairs{{a,a+1,a+10},{a,a+10,a+9}} do
     obj:write('f');for _,v in ipairs(face) do obj:write(string.format(' %d/%d/%d',v,v,v)) end;obj:write('\n')
    end
   end end
  end
  obj:close();local mtl=assert(io.open('/tmp/coplanar-import.mtl','w'));mtl:write('newmtl plain\nKd 1 1 1\n');mtl:close()
  local loaded,shapes=require('tiny_obj_loader').tiny_parse('/tmp/coplanar-import.obj');assert(loaded and #shapes==2)
  local d=meshDebug:new();d:setType('mesh');d:setModeFrontFace('CCW');d:addFrame(3)
  for s,shape in ipairs(shapes) do d:addSubSet(1);assert(d:addVertex(1,s,shape.tVertex));assert(d:addIndex(1,s,shape.tIndex)) end
  d:addNormals();d:addAnim('Static',1,1,1,0);assert(d:check())
  assert(d:save('/tmp/coplanar-import.msh',false,false,true));exercise('/tmp/coplanar-import.msh','obj-charts',{60,4})
  print('COPLANAR IMPORTED ASSETS OK')
 end,debug.traceback)
 if not ok then print('COPLANAR IMPORTED ASSETS FAIL '..tostring(e)) end
 mbm.quit()
end
function onLoop() end
