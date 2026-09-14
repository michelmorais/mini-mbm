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

-- Pure geometry in source-image coordinates (X right, Y down).
local M = {}
local EPS = 1e-5
local function point(x,y) return {x=x,y=y} end
local function cross(a,b,c) return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x) end
function M.area(ring)
    local area=0
    for i,a in ipairs(ring) do
        local b=ring[i%#ring+1]
        area=area+a.x*b.y-a.y*b.x
    end
    return area/2
end
function M.contains(ring,x,y)
    local inside=false
    for i,a in ipairs(ring) do
        local b=ring[i%#ring+1]
        if (a.y>y)~=(b.y>y) and x<(b.x-a.x)*(y-a.y)/(b.y-a.y)+a.x then
            inside=not inside
        end
    end
    return inside
end
function M.inside(rings,x,y)
    local inside=false
    for _,ring in ipairs(rings) do
        if M.contains(ring,x,y) then inside=not inside end
    end
    return inside
end
local function ellipse(cx,cy,rx,ry,n)
    local ring={}
    for i=0,n-1 do
        local a=2*math.pi*i/n
        ring[#ring+1]=point(cx+rx*math.cos(a),cy+ry*math.sin(a))
    end
    return ring
end
function M.form(kind,r,budget,options)
    options=options or {}
    assert(r.w>0 and r.h>0,'invalid_rectangle')
    budget=math.max(2,math.floor(budget or 2))
    local x,y,w,h=r.x,r.y,r.w,r.h
    if kind=='rectangle' or kind=='alpha' then
        return {{point(x,y),point(x+w,y),point(x+w,y+h),point(x,y+h)}}
    end
    local n=math.max(5,budget)
    if kind=='circle' then return {ellipse(x+w/2,y+h/2,w/2,h/2,n)} end
    if kind=='ring' then
        n=math.max(5,math.floor(budget/2))
        local radius=options.inner or 0.5
        local dx,dy=options.dx or 0,options.dy or 0
        assert(radius>0 and radius<1 and math.sqrt((dx/(w/2))^2+(dy/(h/2))^2)+radius<1,
            'invalid_hole')
        return {ellipse(x+w/2,y+h/2,w/2,h/2,n),
            ellipse(x+w/2+dx,y+h/2+dy,w/2*radius,h/2*radius,n)}
    end
    assert(kind=='capsule','invalid_form')
    local top=math.max(EPS,math.min(h/2,options.top or math.min(w/2,h/2)))
    local bottom=options.linked~=false and top or math.max(EPS,math.min(h/2,options.bottom or top))
    local ring={}
    local steps=math.max(3,math.ceil(n/2))
    for i=0,steps do
        local a=math.pi+math.pi*i/steps
        ring[#ring+1]=point(x+w/2+(options.circular~=false and top or w/2)*math.cos(a),y+top+top*math.sin(a))
    end
    for i=0,steps do
        local a=math.pi*i/steps
        ring[#ring+1]=point(x+w/2+(options.circular~=false and bottom or w/2)*math.cos(a),y+h-bottom+bottom*math.sin(a))
    end
    for i=#ring,1,-1 do
        local a,b=ring[i],ring[i%#ring+1]
        if (a.x-b.x)^2+(a.y-b.y)^2<EPS then table.remove(ring,i) end
    end
    return {ring}
end
local function simplify(ring,tolerance)
    local out={}
    for _,p in ipairs(ring) do out[#out+1]=point(p.x,p.y) end
    local changed=true
    while changed and #out>3 do
        changed=false
        for i=1,#out do
            local a,b,c=out[(i-2)%#out+1],out[i],out[i%#out+1]
            local length=math.sqrt((c.x-a.x)^2+(c.y-a.y)^2)
            if length>EPS and math.abs(cross(a,b,c))/length<=tolerance then
                table.remove(out,i); changed=true; break
            end
        end
    end
    return out
end
-- Pixel boundaries retain all alpha components and holes. Alpha is a row-major
-- byte string; threshold and source bounds are evaluated only on regeneration.
function M.alphaContours(bytes,width,height,rect,forms,threshold,preserveHoles)
    assert(#bytes==width*height,'invalid_alpha_size')
    local occupied={}
    local x0,y0=math.max(0,math.floor(rect.x)),math.max(0,math.floor(rect.y))
    local x1,y1=math.min(width,math.ceil(rect.x+rect.w)),math.min(height,math.ceil(rect.y+rect.h))
    for y=y0,y1-1 do
        for x=x0,x1-1 do
            if bytes:byte(y*width+x+1)>(threshold or 0) and
                (not forms or M.inside(forms,x+0.5,y+0.5)) then occupied[y*width+x]=true end
        end
    end
    local function has(x,y)
        return x>=x0 and x<x1 and y>=y0 and y<y1 and occupied[y*width+x]
    end
    local edges,starts={},{}
    local function key(x,y) return y*(width+1)+x end
    local function edge(ax,ay,bx,by)
        local e={a=point(ax,ay),b=point(bx,by)}
        edges[#edges+1]=e
        local k=key(ax,ay); starts[k]=starts[k] or {}; table.insert(starts[k],e)
    end
    for y=y0,y1-1 do
        for x=x0,x1-1 do
            if has(x,y) then
                if not has(x,y-1) then edge(x,y,x+1,y) end
                if not has(x+1,y) then edge(x+1,y,x+1,y+1) end
                if not has(x,y+1) then edge(x+1,y+1,x,y+1) end
                if not has(x-1,y) then edge(x,y+1,x,y) end
            end
        end
    end
    local rings={}
    for _,first in ipairs(edges) do
        if not first.used then
            local ring,e={},first
            repeat
                e.used=true; ring[#ring+1]=e.a
                local nextEdge,best=nil,-math.huge
                for _,candidate in ipairs(starts[key(e.b.x,e.b.y)] or {}) do
                    if not candidate.used or candidate==first then
                        -- At diagonal contacts, turn right to keep components separate.
                        local dx,dy=e.b.x-e.a.x,e.b.y-e.a.y
                        local nx,ny=candidate.b.x-candidate.a.x,candidate.b.y-candidate.a.y
                        local score=math.atan(dx*ny-dy*nx,dx*nx+dy*ny)
                        if score>best then nextEdge,best=candidate,score end
                    end
                end
                assert(nextEdge,'open_alpha_contour')
                e=nextEdge
            until e==first
            ring=simplify(ring,0)
            if preserveHoles~=false or M.area(ring)>0 then rings[#rings+1]=ring end
        end
    end
    assert(#rings>0,'empty_alpha_region')
    return rings
end
function M.simplify(rings,tolerance)
    if not tolerance or tolerance<=0 then return rings end
    local out={}
    for i,ring in ipairs(rings) do out[i]=simplify(ring,tolerance) end
    if not M.validate(out) or #M.components(out)~=#M.components(rings) then return rings end
    for i,r in ipairs(rings) do
        for j,other in ipairs(rings) do
            if i~=j then
                if M.contains(other,r[1].x,r[1].y)~=M.contains(out[j],out[i][1].x,out[i][1].y) then return rings end
            end
        end
    end
    return out
end
function M.components(rings)
    local groups={}
    for i,r in ipairs(rings) do
        local depth=0
        for j,other in ipairs(rings) do
            if i~=j and M.contains(other,r[1].x,r[1].y) then depth=depth+1 end
        end
        if depth%2==0 then groups[#groups+1]={r} end
    end
    for _,r in ipairs(rings) do
        local owner,smallest=nil,math.huge
        local isOuter=false
        for _,g in ipairs(groups) do if g[1]==r then isOuter=true end end
        if not isOuter then
            for _,g in ipairs(groups) do
                local area=math.abs(M.area(g[1]))
                if area<smallest and M.contains(g[1],r[1].x,r[1].y) then owner,smallest=g,area end
            end
            if owner then owner[#owner+1]=r end
        end
    end
    return groups
end
local function intersect(a,b,c,d)
    return cross(a,b,c)*cross(a,b,d)<-EPS and cross(c,d,a)*cross(c,d,b)<-EPS
end
function M.validate(rings)
    for ri,r in ipairs(rings) do
        if #r<3 or math.abs(M.area(r))<=EPS then return nil,'degenerate_contour' end
        for i,a in ipairs(r) do
            local b=r[i%#r+1]
            if (b.x-a.x)^2+(b.y-a.y)^2<=EPS then return nil,'duplicate_vertex' end
            for rj=ri,#rings do
                local other=rings[rj]
                for j,c in ipairs(other) do
                    if rj~=ri or (j>i and j~=i%#r+1 and i~=j%#r+1) then
                        if intersect(a,b,c,other[j%#other+1]) then return nil,'crossing_contours' end
                    end
                end
            end
        end
    end
    return true
end
-- Even/odd horizontal decomposition supports concave contours and arbitrary holes.
-- No bridges or filled triangles are introduced across transparent interiors.
function M.triangulate(rings,budget)
    local valid,reason=M.validate(rings); assert(valid,reason)
    local vertices,indices={},{}
    local function triangle(a,b,c)
        if math.abs(cross(a,b,c))<=EPS then return end
        if cross(a,b,c)<0 then b,c=c,b end
        for _,p in ipairs({a,b,c}) do vertices[#vertices+1]=point(p.x,p.y); indices[#indices+1]=#vertices end
    end
    -- Convex single rings use a fan, giving a rectangle exactly two triangles.
    local convex=#rings==1
    local sign=0
    if convex then
        local r=rings[1]
        for i=1,#r do
            local c=cross(r[i],r[i%#r+1],r[(i+1)%#r+1])
            if math.abs(c)>EPS then
                if sign*c<0 then convex=false; break end
                sign=c
            end
        end
        if convex then for i=2,#r-1 do triangle(r[1],r[i],r[i+1]) end end
    end
    if not convex then
        local levels,seen,edges={},{},{}
        for _,r in ipairs(rings) do
            for i,a in ipairs(r) do
                if not seen[a.y] then levels[#levels+1]=a.y; seen[a.y]=true end
                local b=r[i%#r+1]
                if a.y~=b.y then edges[#edges+1]={a,b} end
            end
        end
        table.sort(levels)
        local function at(e,y) return e[1].x+(e[2].x-e[1].x)*(y-e[1].y)/(e[2].y-e[1].y) end
        for i=1,#levels-1 do
            local lo,hi=levels[i],levels[i+1]
            if hi>lo then
                local active={}
                for _,e in ipairs(edges) do
                    -- The bundled Lua uses float32. Adjacent Y levels may have
                    -- no representable midpoint, so test coverage of the slab
                    -- rather than strict containment of a rounded midpoint.
                    if math.min(e[1].y,e[2].y)<=lo and math.max(e[1].y,e[2].y)>=hi then
                        active[#active+1]=e
                    end
                end
                table.sort(active,function(a,b)
                    local alo,blo=at(a,lo),at(b,lo)
                    local ahi,bhi=at(a,hi),at(b,hi)
                    local difference=(alo-blo)+(ahi-bhi)
                    if difference~=0 then return difference<0 end
                    return alo<blo or (alo==blo and ahi<bhi)
                end)
                assert(#active%2==0,'open_contour '..lo..' '..hi..' '..#active)
                for j=1,#active,2 do
                    local l,r=active[j],active[j+1]
                    local a,b,c,d=point(at(l,lo),lo),point(at(r,lo),lo),point(at(r,hi),hi),point(at(l,hi),hi)
                    triangle(a,b,c); triangle(a,c,d)
                end
            end
        end
    end
    -- Optional refinement increases the budget without changing the silhouette.
    budget=math.min(20000,math.max(2,budget or 2))
    while #indices/3+2<=budget do
        local best,area=1,-1
        for i=1,#indices,3 do
            local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
            local size=math.abs(cross(a,b,c))
            if size>area then best,area=i,size end
        end
        if area<=EPS then break end
        local ia,ib,ic=indices[best],indices[best+1],indices[best+2]
        local a,b,c=vertices[ia],vertices[ib],vertices[ic]
        vertices[#vertices+1]=point((a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3)
        local m=#vertices
        indices[best+2]=m
        for _,v in ipairs({ib,ic,m,ic,ia,m}) do indices[#indices+1]=v end
    end
    assert(#indices>0,'empty_geometry')
    assert(#vertices<=65535,'geometry_too_large')
    return vertices,indices
end
-- Fit alpha silhouettes conservatively. The requested budget is a target; do
-- not sacrifice topology or more than 5% of any contour area to reach it.
function M.fitBudget(rings,budget)
    local _,initial=M.triangulate(rings,2)
    if #initial/3<=budget then return rings end
    local extent=0
    for _,ring in ipairs(rings) do
        local minx,miny,maxx,maxy=math.huge,math.huge,-math.huge,-math.huge
        for _,p in ipairs(ring) do
            minx,miny=math.min(minx,p.x),math.min(miny,p.y)
            maxx,maxy=math.max(maxx,p.x),math.max(maxy,p.y)
        end
        extent=math.max(extent,maxx-minx,maxy-miny)
    end
    local best,count=rings,#initial/3
    for step=1,4 do
        local candidate=M.simplify(rings,extent*step/200)
        local preserve=true
        for i,r in ipairs(rings) do
            local area=math.abs(M.area(r))
            if math.abs(math.abs(M.area(candidate[i]))-area)>area*0.05 then preserve=false; break end
        end
        if preserve then
            local _,indices=M.triangulate(candidate,2)
            if #indices/3<count then best,count=candidate,#indices/3 end
            if count<=budget then break end
        end
    end
    return best
end
return M
