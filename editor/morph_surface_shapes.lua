--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
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
|------------------------------------------------------------------------------------------------------------------------|

   Parametric surface shapes for the generic Morph Editor.
]]--

local tPolygon=require 'morph_surface_polygon'
local M={}

local TAU=math.pi*2
local DEFAULT_SEGMENTS=16

local function copyPoint(point)
    return {x=tonumber(point and point.x) or 0,y=tonumber(point and point.y) or 0,
        z=tonumber(point and point.z) or 0}
end

local function add(a,b)
    return {x=a.x+b.x,y=a.y+b.y,z=a.z+b.z}
end

local function subtract(a,b)
    return {x=a.x-b.x,y=a.y-b.y,z=a.z-b.z}
end

local function scale(point,value)
    return {x=point.x*value,y=point.y*value,z=point.z*value}
end

local function dot(a,b) return a.x*b.x+a.y*b.y+a.z*b.z end

local function distanceSquared(a,b)
    local delta=subtract(a,b)
    return dot(delta,delta)
end

local function cameraBasis(camera)
    local azimuth=tonumber(camera and camera.azimuth) or 0
    local elevation=tonumber(camera and camera.elevation) or 0
    local sinAz,cosAz=math.sin(azimuth),math.cos(azimuth)
    local sinEl,cosEl=math.sin(elevation),math.cos(elevation)
    return {x=cosAz,y=0,z=-sinAz},
        {x=-sinEl*sinAz,y=cosEl,z=-sinEl*cosAz}
end

local function intersectCameraPlane(origin,direction,point,camera)
    if not origin or not direction or not point then return nil end
    local right,up=cameraBasis(camera)
    local normal={x=right.y*up.z-right.z*up.y,
        y=right.z*up.x-right.x*up.z,
        z=right.x*up.y-right.y*up.x}
    local denominator=dot(direction,normal)
    if math.abs(denominator)<=1e-8 then return nil end
    local relative=subtract(point,origin)
    local distance=dot(relative,normal)/denominator
    if distance<0 then return nil end
    return add(origin,scale(direction,distance))
end

local function shapeType(region)
    local shape=region and region.shape
    local value=type(shape)=='table' and shape.type or nil
    if value~='square' and value~='ellipse' then return 'polygon' end
    return value
end

local function shapeFree(region)
    local shape=region and region.shape
    if shapeType(region)=='polygon' then return true end
    return type(shape)=='table' and shape.free==true
end

local function ensureShape(region)
    region.shape=type(region.shape)=='table' and region.shape or {}
    local kind=shapeType(region)
    region.shape.type=kind
    region.shape.free=kind=='polygon' and true or region.shape.free==true
    region.shape.segments=math.max(8,math.floor(tonumber(region.shape.segments) or DEFAULT_SEGMENTS))
    if kind~='polygon' and not shapeFree(region) then region.closed=true end
    return region.shape
end

local function resolved(region,topology)
    return tPolygon.resolvePoints(region,topology)
end

local function captureParameters(region,topology,camera)
    local points=resolved(region,topology)
    if #points==0 then return nil end
    local right,up=cameraBasis(camera)
    local center={x=0,y=0,z=0}
    for _,point in ipairs(points) do center=add(center,point.position) end
    center=scale(center,1/#points)
    local minX,minY=math.huge,math.huge
    local maxX,maxY=-math.huge,-math.huge
    for _,point in ipairs(points) do
        local relative=subtract(point.position,center)
        local x,y=dot(relative,right),dot(relative,up)
        minX,minY=math.min(minX,x),math.min(minY,y)
        maxX,maxY=math.max(maxX,x),math.max(maxY,y)
    end
    return {type=shapeType(region),center=center,right=right,up=up,
        radiusX=math.max((maxX-minX)*0.5,1e-5),radiusY=math.max((maxY-minY)*0.5,1e-5),
        segments=region.shape and region.shape.segments or DEFAULT_SEGMENTS}
end

local function targetPoints(parameters)
    local center,right,up=parameters.center,parameters.right,parameters.up
    local radiusX,radiusY=parameters.radiusX,parameters.radiusY
    if parameters.type=='square' or parameters.type=='polygon' then
        return {
            add(center,add(scale(right,-radiusX),scale(up,-radiusY))),
            add(center,add(scale(right,radiusX),scale(up,-radiusY))),
            add(center,add(scale(right,radiusX),scale(up,radiusY))),
            add(center,add(scale(right,-radiusX),scale(up,radiusY)))
        }
    end
    local result={}
    local segments=math.max(8,math.floor(tonumber(parameters.segments) or DEFAULT_SEGMENTS))
    for index=0,segments-1 do
        local angle=TAU*index/segments
        result[#result+1]=add(center,add(scale(right,math.cos(angle)*radiusX),
            scale(up,math.sin(angle)*radiusY)))
    end
    return result
