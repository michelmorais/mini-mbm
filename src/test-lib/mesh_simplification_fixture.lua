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


local function grid(kind,slope)
 local d=meshDebug:new();d:setType('mesh');d:setModeFrontFace('CCW');d:addFrame(3);d:addSubSet(1)
 local v,idx={},{}
 for y=0,8 do for x=0,8 do
  local z=(slope or 0)*x
  if kind=='angular' then z=z+.0005*x*x end
  if kind=='shallow' then z=z+1e-5*x*x end
  if kind=='near' then z=z+1e-8*x*x end
  if kind=='bent' then z=z+.01*x*x end
  v[#v+1]={x=x,y=y,z=z,u=x/8,v=y/8,nx=0,ny=0,nz=2}
 end end
 if kind=='uv' then v[41].u=v[41].u+.1 end
 if kind=='affine_normal' then for _,p in ipairs(v) do p.nx=p.x/8;p.ny=p.y/16 end end
 if kind=='normal' then v[41].nx=.2 end
 for y=0,7 do for x=0,7 do
  local cut=(kind=='hole' and x>=3 and x<=4 and y>=3 and y<=4)
   or (kind=='multiple' and ((x==2 and y==2) or (x==5 and y==5)))
   or (kind=='inner_concave' and ((y==3 and (x==3 or x==4)) or (x==3 and y==4)))
   or (kind=='touching' and ((x==3 and y==3) or (x==4 and y==4)))
   or (kind=='outer_concave' and x>=1 and x<=2 and y>=1 and y<=2)
  if not cut and not((kind=='concave' or kind=='outer_concave') and x>=4 and y>=4) then
   local a=y*9+x+1
   for _,i in ipairs{a,a+1,a+10,a,a+10,a+9} do idx[#idx+1]=i end
  end
 end end
 assert(d:addVertex(1,1,v));assert(d:addIndex(1,1,idx));d:addAnim('Static',1,1,1,0);assert(d:check())
 return d,v,idx
end
local function signature(d)
 local a={}
 for s=1,d:getTotalSubset(1) do
  for _,p in ipairs(d:getVertex(1,s,1,d:getTotalVertex(1,s))) do
   for _,k in ipairs{'x','y','z','u','v','nx','ny','nz'} do a[#a+1]=string.format('%.9g',p[k]) end
  end
  for _,i in ipairs(d:getIndex(1,s)) do a[#a+1]=tostring(i) end
 end
 return table.concat(a,',')
end
return {grid=grid,signature=signature}
