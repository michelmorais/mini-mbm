--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation      |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ALL   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Procedural temporary assets for Skeletal Animation Editor tutorials.
]]--

local M={}

local function hexColor(value)
    local hex=tostring(value or ''):gsub('^#','')
    if #hex==6 then hex=hex..'FF' end
    if #hex~=8 or not hex:match('^[%x]+$') then return nil end
    return {
        tonumber(hex:sub(1,2),16),tonumber(hex:sub(3,4),16),
        tonumber(hex:sub(5,6),16),tonumber(hex:sub(7,8),16),
    }
end

local function buildCheckerPixels(width,height,colorA,colorB)
    local a,b=hexColor(colorA),hexColor(colorB)
    if not a or not b then return nil end
    local pixels={}
    local block=math.max(1,math.floor(math.min(width,height)/4))
    for y=0,height-1 do
        for x=0,width-1 do
            local useA=(math.floor(x/block)+math.floor(y/block))%2==0
            local color=useA and a or b
            for channel=1,4 do pixels[#pixels+1]=color[channel] end
        end
    end
    return pixels
end

function M.buildVerticalCylinder(radius,height,radialSegments,heightSegments)
    radius=radius or 30
    height=height or 240
    radialSegments=radialSegments or 16
    heightSegments=heightSegments or 12
    assert(radius>0 and height>0,'cylinder dimensions must be positive')
    assert(radialSegments>=3 and heightSegments>=1,'cylinder segments are invalid')
    local vertices,indices={},{}
    local ringSize=radialSegments+1
    for ring=0,heightSegments do
        local v=ring/heightSegments
        local y=-height*0.5+height*v
        for side=0,radialSegments do
            local u=side/radialSegments
            local angle=u*math.pi*2
            vertices[#vertices+1]={x=math.cos(angle)*radius,y=y,
                z=math.sin(angle)*radius,u=u,v=1-v}
        end
    end
    for ring=0,heightSegments-1 do
        for side=0,radialSegments-1 do
            local a=ring*ringSize+side+1
            local b=a+1
            local c=a+ringSize
            local d=c+1
            indices[#indices+1]=a
            indices[#indices+1]=c
            indices[#indices+1]=b
            indices[#indices+1]=b
            indices[#indices+1]=c
            indices[#indices+1]=d
        end
    end
    local function addCap(y,isTop)
        local center=#vertices+1
        vertices[center]={x=0,y=y,z=0,u=0.5,v=0.5}
        local first=#vertices+1
        for side=0,radialSegments do
            local angle=(side/radialSegments)*math.pi*2
            local x,z=math.cos(angle)*radius,math.sin(angle)*radius
            vertices[#vertices+1]={x=x,y=y,z=z,u=0.5+x/(radius*2),v=0.5+z/(radius*2)}
        end
        for side=0,radialSegments-1 do
            local current=first+side
            local nextVertex=current+1
            indices[#indices+1]=center
            indices[#indices+1]=isTop and nextVertex or current
            indices[#indices+1]=isTop and current or nextVertex
        end
    end
    addCap(-height*0.5,false)
    addCap(height*0.5,true)
    return vertices,indices
end

function M.createWormCylinder(tUtil)
    if not tUtil or type(tUtil.getTemporaryFilePath)~='function' then
        return nil,'temporary-path utility is unavailable'
    end
    local texturePath=tUtil.getTemporaryFilePath('.png')
    local meshPath=tUtil.getTemporaryFilePath('.msh')
    local pixels=buildCheckerPixels(16,16,'#38C6A3FF','#174A68FF')
    local savedTexture=mbm.createTexture(pixels,16,16,4,texturePath,texturePath)
    if not savedTexture then return nil,'could not create the temporary tutorial texture' end

    local meshD=meshDebug:new()
    meshD:setType('mesh')
    meshD:setModeFrontFace('CW')
    local frame=meshD:addFrame(3)
    local subset=meshD:addSubSet(frame)
    local vertices,indices=M.buildVerticalCylinder(30,240,16,12)
    if not meshD:addVertex(frame,subset,vertices) then return nil,'could not add cylinder vertices' end
    if not meshD:addIndex(frame,subset,indices) then return nil,'could not add cylinder indices' end
    meshD:setTexture(frame,subset,savedTexture)
    meshD:addNormals()
    if not meshD:addAnim('Static',1,1,1.0,0) then return nil,'could not add the static mesh animation' end
    local valid,validationError=meshD:check()
    if not valid then return nil,validationError or 'generated cylinder validation failed' end
    if not meshD:save(meshPath,false,false) then return nil,'could not save the temporary tutorial mesh' end
    return meshPath,nil,{texturePath=savedTexture,vertices=#vertices,triangles=#indices/3}
end

return M
