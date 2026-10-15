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
local Model=require 'image_mesh_model'
local IO=require 'image_mesh_io'
local function inspect(asset,report)
    assert(asset:check())
    local edges,volume,total,triangles={},0,0,0
    local function key(v)
        local function q(x) return math.floor(x*1000000+.5) end
        return q(v.x)..','..q(v.y)..','..q(v.z)
    end
    for subset=1,asset:getTotalSubset(1) do
        local vertices=asset:getVertex(1,subset,1,asset:getTotalVertex(1,subset))
        local indices=asset:getIndex(1,subset)
        total=total+#vertices;triangles=triangles+#indices/3
        for _,v in ipairs(vertices) do
            assert(v.x==v.x and v.y==v.y and v.z==v.z and v.u==v.u and v.v==v.v)
            assert(math.abs(v.nx*v.nx+v.ny*v.ny+v.nz*v.nz-1)<.001,'normal not unit')
        end
        for i=1,#indices,3 do
            local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
            assert(a and b and c,'index outside subset')
            volume=volume+(a.x*(b.y*c.z-b.z*c.y)+a.y*(b.z*c.x-b.x*c.z)+a.z*(b.x*c.y-b.y*c.x))/6
            for _,pair in ipairs{{a,b},{b,c},{c,a}} do
                local ka,kb=key(pair[1]),key(pair[2]);assert(ka~=kb,'zero edge '..ka..' / '..string.format('%.9f %.9f %.9f / %.9f %.9f %.9f',pair[1].x,pair[1].y,pair[1].z,pair[2].x,pair[2].y,pair[2].z))
                local k=ka<kb and ka..'/'..kb or kb..'/'..ka
                local e=edges[k] or {n=0,b=0};edges[k]=e;e.n=e.n+1;e.b=e.b+(ka<kb and 1 or -1)
            end
        end
    end
    for k,e in pairs(edges) do assert(e.n==2 and e.b==0,'open/nonmanifold edge '..k..' count '..e.n) end
    assert(volume>0,'inverted volume')
    assert(total==report.vertices and triangles==report.triangles)
    return volume
