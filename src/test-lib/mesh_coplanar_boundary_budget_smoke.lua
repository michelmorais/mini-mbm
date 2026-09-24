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


function onInitScene()
 local ok,e=xpcall(function()
  local d=meshDebug:new();d:setType('mesh');d:setModeFrontFace('CCW');d:addFrame(3);d:addSubSet(1)
  local v,idx={},{}
  for n=0,4096 do
   local x,y=(n%64)*3,math.floor(n/64)*3;local offset=#v
   for _,p in ipairs{{0,0},{1,0},{1,1},{0,1},{.5,.5}} do
    v[#v+1]={x=x+p[1],y=y+p[2],z=0,u=p[1],v=p[2],nx=0,ny=0,nz=2}
   end
   for _,i in ipairs{1,2,5,2,3,5,3,4,5,4,1,5} do idx[#idx+1]=offset+i end
  end
  assert(d:addVertex(1,1,v));assert(d:addIndex(1,1,idx));d:addAnim('Static',1,1,1,0)
  local r,e=d:simplify(nil,nil,1,true,0,'coplanar',1e-7,.05,true);assert(r,e)
  assert(r.planarBoundaryFallback and r.planarBoundaryRemovedVertices==0)
  assert(r.resultTriangleCount==8194 and r.planarRegions==4097 and not r.qemRan)
  assert(d:check());print('COPLANAR BOUNDARY COORDINATION BUDGET FALLBACK OK')
 end,debug.traceback)
 if not ok then print('COPLANAR BOUNDARY BUDGET FAIL '..tostring(e)) end
end
function onLoop() mbm.quit() end
