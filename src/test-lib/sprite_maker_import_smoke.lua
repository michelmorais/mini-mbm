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


package.path='editor/?.lua;'..package.path
dofile(os.getenv('SPRITE_MAKER_SCRIPT') or 'editor/sprite_maker.lua')
local init,loop=onInitScene,onLoop
local P=require 'mesh_to_sprite_project'
local X=require 'mesh_to_sprite_export'
local temp=os.tmpname();os.remove(temp)
local png=temp..'.png'
local binary=temp..'.spt'
local project=temp..'.sprite'
local snapshot=temp..'_preview.png'
local started,stage,ticks=0,0,0
local expectedFrames=2
local previewFrame=1
local previewInfo
local function open(path,fn)
    local original=mbm.openFile
    mbm.openFile=function() return path end
    local ok,err=pcall(fn)
    mbm.openFile=original
    assert(ok,err)
end
local function requestPreview()
    assert(#tFrameList==expectedFrames and #tAnimationList==1,'Import lost frames or animation')
    assert(tAnimationList[1].sNameAnim=='bite','Import lost animation name')
    previewInfo=getTextureInfoForAnimImage(tFrameList[previewFrame],1)
end
local function checkPreview()
    local found=false
    for _,target in pairs(tAnimationOptions.tDynamicAnims) do
        if target.tTextureInfo==previewInfo then
            assert(target:save(snapshot))
            local bytes=assert(mbm.readImagePixels(snapshot,'alpha'))
            for i=1,#bytes do if bytes:byte(i)>0 then found=true;break end end
        end
    end
    assert(found,'Imported CCW sprite preview is blank')
end
function onInitScene()
    local ok,err=pcall(function()
        init();started=mbm.getTimeRun()
        local p=P.defaults();p.width=32;p.height=32;p.name='bite'
        assert(mbm.writeImagePixels(png,string.rep(string.char(240,80,30,255),32*32),32,32))
        mbm.addPath('/tmp')
        X.write(p,{png,png},binary)
        open(binary,onImportBinarySprite)
    end)
    if not ok then print('SPRITE IMPORT FAIL '..tostring(err));mbm.quit() end
end
function onLoop(dt)
    local ok,err=pcall(function()
        loop(dt)
        assert(mbm.getTimeRun()-started<15,'timeout')
        ticks=ticks+1
        if stage==0 then requestPreview();if ticks>5 then stage=1 end
        elseif stage==1 then
            checkPreview()
            tFrameBulkOptions.iFrameStart=1;tFrameBulkOptions.iFrameStop=2
            tFrameBulkOptions.iOperation=1
            tFrameBulkOptions.bCopyAnimationsOnDuplicate=false
            local button=tImGui.Button
            tImGui.Button=function(label,...)
                if label==tLang.L('apply_btn') then return true end
                return button(label,...)
            end
            showFrameBulkOperations()
            tImGui.Button=button
            expectedFrames=4;previewFrame=3
            requestPreview();stage=2;ticks=0
        elseif stage==2 then
            requestPreview()
            if ticks<5 then return end
            checkPreview()
            assert(tFrameList[3].tShape.drawMode.modeFrontFace=='CCW')
            assert(onSaveEditionSprite(project))
            open(project,onOpenSprite)
            requestPreview();stage=3;ticks=0
        elseif stage==3 then
            requestPreview()
            if ticks<5 then return end
            checkPreview()
            print('SPRITE IMPORT OK: CCW preview / duplicate / project round trip')
            mbm.quit()
        end
    end)
    if not ok then print('SPRITE IMPORT FAIL '..tostring(err));mbm.quit() end
end
function onEndScene()
    os.remove(png);os.remove(binary);os.remove(project);os.remove(snapshot)
end
