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

   Deterministic, dependency-free facial capture packet decoder and replay source.
   A future webcam or OSC adapter can produce the same packet tables.
]]--

local Mapping = require 'facial_capture_mapping'
local M = {version = 1}

local function finite(value)
    return type(value) == 'number' and value == value and math.abs(value) < math.huge
end

local function trim(text)
    return text:match('^%s*(.-)%s*$')
end

local function fail(message)
    error('facial capture packet: ' .. message, 0)
end

local function decodeString(text, position)
    if text:sub(position, position) ~= '"' then fail('expected string') end
    position = position + 1
    local result = {}
    while position <= #text do
        local character = text:sub(position, position)
        position = position + 1
        if character == '"' then return table.concat(result), position end
        if character:byte() < 32 then fail('control character in string') end
        if character ~= '\\' then
            result[#result + 1] = character
        else
            local escape = text:sub(position, position)
            position = position + 1
            local escaped = {['"'] = '"', ['\\'] = '\\', ['/'] = '/', b = '\b',
                f = '\f', n = '\n', r = '\r', t = '\t'}
            if escaped[escape] then
                result[#result + 1] = escaped[escape]
            elseif escape == 'u' then
                local hex = text:sub(position, position + 3)
                if not hex:match('^%x%x%x%x$') then fail('invalid unicode escape') end
                local code = tonumber(hex, 16)
                if code >= 0xD800 and code <= 0xDFFF then fail('surrogate escape is unsupported') end
                result[#result + 1] = utf8.char(code)
                position = position + 4
            else
                fail('invalid string escape')
            end
        end
    end
    fail('unterminated string')
end

local function skipWhitespace(text, position)
    local _, last = text:find('^%s*', position)
    return (last or position - 1) + 1
end

local function decodeNumber(text, position)
    local first = text:sub(position, position)
    if not first:match('^[%d%-]$') then fail('expected finite number') end
    local token = text:match('[-+%d%.eE]+', position)
    if not token or token:sub(1, 1) == '+' then fail('expected finite number') end
    local mantissa, exponent = token:match('^(-?[^eE]+)[eE]([+-]?%d+)$')
    mantissa = mantissa or token
    if not mantissa:match('^-?%d+$') and not mantissa:match('^-?%d+%.%d+$') then
        fail('invalid number')
    end
    if mantissa:match('^-?0%d') then fail('invalid leading zero') end
    if token:find('[eE]') and not exponent then fail('invalid exponent') end
    local value = tonumber(token)
    if not finite(value) then fail('number is not finite') end
    return value, position + #token
end

local function decodeWeights(text, position)
    position = skipWhitespace(text, position)
    if text:sub(position, position) ~= '{' then fail('weights must be an object') end
    position = skipWhitespace(text, position + 1)
    local weights, seen = {}, {}
    if text:sub(position, position) == '}' then return weights, position + 1 end
    while true do
        local name
        name, position = decodeString(text, position)
        if name == '' then fail('empty weight name') end
        if seen[name] then fail('duplicate weight name ' .. name) end
        seen[name] = true
        position = skipWhitespace(text, position)
        if text:sub(position, position) ~= ':' then fail('expected weight colon') end
        local value
        value, position = decodeNumber(text, skipWhitespace(text, position + 1))
        weights[name] = value
        position = skipWhitespace(text, position)
        local separator = text:sub(position, position)
        if separator == '}' then return weights, position + 1 end
        if separator ~= ',' then fail('expected weight separator') end
        position = skipWhitespace(text, position + 1)
    end
end

function M.decodePacket(json)
    if type(json) ~= 'string' or #json == 0 or #json > 64 * 1024 then
        fail('invalid packet size')
    end
    local text = trim(json)
    local position = skipWhitespace(text, 1)
    if text:sub(position, position) ~= '{' then fail('packet must be an object') end
    position = skipWhitespace(text, position + 1)
    local packet, seen = {weights = nil}, {}
    if text:sub(position, position) == '}' then fail('missing weights') end
    while true do
        local key
        key, position = decodeString(text, position)
        if seen[key] then fail('duplicate packet field ' .. key) end
        seen[key] = true
        position = skipWhitespace(text, position)
        if text:sub(position, position) ~= ':' then fail('expected field colon') end
        position = skipWhitespace(text, position + 1)
        if key == 'weights' then
            packet.weights, position = decodeWeights(text, position)
        elseif key == 'timestamp' then
            packet.timestamp, position = decodeNumber(text, position)
            if packet.timestamp < 0 then fail('timestamp must be non-negative') end
        elseif key == 'sequence' then
            packet.sequence, position = decodeNumber(text, position)
            if packet.sequence < 0 or packet.sequence % 1 ~= 0 then fail('sequence must be a non-negative integer') end
        else
            fail('unsupported field ' .. key)
        end
        position = skipWhitespace(text, position)
        local separator = text:sub(position, position)
        if separator == '}' then break end
        if separator ~= ',' then fail('expected packet separator') end
        position = skipWhitespace(text, position + 1)
    end
    position = skipWhitespace(text, position + 1)
    if position <= #text then fail('trailing packet data') end
    if packet.timestamp == nil then fail('missing timestamp') end
    return packet
end

local function copyPacket(packet)
    if type(packet) ~= 'table' or not finite(packet.timestamp) or packet.timestamp < 0 then
        fail('invalid packet timestamp')
    end
    if packet.sequence ~= nil and
        (not finite(packet.sequence) or packet.sequence < 0 or packet.sequence % 1 ~= 0) then
        fail('invalid packet sequence')
    end
    if type(packet.weights) ~= 'table' then fail('packet weights must be a table') end
    local result = {timestamp = packet.timestamp, sequence = packet.sequence, weights = {}}
    for name, value in pairs(packet.weights) do
        if type(name) ~= 'string' or name == '' then fail('invalid weight name') end
        if not finite(value) then fail('weight ' .. name .. ' is not finite') end
        result.weights[name] = value
    end
    return result
end

function M.newReplay(packets, options)
    if type(packets) ~= 'table' or #packets == 0 then fail('replay requires at least one packet') end
    options = options or {}
    local replay = {
        packets = {}, index = 1, time = 0, loop = options.loop == true,
        resetOnDisconnect = options.resetOnDisconnect == true, connected = true,
        ended = false, duration = 0, lastSequence = nil,
    }
    local previousTimestamp = -math.huge
    local previousSequence = nil
    for position, packet in ipairs(packets) do
        local decoded = type(packet) == 'string' and M.decodePacket(packet) or copyPacket(packet)
        if decoded.timestamp < previousTimestamp then fail('timestamps must be ordered at packet ' .. position) end
        if decoded.sequence ~= nil and previousSequence ~= nil and decoded.sequence <= previousSequence then
            fail('sequences must increase at packet ' .. position)
        end
        replay.packets[#replay.packets + 1] = decoded
        previousTimestamp, previousSequence = decoded.timestamp, decoded.sequence or previousSequence
    end
    replay.duration = previousTimestamp
    if replay.loop and replay.duration <= 0 then fail('loop replay requires a positive duration') end

    function replay:reset()
        self.index, self.time, self.lastSequence = 1, 0, nil
        self.connected, self.ended = true, false
    end

    function replay:stop(mesh, mapping)
        if self.resetOnDisconnect and mesh and mapping then Mapping.reset(mesh, mapping) end
        self.connected, self.ended = false, true
    end

    function replay:status()
        return {connected = self.connected, ended = self.ended, time = self.time,
            packetIndex = self.index, packetCount = #self.packets, duration = self.duration}
    end

    function replay:step(delta, mesh, mapping)
        if not finite(delta) or delta < 0 then fail('replay delta must be non-negative') end
        if not mesh or not mapping then fail('replay requires a mesh and mapping index') end
        local result = {packets = {}, applied = 0, loops = 0, connected = self.connected,
            ended = self.ended}
        if self.ended then return result end
        self.time = self.time + delta
        local guard = 0
        while true do
            while self.index <= #self.packets and self.packets[self.index].timestamp <= self.time do
                local packet = self.packets[self.index]
                local applied = Mapping.apply(mesh, mapping, packet.weights)
                result.packets[#result.packets + 1] = {timestamp = packet.timestamp,
                    sequence = packet.sequence, report = applied}
                result.applied = result.applied + 1
                self.lastSequence = packet.sequence or self.lastSequence
                self.index = self.index + 1
            end
            if self.index <= #self.packets then break end
            if not self.loop then
                self:stop(mesh, mapping)
                break
            end
            self.time = self.time - self.duration
            self.index, self.lastSequence = 1, nil
            result.loops = result.loops + 1
            guard = guard + 1
            if guard > 1000 then fail('replay loop guard exceeded') end
        end
        result.connected, result.ended = self.connected, self.ended
        return result
    end

    return replay
end

function M.newFileReplay(path, options)
    if type(path) ~= 'string' or path == '' then fail('invalid replay path') end
    local file, errorMessage = io.open(path, 'rb')
    if not file then fail(errorMessage or 'could not open replay file') end
    local packets = {}
    for line in file:lines() do
        if trim(line) ~= '' then packets[#packets + 1] = line end
    end
    file:close()
    return M.newReplay(packets, options)
end

return M
