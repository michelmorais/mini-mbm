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

package.path='editor/?.lua;'..package.path
local Asset=require 'image_mesh_asset'
local function data(asset,report)
    assert(asset:check())
    local vertices,indices=Asset.geometry(asset)
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
    for _,e in pairs(edges) do assert(e.n==2 and e.balance==0,'open/nonmanifold side mesh') end
    assert(volume>0)
    return vertices,indices
end

local function backCount(asset,depth)
    local vertices,indices=data(asset)
    local count=0
    for i=1,#indices,3 do
        local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
        local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
        local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
        assert((uy*vz-uz*vy)^2+(uz*vx-ux*vz)^2+(ux*vy-uy*vx)^2>1e-16,'degenerate triangle')
        if a.z==depth/2 and b.z==depth/2 and c.z==depth/2 then count=count+1 end
    end
    return count
end
local function run()
    local pixels={}
    for y=0,32 do for x=0,32 do
        local value=(x+y)%3==0 and 255 or 0
        pixels[#pixels+1]=value;pixels[#pixels+1]=value;pixels[#pixels+1]=value
    end end
    local path='/tmp/ime_minimal_back.png'
    assert(mbm.createTexture(pixels,33,33,3,'ime_minimal_back',path))
    local shapes={
        {shape='rectangle',count=2},
        {shape='polygon',count=3,contour={{x=0,y=0},{x=1,y=0},{x=.5,y=.5},{x=1,y=1},{x=0,y=1}}},
        {shape='ellipse',count=14}}
    for _,adaptive in ipairs{false,true} do for _,shape in ipairs(shapes) do
        for _,mode in ipairs{'edge','color','band'} do
            local o={shape=shape.shape,contour=shape.contour,ellipseSegments=16,columns=8,rows=8,
                followImage=adaptive,depth=10,relief=20,lockBorder=false,sideMode=mode,sideInset=1}
            local asset,report=mbm.generateImageMesh(path,o);assert(asset,report)
            assert(backCount(asset,10)==shape.count,'back is not minimal')
            local vertices,indices=Asset.geometry(asset)
            assert(#vertices==report.vertices and #indices==report.triangles*3)
            -- Relief copying keeps the same front despite changing rear topology.
            o.backRelief=true
            local original,message=mbm.generateImageMesh(path,o);assert(original,message)
            data(original)
            local before=Asset.geometry(original)
            for i,v in ipairs(vertices) do
                if v.z>0 then break end
                for _,k in ipairs{'x','y','z','u','v','nx','ny','nz'} do
                    assert(math.abs(v[k]-before[i][k])<1e-5,'front changed: '..k)
                end
            end
            local output='/tmp/ime_minimal_back.msh'
            assert(asset:save(output,false,false,true))
            local loaded=meshDebug:new();assert(loaded:load(output))
            assert(backCount(loaded,10)==shape.count,'roundtrip changed back')
            local simplified,why=loaded:simplify(.85,nil,1,true,0)
            assert(simplified,why)
            assert(backCount(loaded,10)==shape.count,'simplification changed minimal back')
        end
    end end
    local repeated,message=mbm.generateImageMesh(path,{columns=8,rows=8,depth=10,sideMode='repeat',sideTexture=path})
    assert(repeated,message);assert(backCount(repeated,10)>2,'repeat seams must be retained')
    -- Optional user fixture; source assets are read-only and no project is rewritten.
    local projectPath=os.getenv('MBM_IMAGE_MESH_TEST_PROJECT')
    if projectPath then
        local project=require('image_mesh_io').load(projectPath)
        local options=require('image_mesh_model').options(project,project.regions[1])
        local asset,report=mbm.generateImageMesh(project.image.path,options);assert(asset,report)
        assert(backCount(asset,options.depth)==2)
        local simplified,why=asset:simplify(options.simplifyRatio,nil,1,options.simplifyDetails,options.simplifyBoundary)
        assert(simplified,why)
        assert(backCount(asset,options.depth)==2)
    end
    print('IMAGE MESH MINIMAL BACK / CLOSED STRIPS / UNCHANGED FRONT / ROUNDTRIP OK')
end
function onInitScene()
    local ok,message=xpcall(run,debug.traceback)
    if not ok then print('IMAGE MESH MINIMAL BACK FAIL: '..tostring(message)) end
    mbm.quit()
end
