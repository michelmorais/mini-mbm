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
local Asset=require 'image_mesh_asset'
local Sides=require 'image_mesh_sides'
local Canvas=require 'image_mesh_canvas'
local Presets=require 'image_mesh_presets'
local IO=require 'image_mesh_io'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local started,baseline
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


local function close(a,b) assert(math.abs(a-b)<.0001,tostring(a)..' != '..tostring(b)) end
local function run()
    init()
    local path='/tmp/ime_sides.png';local pixels={}
    for y=0,128 do for x=0,128 do
        pixels[#pixels+1]=x;pixels[#pixels+1]=y;pixels[#pixels+1]=128
    end end
    assert(mbm.createTexture(pixels,129,129,3,'ime_sides',path))
    local tile={};for y=0,7 do for x=0,7 do local c=(x+y)%2==0 and 255 or 30
        tile[#tile+1]=c;tile[#tile+1]=120;tile[#tile+1]=255-c end end
    local tilePath='/tmp/ime_sides_tile.png';assert(mbm.createTexture(tile,8,8,3,'ime_sides_tile',tilePath))
    for _,adaptive in ipairs{false,true} do for _,shape in ipairs{'rectangle','ellipse','polygon'} do
        local o={cropWidth=129,cropHeight=129,shape=shape,columns=6,rows=6,ellipseSegments=16,
            followImage=adaptive,sideInset=3,sideColor=0xC04080,sideTexture=tilePath,sideRepeatU=3.5,sideRepeatV=2.5,
            contour={{x=0,y=0},{x=1,y=0},{x=1,y=1},{x=.65,y=1},{x=.65,y=.3},{x=.35,y=.3},{x=.35,y=1},{x=0,y=1}}}
        local inner,maximum=mbm.getImageMeshSideContour(o);assert(inner,maximum)
        assert(maximum>=3)
        if shape=='rectangle' then close(inner[1].x,3/128);close(inner[1].y,3/128) end
        if shape=='polygon' then assert(maximum<25,'concave inset crossed narrow sections') end
        o.sideInset=maximum+1
        local invalid,message,limit=mbm.getImageMeshSideContour(o);assert(not invalid and limit<o.sideInset)
        o.sideInset=3
        local edge,er=mbm.generateImageMesh(path,o);assert(edge,er)
        local ev=data(edge,er)
        for _,mode in ipairs{'band','color','repeat'} do
            o.sideMode=mode
            local asset,report=mbm.generateImageMesh(path,o);assert(asset,report)
            local vv=data(asset,report)
            if mode~='repeat' then
                assert(report.vertices==er.vertices and report.triangles==er.triangles)
                for i,a in ipairs(ev) do for _,k in ipairs{'x','y','z','nx','ny','nz'} do close(a[k],vv[i][k]) end end
            else assert(report.vertices>er.vertices,'repeat seams missing') end
            if mode=='band' then
                local side=1
                while side<=#ev and ev[side].z<0 do side=side+1 end
                while side<=#ev and ev[side].z>0 do side=side+1 end
                for i=1,side-1 do close(ev[i].u,vv[i].u);close(ev[i].v,vv[i].v) end
                for i=side,#ev do
                    if vv[i].z<0 then close(ev[i].u,vv[i].u);close(ev[i].v,vv[i].v)
                    else assert(math.abs(ev[i].u-vv[i].u)+math.abs(ev[i].v-vv[i].v)>1e-6,'inner band UV was not applied') end
                end
            end
            o.sideBandInvert=true
            local inverted,ir=mbm.generateImageMesh(path,o);assert(inverted,ir)
            local iv,ii=data(inverted,ir)
            local _,indices=Asset.geometry(asset)
            assert(ir.vertices==report.vertices and ir.triangles==report.triangles)
            for i,index in ipairs(indices) do assert(ii[i]==index,'inversion changed topology') end
            -- Locate the side block independently of report fields (front, then back, then walls).
            local sideStart=1
            while sideStart<=#vv and vv[sideStart].z<0 do sideStart=sideStart+1 end
            while sideStart<=#vv and vv[sideStart].z>0 do sideStart=sideStart+1 end
            for i,v in ipairs(vv) do
                for _,k in ipairs{'x','y','z','nx','ny','nz'} do close(v[k],iv[i][k]) end
                if mode=='band' and i>=sideStart then
                    if v.z>0 then close(iv[i].u,ev[i].u);close(iv[i].v,ev[i].v)
                    else assert(math.abs(iv[i].u-v.u)+math.abs(iv[i].v-v.v)>1e-6,'inverted front needs inner UV') end
                else close(iv[i].u,v.u);close(iv[i].v,v.v) end
            end
            if mode=='band' then
                local file='/tmp/ime_band_inverted_'..shape..'_'..tostring(adaptive)..'.msh'
                assert(inverted:save(file,false,false,true))
                local loaded=meshDebug:new();assert(loaded:load(file))
                local lv=data(loaded,ir)
                for i,v in ipairs(iv) do close(v.u,lv[i].u);close(v.v,lv[i].v) end
            end
            o.sideBandInvert=false
            local expected=mode=='band' and 1 or 2
            assert(asset:getTotalSubset(1)==expected,'wrong material count')
            if mode=='color' then assert(asset:getTexture(1,2)=='#C04080FF','solid color not encoded') end
            if mode=='repeat' then
                assert(asset:getTexture(1,2)==tilePath)
                for _,v in ipairs(asset:getVertex(1,2,1,asset:getTotalVertex(1,2))) do
                    assert(v.u>=0 and v.u<=1 and v.v>=0 and v.v<=1,'UV relies on global repeat sampler')
                end
                local clone,cr=mbm.generateImageMesh(path,o);assert(clone,cr)
                local simple,why=clone:simplify(.9,nil,1,true,0)
                assert(simple,why)
                local total=clone:getTotalVertex(1,1)+clone:getTotalVertex(1,2)
                data(clone,{vertices=total})
                assert(clone:getTotalSubset(1)==2 and clone:getTexture(1,2)==tilePath,'simplification lost material')
            end
            local file='/tmp/ime_sides_'..shape..'_'..mode..'.msh'
            assert(asset:save(file,false,false,true))
            local loaded=meshDebug:new();assert(loaded:load(file))
            assert(loaded:getTotalSubset(1)==expected)
            data(loaded,report)
            o.maxVertices=report.vertices-1
            local bad,err=mbm.generateImageMesh(path,o);assert(not bad and err:find('budget'),'side budget ignored')
            o.maxVertices=nil
        end
        o.sideMode='edge'
        print('SIDE '..shape..' '..tostring(adaptive)..' / UV / BAND INVERSION / CLOSED / MATERIAL / SIMPLIFY / EXPORT OK')
    end end
    -- Both API nil and editor empty-string defaults repeat only the source crop.
    for _,texture in ipairs{false,''} do
        local o={x=20,y=30,cropWidth=70,cropHeight=60,sideMode='repeat',
            sideRepeatU=3.5,sideRepeatV=2.5,sideTexture=texture or nil}
        local asset,report=mbm.generateImageMesh(path,o);assert(asset,report)
        data(asset,report)
        assert(asset:getTexture(1,2)==path)
        for _,v in ipairs(asset:getVertex(1,2,1,asset:getTotalVertex(1,2))) do
            assert(v.u>=20.5/129-1e-6 and v.u<=89.5/129+1e-6,'repeat escaped source crop U')
            assert(v.v>=30.5/129-1e-6 and v.v<=89.5/129+1e-6,'repeat escaped source crop V')
        end
        assert(asset:save('/tmp/ime_sides_source_crop.msh',false,false,true))
        local loaded=meshDebug:new();assert(loaded:load('/tmp/ime_sides_source_crop.msh'))
        assert(loaded:getTexture(1,2)==path);data(loaded,report)
        o.sideTexture='/tmp/ime_side_texture_does_not_exist.png'
        local bad,err=mbm.generateImageMesh(path,o)
        assert(not bad and err:find('Side texture'),'invalid explicit texture silently replaced')
    end
    local default,why=mbm.generateImageMesh(path,{sideMode='repeat'})
    assert(default,why);data(default,why)
    print('SIDE DEFAULT / SOURCE CROP / EXPORT / INVALID PATH OK')
    for _,adaptive in ipairs{false,true} do for _,shape in ipairs{'rectangle','ellipse','polygon'} do
        for _,mode in ipairs{'edge','band','color','repeat'} do
            local o={shape=shape,followImage=adaptive,sideMode=mode,sideInset=3,sideRepeatU=2.5,
                sideColor=0xC04080,backColor=0x1234AB,columns=6,rows=6,
                contour={{x=0,y=0},{x=1,y=0},{x=1,y=1},{x=.5,y=.6},{x=0,y=1}}}
            local original,r=mbm.generateImageMesh(path,o);assert(original,r)
            local ov,oi=data(original,r)
            o.backExternal=true
            for _,texture in ipairs{false,'',tilePath} do
                o.backTexture=texture or nil
                local ext,xr=mbm.generateImageMesh(path,o);assert(ext,xr)
                local xv,xi=data(ext,xr)
                assert(xr.vertices==r.vertices and xr.triangles==r.triangles)
                assert(ext:getTotalSubset(1)==3 and ext:getTexture(1,2)==(texture==tilePath and tilePath or path))
                for i,index in ipairs(oi) do assert(xi[i]==index) end
                local front=ext:getTotalVertex(1,1);local back=ext:getTotalVertex(1,2)
                for i,v in ipairs(xv) do
                    for _,k in ipairs{'x','y','z','nx','ny','nz'} do close(v[k],ov[i][k]) end
                    if i>front and i<=front+back and texture==tilePath then
                        close(v.u,((v.x/100+.5)*7+.5)/8)
                        close(v.v,((.5-v.y/100)*7+.5)/8)
                    else close(v.u,ov[i].u);close(v.v,ov[i].v) end
                end
                o.backMirror=true
                local mirror,mr=mbm.generateImageMesh(path,o);assert(mirror,mr)
                local mv=data(mirror,mr)
                for i,v in ipairs(xv) do
                    close(mv[i].v,v.v)
                    close(mv[i].u,(i>front and i<=front+back) and 1-v.u or v.u)
                end
                o.backMirror=false
                assert(ext:simplify(.95,nil,1,true,0));data(ext,xr)
                local file='/tmp/ime_external_'..shape..'_'..mode..'_'..tostring(adaptive)..'.msh'
                assert(ext:save(file,false,false,true))
                local loaded=meshDebug:new();assert(loaded:load(file));data(loaded,xr)
                assert(loaded:getTexture(1,2)==ext:getTexture(1,2))
            end
            o.backTexture='/tmp/ime_missing_back_file.png'
            local bad,err=mbm.generateImageMesh(path,o);assert(not bad and err:find('Back texture'))
            o.backTexture=tilePath;o.backOpen=true
            assert(not mbm.generateImageMesh(path,o),'external back conflict accepted')
            o.backOpen=false;o.backExternal=false
            o.backSolid=true
            local solid,sr=mbm.generateImageMesh(path,o);assert(solid,sr)
            local sv,si=data(solid,sr)
            assert(sr.vertices==r.vertices and sr.triangles==r.triangles)
            assert(solid:getTotalSubset(1)==3 and solid:getTexture(1,2)=='#1234ABFF')
            assert(solid:getTexture(1,3)==(mode=='color' and '#C04080FF' or path))
            for i,index in ipairs(oi) do assert(si[i]==index) end
            local front=solid:getTotalVertex(1,1);local back=solid:getTotalVertex(1,2)
            for i,v in ipairs(sv) do
                for _,k in ipairs{'x','y','z','nx','ny','nz'} do close(v[k],ov[i][k]) end
                if i>front and i<=front+back then close(v.u,.5);close(v.v,.5)
                else close(v.u,ov[i].u);close(v.v,ov[i].v) end
            end
            local simplified,why=solid:simplify(.95,nil,1,true,0)
            assert(simplified,shape..' '..mode..' '..tostring(adaptive)..': '..tostring(why));data(solid,sr)
            local file='/tmp/ime_solid_'..shape..'_'..mode..'_'..tostring(adaptive)..'.msh'
            assert(solid:save(file,false,false,true))
            local loaded=meshDebug:new();assert(loaded:load(file));data(loaded,sr)
            assert(loaded:getTotalSubset(1)==3 and loaded:getTexture(1,2)=='#1234ABFF')
            o.backOpen=true
            local invalid=mbm.generateImageMesh(path,o);assert(not invalid,'conflicting back modes accepted')
        end
    end end
    print('SOLID BACK / ALL SIDE MODES / GEOMETRY / SIMPLIFY / EXPORT OK')
    api.openImage(path)
    assert(api.action(function(p) Model.add(p,'rectangle',10,10,100,100) end))
    api.select(1,false)
    local E=api.state
    E.values.sideMode='band';E.values.sideInset=5;assert(api.applyProperties())
    E.tool='side_band';Canvas.sync(E)
    local cached,region=Sides.contour(E)
    assert(cached.points and cached.maximum>5)
    local start=cached.points[1]
    local function touch(fn,x,y)
        local t=Canvas.transform(E)
        fn(0,(t.x+x*t.scale)/E.camera2d.sx,(t.y+y*t.scaleY)/E.camera2d.sy)
    end
    local x=region.x+start.x*(region.w-1);local y=region.y+start.y*(region.h-1)
    touch(onTouchDown,x,y);assert(E.drag and E.drag.mode=='side_inset','band handle not hit')
    touch(onTouchMove,x+3,y+3);touch(onTouchUp,x+3,y+3)
    close(E.project.regions[1].overrides.sideInset,8)
    assert(E.project.regions[1].x==10 and E.project.regions[1].w==100,'band moved geometry crop')
    api.undo(false);close(E.values.sideInset,5)
    api.undo(true);close(E.values.sideInset,8)
    E.tool='side_band'
    cached,region=Sides.contour(E)
    x=region.x+cached.points[1].x*(region.w-1);y=region.y+cached.points[1].y*(region.h-1)
    touch(onTouchDown,x,y);touch(onTouchMove,x+1000,y+1000)
    close(E.project.regions[1].overrides.sideInset,cached.maximum)
    Canvas.cancel(E);close(E.project.regions[1].overrides.sideInset,8)
    cached=Sides.contour(E);close(cached.width,8)
    E.values.sideMode='repeat';E.values.sideTexture=tilePath;E.values.sideRepeatU=3.5;E.values.sideRepeatV=2.5
    E.values.backSolid=true;E.values.backColor=0x1234AB
    E.values.sideBandInvert=true
    assert(api.applyProperties())
    assert(api.action(function(p) Presets.store(p,'Side tile',E.values) end))
    api.saveProject('/tmp/ime_sides_project.imesh')
    api.openProject('/tmp/ime_sides_project.imesh');api.select(1,false)
    assert(E.values.sideTexture==tilePath and E.values.sideRepeatV==2.5,'side project persistence')
    assert(E.values.backSolid and E.values.backColor==0x1234AB,'solid back project persistence')
    assert(E.values.sideBandInvert==true,'band inversion project persistence')
    Presets.save(E.project.presets[1],'/tmp/ime_sides.imeshpreset',tUtil.save)
    assert(Presets.load('/tmp/ime_sides.imeshpreset').settings.sideTexture==tilePath,'preset relative path')
    assert(Presets.load('/tmp/ime_sides.imeshpreset').settings.sideBandInvert==true,'band inversion preset persistence')
    assert(Presets.load('/tmp/ime_sides.imeshpreset').settings.backSolid,'solid back preset persistence')
    E.values.backSolid=false;E.values.backExternal=true;E.values.backTexture=tilePath;E.values.backMirror=true
    assert(api.applyProperties())
    assert(api.action(function(p) Presets.store(p,'External back',E.values) end))
    api.saveProject('/tmp/ime_external_project.imesh')
    api.openProject('/tmp/ime_external_project.imesh');api.select(1,false)
    assert(E.values.backExternal and E.values.backTexture==tilePath and E.values.backMirror,'external back persistence')
    Presets.save(E.project.presets[2],'/tmp/ime_external.imeshpreset',tUtil.save)
    local preset=Presets.load('/tmp/ime_external.imeshpreset')
    assert(preset.settings.backExternal and preset.settings.backTexture==tilePath,'external preset path')
    print('EXTERNAL BACK / UV / MIRROR / SIMPLIFY / EXPORT / PERSISTENCE OK')
    assert(mbm.createDirectories('/tmp/ime_portable'))
    local portable='/tmp/ime_portable/module.msh'
    os.remove(portable)
    for i=1,3 do os.remove('/tmp/ime_portable/module_texture_0'..i..'.png') end
    assert(api.exportOne(portable,true))
    local packed=meshDebug:new();assert(packed:load(portable))
    for subset=1,packed:getTotalSubset(1) do
        local texture=packed:getTexture(1,subset)
        assert(not texture:find('/') and not texture:find('\\'),'portable texture not relative')
        assert(IO.exists('/tmp/ime_portable/'..texture))
        for _,v in ipairs(packed:getVertex(1,subset,1,packed:getTotalVertex(1,subset))) do
            assert(v.u>=0 and v.u<=1 and v.v>=0 and v.v<=1,'invalid original UV')
        end
    end
    data(packed,{})
    assert(packed:getTexture(1,2)==packed:getTexture(1,3),'same source duplicated within mesh')
    assert(not IO.exists('/tmp/ime_portable/module_texture_03.png'),'duplicate material PNG created')
    local Portable=require 'image_mesh_portable'
    local asset,ar=mbm.generateImageMesh(path,Model.options(E.project,E.project.regions[1]));assert(asset,ar)
    local originalVertices=Asset.vertices(asset)
    local packedVertices=Asset.vertices(packed)
    assert(#originalVertices==#packedVertices)
    for i,v in ipairs(originalVertices) do
        local out=packedVertices[i]
        close(out.x,-v.x);close(out.y,v.y);close(out.z,-v.z)
        close(out.nx,-v.nx);close(out.ny,v.ny);close(out.nz,-v.nz)
        close(out.u,v.u);close(out.v,v.v)
    end
    local function quiet(fn) return pcall(fn) end
    assert(api.exportOne(portable,true),'portable overwrite failed')
    local function bytes(file) local f=assert(io.open(file,'rb'));local b=f:read('a');f:close();return b end
    local saved=bytes(portable)
    local textureSaved=bytes('/tmp/ime_portable/module_texture_01.png')
    -- Force a commit failure after the first texture was replaced; all backups must restore.
    asset:setTexture(1,1,tilePath) -- replacement differs from original, testing real backup restoration
    local rename=os.rename
    os.rename=function(from,to)
        if from:find('module.msh.ime-tmp-',1,true) then return nil,'simulated rename failure' end
        return rename(from,to)
    end
    local committed=pcall(Portable.save,asset,portable,quiet)
    os.rename=rename
    assert(not committed,'injected commit failure ignored')
    assert(bytes(portable)==saved and bytes('/tmp/ime_portable/module_texture_01.png')==textureSaved,'overwrite rollback damaged previous export')
    asset:setTexture(1,2,'/tmp/missing-portable-texture.png')
    assert(not pcall(Portable.save,asset,portable,quiet))
    assert(bytes(portable)==saved and bytes('/tmp/ime_portable/module_texture_01.png')==textureSaved,'generation failure damaged previous export')
    assert(not pcall(Portable.save,asset,'/tmp/ime_portable/failed.msh',quiet))
    assert(not IO.exists('/tmp/ime_portable/failed_texture_01.png'),'partial PNG not cleaned')
    assert(not IO.exists('/tmp/ime_portable/failed.msh'),'partial mesh not cleaned')
    local su,sv,ou,ov=mbm.exportImageMeshTexture(path,'/tmp/ime_portable/pixels.png',20.5/129,30.5/129,89.5/129,89.5/129,4)
    assert(su,sv);close(su,129/78);close(sv,129/68);close(ou,-16/78);close(ov,-26/68)
    assert(mbm.createDirectories('/tmp/ime_portable/batch'))
    local batchPath='/tmp/ime_portable/batch/'..IO.exportName(E.project.regions[1])
    os.remove(batchPath)
    for i=1,3 do os.remove(batchPath:gsub('%.msh$','')..string.format('_texture_%02d.png',i)) end
    api.beginBatch('/tmp/ime_portable/batch',true)
    while E.batch do api.batchStep() end
    assert(IO.exists(batchPath),'portable batch export failed')
    api.beginBatch('/tmp/ime_portable/batch',true)
    while E.batch do api.batchStep() end
    assert(IO.exists(batchPath),'portable batch overwrite failed')
    E.portableCrop=true
    assert(api.exportOne('/tmp/ime_portable/cropped.msh',true))
    E.portableCrop=false
    local cropped=meshDebug:new();assert(cropped:load('/tmp/ime_portable/cropped.msh'))
    local cv=Asset.vertices(cropped)
    assert(math.abs(cv[1].u-packedVertices[1].u)>1e-4,'optional crop did not remap UV')
    -- Three modules with different UV crops must share one full source image in a batch.
    local beforeProject=Model.copy(E.project)
    assert(api.action(function(p)
        p.regions={};p.nextId=1
        for i=1,3 do Model.add(p,'rectangle',i*10,10,40,40) end
    end))
    local directory='/tmp/ime_portable/shared_batch'
    assert(mbm.createDirectories(directory))
    for _,r in ipairs(E.project.regions) do
        local file=directory..'/'..IO.exportName(r)
        os.remove(file)
        for subset=1,3 do os.remove(file:gsub('%.msh$','')..string.format('_texture_%02d.png',subset)) end
    end
    local exports=0;local encode=mbm.exportImageMeshTexture
    mbm.exportImageMeshTexture=function(...) exports=exports+1;return encode(...) end
    api.beginBatch(directory,true)
    while E.batch do api.batchStep() end
    mbm.exportImageMeshTexture=encode
    assert(exports==1,'shared batch encoded texture more than once: '..exports)
    local reference
    for _,r in ipairs(E.project.regions) do
        local mesh=meshDebug:new();assert(mesh:load(directory..'/'..IO.exportName(r)))
        local texture=mesh:getTexture(1,1)
        reference=reference or texture;assert(texture==reference,'batch texture references differ')
    end
    -- Cropped modules with different sampled areas must remain distinct.
    E.portableCrop=true;exports=0
    mbm.exportImageMeshTexture=function(...) exports=exports+1;return encode(...) end
    api.beginBatch(directory,true)
    while E.batch do api.batchStep() end
    mbm.exportImageMeshTexture=encode;E.portableCrop=false
    assert(exports==3,'different cropped textures were merged')
    assert(api.action(function(p) for k in pairs(p) do p[k]=nil end;for k,v in pairs(beforeProject) do p[k]=v end end))
    api.select(1,false)
    print('PORTABLE / FULL IMAGE UV / CROP / OVERWRITE / ROLLBACK / SHARED BATCH OK')
    assert(api.exportOne('/tmp/ime_sides_editor.msh'))
    local source,report=mbm.generateImageMesh(path,Model.options(E.project,E.project.regions[1]));assert(source,report)
    local loaded=meshDebug:new();assert(loaded:load('/tmp/ime_sides_editor.msh'))
    local a,b=Asset.vertices(source),Asset.vertices(loaded)
    for i,v in ipairs(a) do close(v.x,-b[i].x);close(v.z,-b[i].z);close(v.u,b[i].u);close(v.v,b[i].v) end
    api.setEditMode(false);api.rebuild();api.setWireframe(true)
    assert(E.wireObject,'multi-subset wireframe failed')
    api.setWireframe(false);api.setEditMode(true)
    E.values.sideMode='band';assert(api.applyProperties());E.tool='side_band';Canvas.sync(E)
    started=mbm.getTimeRun()
    print('SIDE EDITOR INPUT / HISTORY / PRESETS / PATHS / EXPORT / WIREFRAME OK')
end
function onInitScene()
    local ok,err=pcall(run)
    if not ok then print('SIDE FAIL '..tostring(err));mbm.quit() end
end
function onLoop(delta)
    if not started then return end
    local elapsed=mbm.getTimeRun()-started
    local E=api.state
    if elapsed<.6 then E.values.sideMode='band'
    elseif elapsed<1.2 then E.values.sideMode='color'
    elseif elapsed<1.8 then E.values.sideMode='repeat'
    else E.values.sideMode='band' end
    local header=tImGui.CollapsingHeader
    tImGui.CollapsingHeader=function(label,...)
        if label==tLang.L('ime_side_group') then tImGui.SetNextItemOpen(true,0) end
        return header(label,...)
    end
    loop(delta);tImGui.CollapsingHeader=header
    if mbm.getTimeRun()-started>2 and not baseline then baseline={canvas=E.canvasBuilds,queries=E.sideQueries,builds=E.builds} end
    if mbm.getTimeRun()-started>4 then
        assert(baseline.canvas==E.canvasBuilds and baseline.queries==E.sideQueries and baseline.builds==E.builds,'idle side work')
        print('SIDE GUI / IDLE OK');mbm.quit()
    end
end
