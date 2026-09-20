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

local M={}
M.defaults={preserveAspect=true,width=100,height=100,depth=20,relief=8,columns=24,rows=24,
    borderWidth=0.1,lockBorder=true,invert=false,maxVertices=65535,maxTriangles=131070,ellipseSegments=48}
local limits={width={0.001,1000000},height={0.001,1000000},depth={0.001,1000000},relief={0,1000000},
    columns={1,255,true},rows={1,255,true},borderWidth={0,0.5},maxVertices={1,65535,true},
    maxTriangles={1,131070,true},ellipseSegments={8,128,true}}
local function number(v,lo,hi,integer)
    return type(v)=='number' and v==v and v>=lo and v<=hi and (not integer or v%1==0)
end
function M.copy(value)
    if type(value)~='table' then return value end
    local out={}; for k,v in pairs(value) do out[k]=M.copy(v) end; return out
end
function M.new(path,width,height)
    return {version=1,image={path=path or '',width=width or 0,height=height or 0},defaults=M.copy(M.defaults),regions={},nextId=1}
end
function M.region(project,id)
    for i,r in ipairs(project.regions) do if r.id==id then return r,i end end
end
function M.options(project,region)
    local o=M.copy(project.defaults)
    for k,v in pairs(region.overrides) do o[k]=v end
    o.maxTriangles=2*o.maxVertices
    o.preserveAspect=o.preserveAspect~=false
    if o.preserveAspect then o.height=o.width*math.max(1,region.h-1)/math.max(1,region.w-1) end
    o.x=region.x; o.y=region.y; o.cropWidth=region.w; o.cropHeight=region.h
    o.shape=region.shape; o.contour=M.copy(region.contour)
    return o
end
function M.validateOptions(options,complete)
    assert(type(options)=='table','ime_invalid_options')
    for k,v in pairs(options) do
        if k=='invert' or k=='lockBorder' or k=='preserveAspect' then assert(type(v)=='boolean','ime_invalid_options')
        else local range=limits[k]; assert(range and number(v,table.unpack(range)),'ime_invalid_options') end
    end
    if complete then for k in pairs(M.defaults) do assert(k=='preserveAspect' or options[k]~=nil,'ime_invalid_options') end end
