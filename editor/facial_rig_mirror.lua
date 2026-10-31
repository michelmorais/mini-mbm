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
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  |
| OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                                                          |
|------------------------------------------------------------------------------------------------------------------------|

   Reflect a dense facial target across the X face plane using neutral vertex positions.
   Ambiguous coincident source vertices with different deltas are rejected rather than mixed.
]]--

local M={}

function M.pairedName(name)
    local base,side=name:match('^(.*)([Ll]eft)$')
    if base then return base .. (side=='Left' and 'Right' or 'right') end
    base,side=name:match('^(.*)([Rr]ight)$')
    if base then return base .. (side=='Right' and 'Left' or 'left') end
    local prefix,separator,letter=name:match('^(.*)([_%.%-])([LR])$')
    if prefix then return prefix..separator..(letter=='L' and 'R' or 'L') end
    prefix,letter=name:match('^(.*)_([lr])$')
    if prefix then return prefix..'_'..(letter=='l' and 'r' or 'l') end
    return name..'_mirror'
end

local function key(x,y,z,scale)
    return math.floor(x/scale+0.5)..':'..math.floor(y/scale+0.5)..':'..
        math.floor(z/scale+0.5)
end

local function nonzero(delta)
    return delta and (math.abs(delta.x or 0)>1e-8 or math.abs(delta.y or 0)>1e-8 or
        math.abs(delta.z or 0)>1e-8)
end

local function differs(first,second,tolerance)
    first=first or {x=0,y=0,z=0}
    second=second or {x=0,y=0,z=0}
    return math.abs((first.x or 0)-(second.x or 0))>tolerance or
        math.abs((first.y or 0)-(second.y or 0))>tolerance or
        math.abs((first.z or 0)-(second.z or 0))>tolerance
end

function M.build(target,vertices,planeX,tolerance)
    if not target or type(vertices)~='table' or #vertices==0 or
            type(planeX)~='number' or planeX~=planeX or
            type(tolerance)~='number' or tolerance<=0 then return nil,'invalid' end
    local positions=target.positionDeltas or {}
    local normals=target.normalDeltas or {}
    if #positions~=#vertices or (#normals>0 and #normals~=#vertices) then return nil,'invalid' end
    local buckets={}
    for index,point in ipairs(vertices) do
        local bucketKey=key(point.x,point.y,point.z,tolerance)
        local bucket=buckets[bucketKey]
        if not bucket then bucket={}; buckets[bucketKey]=bucket end
        bucket[#bucket+1]=index
    end
    local result={positions={},normals={},matched=0,totalActive=0,unmatchedActive=0,ambiguous=0,
        vertexCount=#vertices}
    local sourceUsed={}
    local epsilon=math.max(1e-7,tolerance*0.01)
    for destination,point in ipairs(vertices) do
        local x,y,z=2*planeX-point.x,point.y,point.z
        local qx,qy,qz=math.floor(x/tolerance+0.5),
            math.floor(y/tolerance+0.5),math.floor(z/tolerance+0.5)
        local best,bestDistance,candidates=nil,tolerance*tolerance,{}
        for dx=-1,1 do for dy=-1,1 do for dz=-1,1 do
            local bucket=buckets[(qx+dx)..':'..(qy+dy)..':'..(qz+dz)]
            for _,source in ipairs(bucket or {}) do
                local candidate=vertices[source]
                local distance=(candidate.x-x)^2+(candidate.y-y)^2+(candidate.z-z)^2
                if distance<=tolerance*tolerance then
                    candidates[#candidates+1]=source
                    if distance<=bestDistance then best=source; bestDistance=distance end
                end
            end
        end end end
        if best then
            local ambiguous=false
            for _,source in ipairs(candidates) do
                local candidate=vertices[source]
                local distance=(candidate.x-x)^2+(candidate.y-y)^2+(candidate.z-z)^2
                if distance<=bestDistance+tolerance*tolerance*1e-5 and
                        (differs(positions[best],positions[source],epsilon) or
                        #normals>0 and differs(normals[best],normals[source],epsilon)) then
                    ambiguous=true
                    break
                end
            end
            if ambiguous then result.ambiguous=result.ambiguous+1 end
            for _,source in ipairs(candidates) do sourceUsed[source]=true end
            local delta=positions[best]
            result.positions[destination]={x=-delta.x,y=delta.y,z=delta.z}
            if #normals>0 then
                local normal=normals[best]
                result.normals[destination]={x=-normal.x,y=normal.y,z=normal.z}
            end
            result.matched=result.matched+1
        else
            result.positions[destination]={x=0,y=0,z=0}
            if #normals>0 then result.normals[destination]={x=0,y=0,z=0} end
        end
    end
    for index=1,#vertices do
        if nonzero(positions[index]) or nonzero(normals[index]) then
            result.totalActive=result.totalActive+1
            if not sourceUsed[index] then result.unmatchedActive=result.unmatchedActive+1 end
        end
    end
    return result
end

return M
