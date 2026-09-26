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


-- Repair keeps face order and positions; carry authored normals by source corner.
local M={}
function M.restore(asset,subset,group,hasNormals)
    local source=asset:getVertex(1,subset,1,asset:getTotalVertex(1,subset))
    local indices=asset:getIndex(1,subset)
    if not indices or #indices==0 then indices={};for i=1,#source do indices[i]=i end end
    assert(#indices==#group.indices,'Repair changed triangle count')
    local vertices,output,keys={},{},{}
    for i=1,#indices,3 do
        local order={}
        for corner=0,2 do
            local id=group.indices[i+corner]
            local point=group.vertices[id]
            local previous
            for k=0,2 do
                local v=source[indices[i+k]]
                if point.x==v.x and point.y==v.y and point.z==v.z then previous=k;break end
            end
            assert(previous,'Repair moved a source corner')
            order[corner+1]=previous
        end
        local sign=order[2]==(order[1]+1)%3 and 1 or -1
        for corner=0,2 do
            local id=group.indices[i+corner]
            local v=source[indices[i+order[corner+1]]]
            local p=group.vertices[id]
            local key=string.format('%d %.17g %.17g',id,v.u,v.v)
            if hasNormals then key=key..string.format(' %.17g %.17g %.17g',v.nx*sign,v.ny*sign,v.nz*sign) end
            local index=keys[key]
            if not index then
                index=#vertices+1;keys[key]=index
                vertices[index]={x=p.x,y=p.y,z=p.z,u=v.u,v=v.v}
                if hasNormals then vertices[index].nx=v.nx*sign;vertices[index].ny=v.ny*sign;vertices[index].nz=v.nz*sign end
            end
            output[#output+1]=index
        end
    end
    group.vertices,group.indices=vertices,output
end
return M
