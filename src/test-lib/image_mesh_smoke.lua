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
    local fv,volume=inspect(flat,fr); near(volume,100*80*20)
    local directions={}
    for _,v in ipairs(fv) do
        if math.abs(v.nx)>0.99 then directions[v.nx>0 and 'right' or 'left']=true end
        if math.abs(v.ny)>0.99 then directions[v.ny>0 and 'top' or 'bottom']=true end
        if math.abs(v.nz)>0.99 then directions[v.nz>0 and 'back' or 'front']=true end
    end
    for _,face in ipairs({'right','left','top','bottom','back','front'}) do assert(directions[face],'missing face: '..face) end
    local alphaPixels={}
    for y=0,6 do for x=0,6 do
        alphaPixels[#alphaPixels+1]=100+x*20; alphaPixels[#alphaPixels+1]=100+y*20
        alphaPixels[#alphaPixels+1]=150; alphaPixels[#alphaPixels+1]=(x>=2 and x<=4 and y>=2 and y<=4) and 255 or 0
    end end
    local alphaImage='/tmp/ime_transparent_margin.png'
    assert(mbm.createTexture(alphaPixels,7,7,4,'ime_transparent_margin',alphaImage))
    local solid,sr=mbm.generateImageMesh(alphaImage,{columns=4,rows=4,depth=30,relief=0})
    assert(solid,sr); local sv=inspect(solid,sr)
    near(sv[1].u,0.5/7); near(sv[1].v,0.5/7) -- front retains original transparency
    for i=51,#sv do
        local v=sv[i]; local x,y=math.floor(v.u*7),math.floor(v.v*7)
        assert(x>=2 and x<=4 and y>=2 and y<=4,'side samples transparent margin')
    end
    for i=51,#sv,4 do for j=i+1,i+3 do near(sv[j].u,sv[i].u); near(sv[j].v,sv[i].v) end end
    assert(solid:save('/tmp/ime_transparent_margin.msh',false,false,true))
    local savedSolid=meshDebug:new(); assert(savedSolid:load('/tmp/ime_transparent_margin.msh')); inspect(savedSolid,sr)
    print('IMAGE MESH SIX FACES / OPAQUE SIDE UV OK')
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
    local contour={{x=0,y=0},{x=1,y=0},{x=1,y=0.4},{x=0.4,y=0.4},{x=0.4,y=1},{x=0,y=1}}
    local polyOptions={shape='polygon',contour=contour,columns=12,rows=12,relief=0,width=100,height=80,depth=20}
    local polygon,pr=mbm.generateImageMesh(image,polyOptions); assert(polygon,pr)
    local pv,pvolume=inspect(polygon,pr); near(pvolume,100*80*20*0.64)
    for _,v in ipairs(pv) do assert(v.x<=-10.0001 or v.y>=8-0.0001 or math.abs(v.x+10)<0.001, 'triangle vertex outside concave region') end
    local reverse={}; for i=#contour,1,-1 do reverse[#reverse+1]=contour[i] end
    polyOptions.contour=reverse
    local reversed,rr=mbm.generateImageMesh(image,polyOptions); assert(reversed,rr); inspect(reversed,rr)
    polyOptions.contour={{x=0,y=0},{x=1,y=1},{x=0,y=1},{x=1,y=0}}
    local crossed,ce=mbm.generateImageMesh(image,polyOptions); assert(not crossed and ce:find('intersect'))
    polyOptions.contour={{x=0,y=0},{x=0.5,y=0},{x=1,y=0},{x=1,y=1},{x=0,y=1}}
    local collinear,clr=mbm.generateImageMesh(image,polyOptions); assert(collinear,clr); inspect(collinear,clr)
    polyOptions.contour={{x=0,y=0},{x=0,y=0},{x=1,y=1}}
    assert(not mbm.generateImageMesh(image,polyOptions))
    -- With no refinement, every front triangle must radiate from the center.
    local radial,rr=mbm.generateImageMesh(image,{shape='ellipse',ellipseSegments=32,columns=1,rows=1,relief=0})
    assert(radial,rr); inspect(radial,rr)
    local rv=radial:getVertex(1,1,1,rr.vertices); local ri=radial:getIndex(1,1)
    local front=0
    for i=1,#ri,3 do
        local a,b,c=rv[ri[i]],rv[ri[i+1]],rv[ri[i+2]]
        if a.nz< -0.99 and b.nz< -0.99 and c.nz< -0.99 then
            front=front+1
            assert((math.abs(a.x)<0.001 and math.abs(a.y)<0.001) or
                (math.abs(b.x)<0.001 and math.abs(b.y)<0.001) or
                (math.abs(c.x)<0.001 and math.abs(c.y)<0.001),'ellipse fan is not centered')
        end
    end
    assert(front==32)
    local ellipse,er=mbm.generateImageMesh(image,{shape='ellipse',ellipseSegments=32,columns=16,rows=12,relief=0,width=100,height=80,depth=20})
    assert(ellipse,er); local _,ev=inspect(ellipse,er)
    near(ev,100*80*20*32*math.sin(2*math.pi/32)/8)
    local oval,ovr=mbm.generateImageMesh(image,{shape='ellipse',columns=24,rows=16,relief=10,lockBorder=true})
    assert(oval,ovr); inspect(oval,ovr)
    assert(not mbm.generateImageMesh(image,{shape='ellipse',ellipseSegments=2}))
    local rejected,message=mbm.generateImageMesh(image,{shape='ellipse',maxTriangles=10})
    assert(not rejected and message:match('triangles >= %d+, limit 10;'))
    rejected,message=mbm.generateImageMesh(image,{columns=1,rows=1,maxVertices=1})
    assert(not rejected and message:find('vertices >= 24, limit 1;',1,true))
    rejected,message=mbm.generateImageMesh(image,{shape='ellipse',columns=255,rows=255,ellipseSegments=128})
    local required,limit=message:match('vertices >= (%d+), limit (%d+);')
    assert(not rejected and tonumber(required)>65535 and tonumber(limit)==65535)
    assert(message:find('during contour refinement',1,true))
    assert(not pcall(mbm.generateImageMesh,image,{shape='unknown'}))
    assert(polygon:save('/tmp/mbm_image_mesh_concave.msh',false,false,true))
    assert(ellipse:save('/tmp/mbm_image_mesh_ellipse.msh',false,false,true))
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
