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
local formats = {"wav", "aiff", "caf", "mp3", "m4a", "ogg"}
local active_audio = nil
local start_time = nil
local end_callback_received = false
local controls_checked = false

function onInitScene()
    for _, extension in ipairs(formats) do
        local sound = audio:new("tom audio Unicode á." .. extension, false, false, false)
        assert(sound, "AVFoundation failed to load " .. extension)
        local length_ms = sound:getLen()
        print("info", "green", "AVFoundation " .. extension .. " duration_ms=" .. tostring(length_ms))
        assert(length_ms >= 200 and length_ms <= 300,
               "AVFoundation duration is not milliseconds for " .. extension .. ": " .. tostring(length_ms))
        sound:destroy()
    end

    active_audio = assert(audio:new("tom audio Unicode á.wav", false, false, false),
                          "AVFoundation failed to create the control-test audio")
    assert(active_audio:setVolume(0.4), "setVolume failed")
    assert(math.abs(active_audio:getVolume() - 0.4) < 0.01, "getVolume mismatch")
    assert(active_audio:setPan(-0.25), "setPan failed")
    assert(math.abs(active_audio:getPan() + 0.25) < 0.01, "getPan mismatch")
    assert(active_audio:getPitch() == 1.0, "unsupported pitch must remain neutral")
    assert(active_audio:setPitch(1.25) == false, "unsupported pitch unexpectedly succeeded")
    assert(active_audio:setPosition(10) == false, "unsupported seek unexpectedly succeeded")
    active_audio:onEnd(function()
        end_callback_received = true
    end)
    assert(active_audio:play(false), "play failed")
    start_time = mbm.getTimeRun()
end

function onLoop()
    local elapsed = mbm.getTimeRun() - start_time
    if not controls_checked and elapsed >= 0.05 then
        assert(active_audio:pause(), "pause failed")
        assert(active_audio:isPaused(), "isPaused did not reflect pause")
        assert(active_audio:play(false), "play after pause failed")
        controls_checked = true
    end
    if elapsed >= 1.0 then
        assert(end_callback_received, "end-of-stream callback was not delivered")
        assert(not active_audio:isPlaying(), "audio remained playing after end-of-stream")
        assert(active_audio:play(true), "looping play failed")
        assert(active_audio:stop(), "stop failed")
        active_audio:destroy()
        active_audio = nil
        print("info", "green", "MACOS_AVFOUNDATION_SMOKE_OK formats=6")
        mbm.quit()
    end
end
