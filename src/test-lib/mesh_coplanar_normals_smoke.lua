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
function onInitScene()
 local ok,e=xpcall(function()
  for _,kind in ipairs{'flat','hole'} do for _,slope in ipairs{0,.5} do
   for _,reduce in ipairs{false,true} do
    local _,v,idx=helper.grid(kind,slope)
    for _,p in ipairs(v) do p.nx=p.x/8;p.ny=p.y/16 end
    local d=meshDebug:new();d:setType('mesh');d:setModeFrontFace('CCW');d:addFrame(3);d:addSubSet(1)
    assert(d:addVertex(1,1,v));assert(d:addIndex(1,1,idx))
    local r,err=d:simplify(nil,nil,nil,true,0,'coplanar',nil,nil,reduce);assert(r,err)
    local out=d:getVertex(1,1,1,d:getTotalVertex(1,1));local indices=d:getIndex(1,1)
    assert(#indices/3==(kind=='hole' and (reduce and 8 or 40) or (reduce and 2 or 30)))
    for _,p in ipairs(out) do assert(p.nx==p.x/8 and p.ny==p.y/16 and p.nz==2) end
    for i=1,#indices,3 do
     local a,b,c=out[indices[i]],out[indices[i+1]],out[indices[i+2]]
     for j=0,8 do for k=0,8-j do
      local u,w=j/8,k/8;local t=1-u-w
      local x,y=u*a.x+w*b.x+t*c.x,u*a.y+w*b.y+t*c.y
      assert(math.abs(u*a.nx+w*b.nx+t*c.nx-x/8)<1e-12)
      assert(math.abs(u*a.ny+w*b.ny+t*c.ny-y/16)<1e-12)
     end end
    end
   end
  end end
  for _,kind in ipairs{'nonlinear','unit','zero','cancel','near'} do
   local _,v,idx=helper.grid(kind=='near' and 'near' or 'flat')
   for _,p in ipairs(v) do
    p.nx=p.x/8;p.ny=p.y/16
    if kind=='nonlinear' then p.nx=p.x*p.x/64 end
    if kind=='unit' then local n=math.sqrt(p.nx*p.nx+p.ny*p.ny+4);p.nx=p.nx/n;p.ny=p.ny/n;p.nz=2/n end
    if kind=='zero' then p.nx=0;p.ny=0;p.nz=0 end
    if kind=='cancel' then p.nx=0;p.ny=0;p.nz=p.x-4 end
   end
   local d=meshDebug:new();d:setType('mesh');d:setModeFrontFace('CCW');d:addFrame(3);d:addSubSet(1)
   assert(d:addVertex(1,1,v));assert(d:addIndex(1,1,idx));local before=helper.signature(d)
   local r,err=d:simplify(nil,nil,nil,true,0,'coplanar',nil,nil,true);assert(r,err)
   assert(r.unchanged and helper.signature(d)==before,kind)
  end
  -- Separate normal charts remain separate even when their positions coincide
  -- along the shared edge; boundary coordination must not weld their attributes.
  local charts=meshDebug:new();charts:setType('mesh');charts:setModeFrontFace('CCW');charts:addFrame(3)
  for subset=1,2 do
   charts:addSubSet(1);local _,v,idx=helper.grid('flat')
   for _,p in ipairs(v) do p.x=p.x+(subset-1)*8;p.nx=p.x/8;p.ny=subset;p.nz=2+p.x/16 end
   assert(charts:addVertex(1,subset,v));assert(charts:addIndex(1,subset,idx))
  end
  local report,err=charts:simplify(nil,nil,nil,true,0,'coplanar',nil,nil,true);assert(report,err)
  for subset=1,2 do
   assert(#charts:getIndex(1,subset)==6)
   for _,p in ipairs(charts:getVertex(1,subset,1,charts:getTotalVertex(1,subset))) do
    assert(p.nx==p.x/8 and p.ny==subset and p.nz==2+p.x/16)
   end
  end
  print('COPLANAR AFFINE NORMALS OK')
 end,debug.traceback)
 if not ok then print('COPLANAR AFFINE NORMALS FAIL '..tostring(e)) end
 mbm.quit()
end
function onLoop() end
