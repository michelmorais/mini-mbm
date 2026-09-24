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

local HoleGeometry=require 'image_mesh_holes_geometry'
local CurvedModel=require 'image_mesh_curved_model'
local M={curved=CurvedModel}
M.defaults={preserveAspect=true,width=100,height=100,depth=20,relief=8,columns=24,rows=24,
    borderWidth=0.1,lockBorder=true,invert=false,maxVertices=65535,maxTriangles=131070,ellipseSegments=48}
M.grooveDefaults={followImage=false,twoLevels=false,grooveThreshold=0.5,grooveTransition=0.1,heightTolerance=0.03,smoothPasses=0}
M.simplifyDefaults={simplify=false,simplifyRatio=0.9,simplifyDetails=true,simplifyBoundary=0}
M.backDefaults={backExternal=false,backTexture='',backSolid=false,backColor=0x808080,backRelief=false,backMirror=false,backOpen=false,backRemap=false}
M.sideDefaults={sideMode='edge',sideBandPerpendicular=false,sideBandInvert=false,sideInset=1,sideRepeatU=1,sideRepeatV=1,sideColor=0x808080,sideTexture=''}
M.heightDefaults={heightFinishing=true,curvedPainting=false,curvedInterior=false,curvedFaceted=false,curvedFacetSectors=8,curvedFacetRings=1,curvedSimplify=false,curvedSimplifyRatio=.5,curvedSimplifyError=.01,curvedX=.5,curvedY=.5,curvedRadius=0,curvedEdge=1,curvedTarget=8,curvedSymmetric=true,heightSource='image',baseHeight=0.5,heightChannel='luminance',heightImage='',heightImageToRegion=false,heightBlack=0,heightWhite=1,heightCurve=1}
M.optionalDefaults={}
for _,defaults in ipairs({M.grooveDefaults,M.simplifyDefaults,M.backDefaults,M.sideDefaults,M.heightDefaults}) do
    for k,v in pairs(defaults) do M.defaults[k]=v; M.optionalDefaults[k]=v end
end
local limits={curvedFacetSectors={8,128,true},curvedFacetRings={1,16,true},curvedSimplifyRatio={.01,1},curvedSimplifyError={.0001,.25},curvedX={0,1},curvedY={0,1},curvedRadius={0,1000000},curvedEdge={.001,1000000},curvedTarget={.001,1000000},heightBlack={0,1},heightWhite={0,1},heightCurve={0.1,10},baseHeight={0,1},backColor={0,16777215,true},sideInset={1,1000000},sideRepeatU={0.1,64},sideRepeatV={0.1,64},sideColor={0,16777215,true},simplifyRatio={0.001,0.95},simplifyBoundary={0,0.25},grooveThreshold={0,1},grooveTransition={0.001,1},heightTolerance={0.001,1},smoothPasses={0,4,true},width={0.001,1000000},height={0.001,1000000},depth={0.001,1000000},relief={0,1000000},
    columns={1,255,true},rows={1,255,true},borderWidth={0,0.5},maxVertices={1,65535,true},
    maxTriangles={1,131070,true},ellipseSegments={8,128,true}}
local function number(v,lo,hi,integer)
    return type(v)=='number' and v==v and v>=lo and v<=hi and (not integer or v%1==0)
end
-- Shared by widgets and project validation, so their accepted ranges cannot drift.
function M.clampNumber(value,lo,hi,fallback,integer)
    if type(value)~='number' or value~=value or (hi==math.huge and value==math.huge) then return fallback end
    value=math.max(lo,math.min(hi,value))
    return integer and math.floor(value) or value
end
function M.clampOption(key,value,fallback)
    local range=assert(limits[key])
    return M.clampNumber(value,range[1],range[2],fallback,range[3])
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
function M.backMode(project,region)
    local scope=region.overrides
    if scope.backOpen==nil and scope.backRelief==nil and scope.backRemap==nil and scope.backSolid==nil and scope.backExternal==nil then scope=project.defaults end
    if scope.backExternal then return 'external' end
    if scope.backSolid then return 'solid' end
    if scope.backOpen then return 'open' end
    if scope.backRemap then return 'remap' end
    if scope.backRelief then return 'relief' end
    return 'flat'
end
function M.options(project,region)
    local o=M.copy(project.defaults)
    for k,v in pairs(region.overrides) do o[k]=v end
    for k,v in pairs(M.optionalDefaults) do if o[k]==nil then o[k]=v end end
    local backMode=M.backMode(project,region)
    o.backOpen=backMode=='open'; o.backRemap=backMode=='remap'; o.backRelief=backMode=='relief'; o.backSolid=backMode=='solid'; o.backExternal=backMode=='external'
    o.maxTriangles=2*o.maxVertices
    o.preserveAspect=o.preserveAspect~=false
    if o.preserveAspect then o.height=o.width*math.max(1,region.h-1)/math.max(1,region.w-1) end
    o.x=region.x; o.y=region.y; o.cropWidth=region.w; o.cropHeight=region.h
    o.shape=region.shape; o.contour=M.copy(region.contour)
    o.heightEdits=M.copy(region.heightEdits)
    o.holes=M.copy(region.holes)
    o.heightAreas=M.copy(region.heightAreas)
    o.curvedNodes=M.copy(region.curvedNodes)
    if o.backRemap then
        local crop=region.backCrop or region
        o.backX=crop.x; o.backY=crop.y; o.backCropWidth=crop.w; o.backCropHeight=crop.h
    end
    return o
