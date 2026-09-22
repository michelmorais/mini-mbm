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

-- Tiny paint footprints must survive coarse topology and a permissive global tolerance.
local function data(asset,report)
    assert(asset:check())
    local vertices=asset:getVertex(1,1,1,report.vertices)
    local indices=asset:getIndex(1,1)
    local edges={};local volume=0
    local function key(v) return string.format('%.5f,%.5f,%.5f',v.x+0.,v.y+0.,v.z+0.) end
    for _,v in ipairs(vertices) do
        assert(v.x==v.x and v.y==v.y and v.z==v.z)
        assert(math.abs(v.nx*v.nx+v.ny*v.ny+v.nz*v.nz-1)<1e-4)
    end
    for i=1,#indices,3 do
        local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
        volume=volume+(a.x*(b.y*c.z-b.z*c.y)+a.y*(b.z*c.x-b.x*c.z)+a.z*(b.x*c.y-b.y*c.x))/6
        for _,pair in ipairs{{a,b},{b,c},{c,a}} do
            local ka,kb=key(pair[1]),key(pair[2]);assert(ka~=kb,'degenerate edge')
            local k=ka<kb and ka..'/'..kb or kb..'/'..ka
            local e=edges[k] or {n=0,balance=0};edges[k]=e
            e.n=e.n+1;e.balance=e.balance+(ka<kb and 1 or -1)
        end
    end
    for _,e in pairs(edges) do assert(e.n==2 and e.balance==0,'open/nonmanifold painted mesh') end
    assert(volume>0)
    return vertices,indices
end
local function height(vertices,indices,u,v)
    local x,y=(u-.5)*100,(.5-v)*80
    local best=math.huge
    for i=1,#indices,3 do
        local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
        local det=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y)
        if math.abs(det)>1e-10 then
            local wa=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/det
            local wb=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/det
            if wa>=-1e-7 and wb>=-1e-7 and wa+wb<=1.0000001 then
                best=math.min(best,wa*a.z+wb*b.z+(1-wa-wb)*c.z)
            end
        end
    end
    assert(best<math.huge,'missing front triangle')
    return (-best-10)/8