end

local function nearestVertex(topology,target,used)
    local bestIndex,bestDistance=nil,math.huge
    for index,vertex in ipairs(topology and topology.vertices or {}) do
        if not used[index] then
            local distance=distanceSquared(vertex,target)
            if distance<bestDistance then bestIndex,bestDistance=index,distance end
        end
    end
    if bestIndex then return bestIndex end
    for index,vertex in ipairs(topology and topology.vertices or {}) do
        local distance=distanceSquared(vertex,target)
        if distance<bestDistance then bestIndex,bestDistance=index,distance end
    end
    return bestIndex
end

local function assignTargets(region,targets,topology,reuseExisting)
    local points,used={},{}
    for index,target in ipairs(targets) do
        local vertexIndex
        local existing=reuseExisting and region.points and region.points[index]
        -- During a drag, preserve the authored vertex anchors. Re-scanning the
        -- complete mesh for every mouse event makes large meshes unusable.
        if existing and topology.vertices[existing.index] and not used[existing.index] then
            vertexIndex=existing.index
        else
            vertexIndex=nearestVertex(topology,target,used)
        end
        if vertexIndex then
            used[vertexIndex]=true
            local vertex=topology.vertices[vertexIndex]
            local offset=subtract(target,vertex)
            points[index]={index=vertexIndex,offset=offset}
        end
    end
    region.points=points
    region.closed=true
    region.derivedVertexWeights={}
    region.geometryFingerprint=''
    region.topologySignature=topology.signature or ''
end

local function normalizeParameters(parameters,forceSquare)
    parameters.radiusX=math.max(tonumber(parameters.radiusX) or 1e-5,1e-5)
    parameters.radiusY=math.max(tonumber(parameters.radiusY) or 1e-5,1e-5)
    if forceSquare and parameters.type=='square' then
        local radius=math.max(parameters.radiusX,parameters.radiusY)
        parameters.radiusX,parameters.radiusY=radius,radius
    end
    return parameters
end

function M.normalize(region)
    if type(region)~='table' then return nil end
    ensureShape(region)
    return region
end

function M.new(typeName,isFree)
    local kind=typeName=='square' and 'square' or typeName=='ellipse' and 'ellipse' or 'polygon'
    return {type=kind,free=kind=='polygon' and true or isFree==true,segments=DEFAULT_SEGMENTS}
end

function M.type(region) return shapeType(region) end
function M.isFree(region) return shapeFree(region) end
function M.isParametric(region)
    local kind=shapeType(region)
    return (kind=='square' or kind=='ellipse') and not shapeFree(region)
end
function M.cameraBasis(camera) return cameraBasis(camera) end
function M.parameters(region,topology,camera) return captureParameters(region,topology,camera) end
function M.intersectCameraPlane(origin,direction,point,camera)
    return intersectCameraPlane(origin,direction,point,camera)
end

function M.seed(region,center,topology,camera,extent)
    ensureShape(region)
    local right,up=cameraBasis(camera)
    local size=math.max(tonumber(extent) or 1,0.001)
    local radiusX=size*0.08
    local radiusY=(shapeType(region)=='square' or shapeType(region)=='polygon') and radiusX or size*0.06
    local parameters={type=shapeType(region),center=copyPoint(center),right=right,up=up,
        radiusX=radiusX,radiusY=radiusY,segments=region.shape.segments}
    normalizeParameters(parameters)
    assignTargets(region,targetPoints(parameters),topology)
    return parameters
end

