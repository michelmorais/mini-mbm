--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

]]--

-- Run from the repository root with --disable_select_monitor --nosplash.
package.path='editor/?.lua;'..package.path
local api={}
assert(assert(loadfile('editor/normal_map_editor.lua'))(api)==nil,'Editor returned a class-style scene')
local E=api.state
local init,loop,shutdown=onInitScene,onLoop,onEndScene
local root='/tmp/mini-mbm-normal-smoke'
local source=root..'/source.png'
local exported=root..'/normal.png'
local started,stable,builds,uploads,exportRequested
local function protect(fn,...)
    local out=table.pack(pcall(fn,...))
    if not out[1] then print('NORMAL MAP EDITOR SMOKE FAIL: '..tostring(out[2]));mbm.quit() end
    return table.unpack(out,1,out.n)
end
local material,rt,mappedPixels,materialCompared,resourceChecked
local function materialPreview()
    assert(mbm.isNormalMapping3DCompiled())
    mbm.setLightEnabled('3d',true)
    mbm.setAmbientLight('3d',.1,.1,.1)
    mbm.setDirectionalLight('3d',-.8,-.3,.6,.5,.5,.5)
    local asset=meshDebug:new();asset:setType('mesh');asset:setModeFrontFace('CCW')
    asset:enableNormal(true);asset:enableUv(true)
    local f=asset:addFrame(3);local subset=asset:addSubSet(f)
    assert(asset:addVertex(f,subset,{
        {x=-1,y=-1,z=0,nx=0,ny=0,nz=-1,u=0,v=0},
        {x=1,y=-1,z=0,nx=0,ny=0,nz=-1,u=1,v=0},
        {x=-1,y=1,z=0,nx=0,ny=0,nz=-1,u=0,v=1},
        {x=1,y=1,z=0,nx=0,ny=0,nz=-1,u=1,v=1}}))
    assert(asset:addIndex(f,subset,{1,2,3,3,2,4,3,2,1,4,2,3}))
    asset:setTexture(f,subset,'#FFFFFFFF')
    asset:setMaterialTexture(f,subset,'normal',exported)
    assert(asset:prepareNormalMap(f,subset,'generate'))
    assert(asset:addAnim('Static',1,1,1,0))
    assert(asset:save(root..'/plane.msh',false,false,true))
    material=mesh:new('3d');assert(material:load(root..'/plane.msh'))
    rt=render2texture:new('2ds');assert(rt:create(96,96,true))
    rt:getCamera('3d'):setPos(0,0,-4);rt:getCamera('3d'):setFocus(0,0,0)
    rt:setColor(0,0,0,1);rt:add(material)
end
function onInitScene()
    protect(function()
        assert(mbm.createDirectories(root))
        assert(not mbm.writeImagePixels(root..'/invalid.png','bad',1,1))
        local saved,writeError=mbm.writeImagePixels(root..'/missing/file.png',string.char(0,0,0,255),1,1)
        assert(not saved and type(writeError)=='string' and #writeError>0,'Missing write error')
        -- The legacy vector overload must still encode RGB, including its trailing sentinel byte.
        local rgbPath=root..'/legacy-rgb.png'
        assert(mbm.createTexture({12,34,56,78,90,123},2,1,3,rgbPath,rgbPath))
        local rgb,rgbWidth,rgbHeight=mbm.readImagePixels(rgbPath)
        assert(rgb==string.char(12,34,56,255,78,90,123,255) and rgbWidth==2 and rgbHeight==1)
        local rows={}
        for y=1,192 do
            local row={}
            for x=1,192 do
                local dx,dy=(x-96)/65,(y-96)/65
                local height=math.exp(-3*(dx*dx+dy*dy))*.7+.15+.08*math.sin(x*.3)*math.cos(y*.3)
                local v=math.floor(height*255)
                row[x]=string.char(v,v,v,255)
            end
            rows[y]=table.concat(row)
        end
        local bytes=table.concat(rows)
        assert(mbm.writeImagePixels(source,bytes,192,192))
        local decoded,w,h=mbm.readImagePixels(source)
        assert(decoded==bytes and w==192 and h==192,'PNG round trip')
        init();tLang.setLanguage('en');tUtil.sMessageOverlay=nil
        E.open(source,{strength=5,blur=2})
        started=mbm.getTimeRun()
    end)
end
function onLoop(delta)
    protect(function()
        if stable and mbm.getTimeRun()-stable>7 and not resourceChecked then
            -- Replace GPU storage before this frame submits any ImGui image commands.
            local info=E.preview.slots.normal.info
            info:release();assert(not info:isLoaded())
            assert(info:reload(exported) and info:isLoaded())
            resourceChecked=true
        end
        loop(delta)
        assert(not E.status or E.status:find(exported,1,true) or E.status==tLang.L('nmg_exporting'),E.status)
        if E.result and not E.job and not E.lightJob and not E.pending then
            if not exportRequested then E.export(exported);exportRequested=true end
            if exportRequested and not E.exportJob then
                local decoded,w,h=mbm.readImagePixels(exported)
                assert(decoded==E.result.bytes and w==192 and h==192,'Export differs from preview')
                if not stable then
                    stable=mbm.getTimeRun();builds=E.builds;uploads=E.uploads
                    materialPreview()
                end
                if mbm.getTimeRun()-stable>1 and not mappedPixels then
                    assert(rt:save(root..'/material-mapped.png'))
                    mappedPixels=assert(mbm.readImagePixels(root..'/material-mapped.png'))
                    assert(material:setNormalMapSettings('+Y',0))
                elseif mbm.getTimeRun()-stable>2 and not materialCompared then
                    assert(rt:save(root..'/material-flat.png'))
                    local flat=assert(mbm.readImagePixels(root..'/material-flat.png'))
                    assert(flat~=mappedPixels,'Exported map did not change engine material lighting')
                    materialCompared=true
                end
                assert(E.builds==builds and E.uploads==uploads,'Idle editor rebuilt/uploaded')
                if mbm.getTimeRun()-stable>8 then
                    assert(resourceChecked)
                    assert(materialCompared);print('NORMAL MAP EDITOR SMOKE PASS');mbm.quit()
                end
            end
        end
        assert(not started or mbm.getTimeRun()-started<25,'Smoke timed out')
    end)
end
function onEndScene()
    if rt then rt:clear();rt:release();rt:destroy() end
    if material then material:destroy() end
    shutdown()
    for _,slot in pairs(E.preview.slots) do
        assert(not slot.info or not slot.info:isLoaded(),'GPU preview not released')
        local f=io.open(slot.path,'rb');if f then f:close();error('Temporary file leaked') end
    end
end