end
local function run()
    local pixels={};for i=1,65*65*3 do pixels[i]=128 end
    local path='/tmp/ime_bevel.png';assert(mbm.createTexture(pixels,65,65,3,'ime_bevel',path))
    local o={width=100,height=80,depth=20,relief=0,columns=4,rows=4,bevel=true,bevelWidth=2}
    for mask=1,7 do
        o.bevelFront=mask%2==1;o.bevelBack=math.floor(mask/2)%2==1;o.bevelCorners=mask>=4
        for _,mode in ipairs{'edge','color','band','repeat'} do
            o.sideMode=mode;print('CASE',mask,mode)
            local asset,r=mbm.generateImageMesh(path,o);assert(asset,r)
            local volume=inspect(asset,r);assert(volume<160000)
        end
    end
    o.bevelFront=true;o.bevelBack=true;o.bevelCorners=true;o.sideMode='color';o.backSolid=true
    for _,shape in ipairs{'rectangle','polygon','ellipse'} do
        o.shape=shape;o.ellipseSegments=8;o.contour={{x=.1,y=.1},{x=.9,y=.2},{x=.8,y=.9},{x=.2,y=.8}}
        for _,adaptive in ipairs{false,true} do
            o.followImage=adaptive;o.relief=8;print('CASE',shape,adaptive)
            local asset,r=mbm.generateImageMesh(path,o);assert(asset,r);inspect(asset,r)
        end
    end
    o.shape='rectangle';o.followImage=false;o.relief=0
    o.bevelRounded=false
    local flat,flatReport=mbm.generateImageMesh(path,o);assert(flat,flatReport)
    local flatVolume=inspect(flat,flatReport)
    o.bevelRounded=true
    local previous=flatVolume
    for _,segments in ipairs{2,4,8} do
        o.bevelSegments=segments
        local rounded,rr=mbm.generateImageMesh(path,o);assert(rounded,rr)
        local volume=inspect(rounded,rr)
        assert(volume>previous and volume<160000,'rounded profile does not retain the expected volume')
        previous=volume
        local smooth=0
        for subset=1,rounded:getTotalSubset(1) do
            for _,v in ipairs(rounded:getVertex(1,subset,1,rounded:getTotalVertex(1,subset))) do
                if v.x>48.001 and v.z< -8.001 and math.abs(v.ny)<.00001 then
                    local x,z=v.x-48,v.z+8;local length=math.sqrt(x*x+z*z)
                    assert(math.abs(v.nx-x/length)<.001 and math.abs(v.nz-z/length)<.001 and math.abs(v.ny)<.001,'rounded normals are faceted')
                    smooth=smooth+1
                end
            end
        end
        assert(smooth>0,'rounded normal samples missing')
        assert(rr.triangles>flatReport.triangles,'rounded profile did not add geometry')
    end
    for _,shape in ipairs{'rectangle','polygon','ellipse'} do
        o.shape=shape;o.followImage=true;o.relief=8;o.bevelSegments=4
        local rounded,rr=mbm.generateImageMesh(path,o);assert(rounded,rr);inspect(rounded,rr)
    end
    o.shape='rectangle';o.followImage=false;o.relief=0
    for mask=1,7 do
        o.bevelFront=mask%2==1;o.bevelBack=math.floor(mask/2)%2==1;o.bevelCorners=mask>=4
        o.bevelWidth=8;o.bevelSegments=4
        local rounded,rr=mbm.generateImageMesh(path,o);assert(rounded,rr);inspect(rounded,rr)
    end
    o.bevelFront=true;o.bevelBack=true;o.bevelCorners=true;o.bevelWidth=2
    for _,segments in ipairs{0,1,9} do
        o.bevelSegments=segments;assert(not mbm.generateImageMesh(path,o),'invalid rounded segment count accepted')
    end
    o.bevelSegments=4
    for _,mode in ipairs{'edge','color','band','repeat'} do
        o.sideMode=mode
        local rounded,rr=mbm.generateImageMesh(path,o);assert(rounded,rr);inspect(rounded,rr)
        for subset=1,rounded:getTotalSubset(1) do
            for _,v in ipairs(rounded:getVertex(1,subset,1,rounded:getTotalVertex(1,subset))) do
                assert(v.u>=0 and v.u<=1 and v.v>=0 and v.v<=1,'rounded UV outside source texture')
            end
        end
    end
    o.sideMode='color';o.relief=8;o.bevelWidth=8
    local reliefMesh,reliefReport=mbm.generateImageMesh(path,o);assert(reliefMesh,reliefReport);inspect(reliefMesh,reliefReport)
    for subset=1,reliefMesh:getTotalSubset(1) do
        for _,v in ipairs(reliefMesh:getVertex(1,subset,1,reliefMesh:getTotalVertex(1,subset))) do
            if v.x>20 and v.y>0 and v.z< -10 and v.ny==0 then
                assert(v.nx>=-.0001,'rounded relief normal inverted toward the interior')
            end
        end
    end
    o.relief=0;o.bevelWidth=2;o.bevelRounded=false
    local asset,r=mbm.generateImageMesh(path,o);assert(asset,r)
    o.maxVertices=r.vertices-1;assert(not mbm.generateImageMesh(path,o));o.maxVertices=nil
    o.maxTriangles=r.triangles-1;assert(not mbm.generateImageMesh(path,o));o.maxTriangles=nil
    for _,bad in ipairs{{bevelWidth=10},{bevelWidth=0},{backOpen=true,backSolid=false},{voxelized=true},{heightSource='curved'}} do
        local options=Model.copy(o);for k,v in pairs(bad) do options[k]=v end
        assert(not mbm.generateImageMesh(path,options),'unsupported bevel accepted')
    end
    -- Concavities, multiple holes and varying border heights must remain closed.
    for _,rounded in ipairs{false,true} do
        for _,case in ipairs{
            {shape='polygon',contour={{x=0,y=0},{x=1,y=0},{x=.5,y=.5},{x=1,y=1},{x=0,y=1}}},
            {holes={{{x=.2,y=.2},{x=.4,y=.2},{x=.4,y=.4},{x=.2,y=.4}},{{x=.6,y=.6},{x=.8,y=.6},{x=.8,y=.8},{x=.6,y=.8}}}},
            {lockBorder=false,relief=8,heightSource='manual',heightEdits={{x=1,y=.5,radius=.4,mode='raise',strength=1,height=1}}}
        } do
            local options=Model.copy(o);options.bevelRounded=rounded
            for k,v in pairs(case) do options[k]=v end
            local mesh,r=mbm.generateImageMesh(path,options);assert(mesh,r);inspect(mesh,r)
            assert(r.bevelMaximumWidth>options.bevelWidth)
        end
    end
    for _,rounded in ipairs{false,true} do
        local options=Model.copy(o);options.bevelRounded=rounded;options.sideMode='repeat'
        options.sideRepeatU=3.5;options.sideRepeatV=2.25
        local mesh,r=mbm.generateImageMesh(path,options);assert(mesh,r);inspect(mesh,r)
    end
    -- All three coordinates of the corner lie on the same radius-two sphere.
    local spherical=Model.copy(o);spherical.bevelRounded=true
    local mesh,r=mbm.generateImageMesh(path,spherical);assert(mesh,r)
    local sphereSamples=0
    for subset=1,mesh:getTotalSubset(1) do
        for _,v in ipairs(mesh:getVertex(1,subset,1,mesh:getTotalVertex(1,subset))) do
            if v.x>48.001 and v.y>38.001 and v.z< -8.001 then
                local x,y,z=v.x-48,v.y-38,v.z+8
                assert(math.abs(x*x+y*y+z*z-4)<.002,'corner is not spherical')
                assert(math.abs(v.nx-x/2)<.001 and math.abs(v.ny-y/2)<.001 and math.abs(v.nz-z/2)<.001)
                sphereSamples=sphereSamples+1
            end
        end
    end
    assert(sphereSamples>0,'missing spherical corner patch')
    -- Unlocked, uniform manual height: bevel the actual front at -14,
    -- not the old baseline at -10. Preserve the requested height/depth settings.
    o.lockBorder=false;o.heightSource='manual';o.baseHeight=.5;o.relief=8;o.bevelWidth=5
    for _,rounded in ipairs{false,true} do
        o.bevelRounded=rounded
        for _,adaptive in ipairs{false,true} do
            o.followImage=adaptive
            local a,r=mbm.generateImageMesh(path,o);assert(a,r);inspect(a,r)
            local minZ,maxZ,frontX=math.huge,-math.huge,-math.huge
            for subset=1,a:getTotalSubset(1) do
                for _,v in ipairs(a:getVertex(1,subset,1,a:getTotalVertex(1,subset))) do
                    minZ=math.min(minZ,v.z);maxZ=math.max(maxZ,v.z)
                    if math.abs(v.z+14)<.0001 then frontX=math.max(frontX,v.x) end
                end
            end
            assert(math.abs(minZ+14)<.0001 and math.abs(maxZ-10)<.0001,'unlocked height was changed')
            assert(math.abs(frontX-45)<.001,'bevel does not start at the actual front height')
        end
    end
    o.bevelRounded=false;o.followImage=false;o.relief=0;o.lockBorder=true
    o.bevel=false;local plain,pr=mbm.generateImageMesh(path,o);assert(plain,pr)
    o.bevel=nil;local legacy,lr=mbm.generateImageMesh(path,o);assert(legacy,lr)
    assert(pr.vertices==lr.vertices and pr.triangles==lr.triangles)
    local project=Model.new(path,65,65);Model.primitive(project,'rectangle',30,30)
    project.regions[1].overrides.bevel=true;project.regions[1].overrides.bevelWidth=2
    Model.validate(project);assert(Model.options(project,project.regions[1]).bevel)
    print('IMAGE MESH BEVEL SMOKE OK')
end
function onInitScene()
    local ok,err=pcall(run)
    if not ok then print('IMAGE MESH BEVEL SMOKE FAIL: '..tostring(err)) end
    mbm.quit()
end
function onLoop() end
