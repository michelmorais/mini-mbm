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
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|
]]--

package.path = 'editor/?.lua;' .. package.path
local Stream = require 'facial_capture_stream'

local packet = Stream.decodePacket([[{"timestamp":0.5,"sequence":2,"weights":{"jawOpen":0.25,"EyeBlink_L":1}}]])
assert(packet.timestamp == .5 and packet.sequence == 2)
assert(packet.weights.jawOpen == .25 and packet.weights.EyeBlink_L == 1)
assert(not pcall(Stream.decodePacket, '{"timestamp":0,"weights":{"jawOpen":NaN}}'))
assert(not pcall(Stream.decodePacket, '{"timestamp":-1,"weights":{}}'))
assert(not pcall(Stream.decodePacket, '{"timestamp":x1,"weights":{}}'))
assert(not pcall(Stream.decodePacket, '{"timestamp":0,"weights":{"jawOpen":1},"extra":true}'))
assert(not pcall(Stream.decodePacket, '{"timestamp":0,"weights":{"jawOpen":1,"jawOpen":0}}'))

local mesh = {weights = {}, calls = 0, resets = 0}
function mesh:setFacialWeight(name, value)
    self.weights[name], self.calls = value, self.calls + 1
    return true
end
function mesh:resetFacialWeights()
    self.weights, self.resets = {}, self.resets + 1
    return true
end
local Mapping = require 'facial_capture_mapping'
local mapping = Mapping.buildTargetIndex({'jawOpen', 'eyeBlinkLeft'})
local replay = Stream.newReplay({
    '{"timestamp":0,"sequence":1,"weights":{"jawOpen":0.1}}',
    '{"timestamp":0.5,"sequence":2,"weights":{"EyeBlink_L":0.8}}',
    '{"timestamp":1,"sequence":3,"weights":{"jawOpen":0.6}}',
}, {resetOnDisconnect = true})
assert(replay:status().connected)
local report = replay:step(0, mesh, mapping)
assert(#report.packets == 1 and mesh.weights.jawOpen == .1)
report = replay:step(.25, mesh, mapping)
assert(#report.packets == 0)
report = replay:step(.25, mesh, mapping)
assert(#report.packets == 1 and mesh.weights.eyeBlinkLeft == .8)
report = replay:step(.5, mesh, mapping)
assert(#report.packets == 1 and report.ended and not report.connected and mesh.weights.jawOpen == nil)
assert(mesh.resets == 1 and replay:step(1, mesh, mapping).ended)

local loop = Stream.newReplay({
    {timestamp = 0, sequence = 1, weights = {jawOpen = 0}},
    {timestamp = 1, sequence = 2, weights = {jawOpen = 1}},
}, {loop = true})
report = loop:step(1.25, mesh, mapping)
assert(report.loops == 1 and #report.packets == 3 and loop:status().connected)
assert(not pcall(Stream.newReplay, {
    {timestamp = .5, weights = {}}, {timestamp = .25, weights = {}}
}))
assert(not pcall(Stream.newReplay, {
    {timestamp = 0, sequence = 2, weights = {}}, {timestamp = 1, sequence = 1, weights = {}}
}))

local path = (os.getenv('TMPDIR') or '/tmp') .. '/mini-mbm-facial-capture-stream.ndjson'
local file = assert(io.open(path, 'wb'))
file:write('{"timestamp":0,"weights":{"jawOpen":0.3}}\n\n{"timestamp":1,"weights":{"jawOpen":0.4}}\n')
assert(file:close())
local fromFile = Stream.newFileReplay(path)
os.remove(path)
assert(fromFile:status().packetCount == 2 and fromFile.duration == 1)

print('FACIAL CAPTURE STREAM PASS')
