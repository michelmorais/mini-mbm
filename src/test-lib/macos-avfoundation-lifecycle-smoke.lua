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

local natural_audio = {}
local natural_callbacks = 0
local destroyed_callbacks = 0
local start_time = nil
local shutdown_audio = nil

local function new_audio(file_name)
    return assert(audio:new(file_name, false, false, false), "failed to load " .. file_name)
end

function onInitScene()
    -- Completion handlers from these objects must be invalidated before immediate deletion.
    for _ = 1, 32 do
        local sound = new_audio("lifecycle tone.wav")
        sound:onEnd(function()
            destroyed_callbacks = destroyed_callbacks + 1
        end)
        assert(sound:play(false), "play before immediate destroy failed")
        sound:destroy()
    end

    -- Replacing and stopping scheduled buffers must not allow an older generation to finish a newer one.
    local rapid = new_audio("lifecycle tone.wav")
    rapid:onEnd(function()
        destroyed_callbacks = destroyed_callbacks + 1
    end)
    for _ = 1, 100 do
        assert(rapid:play(false), "rapid play failed")
        assert(rapid:stop(), "rapid stop failed")
    end
    assert(rapid:play(true), "rapid loop play failed")
    rapid:destroy()

    -- Exercise simultaneous natural completions and scene-thread callback delivery.
    for index = 1, 16 do
        local sound = new_audio("lifecycle tone.wav")
        sound:onEnd(function()
            natural_callbacks = natural_callbacks + 1
        end)
        assert(sound:play(false), "simultaneous play failed at " .. tostring(index))
        natural_audio[index] = sound
    end

    -- The Opus container is deliberately unsupported and must resolve to the matching WAV.
    local opus_fallback = new_audio("lifecycle Opus fallback.ogg")
    assert(opus_fallback:getLen() >= 200 and opus_fallback:getLen() <= 300,
           "Opus WAV fallback duration mismatch")
    opus_fallback:destroy()

    start_time = mbm.getTimeRun()
end

function onLoop()
    -- Completion is reported by the audio-render thread, dispatched to the main
    -- queue, then delivered by the Lua scene callback queue. Allow those stages
    -- to drain without asserting transient intermediate counts every frame.
    if mbm.getTimeRun() - start_time < 2.0 then
        return
    end
    assert(destroyed_callbacks == 0,
           "callback ran after stop/destroy: " .. tostring(destroyed_callbacks))
    assert(natural_callbacks == #natural_audio,
           "missing simultaneous callbacks: " .. tostring(natural_callbacks) .. "/" .. tostring(#natural_audio))
    for _, sound in ipairs(natural_audio) do
        assert(not sound:isPlaying(), "completed simultaneous audio still reports playing")
        sound:destroy()
    end
    natural_audio = {}
    -- Leave one active loop owned by the scene so mbm.quit() exercises engine-level teardown.
    shutdown_audio = new_audio("lifecycle tone.wav")
    assert(shutdown_audio:play(true), "shutdown loop play failed")
    print("info", "green", "MACOS_AVFOUNDATION_LIFECYCLE_SMOKE_OK callbacks=" .. tostring(natural_callbacks))
    mbm.quit()
end
