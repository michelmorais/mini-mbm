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

-- Run from repository root with mini-mbm --scene ... --disable_select_monitor.
-- Regression: accurate vertex heights are insufficient if triangles cross ramps.
local function run()
    local samples={}
    for x=0,128 do
        local t=x%32
        samples[x]=math.floor(math.max(0,math.min(1,(t-6)/4,(26-t)/4))*255+.5)
    end
    local function expected(pixel)
        pixel=math.max(0,math.min(128,pixel))
        local x=math.floor(pixel);local f=pixel-x
        return 10+8*(samples[x]*(1-f)+samples[math.min(x+1,128)]*f)/255
    end
    for _,horizontal in ipairs({false,true}) do
        local pixels={}
        for y=0,128 do for x=0,128 do
            local c=samples[horizontal and y or x]
            pixels[#pixels+1]=c;pixels[#pixels+1]=c;pixels[#pixels+1]=c;pixels[#pixels+1]=255
        end end
        local name=horizontal and 'horizontal' or 'vertical'
        local path='/tmp/mbm_relief_regression_'..name..'.png'
        assert(mbm.createTexture(pixels,129,129,4,'relief_'..name,path))
        local asset,report=mbm.generateImageMesh(path,{shape='ellipse',ellipseSegments=36,
            width=100,height=100,depth=20,relief=8,columns=30,rows=30,
            followImage=true,twoLevels=false,smoothPasses=0,lockBorder=false,heightTolerance=.26})
        assert(asset,report)
        local vertices=asset:getVertex(1,1,1,report.vertices);local ids=asset:getIndex(1,1)
        local worst,front=0,0
        for i=1,#ids,3 do
            local a,b,c=vertices[ids[i]],vertices[ids[i+1]],vertices[ids[i+2]]
            local nz=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)
            if nz<0 and a.z<=-10 and b.z<=-10 and c.z<=-10 then
                front=front+1
                for _,weights in ipairs({{1/3,1/3},{.5,0},{0,.5},{.5,.5},{.2,.2},{.6,.2},{.2,.6}}) do
                    local u,v=weights[1],weights[2];local w=1-u-v
                    local coord=horizontal and (.5-(a.y*u+b.y*v+c.y*w)/100) or (.5+(a.x*u+b.x*v+c.x*w)/100)
                    local actual=-(a.z*u+b.z*v+c.z*w)
                    worst=math.max(worst,math.abs(actual-expected(coord*128)))
                end
            end
        end
        assert(front>0)
        assert(worst<.03,'ramp interpolation error: '..worst)
        assert(report.vertices<=65535)
        print('RELIEF '..name..' front='..front..' maxSampleError='..worst)
        -- Two-height shading: plateau corners must not average ramp normals.
        local options={shape='ellipse',ellipseSegments=36,width=100,height=100,depth=20,
            relief=8,columns=30,rows=30,followImage=true,twoLevels=true,
            grooveThreshold=.5,grooveTransition=.15,lockBorder=false}
        local split,sr=mbm.generateImageMesh(path,options);assert(split,sr)
        local sv=split:getVertex(1,1,1,sr.vertices);local si=split:getIndex(1,1)
        local plateauFaces=0
        for i=1,#si,3 do
            local a,b,c=sv[si[i]],sv[si[i+1]],sv[si[i+2]]
            if math.abs(a.z+18)<1e-5 and math.abs(b.z+18)<1e-5 and math.abs(c.z+18)<1e-5 then
                plateauFaces=plateauFaces+1
                for _,v in ipairs({a,b,c}) do
                    assert(math.abs(v.nx)<.01 and math.abs(v.ny)<.01 and v.nz<-.999,
                        'plateau normal contaminated by groove wall')
                end
            end
        end
        assert(plateauFaces>0)
        -- Shared topology still supports aggressive simplification. Budget uses
        -- the actual vertex count, without adding normal-seam copies.
        options.maxVertices=sr.vertices
        local exact,er=mbm.generateImageMesh(path,options);assert(exact,er)
        assert(er.vertices==sr.vertices and er.triangles==sr.triangles)
        print('PLATEAU NORMALS '..name..' faces='..plateauFaces..' vertices='..sr.vertices)
        os.remove(path)
    end
end
function onInitScene()
    local ok,err=pcall(run)
    if ok then print('IMAGE MESH RELIEF REGRESSION OK')
    else print('IMAGE MESH RELIEF REGRESSION FAIL: '..tostring(err)) end
    mbm.quit()
end
function onLoop() mbm.quit() end
