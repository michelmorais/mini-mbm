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
local api={}; assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local frame,started,baseline=0,nil,nil
local function runInit()
    init(); started=mbm.getTimeRun()
    local pixels={}; for y=0,95 do for x=0,127 do
        local v=((x%32)<3 or (y%32)<3) and 20 or 160+(x+y)%70
        pixels[#pixels+1]=v; pixels[#pixels+1]=v; pixels[#pixels+1]=v
    end end
    local source='/tmp/ime_editor_source.png'
    assert(mbm.createTexture(pixels,128,96,3,'ime_editor_source',source))
    assert(api.openImage(source))
    assert(api.action(function(p)
        p.defaults.columns=8; p.defaults.rows=8
        Model.grid(p,{columns=4,rows=3,marginX=0,marginY=0,gapX=0,gapY=0})
        Model.add(p,'ellipse',0,0,32,32)
        Model.fromPoints(p,{{x=32,y=32},{x=63,y=32},{x=63,y=43},{x=43,y=43},{x=43,y=63},{x=32,y=63}})
    end))
    api.select(13); api.rebuild(); assert(api.state.preview and api.state.report)
    api.select(14); api.rebuild(); assert(api.state.preview and api.state.report)
    local before=api.state.project.defaults.relief
    assert(api.action(function(p) p.defaults.relief=3 end)); api.undo(false)
    assert(api.state.project.defaults.relief==before); api.undo(true); assert(api.state.project.defaults.relief==3)
    assert(api.saveProject('/tmp/ime_editor.imesh')); assert(not api.state.modified)
    assert(api.openProject('/tmp/ime_editor.imesh')); assert(#api.state.project.regions==14)
    api.select(1); api.select(2,true)
    api.state.values.depth=40; api.applyProperties()
    assert(Model.options(api.state.project,api.state.project.regions[1]).depth==40)
    assert(Model.options(api.state.project,api.state.project.regions[2]).depth==40)
    assert(Model.options(api.state.project,api.state.project.regions[3]).depth==20)
    api.exportOne('/tmp/ime_editor_selected.msh')
    for _,r in ipairs(api.state.project.regions) do os.remove('/tmp/image-mesh-stage2-export/'..IO.exportName(r)) end
    api.beginBatch('/tmp/image-mesh-stage2-export')
    -- Relinking a missing source restores the saved project only after size validation.
    local missing=Model.copy(api.state.project); missing.image.path='/tmp/ime_missing_source.png'
    IO.save(missing,'/tmp/ime_missing.imesh',tUtil.save)
    assert(not api.openProject('/tmp/ime_missing.imesh')); assert(api.state.missing)
    api.relink(source); assert(not api.state.missing and api.state.modified)
    api.select(14); api.rebuild(); assert(api.state.preview)
    print('IMAGE MESH EDITOR PROJECT / HISTORY / MULTISELECT / RELINK OK')
end
function onInitScene()
    local ok,err=pcall(runInit); if not ok then print('IMAGE MESH EDITOR SMOKE FAIL: '..tostring(err)); mbm.quit() end
end
function onLoop(delta)
    frame=frame+1
    local e=api.state
    local mouse,click,down
    local commands={
        [90]={'rectangle',5,5,true,true},[91]={'rectangle',25,25,false,true},[92]={'rectangle',25,25,false,false},
        [94]={'ellipse',40,5,true,true},[95]={'ellipse',60,25,false,true},[96]={'ellipse',60,25,false,false},
        [98]={'polygon',70,40,true,true},[100]={'polygon',100,40,true,true},[102]={'polygon',100,55,true,true},
        [104]={'polygon',85,55,true,true},[106]={'polygon',85,75,true,true},[108]={'polygon',70,75,true,true},
        [112]={'select',10,10,true,true},[113]={'select',16,14,false,true},[114]={'select',16,14,false,false},
        [117]={'select',31,29,true,true},[118]={'select',35,33,false,true},[119]={'select',35,33,false,false},
        [120]={'select',70,40,true,true},[121]={'select',72,42,false,true},[122]={'select',72,42,false,false},
    }
    if frame==93 then assert(#e.project.regions==15 and e.project.regions[15].w==21) end
    if frame==97 then assert(#e.project.regions==16 and e.project.regions[16].shape=='ellipse') end
    if frame==110 then api.finishPolygon(); assert(#e.project.regions==17) end
    if frame==112 then api.select(15) end
    if frame==115 then assert(e.project.regions[15].x==11 and e.project.regions[15].y==9); api.undo(false); assert(e.project.regions[15].x==5) end
    if frame==116 then api.undo(true); assert(e.project.regions[15].x==11) end
    if frame==120 then assert(e.project.regions[15].w==25 and e.project.regions[15].h==25); api.select(17) end
    if frame==89 then e.tool='rectangle' elseif frame==93 then e.tool='ellipse' elseif frame==97 then e.tool='polygon' elseif frame==111 then e.tool='select' end
    local command=commands[frame]
    local originalMouse,originalClick,originalDown,originalHover=tImGui.GetMousePos,tImGui.IsMouseClicked,tImGui.IsMouseDown,tImGui.IsItemHovered
    if frame>=90 and frame<=122 then
        if command then
            e.tool=command[1]; local o=e.canvasTransform
            mouse={x=o.x+(command[2]+0.25)*o.scale,y=o.y+(command[3]+0.25)*o.scale}; click=command[4]; down=command[5]
        else mouse=originalMouse(); click=false; down=false end
        tImGui.GetMousePos=function() return mouse end
        tImGui.IsMouseClicked=function(button) return button==0 and click end
        tImGui.IsMouseDown=function(button) return button==0 and down end
        tImGui.IsItemHovered=function(flags)
            local origin=tImGui.GetItemRectMin(); local o=e.canvasTransform
            return o and origin.x==o.x and origin.y==o.y
        end
    end
    loop(delta)
    tImGui.GetMousePos,tImGui.IsMouseClicked,tImGui.IsMouseDown,tImGui.IsItemHovered=originalMouse,originalClick,originalDown,originalHover
    if frame==130 then
        assert(e.project.regions[17].contour[1].x>0)
        assert(e.preview and e.report,'edited contour did not generate')
        print('IMAGE MESH EDITOR DRAW / MOVE / RESIZE / POLYGON POINT INPUT OK')
    end
    if frame==30 then
        assert(not api.state.batch)
        for _,r in ipairs(api.state.project.regions) do
            local path='/tmp/image-mesh-stage2-export/'..IO.exportName(r)
            local mesh=meshDebug:new(); assert(mesh:load(path)); assert(mesh:check())
        end
        baseline=api.state.builds
    end
    if frame==80 then assert(api.state.builds==baseline,'idle editor rebuilt geometry'); assert(not api.state.target.visible,'idle render target still active'); print('IMAGE MESH EDITOR UI / BATCH / IDLE OK') end
    if started and mbm.getTimeRun()-started>8 then mbm.quit() end
end
function onEndScene() finish() end
