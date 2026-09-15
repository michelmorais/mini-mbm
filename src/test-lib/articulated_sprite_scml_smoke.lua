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

-- Optional real asset fixture, supplied locally (not redistributed with the engine).
package.path='editor/?.lua;'..package.path
local editor={}
assert(loadfile('editor/sprite_maker_articulated.lua'))(editor)
local init,loop=onInitScene,onLoop
local start,checked,initialized
local visited={}
local previousClip,previousPreview
local IO=require 'articulated_sprite_io'
function onInitScene()
    init()
    start=mbm.getTimeRun()
    editor.importScml('/home/michel/Downloads/SpriterFile/FW_Hero_1.scml')
    local p=editor.state.project
    assert(#p.clips==8 and #p.frames==8 and #p.images==27)
    local path=os.tmpname()..'.spt'
    IO.export(p,path)
    local roundtrip=IO.import(path)
    assert(#roundtrip.clips==8 and #roundtrip.frames==8)
    for i,clip in ipairs(roundtrip.clips) do
        assert(clip.name==p.clips[i].name and clip.frame==i)
        assert(#clip.tracks==#p.clips[i].tracks)
        assert(clip.loop==(i~=8))
        for ti,track in ipairs(clip.tracks) do assert(#track.keys==#p.clips[i].tracks[ti].keys) end
    end
    os.remove(path)
    editor.state.playing=true
    editor.state.camera:setPos(0,120)
    start=mbm.getTimeRun()
    initialized=true
    print('SCML ROUNDTRIP OK: 8 articulated clips, 8 base frames, 27 images')
end
function onLoop(delta)
    assert(initialized,'SCML initialization failed')
    local elapsed=mbm.getTimeRun()-start
    local index=math.min(8,math.floor(elapsed)+1)
    if editor.state.clip~=index then editor.selectClip(index); editor.state.playing=true end
    loop(delta)
    assert(editor.state.preview,'Missing SCML preview')
    if previousClip==index then assert(previousPreview==editor.state.preview,'Idle preview rebuilt') end
    previousClip,previousPreview=index,editor.state.preview
    visited[index]=true
    assert(editor.state.preview:getArticulatedAnimationTime(editor.state.project.clips[index].name))
    if elapsed>8 then
        for i=1,8 do assert(visited[i],'Clip not exercised: '..i) end
        if not checked then print('SCML PLAYBACK OK'); checked=true end
        mbm.quit()
    end
end