end
function M.validate(p)
    assert(type(p)=='table' and p.version==1,'ime_invalid_project')
    assert(type(p.image)=='table' and type(p.image.path)=='string' and #p.image.path<4096,'ime_invalid_project')
    assert(number(p.image.width,1,16777216,true) and number(p.image.height,1,16777216,true) and
        p.image.width*p.image.height<=16777216,'ime_invalid_image')
    M.validateOptions(p.defaults,true)
    assert(type(p.regions)=='table' and #p.regions<=256 and number(p.nextId,1,2147483647,true),'ime_invalid_project')
    local seen={}
    for _,r in ipairs(p.regions) do
        assert(type(r)=='table' and number(r.id,1,p.nextId-1,true) and not seen[r.id],'ime_invalid_project'); seen[r.id]=true
        assert(type(r.name)=='string' and #r.name>0 and #r.name<=128,'ime_invalid_name')
        assert(r.shape=='rectangle' or r.shape=='ellipse' or r.shape=='polygon','ime_invalid_shape')
        assert(number(r.x,0,p.image.width-1,true) and number(r.y,0,p.image.height-1,true) and
            number(r.w,1,p.image.width-r.x,true) and number(r.h,1,p.image.height-r.y,true),'ime_invalid_crop')
        M.validateOptions(r.overrides,false)
        if r.shape=='polygon' then
            assert(type(r.contour)=='table' and #r.contour>=3 and #r.contour<=128,'ime_invalid_contour')
            for _,point in ipairs(r.contour) do assert(type(point)=='table' and number(point.x,0,1) and number(point.y,0,1),'ime_invalid_contour') end
        end
    end
    return true
end
function M.add(p,shape,x,y,w,h,contour)
    assert(#p.regions<256,'ime_region_limit')
    local r={id=p.nextId,name=string.format('module_%03d',p.nextId),shape=shape,x=x,y=y,w=w,h=h,
        contour=M.copy(contour),overrides={}}
    p.nextId=p.nextId+1; p.regions[#p.regions+1]=r; return r
end
-- Presets become ordinary editable regions; no new project schema is needed.
function M.primitive(p,kind,w,h,sides,cx,cy)
    assert(kind=='rectangle' or kind=='circle' or kind=='ellipse' or kind=='triangle' or kind=='regular','ime_invalid_shape')
    assert(type(w)=='number' and w==math.floor(w) and w>=2 and w<=p.image.width,'ime_invalid_crop')
    if kind=='circle' then h=w end
    assert(type(h)=='number' and h==math.floor(h) and h>=2 and h<=p.image.height,'ime_invalid_crop')
    local x=math.max(0,math.min(p.image.width-w,math.floor((cx or p.image.width/2)-w/2)))
    local y=math.max(0,math.min(p.image.height-h,math.floor((cy or p.image.height/2)-h/2)))
    local shape=kind; local contour
    if kind=='circle' then shape='ellipse'
    elseif kind=='triangle' then shape='polygon'; contour={{x=0.5,y=0},{x=1,y=1},{x=0,y=1}}
    elseif kind=='regular' then
        assert(type(sides)=='number' and sides==math.floor(sides) and sides>=3 and sides<=32,'ime_invalid_sides')
        shape='polygon'; contour={}
        for i=0,sides-1 do
            local angle=-math.pi/2+2*math.pi*i/sides
            contour[#contour+1]={x=0.5+0.5*math.cos(angle),y=0.5+0.5*math.sin(angle)}
        end
    end
    local region=M.add(p,shape,x,y,w,h,contour)
    return region
end
function M.grid(p,g)
    assert(number(g.columns,1,64,true) and number(g.rows,1,64,true),'ime_invalid_grid')
    assert(number(g.marginX,0,p.image.width,true) and number(g.marginY,0,p.image.height,true) and
        number(g.gapX,0,p.image.width,true) and number(g.gapY,0,p.image.height,true),'ime_invalid_grid')
    assert(#p.regions+g.columns*g.rows<=256,'ime_region_limit')
    local w=math.floor((p.image.width-2*g.marginX-(g.columns-1)*g.gapX)/g.columns)
    local h=math.floor((p.image.height-2*g.marginY-(g.rows-1)*g.gapY)/g.rows)
    assert(w>=1 and h>=1,'ime_invalid_grid')
    local added={}
    for row=0,g.rows-1 do for col=0,g.columns-1 do
        local r=M.add(p,'rectangle',g.marginX+col*(w+g.gapX),g.marginY+row*(h+g.gapY),w,h)
        added[#added+1]=r.id
    end end
    return added
end
function M.fromPoints(p,points)
    assert(#points>=3 and #points<=128,'ime_invalid_contour')
    local x,y,xx,yy=math.huge,math.huge,-math.huge,-math.huge
    for _,v in ipairs(points) do x=math.min(x,v.x); y=math.min(y,v.y); xx=math.max(xx,v.x); yy=math.max(yy,v.y) end
    x=math.floor(x); y=math.floor(y); xx=math.min(p.image.width-1,math.ceil(xx)); yy=math.min(p.image.height-1,math.ceil(yy))
    assert(xx>x and yy>y,'ime_invalid_contour')
    local contour={}; for _,v in ipairs(points) do contour[#contour+1]={x=(v.x-x)/(xx-x),y=(v.y-y)/(yy-y)} end
    return M.add(p,'polygon',x,y,xx-x+1,yy-y+1,contour)
end
function M.outline(r,segments)
    local points={}
    if r.shape=='polygon' then
        for _,p in ipairs(r.contour) do points[#points+1]={x=r.x+p.x*(r.w-1),y=r.y+p.y*(r.h-1)} end
    elseif r.shape=='ellipse' then
        for i=0,(segments or 48)-1 do local a=2*math.pi*i/(segments or 48)
            points[#points+1]={x=r.x+(0.5+0.5*math.cos(a))*(r.w-1),y=r.y+(0.5+0.5*math.sin(a))*(r.h-1)}
        end
    else points={{x=r.x,y=r.y},{x=r.x+r.w-1,y=r.y},{x=r.x+r.w-1,y=r.y+r.h-1},{x=r.x,y=r.y+r.h-1}} end
    return points
end
-- Expand the crop from the drag-start snapshot, preserving every other image point.
function M.movePoint(r,before,index,x,y)
    local points=M.outline(before)
    points[index]={x=x,y=y}
    local left=math.min(before.x,math.floor(x))
    local top=math.min(before.y,math.floor(y))
    local right=math.max(before.x+before.w-1,math.ceil(x))
    local bottom=math.max(before.y+before.h-1,math.ceil(y))
    r.x=left; r.y=top; r.w=right-left+1; r.h=bottom-top+1
    for i,p in ipairs(points) do
        r.contour[i]={x=(p.x-left)/math.max(1,r.w-1),y=(p.y-top)/math.max(1,r.h-1)}
    end
end
function M.contains(points,x,y)
    local inside=false
    for i,a in ipairs(points) do local b=points[i%#points+1]
        if (a.y>y)~=(b.y>y) and x<(b.x-a.x)*(y-a.y)/(b.y-a.y)+a.x then inside=not inside end
    end
    return inside
end
function M.history() return {past={},future={}} end
function M.commit(history,before)
    history.past[#history.past+1]=M.copy(before); if #history.past>40 then table.remove(history.past,1) end
    history.future={}
end
function M.undo(history,current,redo)
    local from,to=history.past,history.future; if redo then from,to=to,from end
    if #from==0 then return nil end
    to[#to+1]=M.copy(current); return table.remove(from)
end
return M