end
function onInitScene()
    local ok,err=pcall(function()
        local pixels={};for i=1,129*97*3 do pixels[i]=0 end
        local path='/tmp/ime_local_paint_source.png'
        assert(mbm.createTexture(pixels,129,97,3,'ime_local_paint_source',path))
        for _,shape in ipairs{'rectangle','ellipse','polygon'} do
            local o={shape=shape,columns=4,rows=4,followImage=true,heightTolerance=.5,
                twoLevels=true,lockBorder=false,width=100,height=80,depth=20,relief=8,
                contour={{x=0,y=0},{x=1,y=0},{x=1,y=.5},{x=.5,y=.5},{x=.5,y=1},{x=0,y=1}}}
            local original,base=mbm.generateImageMesh(path,o);assert(original,base)
            local dab={x=43/128,y=39/96,radius=2.2/96,strength=1,height=1,mode='raise'}
            o.heightEdits={dab}
            local asset,report=mbm.generateImageMesh(path,o);assert(asset,report)
            local vv,ii=data(asset,report)
            assert(report.triangles>base.triangles and report.triangles<2500,'local refinement count')
            for y=37,41 do for x=41,45 do
                local f=math.max(0,1-math.sqrt((x-43)^2+(y-39)^2)/2.2)
                local expected=f*f*(3-2*f)
                local measured=height(vv,ii,x/128,y/96)
                assert(math.abs(measured-expected)<.035,string.format('painted height not represented: %s pixel %d,%d expected %.6f actual %.6f',shape,x,y,expected,measured))
            end end
            assert(math.abs(height(vv,ii,.25,.25))<.001,'height changed outside painted area')
            local again,againReport=mbm.generateImageMesh(path,o);assert(again,againReport)
            assert(againReport.vertices==report.vertices)
            for i,v in ipairs(again:getVertex(1,1,1,report.vertices)) do
                for _,k in ipairs{'x','y','z','nx','ny','nz','u','v'} do assert(v[k]==vv[i][k],'nondeterministic paint') end
            end
            o.maxVertices=base.vertices+8
            local rejected,message=mbm.generateImageMesh(path,o)
            assert(not rejected and message:find('budget') and message:find('limit'),'paint budget bypassed')
            o.maxVertices=nil;dab.mode='lower'
            local noop,nr=mbm.generateImageMesh(path,o);assert(noop,nr)
            assert(nr.vertices==base.vertices and nr.triangles==base.triangles,'no-effect paint changed topology')
            print('LOCAL PAINT '..shape..' / ACCURACY / CLOSED / BUDGET / REPEATABLE / NOOP OK',report.triangles)
        end
        -- Negative corrections use the same raster index and must refine a small depression.
        for i=1,#pixels do pixels[i]=255 end
        local white='/tmp/ime_local_paint_white.png'
        assert(mbm.createTexture(pixels,129,97,3,'ime_local_paint_white',white))
        local lowerOptions={columns=4,rows=4,followImage=true,heightTolerance=.5,twoLevels=true,
            lockBorder=false,width=100,height=80,depth=20,relief=8,
            heightEdits={{x=43/128,y=39/96,radius=2.2/96,strength=1,height=0,mode='lower'}}}
        local depression,dr=mbm.generateImageMesh(white,lowerOptions);assert(depression,dr)
        local dv,di=data(depression,dr)
        assert(height(dv,di,43/128,39/96)<.035,'negative correction missed')
        assert(math.abs(height(dv,di,.25,.25)-1)<.001,'depression spread outside paint')
        -- Filled pixels next to an existing plateau must stay flat BETWEEN pixels too.
        local edgePixels={}
        for y=0,96 do for x=0,128 do for c=1,3 do edgePixels[#edgePixels+1]=x>43 and 255 or 0 end end end
        local edgePath='/tmp/ime_local_paint_edge.png'
        assert(mbm.createTexture(edgePixels,129,97,3,'ime_local_paint_edge',edgePath))
        local edgeOptions={columns=4,rows=4,followImage=true,heightTolerance=.5,twoLevels=true,
            grooveTransition=.1,lockBorder=false,width=100,height=80,depth=20,relief=8,
            heightEdits={{x=43/128,y=39/96,radius=.001,strength=1,height=1,mode='flatten'},
                         {x=43/128,y=40/96,radius=.001,strength=1,height=1,mode='flatten'}}}
        local filled,fr=mbm.generateImageMesh(edgePath,edgeOptions);assert(filled,fr)
        local fv,fi=data(filled,fr)
        for _,x in ipairs{43,43.25,43.5,43.75,44} do
            assert(math.abs(height(fv,fi,x/128,39.5/96)-1)<.035,'flat painted raster rippled between pixels')
        end
        local originalOptions={columns=30,rows=30,followImage=true,twoLevels=true,
            grooveTransition=.1,lockBorder=false,width=100,height=80,depth=20,relief=8,
            heightEdits=edgeOptions.heightEdits}
        local plateau,pr=mbm.generateImageMesh(edgePath,originalOptions);assert(plateau,pr)
        local pv,pi=data(plateau,pr);local plateaus=0
        for i=1,#pi,3 do
            local a,b,c=pv[pi[i]],pv[pi[i+1]],pv[pi[i+2]]
            if math.abs(a.z+18)<1e-5 and math.abs(b.z+18)<1e-5 and math.abs(c.z+18)<1e-5 then
                plateaus=plateaus+1
                for _,v in ipairs{a,b,c} do assert(v.nz<-.999 and math.abs(v.nx)<.01 and math.abs(v.ny)<.01,string.format('plateau normal lost after painting at %.8f,%.8f,%.8f normal %.5f,%.5f,%.5f',v.x,v.y,v.z,v.nx,v.ny,v.nz)) end
            end
        end
        assert(plateaus>0)
        print('PAINTED RASTER INTERPOLATION / PLATEAU NORMALS OK')
        print('IMAGE MESH PAINT REFINEMENT OK')
    end)
    if not ok then print('PAINT REFINEMENT FAIL '..tostring(err)) end
    mbm.quit()
end
