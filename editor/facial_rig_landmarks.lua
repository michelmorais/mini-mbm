--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to     |
| permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Facial landmark helpers built on the generic MSH topology module.
]]--

local tTopology=require 'morph_surface_topology'
local M={
    inspect=tTopology.inspect,
    intersectRaySurface=tTopology.intersectRaySurface,
    nearestSurfaceVertex=tTopology.nearestSurfaceVertex,
    nearestVertexOnSurfaceHit=tTopology.nearestVertexOnSurfaceHit,
    nearestVertex=tTopology.nearestVertex,
    pointForVertex=tTopology.pointForVertex
}

function M.nearestLandmark(landmarks,origin,direction,maxDistance)
    if not landmarks or not origin or not direction then return nil end
    local directionLength=math.sqrt(direction.x*direction.x+direction.y*direction.y+
        direction.z*direction.z)
    if directionLength<=1e-8 then return nil end
    local dx,dy,dz=direction.x/directionLength,direction.y/directionLength,direction.z/directionLength
    local limitSquared=(tonumber(maxDistance) or math.huge)^2
    local bestIndex,bestDistance,bestDepth=nil,math.huge,math.huge
    for index,landmark in ipairs(landmarks) do
        local point=landmark.placed and landmark.position
        if point then
            local ox,oy,oz=point.x-origin.x,point.y-origin.y,point.z-origin.z
            local depth=ox*dx+oy*dy+oz*dz
            if depth>=0 then
                local hx,hy,hz=origin.x+dx*depth,origin.y+dy*depth,origin.z+dz*depth
                local px,py,pz=point.x-hx,point.y-hy,point.z-hz
                local distance=px*px+py*py+pz*pz
                if distance<=limitSquared and (distance<bestDistance or
                        distance==bestDistance and depth<bestDepth) then
                    bestIndex,bestDistance,bestDepth=index,distance,depth
                end
            end
        end
    end
    return bestIndex and {index=bestIndex,landmark=landmarks[bestIndex],
        distance=math.sqrt(bestDistance)} or nil
end

return M
