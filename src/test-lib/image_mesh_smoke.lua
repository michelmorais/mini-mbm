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
local started, rendered
local function near(a, b) assert(math.abs(a-b) < 0.00001*math.max(1,math.abs(a),math.abs(b)), tostring(a).." ~= "..tostring(b)) end
local function inspect(asset, report)
    assert(asset:check())
    assert(asset:getTotalVertex(1,1) == report.vertices)
    local vertices = asset:getVertex(1,1,1,report.vertices)
    local indices = asset:getIndex(1,1)
    assert(#indices == report.triangles*3)
    local edges, volume = {}, 0
    local function key(v) return string.format('%.5f,%.5f,%.5f', v.x+0.0, v.y+0.0, v.z+0.0) end
    local function edge(a,b)
        local ka,kb=key(a),key(b)
        assert(ka~=kb)
        local k=ka<kb and ka..'/'..kb or kb..'/'..ka
        local item=edges[k] or {count=0,balance=0}; edges[k]=item
        item.count=item.count+1; item.balance=item.balance+(ka<kb and 1 or -1)
    end
    for _,v in ipairs(vertices) do
        near(v.nx*v.nx+v.ny*v.ny+v.nz*v.nz,1)
        assert(v.u>=0 and v.u<=1 and v.v>=0 and v.v<=1)
    end
    for i=1,#indices,3 do
        local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
        local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
        local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
        local nx,ny,nz=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx
        assert(nx*nx+ny*ny+nz*nz>0)
        assert(nx*a.nx+ny*a.ny+nz*a.nz>0)
        volume=volume+(a.x*(b.y*c.z-b.z*c.y)+a.y*(b.z*c.x-b.x*c.z)+a.z*(b.x*c.y-b.y*c.x))/6
        edge(a,b); edge(b,c); edge(c,a)
    end
    for _,e in pairs(edges) do assert(e.count==2 and e.balance==0, 'open or non-manifold mesh') end
    assert(volume>0, 'inward winding')
    return vertices,volume
end
local function runTests()
    started=mbm.getTimeRun()
    mbm.addPath('/tmp')
    local pixels={}
    for y=0,4 do for x=0,4 do
        local value=x*255/4
        pixels[#pixels+1]=value; pixels[#pixels+1]=value; pixels[#pixels+1]=value; pixels[#pixels+1]=255
    end end
    local image='/tmp/mbm_image_mesh_gradient.png'
    assert(mbm.createTexture(pixels,5,5,4,'image_mesh_gradient',image))
    local opts={columns=4,rows=4,width=100,height=80,depth=20,relief=10,lockBorder=false}
    local asset,report=mbm.generateImageMesh(image,opts); assert(asset,report)
    local vertices=inspect(asset,report)
    near(vertices[1].z,-10); near(vertices[5].z,-20)
    near(vertices[1].u,0.1); near(vertices[5].u,0.9)
    assert(vertices[1].y>vertices[21].y)
    near(report.minHeight,0); near(report.maxHeight,10)
    opts.invert=true
    local inverted,ir=mbm.generateImageMesh(image,opts); assert(inverted,ir)
    local iv=inspect(inverted,ir); near(iv[1].z,-20); near(iv[5].z,-10)
    opts.invert=false; opts.lockBorder=true
    local locked,lr=mbm.generateImageMesh(image,opts); assert(locked,lr)
    local lv=inspect(locked,lr)
    for r=0,4 do for c=0,4 do if r==0 or r==4 or c==0 or c==4 then near(lv[r*5+c+1].z,-10) end end end
    opts.relief=0
    local flat,fr=mbm.generateImageMesh(image,opts); assert(flat,fr)
    local _,volume=inspect(flat,fr); near(volume,100*80*20)
    opts.relief=10; opts.x=2; opts.cropWidth=1; opts.y=1; opts.cropHeight=2; opts.lockBorder=false
    local cropped,cr=mbm.generateImageMesh(image,opts); assert(cropped,cr)
    inspect(cropped,cr); near(cr.minHeight,cr.maxHeight)
    local again,ar=mbm.generateImageMesh(image,opts); assert(again,ar)
    local av=again:getVertex(1,1,1,ar.vertices)
    local cv=cropped:getVertex(1,1,1,cr.vertices)
    for i,v in ipairs(cv) do for k,value in pairs(v) do assert(av[i][k]==value) end end
    for _,bad in ipairs({{columns=0},{columns=256},{columns=255,rows=255},{maxVertices=1},{maxTriangles=1},
                         {x=5},{cropWidth=6},{depth=0},{relief=-1},{width=0/0},{borderWidth=0.6}}) do
        local result,err=mbm.generateImageMesh(image,bad); assert(result==nil and type(err)=='string' and #err>0)
    end
    assert(not pcall(mbm.generateImageMesh,image,{rows=-1}))
    assert(not pcall(mbm.generateImageMesh,image,{invert=1}))
    local missing,err=mbm.generateImageMesh('/tmp/mbm_image_mesh_missing.png',{}); assert(not missing and err)
    local output='/tmp/mbm_image_mesh_smoke.msh'
    assert(asset:save(output,false,false,true))
    local restored=meshDebug:new(); assert(restored:load(output)); inspect(restored,report)
    local camera=mbm.getCamera('3d'); camera:setPos(120,90,-240); camera:setFocus(0,0,0)
    mbm.setLightEnabled('3d',true); mbm.setAmbientLight('3d',0.3,0.3,0.3)
    mbm.setDirectionalLight('3d',0.3,-0.4,1,0.8,0.8,0.8)
    rendered=mesh:new('3d'); assert(rendered:load(output))
    print('IMAGE MESH GEOMETRY / VALIDATION / ROUNDTRIP OK')
end
function onInitScene()
    local ok,err=pcall(runTests)
    if not ok then print("IMAGE MESH FAIL: "..tostring(err)); started=nil; mbm.quit() end
end
function onLoop(delta)
    if started and mbm.getTimeRun()-started>=3 then
        assert(mbm.getObjectsRendered()>0, 'generated mesh was not rendered')
        print('IMAGE MESH RENDER OK'); mbm.quit()
    end
end
