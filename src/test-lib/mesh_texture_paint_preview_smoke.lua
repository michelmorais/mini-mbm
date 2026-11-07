--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                           |
|                                                                                                                       |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated            |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation         |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and         |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                      |
|                                                                                                                       |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                         |
|                                                                                                                       |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE    |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR   |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR         |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.        |
|-----------------------------------------------------------------------------------------------------------------------|
]]--

-- Run from the repository root with mini-mbm --scene <this file>
-- --disable_select_monitor --nosplash -w 320 -h 240 and a 25-second process timeout.
package.path = 'src/test-lib/?.lua;editor/?.lua;' .. package.path
dofile('editor/mesh_texture_paint_editor.lua')
local init, shutdown, editorLoop = onInitScene, onEndScene, onLoop
local touchDown, touchUp, keyDown = onTouchDown, onTouchUp, onKeyDown

-- Exercise the real input/preview pipeline without navigating the editor UI.
-- Keep access to private functions here instead of exposing a production test API.
local function upvalue(fn, name, seen)
    seen = seen or {}
    if seen[fn] then return end
    seen[fn] = true
    local children = {}
    for i = 1, math.huge do
        local key, value = debug.getupvalue(fn, i)
        if not key then break end
        if key == name then return value end
        if type(value) == 'function' then children[#children + 1] = value end
    end
    for _, child in ipairs(children) do
        local value = upvalue(child, name, seen)
        if value ~= nil then return value end
    end
end
local state = assert(upvalue(editorLoop, 'state'))
local loadMesh = assert(upvalue(editorLoop, 'loadMesh'))
local requestPreview = assert(upvalue(editorLoop, 'requestPreviewRebuild'))
local processPreview = assert(upvalue(editorLoop, 'processPreviewJob'))
local processStroke = assert(upvalue(editorLoop, 'processPendingStroke'))
local history = assert(upvalue(keyDown, 'history'))
local temp = require('test_temp_path').new()
os.remove(temp)
local source, fixture, capture = temp .. '.png', temp .. '.msh', temp .. '_capture.png'
local rt, started, failed, stage, settled = nil, nil, false, 0, 0
local originalPixels, redPixels, meshReloads, idleBuilds, idleUploads
local original = string.rep(string.char(100, 100, 100, 255), 32 * 32)

local function protect(fn)
    local ok, err = xpcall(fn, debug.traceback)
    if not ok then
        failed = true
        print('MESH TEXTURE PAINT PREVIEW SMOKE FAIL: ' .. tostring(err))
        mbm.quit()
    end
end

local function makeFixture()
    assert(mbm.writeImagePixels(source, original, 32, 32))
    local asset = meshDebug:new()
    asset:setType('mesh'); asset:setStride(3); asset:enableNormal(false); asset:enableUv(true)
    asset:setModeDraw('TRIANGLES'); asset:setModeCullFace('BACK'); asset:setModeFrontFace('CCW')
    local f = asset:addFrame(3)
    local s = asset:addSubSet(f)
    local a = {x=-1,y=-1,z=0,u=0,v=0}
    local b = {x=1,y=-1,z=0,u=1,v=0}
    local c = {x=-1,y=1,z=0,u=0,v=1}
    local d = {x=1,y=1,z=0,u=1,v=1}
    assert(asset:addVertex(f, s, {a,b,c,c,b,d,c,b,a,d,b,c}))
    assert(asset:setTexture(f, s, source))
    local other = asset:addSubSet(f)
    assert(asset:addVertex(f, other, {
        {x=-.8,y=-.8,z=-.1,u=0,v=0}, {x=.8,y=-.8,z=-.1,u=1,v=0},
        {x=0,y=.8,z=-.1,u=.5,v=1}}))
    assert(asset:setTexture(f, other, '#00FFFFFF'))
    assert(asset:addAnim('Static', 1, 1, 1, 0))
    assert(asset:save(fixture, false, false, false))
end

local function paint(color, cancel)
    state.brush.color = color
    state.brush.radius = 6
    local w, h = mbm.getSizeScreen()
    touchDown(1, w / 2, h / 2)
    assert(state.stroke, 'Center click did not start a stroke: ' .. state.status)
    if cancel then
        processStroke()
        keyDown(mbm.getKeyCode('ESC'))
        assert(not state.stroke, 'Escape did not cancel the stroke')
    else
        touchUp(1, w / 2, h / 2)
    end
    settled = 0
end

local function pixels(colorChannel)
    assert(rt:save(capture))
    local bytes, w, h = mbm.readImagePixels(capture)
    assert(bytes and w == 96 and h == 96)
    if colorChannel then
        local count = 0
        for i = 1, #bytes, 4 do
            local r, g, b = bytes:byte(i, i + 2)
            local channels = {r, g, b}
            if channels[colorChannel] > 180 and
                channels[colorChannel % 3 + 1] < 60 and channels[(colorChannel + 1) % 3 + 1] < 60 then
                count = count + 1
            end
        end
        assert(count > 20, 'Paint is missing from the rendered mesh at stage ' .. stage)
    end
    return bytes
end

function onInitScene()
    started = mbm.getTimeRun()
    protect(function()
        init()
        makeFixture()
        loadMesh(fixture)
        rt = render2texture:new('2ds')
        assert(rt:create(96, 96, true))
        rt:getCamera('3d'):setPos(0, 0, 4)
        rt:getCamera('3d'):setFocus(0, 0, 0)
        rt:setColor(0, 0, 0, 0)
    end)
end

function onLoop()
    if failed then return end
    protect(function()
        assert(mbm.getTimeRun() - started < 20, 'Preview test timed out')
        processStroke()
        requestPreview()
        processPreview()
        if state.previewJob or state.previewDirty or state.painting then settled = 0; return end
        settled = settled + 1
        if settled < 3 then return end -- allow the GPU to render the new binding/content
        settled = 0
        if stage == 0 then
            rt:add(state.previewMesh)
        elseif stage == 1 then
            originalPixels = pixels()
            assert(originalPixels:find(string.char(100,100,100), 1, true), 'Fixture did not render')
            paint({r=1,g=0,b=0,a=1})
        elseif stage == 2 then
            redPixels = pixels(1)
            assert(redPixels ~= originalPixels)
            meshReloads = state.previewMeshReloads
            paint({r=0,g=1,b=0,a=1})
        elseif stage == 3 then
            assert(pixels(2) ~= redPixels)
            assert(state.previewMeshReloads == meshReloads, 'Second stroke unnecessarily rebuilt the mesh')
            history(false)
        elseif stage == 4 then
            assert(pixels(1) == redPixels, 'Undo did not restore the rendered paint')
            paint({r=0,g=0,b=1,a=1}, true)
        elseif stage == 5 then
            assert(pixels(1) == redPixels, 'Cancelled stroke changed the rendered paint')
            rt:clear()
            local previousUiPath = state.previewUiTexturePath
            loadMesh(fixture)
            if previousUiPath then os.remove(previousUiPath) end
        elseif stage == 6 then
            rt:add(state.previewMesh)
        elseif stage == 7 then
            assert(pixels() == originalPixels, 'Reopening the mesh retained the old paint')
            paint({r=0,g=0,b=1,a=1})
        elseif stage == 8 then
            pixels(3)
            assert(mbm.readImagePixels(source) == original, 'Painting overwrote the original texture')
            require('mesh_texture_paint_save_checks').run(state, editorLoop, upvalue, temp)
            idleBuilds, idleUploads = state.previewBuilds, state.previewTextureReloads
        else
            assert(state.previewBuilds == idleBuilds and state.previewTextureReloads == idleUploads,
                'Idle preview rebuilt or uploaded textures')
            if stage == 20 then print('MESH TEXTURE PAINT PREVIEW SMOKE PASS'); mbm.quit() end
        end
        stage = stage + 1
    end)
end

function onEndScene()
    if rt then rt:clear(); rt:release(); rt:destroy() end
    local uiPath = state.previewUiTexturePath
    shutdown()
    if uiPath then os.remove(uiPath) end
    os.remove(source); os.remove(fixture); os.remove(capture)
end