function M.handles(region,topology,camera)
    if not M.isParametric(region) then return {} end
    local parameters=captureParameters(region,topology,camera)
    if not parameters then return {} end
    local result={{id='center',kind='center',position=parameters.center}}
    if parameters.type=='square' then
        local locations={{'uniform',parameters.radiusX,parameters.radiusY},
            {'width',parameters.radiusX,0},{'height',0,parameters.radiusY}}
        for _,entry in ipairs(locations) do
            result[#result+1]={id=entry[1],kind='resize',position=add(parameters.center,
                add(scale(parameters.right,entry[2]),scale(parameters.up,entry[3])))}
        end
    else
        local locations={{'uniform',parameters.radiusX*0.707,parameters.radiusY*0.707},
            {'width',parameters.radiusX,0},{'height',0,parameters.radiusY}}
        for _,entry in ipairs(locations) do
            result[#result+1]={id=entry[1],kind='resize',position=add(parameters.center,
                add(scale(parameters.right,entry[2]),scale(parameters.up,entry[3])))}
        end
    end
    return result
end

function M.nearestHandle(handles,point,maxDistance)
    local best,bestDistance=nil,(tonumber(maxDistance) or math.huge)^2
    for _,handle in ipairs(handles or {}) do
        local distance=distanceSquared(handle.position,point)
        if distance<=bestDistance then best,bestDistance=handle,distance end
    end
    return best
end

function M.nearestHandleOnRay(handles,origin,direction,maxDistance)
    if not origin or not direction then return nil end
    local directionLengthSquared=dot(direction,direction)
    if directionLengthSquared<=1e-12 then return nil end
    local best,bestDistance=nil,(tonumber(maxDistance) or math.huge)^2
    for _,handle in ipairs(handles or {}) do
        local relative=subtract(handle.position,origin)
        local amount=math.max(0,dot(relative,direction)/directionLengthSquared)
        local closest=add(origin,scale(direction,amount))
        local distance=distanceSquared(handle.position,closest)
        if distance<=bestDistance then
            best,bestDistance=handle,distance
        end
    end
    return best
end

function M.applyCenterDrag(region,base,newCenter,topology)
    if not base or not newCenter then return false end
    local parameters={type=base.type,center=copyPoint(newCenter),right=base.right,up=base.up,
        radiusX=base.radiusX,radiusY=base.radiusY,segments=base.segments}
    assignTargets(region,targetPoints(parameters),topology,true)
    return true
end

function M.applyResize(region,base,handleId,point,topology)
    if not base or not point then return false end
    local relative=subtract(point,base.center)
    local x,y=dot(relative,base.right),dot(relative,base.up)
    local parameters={type=base.type,center=copyPoint(base.center),right=base.right,up=base.up,
        radiusX=base.radiusX,radiusY=base.radiusY,segments=base.segments}
    if handleId=='uniform' and base.type=='square' then
        local radius=math.max(math.abs(x),math.abs(y),1e-5)
        parameters.radiusX,parameters.radiusY=radius,radius
    elseif handleId=='uniform' then
        local scaleX=math.abs(x)/base.radiusX
        local scaleY=math.abs(y)/base.radiusY
        local factor=math.max(scaleX,scaleY,1e-4)
        parameters.radiusX=base.radiusX*factor
        parameters.radiusY=base.radiusY*factor
    elseif handleId=='width' then
        parameters.radiusX=math.max(math.abs(x),1e-5)
    elseif handleId=='height' then
        parameters.radiusY=math.max(math.abs(y),1e-5)
    end
    normalizeParameters(parameters)
    assignTargets(region,targetPoints(parameters),topology,true)
    return true
end

function M.change(region,typeName,isFree,topology,camera)
    local kind=typeName=='square' and 'square' or typeName=='ellipse' and 'ellipse' or 'polygon'
    -- Capture the current contour before changing the mode. This also makes
    -- the transition from a free square/ellipse back to a constrained shape
    -- deterministic instead of leaving the old arbitrary contour in place.
    local parameters=captureParameters(region,topology,camera)
    region.shape=M.new(kind,isFree)
    if kind=='polygon' or isFree==true then
        region.closed=#(region.points or {})>=3
        region.derivedVertexWeights={}
        region.geometryFingerprint=''
        return true
    end
    if parameters then
        parameters.type=kind
        parameters.segments=region.shape.segments
        normalizeParameters(parameters,true)
        assignTargets(region,targetPoints(parameters),topology)
    else
        region.closed=false
    end
    return true
end

return M
