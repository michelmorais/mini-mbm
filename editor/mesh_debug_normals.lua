--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

]]--

-- Shared policy for vertex, subset and all-frame normal operations.
local M={}
local function finite(n) return type(n)=='number' and n==n and math.abs(n)<math.huge end
function M.select(v,g,mode)
    if mode==nil or mode==1 then
        local x,y,z=v.nx,v.ny,v.nz
        if finite(x) and finite(y) and finite(z) then
            local scale=math.max(math.abs(x),math.abs(y),math.abs(z))
            if scale>0 then
                local sx,sy,sz=x/scale,y/scale,z/scale
                local length=math.sqrt(sx*sx+sy*sy+sz*sz)
                local nx,ny,nz=sx/length,sy/length,sz/length
                if not g or nx*g.x+ny*g.y+nz*g.z>0 then
                    if math.abs(scale*length-1)<=.001 then return nil end
                    return nx,ny,nz -- preserve direction, repair length only
                end
            end
        end
    end
    if g and finite(g.x) and finite(g.y) and finite(g.z) then
        if v.nx==g.x and v.ny==g.y and v.nz==g.z then return nil end
        return g.x,g.y,g.z
    end
end
-- Connected surface patches are inferred from shared edges and face angles.
-- At a crease, the largest patch takes priority, independent of triangle count.
function M.surfaces(vertices,indices,degrees)
    degrees=finite(degrees) and math.max(1,math.min(89,degrees)) or 25
    local cosine=math.cos(math.rad(degrees))
    local faces,parent,edges={},{},{}
    local function root(i)
        while parent[i]~=i do parent[i]=parent[parent[i]];i=parent[i] end
        return i
    end
    for at=1,#indices-2,3 do
        local ids={indices[at],indices[at+1],indices[at+2]}
        local a,b,c=vertices[ids[1]],vertices[ids[2]],vertices[ids[3]]
        if a and b and c then
            local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
            local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
            local x,y,z=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx
            local length=math.sqrt(x*x+y*y+z*z)
            if finite(length) and length>0 then
                local i=#faces+1
                faces[i]={ids=ids,x=x/length,y=y/length,z=z/length,area=length*.5}
                parent[i]=i
                for e=1,3 do
                    local v,w=ids[e],ids[e%3+1]
                    if v>w then v,w=w,v end
                    local key=v..':'..w
                    local edge=edges[key]
                    if not edge then edges[key]={i}
                    else edge[#edge+1]=i end
                end
            end
        end
    end
    for _,edge in pairs(edges) do
        if #edge==2 then
            local a,b=faces[edge[1]],faces[edge[2]]
            if a.x*b.x+a.y*b.y+a.z*b.z>=cosine then
                local ra,rb=root(edge[1]),root(edge[2])
                if ra~=rb then parent[math.max(ra,rb)]=math.min(ra,rb) end
            end
        end
    end
    local areas,incident={},{}
    for i,face in ipairs(faces) do
        local region=root(i);areas[region]=(areas[region] or 0)+face.area
        for _,v in ipairs(face.ids) do
            incident[v]=incident[v] or {}
            local sum=incident[v][region] or {x=0,y=0,z=0}
            incident[v][region]=sum
            sum.x=sum.x+face.x;sum.y=sum.y+face.y;sum.z=sum.z+face.z
        end
    end
    local result={}
    for vertex,regions in pairs(incident) do
        local best
        for region in pairs(regions) do
            if not best or areas[region]>areas[best] or (areas[region]==areas[best] and region<best) then best=region end
        end
        -- Similar-size patches have no dominant surface: blend rather than
        -- choosing an arbitrary side of a symmetric corner (e.g. a cube).
        local x,y,z=0,0,0
        for region,n in pairs(regions) do
            if areas[region]>=areas[best]*.95 then x=x+n.x;y=y+n.y;z=z+n.z end
        end
        local length=math.sqrt(x*x+y*y+z*z)
        if length>0 then result[vertex]={x=x/length,y=y/length,z=z/length} end
    end
    return result
end
function M.label(mode)
    if mode==3 then return 'normal_method_surfaces' end
    if mode==2 then return 'normal_method_uniform' end
    return 'normal_method_repair'
end
function M.geometry(meshD,f,s,state,fallback)
    if (state.normalMethod or 1)~=3 then return fallback(meshD,f,s) end
    local count=meshD:getTotalVertex(f,s)
    if count<=0 then return {} end
    local vertices=meshD:getVertex(f,s,1,count)
    local indices=meshD:getIndex(f,s)
    if not indices then indices={};for i=1,#vertices do indices[i]=i end end
    return M.surfaces(vertices,indices,state.normalSurfaceAngle or 25)
end
function M.draw(gui,lang,state,id)
    gui.Text(lang.L('normal_method'))
    gui.PushItemWidth(320)
    local changed,mode=gui.Combo('##'..id,state.normalMethod or 1,
        {lang.L('normal_method_repair'),lang.L('normal_method_uniform'),lang.L('normal_method_surfaces')},-1)
    gui.PopItemWidth()
    if changed then state.normalMethod=mode end
    if state.normalMethod==3 then
        gui.PushItemWidth(160)
        local adjusted,angle=gui.DragFloat(lang.L('normal_surface_angle')..'##'..id,state.normalSurfaceAngle or 25,1,1,89,'%.1f')
        gui.PopItemWidth()
        if adjusted then state.normalSurfaceAngle=finite(angle) and math.max(1,math.min(89,angle)) or 25 end
        gui.TextWrapped(lang.L('normal_surfaces_help'))
    else
        gui.TextWrapped(lang.L((state.normalMethod or 1)==1 and 'normal_repair_help' or 'normal_uniform_help'))
    end
end
return M