end
function M.validateOptions(options,complete)
    assert(type(options)=='table','ime_invalid_options')
    for k,v in pairs(options) do
        if k=='heightSource' then assert(v=='image' or v=='manual' or v=='mixed' or v=='curved','ime_invalid_options')
        elseif k=='heightChannel' then assert(v=='luminance' or v=='red' or v=='green' or v=='blue' or v=='alpha','ime_invalid_options')
        elseif k=='sideMode' then assert(v=='edge' or v=='color' or v=='repeat' or v=='band','ime_invalid_options')
        elseif k=='sideTexture' or k=='backTexture' or k=='heightImage' then assert(type(v)=='string' and #v<4096 and not v:find('%z'),'ime_invalid_options')
        elseif k=='heightFinishing' or k=='curvedPainting' or k=='curvedInterior' or k=='curvedFaceted' or k=='curvedSimplify' or k=='curvedSymmetric' or k=='heightImageToRegion' or k=='backExternal' or k=='backSolid' or k=='sideBandPerpendicular' or k=='sideBandInvert' or k=='backOpen' or k=='backRemap' or k=='backRelief' or k=='backMirror' or k=='simplify' or k=='simplifyDetails' or k=='invert' or k=='lockBorder' or k=='preserveAspect' or k=='followImage' or k=='twoLevels' then assert(type(v)=='boolean','ime_invalid_options')
        else local range=limits[k]; assert(range and number(v,table.unpack(range)),'ime_invalid_options') end
    end
    if options.heightBlack~=nil and options.heightWhite~=nil then assert(options.heightBlack<=options.heightWhite,'ime_height_levels_invalid') end
    assert(not (options.backExternal and (options.backSolid or options.backOpen or options.backRemap or options.backRelief)),'ime_invalid_options')
    assert(not (options.backSolid and (options.backOpen or options.backRemap or options.backRelief)),'ime_invalid_options')
    assert(not (options.backOpen and (options.backRemap or options.backRelief)) and
        not (options.backRemap and options.backRelief),'ime_invalid_options')
    if complete then for k in pairs(M.defaults) do assert(k=='preserveAspect' or M.optionalDefaults[k]~=nil or options[k]~=nil,'ime_invalid_options') end end
end
function M.backRegion(region,mirror)
    local crop=region.backCrop or region
    local contour=region.contour
    if mirror and region.shape=='polygon' then
        contour={}
        for _,point in ipairs(region.contour) do contour[#contour+1]={x=1-point.x,y=point.y} end
    end
    return {x=crop.x,y=crop.y,w=crop.w,h=crop.h,shape=region.shape,contour=contour}
end
function M.ensureBackCrops(p)
    for _,r in ipairs(p.regions) do
        if M.backMode(p,r)=='remap' and not r.backCrop then r.backCrop={x=r.x,y=r.y,w=r.w,h=r.h} end
    end
end
function M.settings(values)
    local out={}
    for k,v in pairs(M.defaults) do
        out[k]=values[k]; if out[k]==nil then out[k]=v end
    end
    out.maxTriangles=2*out.maxVertices
    M.validateOptions(out,true)
    return out
end
function M.validatePresets(presets)
    assert(type(presets)=='table' and #presets<=128,'ime_preset_invalid')
    local seen={}
    for key,preset in pairs(presets) do
        assert(type(key)=='number' and key%1==0 and key>=1 and key<=#presets,'ime_preset_invalid')
        assert(type(preset)=='table' and type(preset.name)=='string' and #preset.name<=128 and
            preset.name:match('%S') and not preset.name:find('%c') and not seen[preset.name],'ime_preset_invalid')
        seen[preset.name]=true
        M.validateOptions(preset.settings,true)
    end
end
function M.validate(p)
    assert(type(p)=='table' and p.version==1,'ime_invalid_project')
    assert(type(p.image)=='table' and type(p.image.path)=='string' and #p.image.path<4096,'ime_invalid_project')
    assert(number(p.image.width,1,16777216,true) and number(p.image.height,1,16777216,true) and
        p.image.width*p.image.height<=16777216,'ime_invalid_image')
    M.validateOptions(p.defaults,true)
    if p.presets~=nil then M.validatePresets(p.presets) end
    assert(type(p.regions)=='table' and #p.regions<=256 and number(p.nextId,1,2147483647,true),'ime_invalid_project')
    local seen={}
    for _,r in ipairs(p.regions) do
        assert(type(r)=='table' and number(r.id,1,p.nextId-1,true) and not seen[r.id],'ime_invalid_project'); seen[r.id]=true
        assert(type(r.name)=='string' and #r.name>0 and #r.name<=128,'ime_invalid_name')
        assert(r.locked==nil or type(r.locked)=='boolean','ime_invalid_project')
        assert(r.shape=='rectangle' or r.shape=='ellipse' or r.shape=='polygon','ime_invalid_shape')
        assert(number(r.x,0,p.image.width-1,true) and number(r.y,0,p.image.height-1,true) and
            number(r.w,1,p.image.width-r.x,true) and number(r.h,1,p.image.height-r.y,true),'ime_invalid_crop')
        M.validateOptions(r.overrides,false)
        local black=r.overrides.heightBlack or p.defaults.heightBlack or 0
        local white=r.overrides.heightWhite or p.defaults.heightWhite or 1
        assert(black<=white,'ime_height_levels_invalid')
        if r.backCrop then
            local c=r.backCrop
            assert(type(c)=='table' and number(c.x,0,p.image.width-1,true) and number(c.y,0,p.image.height-1,true) and
                number(c.w,1,p.image.width-c.x,true) and number(c.h,1,p.image.height-c.y,true),'ime_invalid_back_crop')
        end
        if r.heightEdits then
            assert(type(r.heightEdits)=='table' and #r.heightEdits<=4096,'ime_paint_limit')
            for _,dab in ipairs(r.heightEdits) do
                assert(type(dab)=='table' and number(dab.x,0,1) and number(dab.y,0,1) and
                    number(dab.radius,0.001,1) and number(dab.strength,0,1) and number(dab.height,0,1) and
                    (dab.mode=='raise' or dab.mode=='lower' or dab.mode=='flatten' or dab.mode=='smooth'),'ime_invalid_paint')
            end
        end
        if r.shape=='polygon' then
            assert(type(r.contour)=='table' and #r.contour>=3 and #r.contour<=128,'ime_invalid_contour')
            for _,point in ipairs(r.contour) do assert(type(point)=='table' and number(point.x,0,1) and number(point.y,0,1),'ime_invalid_contour') end
        end
        CurvedModel.validate(r.curvedNodes)
        if r.heightAreas then
            assert(type(r.heightAreas)=='table' and #r.heightAreas<=32,'ime_areas_limit')
            for _,area in ipairs(r.heightAreas) do
                assert(type(area)=='table' and #area>=(area.shape=='line' and 2 or 3) and #area<=128,'ime_areas_invalid')
                assert(type(area.name)=='string' and #area.name<=128 and not area.name:find('%c'),'ime_areas_invalid')
                assert(area.mode==nil or area.mode=='raise' or area.mode=='lower' or area.mode=='flatten','ime_areas_invalid')
                assert(type(area.enabled)=='boolean' and number(area.height,0,1) and number(area.transition,0,1),'ime_areas_invalid')
                assert(area.shape=='rectangle' or area.shape=='ellipse' or area.shape=='polygon' or area.shape=='line','ime_areas_invalid')
                for _,point in ipairs(area) do assert(type(point)=='table' and number(point.x,0,1) and number(point.y,0,1),'ime_areas_invalid') end
                if area.shape=='line' then assert(number(area.lineWidth,.001,1),'ime_areas_invalid')
                else assert(HoleGeometry.simple(area),'ime_areas_invalid') end
            end
        end
        if r.holes then
            local outer=M.outline(r,r.overrides.ellipseSegments or p.defaults.ellipseSegments or 48)
            for _,point in ipairs(outer) do point.x=(point.x-r.x)/math.max(1,r.w-1);point.y=(point.y-r.y)/math.max(1,r.h-1) end
            HoleGeometry.validate(outer,r.holes)
        end
    end
    return true
end
function M.add(p,shape,x,y,w,h,contour)
    assert(#p.regions<256,'ime_region_limit')
    local r={id=p.nextId,name=string.format('module_%03d',p.nextId),shape=shape,x=x,y=y,w=w,h=h,
        contour=M.copy(contour),overrides={},locked=false}
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
    if before.holes then
        r.holes=M.copy(before.holes)
        for _,hole in ipairs(r.holes) do for _,p in ipairs(hole) do
            p.x=(before.x+p.x*(before.w-1)-left)/math.max(1,r.w-1)
            p.y=(before.y+p.y*(before.h-1)-top)/math.max(1,r.h-1)
        end end
    end
    if before.curvedNodes then
        r.curvedNodes=M.copy(before.curvedNodes)
        for _,node in ipairs(r.curvedNodes) do for _,p in ipairs(node) do
            p.x=(before.x+p.x*(before.w-1)-left)/math.max(1,r.w-1)
            p.y=(before.y+p.y*(before.h-1)-top)/math.max(1,r.h-1)
        end end
    end
    if before.heightAreas then
        r.heightAreas=M.copy(before.heightAreas)
        for _,area in ipairs(r.heightAreas) do for _,p in ipairs(area) do
            p.x=(before.x+p.x*(before.w-1)-left)/math.max(1,r.w-1)
            p.y=(before.y+p.y*(before.h-1)-top)/math.max(1,r.h-1)
        end end
    end
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
