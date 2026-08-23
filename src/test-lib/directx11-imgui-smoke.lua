--[[--------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                     |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                            |
|                                                                                                                       |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated          |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation      |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and      |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                    |
|                                                                                                                       |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of      |
| the Software.                                                                                                         |
|                                                                                                                       |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO    |
| THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS |
| OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.      |
|----------------------------------------------------------------------------------------------------------------------]]

local tImGui = require "ImGui"
local startTime = nil
local frameCount = 0

function onInitScene()
    mbm.showConsole(false)
    startTime = mbm.getTimeRun()
end

function onLoop(delta)
    frameCount = frameCount + 1
    local opened = tImGui.Begin("DirectX 11 ImGui smoke", false, 0)
    if opened then
        tImGui.Text("DX11 renderer initialized")
        tImGui.Text(string.format("Frames: %d", frameCount))
        tImGui.Text(string.format("Delta: %.4f", delta))
        tImGui.Button("Rendering", {x = 140, y = 32})
    end
    tImGui.End()

    if startTime and mbm.getTimeRun() - startTime >= 3 then
        print("info", "green", "DIRECTX11_IMGUI_SMOKE_OK frames=" .. tostring(frameCount))
        mbm.quit()
    end
end
