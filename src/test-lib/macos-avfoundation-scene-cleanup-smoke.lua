--[[--------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                     |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                            |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated          |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and      |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                    |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO     |
| THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS|
| OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR   |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.      |
|----------------------------------------------------------------------------------------------------------------------]]

local sound = nil
local start_time = nil
local changed_scene = false

function onInitScene()
    sound = assert(audio:new("lifecycle tone.wav", false, false, false), "scene cleanup audio load failed")
    sound:onEnd(function()
        error("old-scene audio callback ran after scene teardown")
    end)
    assert(sound:play(false), "scene cleanup audio play failed")
    start_time = mbm.getTimeRun()
end

function onLoop()
    if not changed_scene and mbm.getTimeRun() - start_time >= 0.05 then
        changed_scene = true
        mbm.loadScene("src/test-lib/macos-avfoundation-scene-cleanup-target.lua")
    end
end
