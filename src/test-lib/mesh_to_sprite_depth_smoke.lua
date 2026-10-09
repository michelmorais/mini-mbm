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


package.path='src/test-lib/?.lua;editor/?.lua;'..package.path
dofile('editor/mesh_to_sprite_editor.lua')
local init,loop,finish=onInitScene,onLoop,onEndScene
local e=MeshToSpriteEditor
local step,started=0,0
local prefix=require('test_temp_path').new();os.remove(prefix)
local nearImage=prefix..'_near.png'
local referenceImage=prefix..'_reference.png'
function onInitScene()
    init();started=mbm.getTimeRun()
    e.loadSource('src/test-lib/ChompBot.msh')
    local p=e.project
    p.width=512;p.height=512;p.clip='bite';p.stop=1.5;p.cycle=false
    p.camera={azimuth=0.595,elevation=0.3749999,distance=150,
        fx=10.39224,fy=0.4708099,fz=1.274649,near=0.1,far=100000,roll=0}
    p.background={r=0.3,g=0.3,b=0.3,a=1};e.time=0.75;e.refresh=true
end
function onLoop(dt)
    local ok,err=pcall(function()
        -- Deterministic delta exercises the editor playback stop as well.
        loop(0.02);step=step+1
        assert(mbm.getTimeRun()-started<12,'timeout')
        if step==3 then
            assert(e.target:save(nearImage))
            e.project.camera.near=10;e.refresh=true
        elseif step==6 then
            assert(e.target:save(referenceImage))
            local a,w,h=mbm.readImagePixels(nearImage)
            local b=assert(mbm.readImagePixels(referenceImage))
            assert(w==512 and h==512 and #a==#b)
            local changed,visible=0,0
            local background=b:sub(1,4)
            for i=1,#a,4 do
                local pixel=b:sub(i,i+3)
                if a:sub(i,i+3)~=pixel then changed=changed+1 end
                if pixel~=background then visible=visible+1 end
            end
            assert(visible>1000,'Comparison images are blank')
            -- Allow rasterization edge rounding; 16-bit depth changed 902 pixels.
            assert(changed<64,'Render-target depth loses occlusion: '..changed..' pixels differ')
            e.time=e.project.stop-0.001;e.playing=true
            print('M2S DEPTH pixels changed: '..changed)
        elseif step==7 then
            assert(not e.playing and e.time==e.project.stop,'Non-looping playback did not stop')
            print('M2S DEPTH OK: ChompBot 512 / occlusion / playback stop');mbm.quit()
        end
    end)
    if not ok then print('M2S DEPTH FAIL '..tostring(err));mbm.quit() end
end
function onEndScene()
    finish();os.remove(nearImage);os.remove(referenceImage)
end
