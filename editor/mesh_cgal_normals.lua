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


-- Rebuild normals per connected smooth fan; never weld coincident positions.
local M={}
function M.rebuild(group,featureAngle)
    local points,indices=group.vertices,group.indices
    local parent,rank,faces,edges={},{},{},{}
    local function root(i)
        while parent[i]~=i do parent[i]=parent[parent[i]];i=parent[i] end
        return i
    end
    local function join(a,b)
        a,b=root(a),root(b)
        if a==b then return end
        if rank[a]<rank[b] then a,b=b,a end
        parent[b]=a
        if rank[a]==rank[b] then rank[a]=rank[a]+1 end
    end
    for i=1,#indices do parent[i]=i;rank[i]=0 end
    for i=1,#indices,3 do
        local a,b,c=points[indices[i]],points[indices[i+1]],points[indices[i+2]]
        local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
        local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
        local nx,ny,nz=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx
        local length=math.sqrt(nx*nx+ny*ny+nz*nz)
        assert(length>0 and length<math.huge,'Degenerate output triangle')
        faces[i]={x=nx,y=ny,z=nz,length=length}
        for corner=0,2 do
            local first,second=i+corner,i+(corner+1)%3
            local aId,bId=indices[first],indices[second]
            local key=math.min(aId,bId)..':'..math.max(aId,bId)
            local edge=edges[key]
            if not edge then edges[key]={first=first,second=second,face=i,count=1}
            else
                edge.count=edge.count+1
                if edge.count==2 then edge.otherFirst=first;edge.otherSecond=second;edge.otherFace=i end
            end
        end
    end
    local threshold=math.cos(math.rad(featureAngle))
    for _,edge in pairs(edges) do
        -- Boundary, non-manifold and same-direction edges stay split.
        if edge.count==2 and indices[edge.first]==indices[edge.otherSecond] and
            indices[edge.second]==indices[edge.otherFirst] then
            local a,b=faces[edge.face],faces[edge.otherFace]
            local cosine=(a.x*b.x+a.y*b.y+a.z*b.z)/(a.length*b.length)
            if cosine>=threshold-1e-12 and cosine> -1+1e-12 then
                join(edge.first,edge.otherSecond);join(edge.second,edge.otherFirst)
            end
        end
    end
    local sums={}
    for i=1,#indices,3 do
        local face=faces[i]
        for corner=0,2 do
            local id=root(i+corner)
            local sum=sums[id]
            if not sum then sum={x=0,y=0,z=0,face=face};sums[id]=sum end
            sum.x=sum.x+face.x;sum.y=sum.y+face.y;sum.z=sum.z+face.z
        end
    end
    local vertices,output,keys={},{},{}
    for i,index in ipairs(indices) do
        local id=root(i)
        local mapped=keys[id]
        if not mapped then
            local p,sum=points[index],sums[id]
            local length=math.sqrt(sum.x*sum.x+sum.y*sum.y+sum.z*sum.z)
            if length<=1e-20 then sum=sum.face;length=sum.length end
            mapped=#vertices+1;keys[id]=mapped
            vertices[mapped]={x=p.x,y=p.y,z=p.z,u=p.u,v=p.v,
                nx=sum.x/length,ny=sum.y/length,nz=sum.z/length}
        end
        output[i]=mapped
    end
    group.vertices,group.indices=vertices,output
end
return M
