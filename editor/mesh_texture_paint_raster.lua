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

-- Pure RGBA8 texture-layer operations for the mesh texture paint editor.
-- Coordinates are zero-based pixel-center coordinates.  Buffers keep their
-- payload as a binary string, matching mbm.readImagePixels/writeImagePixels.

local M = {}

M.BYTES_PER_PIXEL = 4
M.DEFAULT_TILE_SIZE = 32

local function integer(value, name)
    assert(type(value) == 'number' and value == math.floor(value) and value >= 1,
        (name or 'value') .. ' must be a positive integer')
    return value
end

local function clamp(value, minimum, maximum)
    if value < minimum then return minimum end
    if value > maximum then return maximum end
    return value
end

local function finiteNumber(value, fallback)
    if type(value) ~= 'number' or value ~= value or value == math.huge or value == -math.huge then
        return fallback
    end
    return value
end

local function byte(value)
    return clamp(math.floor(finiteNumber(value, 0) + 0.5), 0, 255)
end

local function bytesToString(bytes)
    local chunks = {}
    local chunkSize = 4096
    for first = 1, #bytes, chunkSize do
        local chunk = {}
        local last = math.min(#bytes, first + chunkSize - 1)
        for index = first, last do chunk[#chunk + 1] = bytes[index] end
        chunks[#chunks + 1] = string.char(table.unpack(chunk))
    end
    return table.concat(chunks)
end

local function stringToBytes(value, first, last)
    local bytes = {}
    local chunkSize = 4096
    for chunkFirst = first, last, chunkSize do
        local chunkLast = math.min(last, chunkFirst + chunkSize - 1)
        local chunk = {value:byte(chunkFirst, chunkLast)}
        for index = 1, #chunk do bytes[#bytes + 1] = chunk[index] end
    end
    return bytes
end

local function stringToRows(value, width, height)
    local rowBytes = width * M.BYTES_PER_PIXEL
    local rows = {}
    for y = 0, height - 1 do
        local first = y * rowBytes + 1
        rows[y + 1] = value:sub(first, first + rowBytes - 1)
    end
    return rows
end

local function pixelOffset(width, x, y)
    return (y * width + x) * M.BYTES_PER_PIXEL + 1
end

local function assertBuffer(buffer, name)
    assert(type(buffer) == 'table', (name or 'buffer') .. ' must be a table')
    integer(buffer.width, (name or 'buffer') .. '.width')
    integer(buffer.height, (name or 'buffer') .. '.height')
    assert(type(buffer.bytes) == 'string', (name or 'buffer') .. '.bytes must be a string')
    assert(#buffer.bytes == buffer.width * buffer.height * M.BYTES_PER_PIXEL,
        (name or 'buffer') .. '.bytes has the wrong size')
end

local function rowsOf(buffer)
    if not buffer.rows then
        buffer.rows = stringToRows(buffer.bytes, buffer.width, buffer.height)
    end
    return buffer.rows
end

local function rowOf(buffer, y)
    return buffer.rows and buffer.rows[y + 1] or buffer.bytes:sub(
        y * buffer.width * M.BYTES_PER_PIXEL + 1,
        (y + 1) * buffer.width * M.BYTES_PER_PIXEL)
end

local function normalizeColor(color)
    assert(type(color) == 'table', 'dab.color must be a table')
    local function component(named, indexed)
        return color[named] == nil and color[indexed] or color[named]
    end
    return {
        r = byte(component('r', 1)),
        g = byte(component('g', 2)),
        b = byte(component('b', 3)),
        a = byte(color.a == nil and (color[4] == nil and 255 or color[4]) or color.a),
    }
end

local function tileKey(tileX, tileY)
    return tileX .. ':' .. tileY
end

local function tileSizeOf(buffer)
    return integer(buffer.tileSize or M.DEFAULT_TILE_SIZE, 'tileSize')
end

local function copyBounds(bounds)
    if not bounds then return nil end
    return {
        minX = bounds.minX, minY = bounds.minY,
        maxX = bounds.maxX, maxY = bounds.maxY,
    }
end

local function expandBounds(buffer, x, y)
    local bounds = buffer.alphaBounds
    if not bounds then
        buffer.alphaBounds = {minX = x, minY = y, maxX = x, maxY = y}
        return
    end
    bounds.minX, bounds.minY = math.min(bounds.minX, x), math.min(bounds.minY, y)
    bounds.maxX, bounds.maxY = math.max(bounds.maxX, x), math.max(bounds.maxY, y)
end

local function addDirtyTile(layer, tileX, tileY)
    local key = tileKey(tileX, tileY)
    if layer.dirtyTiles[key] then return end
    local tileSize = tileSizeOf(layer)
    local x = tileX * tileSize
    local y = tileY * tileSize
    local tile = {
        key = key,
        tileX = tileX,
        tileY = tileY,
        x = x,
        y = y,
        width = math.min(tileSize, layer.width - x),
        height = math.min(tileSize, layer.height - y),
    }
    layer.dirtyTiles[key] = tile
    layer.dirtyTileList[#layer.dirtyTileList + 1] = tile
end

local function markDirty(layer, x, y)
    local tileSize = tileSizeOf(layer)
    addDirtyTile(layer, math.floor(x / tileSize), math.floor(y / tileSize))
    expandBounds(layer, x, y)
end

local function copyTile(buffer, tile)
    local rows = {}
    for y = tile.y, tile.y + tile.height - 1 do
        local line = rowOf(buffer, y)
        local first = tile.x * M.BYTES_PER_PIXEL + 1
        rows[#rows + 1] = line:sub(first, first + tile.width * M.BYTES_PER_PIXEL - 1)
    end
    return table.concat(rows)
end

local function replaceTileRows(buffer, tile, tileBytes)
    local rowBytes = tile.width * M.BYTES_PER_PIXEL
    local rows = rowsOf(buffer)
    for row = 0, tile.height - 1 do
        local y = tile.y + row
        local line = rows[y + 1]
        local first = tile.x * M.BYTES_PER_PIXEL + 1
        local start = row * rowBytes + 1
        local replacement = tileBytes:sub(start, start + rowBytes - 1)
        rows[y + 1] = line:sub(1, first - 1) .. replacement .. line:sub(first + rowBytes)
    end
    buffer.bytes = table.concat(rows)
end

local function composeTile(base, layer, output, tile)
    local rows = rowsOf(output)
    local first = tile.x * M.BYTES_PER_PIXEL + 1
    local last = (tile.x + tile.width) * M.BYTES_PER_PIXEL
    for y = tile.y, tile.y + tile.height - 1 do
        local baseBytes = stringToBytes(rowOf(base, y), first, last)
        local layerBytes = stringToBytes(rowOf(layer, y), first, last)
        local outputBytes = {}
        for offset = 1, #baseBytes, M.BYTES_PER_PIXEL do
            local baseR, baseG, baseB, baseA = baseBytes[offset], baseBytes[offset + 1],
                baseBytes[offset + 2], baseBytes[offset + 3]
            local layerR, layerG, layerB, layerA = layerBytes[offset], layerBytes[offset + 1],
                layerBytes[offset + 2], layerBytes[offset + 3]
            local source = layerA / 255
            local destination = baseA / 255
            local outputAlpha = source + destination * (1 - source)
            if outputAlpha <= 0 then
                outputBytes[offset], outputBytes[offset + 1], outputBytes[offset + 2], outputBytes[offset + 3] = 0, 0, 0, 0
            else
                outputBytes[offset] = byte((layerR * source + baseR * destination * (1 - source)) / outputAlpha)
                outputBytes[offset + 1] = byte((layerG * source + baseG * destination * (1 - source)) / outputAlpha)
                outputBytes[offset + 2] = byte((layerB * source + baseB * destination * (1 - source)) / outputAlpha)
                outputBytes[offset + 3] = byte(outputAlpha * 255)
            end
        end
        local line = rows[y + 1]
        rows[y + 1] = line:sub(1, first - 1) .. bytesToString(outputBytes) .. line:sub(last + 1)
    end
    return output
end

local function tileList(tiles)
    if not tiles then return {} end
    if #tiles > 0 then return tiles end
    local result = {}
    for _, tile in pairs(tiles) do result[#result + 1] = tile end
    table.sort(result, function(left, right) return left.key < right.key end)
    return result
end

function M.newBuffer(width, height, bytes)
    width = integer(width, 'width')
    height = integer(height, 'height')
    bytes = bytes or string.rep('\0', width * height * M.BYTES_PER_PIXEL)
    assert(type(bytes) == 'string' and #bytes == width * height * M.BYTES_PER_PIXEL,
        'bytes must contain exactly width * height * 4 bytes')
    return {width = width, height = height, bytes = bytes}
end

function M.newLayer(width, height, tileSize)
    if type(width) == 'table' then
        local source = width
        tileSize = height
        assertBuffer(source, 'source')
        width, height = source.width, source.height
    end
    local layer = M.newBuffer(width, height)
    layer.tileSize = integer(tileSize or M.DEFAULT_TILE_SIZE, 'tileSize')
    layer.dirtyTiles = {}
    layer.dirtyTileList = {}
    layer.alphaBounds = nil
    return layer
end

function M.cloneLayer(layer)
    assertBuffer(layer, 'layer')
    local clone = M.newLayer(layer.width, layer.height, layer.tileSize or M.DEFAULT_TILE_SIZE)
    clone.bytes = M.syncBytes(layer)
    clone.touched = layer.touched == true
    clone.alphaBounds = copyBounds(layer.alphaBounds)
    return clone
end

function M.syncBytes(buffer)
    assertBuffer(buffer)
    if buffer.rows then buffer.bytes = table.concat(buffer.rows) end
    return buffer.bytes
end

M.toBytes = M.syncBytes

function M.sample(buffer, x, y)
    assertBuffer(buffer)
    x = clamp(math.floor(finiteNumber(x, 0) + 0.5), 0, buffer.width - 1)
    y = clamp(math.floor(finiteNumber(y, 0) + 0.5), 0, buffer.height - 1)
    local line = rowOf(buffer, y)
    local offset = x * M.BYTES_PER_PIXEL + 1
    return {
        r = line:byte(offset),
        g = line:byte(offset + 1),
        b = line:byte(offset + 2),
        a = line:byte(offset + 3),
    }
end

function M.extractStamp(buffer, centerX, centerY, radius)
    assertBuffer(buffer)
    radius = math.max(0, math.floor(finiteNumber(radius, 0) + .5))
    local size = radius * 2 + 1
    local rows = {}
    for row = 0, size - 1 do
        local sourceY = clamp(math.floor(centerY - radius + row + .5), 0, buffer.height - 1)
        local sourceLine = rowOf(buffer, sourceY)
        local pixels = {}
        for column = 0, size - 1 do
            local sourceX = clamp(math.floor(centerX - radius + column + .5), 0, buffer.width - 1)
            local offset = sourceX * M.BYTES_PER_PIXEL + 1
            local pixel = sourceLine:sub(offset, offset + M.BYTES_PER_PIXEL - 1)
            local dx, dy = sourceX - centerX, sourceY - centerY
            if dx * dx + dy * dy > radius * radius then
                pixels[#pixels + 1] = pixel:sub(1, 3) .. string.char(0)
            else
                pixels[#pixels + 1] = pixel
            end
        end
        rows[#rows + 1] = table.concat(pixels)
    end
    return {width = size, height = size, radius = radius, bytes = table.concat(rows)}
end

M.createTransparentLayer = M.newLayer

function M.tilesForRect(buffer, x0, y0, x1, y1)
    assertBuffer(buffer)
    local tileSize = tileSizeOf(buffer)
    x0, y0 = math.min(x0, x1), math.min(y0, y1)
    x1, y1 = math.max(x0, x1), math.max(y0, y1)
    x0, y0 = math.max(0, math.floor(x0)), math.max(0, math.floor(y0))
    x1, y1 = math.min(buffer.width - 1, math.ceil(x1)), math.min(buffer.height - 1, math.ceil(y1))
    if x0 > x1 or y0 > y1 then return {} end
    local result = {}
    for tileY = math.floor(y0 / tileSize), math.floor(y1 / tileSize) do
        for tileX = math.floor(x0 / tileSize), math.floor(x1 / tileSize) do
            local x = tileX * tileSize
            local y = tileY * tileSize
            result[#result + 1] = {
                key = tileKey(tileX, tileY), tileX = tileX, tileY = tileY, x = x, y = y,
                width = math.min(tileSize, buffer.width - x), height = math.min(tileSize, buffer.height - y),
            }
        end
    end
    return result
end

function M.tilesForDab(buffer, x, y, radius)
    radius = math.max(0, finiteNumber(radius, 0))
    return M.tilesForRect(buffer, x - radius, y - radius, x + radius, y + radius)
end

function M.getDirtyTiles(layer, clear)
    assertBuffer(layer, 'layer')
    local result = {}
    for index, tile in ipairs(layer.dirtyTileList) do result[index] = tile end
    if clear then
        layer.dirtyTiles = {}
        layer.dirtyTileList = {}
    end
    return result
end

M.consumeDirtyTiles = function(layer) return M.getDirtyTiles(layer, true) end

function M.clearDirtyTiles(layer)
    assertBuffer(layer, 'layer')
    layer.dirtyTiles = {}
    layer.dirtyTileList = {}
end

function M.snapshotTiles(buffer, tiles)
    assertBuffer(buffer)
    local snapshot = {width = buffer.width, height = buffer.height, tiles = {}}
    if not tiles and buffer.dirtyTileList then tiles = buffer.dirtyTileList end
    for _, tile in ipairs(tileList(tiles)) do
        local copy = {
            key = tile.key,
            tileX = tile.tileX,
            tileY = tile.tileY,
            x = tile.x,
            y = tile.y,
            width = tile.width,
            height = tile.height,
        }
        copy.bytes = copyTile(buffer, copy)
        snapshot.tiles[#snapshot.tiles + 1] = copy
    end
    return snapshot
end

function M.restoreTiles(buffer, snapshot)
    assertBuffer(buffer)
    assert(type(snapshot) == 'table' and snapshot.width == buffer.width and snapshot.height == buffer.height,
        'snapshot dimensions do not match buffer')
    for _, tile in ipairs(snapshot.tiles or {}) do
        assert(type(tile.bytes) == 'string' and #tile.bytes == tile.width * tile.height * M.BYTES_PER_PIXEL,
            'invalid tile snapshot')
        replaceTileRows(buffer, tile, tile.bytes)
        if buffer.dirtyTiles then
            addDirtyTile(buffer, tile.tileX, tile.tileY)
            expandBounds(buffer, tile.x, tile.y)
            expandBounds(buffer, tile.x + tile.width - 1, tile.y + tile.height - 1)
        end
    end
    return buffer
end

function M.refreshAlphaBounds(buffer)
    assertBuffer(buffer, 'buffer')
    local rows = rowsOf(buffer)
    buffer.alphaBounds = nil
    for y = 0, buffer.height - 1 do
        local line = rows[y + 1]
        for x = 0, buffer.width - 1 do
            if line:byte(x * M.BYTES_PER_PIXEL + 4) > 0 then
                expandBounds(buffer, x, y)
            end
        end
    end
    return copyBounds(buffer.alphaBounds)
end

local function normalizeShape(value)
    return value == 'square' and 'square' or 'circle'
end

local function dabOptions(x, radius, hardness, opacity, color, mode, shape)
    if type(x) == 'table' then
        local dab = x
        return {
            x = finiteNumber(dab.x, 0), y = finiteNumber(dab.y, 0),
            radius = math.max(0, finiteNumber(dab.radius, 0)),
            hardness = clamp(finiteNumber(dab.hardness, 1), 0, 1),
            opacity = clamp(finiteNumber(dab.opacity, 1), 0, 1),
            color = normalizeColor(dab.color or dab), mode = dab.mode or 'paint',
            shape = normalizeShape(dab.shape),
            deferSync = dab.deferSync == true,
        }
    end
    return {
        x = finiteNumber(x, 0), y = finiteNumber(y, 0), radius = math.max(0, finiteNumber(radius, 0)),
        hardness = clamp(finiteNumber(hardness, 1), 0, 1), opacity = clamp(finiteNumber(opacity, 1), 0, 1),
        color = normalizeColor(color), mode = mode or 'paint', shape = normalizeShape(shape), deferSync = false,
    }
end

local function brushDistance(dx, dy, shape)
    if normalizeShape(shape) == 'square' then return math.max(math.abs(dx), math.abs(dy)) end
    return math.sqrt(dx * dx + dy * dy)
end

local function falloff(distance, radius, hardness)
    if radius < 0 or distance > radius then return 0 end
    if radius == 0 then return distance == 0 and 1 or 0 end
    local inner = radius * hardness
    if distance <= inner then return 1 end
    if inner >= radius then return 0 end
    return 1 - (distance - inner) / (radius - inner)
end

local function blendPaint(oldR, oldG, oldB, oldA, color, sourceAlpha)
    if sourceAlpha <= 0 then return oldR, oldG, oldB, oldA end
    local source = sourceAlpha / 255
    local destination = oldA / 255
    local outputAlpha = source + destination * (1 - source)
    if outputAlpha <= 0 then return 0, 0, 0, 0 end
    return byte((color.r * source + oldR * destination * (1 - source)) / outputAlpha),
        byte((color.g * source + oldG * destination * (1 - source)) / outputAlpha),
        byte((color.b * source + oldB * destination * (1 - source)) / outputAlpha),
        byte(outputAlpha * 255)
end

local function sampleStamp(stamp, x, y)
    x = clamp(x, 0, stamp.width - 1)
    y = clamp(y, 0, stamp.height - 1)
    local x0, y0 = math.floor(x), math.floor(y)
    local x1, y1 = math.min(stamp.width - 1, x0 + 1), math.min(stamp.height - 1, y0 + 1)
    local tx, ty = x - x0, y - y0
    local bytes, width = stamp.bytes, stamp.width
    local offset00 = (y0 * width + x0) * M.BYTES_PER_PIXEL + 1
    local offset10 = (y0 * width + x1) * M.BYTES_PER_PIXEL + 1
    local offset01 = (y1 * width + x0) * M.BYTES_PER_PIXEL + 1
    local offset11 = (y1 * width + x1) * M.BYTES_PER_PIXEL + 1
    local alpha00, alpha10 = bytes:byte(offset00 + 3), bytes:byte(offset10 + 3)
    local alpha01, alpha11 = bytes:byte(offset01 + 3), bytes:byte(offset11 + 3)
    local function interpolate(value00, value10, value01, value11)
        local top = value00 * (1 - tx) + value10 * tx
        local bottom = value01 * (1 - tx) + value11 * tx
        return top * (1 - ty) + bottom * ty
    end
    local alpha = interpolate(alpha00, alpha10, alpha01, alpha11)
    if alpha <= 0 then return 0, 0, 0, 0 end
    local function interpolateColor(channel)
        return interpolate(bytes:byte(offset00 + channel) * alpha00,
            bytes:byte(offset10 + channel) * alpha10,
            bytes:byte(offset01 + channel) * alpha01,
            bytes:byte(offset11 + channel) * alpha11) / alpha
    end
    return interpolateColor(0), interpolateColor(1), interpolateColor(2), alpha
end

local function blendErase(oldR, oldG, oldB, oldA, amount)
    local alpha = byte(oldA * (1 - amount))
    if alpha == 0 then return 0, 0, 0, 0 end
    return oldR, oldG, oldB, alpha
end

function M.applyDab(layer, x, radius, hardness, opacity, color, mode, shape)
    assertBuffer(layer, 'layer')
    assert(layer.tileSize, 'applyDab requires a layer created by newLayer')
    local dab = dabOptions(x, radius, hardness, opacity, color, mode, shape)
    assert(dab.mode == 'paint' or dab.mode == 'erase', 'dab.mode must be paint or erase')
    local left = math.max(0, math.floor(dab.x - dab.radius))
    local right = math.min(layer.width - 1, math.ceil(dab.x + dab.radius))
    local top = math.max(0, math.floor(dab.y - dab.radius))
    local bottom = math.min(layer.height - 1, math.ceil(dab.y + dab.radius))
    local changedPixels = 0
    if left > right or top > bottom then return {changedPixels = 0, dirtyTiles = {}} end
    local rowBytes = layer.width * M.BYTES_PER_PIXEL
    local rows = rowsOf(layer)
    local changedRows = {}
    for py = top, bottom do
        local line = rows[py + 1]
        local first = left * M.BYTES_PER_PIXEL + 1
        local last = (right + 1) * M.BYTES_PER_PIXEL
        changedRows[py] = stringToBytes(line, first, last)
    end
    for py = top, bottom do
        for px = left, right do
            local amount = dab.opacity * falloff(brushDistance(px - dab.x, py - dab.y, dab.shape),
                dab.radius, dab.hardness)
            if amount > 0 then
                local offset = (px - left) * M.BYTES_PER_PIXEL + 1
                local row = changedRows[py]
                local oldR, oldG, oldB, oldA = row[offset], row[offset + 1], row[offset + 2], row[offset + 3]
                local newR, newG, newB, newA
                if dab.mode == 'paint' then
                    newR, newG, newB, newA = blendPaint(oldR, oldG, oldB, oldA, dab.color, amount * dab.color.a)
                else
                    newR, newG, newB, newA = blendErase(oldR, oldG, oldB, oldA, amount)
                end
                if oldR ~= newR or oldG ~= newG or oldB ~= newB or oldA ~= newA then
                    row[offset], row[offset + 1], row[offset + 2], row[offset + 3] = newR, newG, newB, newA
                    markDirty(layer, px, py)
                    changedPixels = changedPixels + 1
                end
            end
        end
    end
    if changedPixels > 0 then
        local prefixBytes = left * M.BYTES_PER_PIXEL
        local suffixStart = (right + 1) * M.BYTES_PER_PIXEL + 1
        for py = top, bottom do
            local row = changedRows[py]
            local oldRow = rows[py + 1]
            rows[py + 1] = oldRow:sub(1, prefixBytes) .. bytesToString(row) .. oldRow:sub(suffixStart)
        end
        layer.touched = true
        if not dab.deferSync then M.syncBytes(layer) end
    end
    return {changedPixels = changedPixels, dirtyTiles = M.getDirtyTiles(layer)}
end

function M.tilesForPixels(buffer, samples)
    assertBuffer(buffer)
    assert(type(samples) == 'table', 'samples must be a table')
    local result, seen = {}, {}
    for _, sample in ipairs(samples) do
        if type(sample) == 'table' and type(sample.x) == 'number' and type(sample.y) == 'number' then
            local radius = math.max(0, finiteNumber(sample.radius, 0))
            for _, tile in ipairs(M.tilesForRect(buffer, sample.x - radius, sample.y - radius,
                    sample.x + radius, sample.y + radius)) do
                if not seen[tile.key] then
                    seen[tile.key] = true
                    result[#result + 1] = tile
                end
            end
        end
    end
    return result
end

function M.applyProjectedSamples(layer, samples, opacity, color, deferSync, shape, mode)
    assertBuffer(layer, 'layer')
    assert(layer.tileSize, 'applyProjectedSamples requires a layer created by newLayer')
    assert(type(samples) == 'table', 'samples must be a table')
    local normalizedColor = normalizeColor(color)
    opacity = clamp(finiteNumber(opacity, 1), 0, 1)
    shape = normalizeShape(shape)
    mode = mode or 'paint'
    assert(mode == 'paint' or mode == 'erase', 'projected sample mode must be paint or erase')
    local strongest = {}
    for _, sample in ipairs(samples) do
        if type(sample) == 'table' and type(sample.x) == 'number' and type(sample.y) == 'number' then
            local centerX, centerY = sample.x, sample.y
            local sampleRadius = math.max(0, finiteNumber(sample.radius, 0))
            local sampleShape = normalizeShape(sample.shape or shape)
            local left = math.max(0, math.floor(centerX - sampleRadius))
            local right = math.min(layer.width - 1, math.ceil(centerX + sampleRadius))
            local top = math.max(0, math.floor(centerY - sampleRadius))
            local bottom = math.min(layer.height - 1, math.ceil(centerY + sampleRadius))
            for y = top, bottom do
                for x = left, right do
                    local distance = brushDistance(x - centerX, y - centerY, sampleShape)
                    local coverage = sampleRadius > 0 and falloff(distance, sampleRadius, 1) or
                        (distance == 0 and 1 or 0)
                    local amount = clamp(finiteNumber(sample.amount, 0), 0, 1) * coverage
                    local key = x .. ':' .. y
                    if amount > (strongest[key] or 0) then strongest[key] = amount end
                end
            end
        end
    end
    local rows = rowsOf(layer)
    local changedPixels = 0
    for key, amount in pairs(strongest) do
        if amount > 0 then
            local separator = key:find(':', 1, true)
            local x = tonumber(key:sub(1, separator - 1))
            local y = tonumber(key:sub(separator + 1))
            local row = rows[y + 1]
            local offset = x * M.BYTES_PER_PIXEL + 1
            local oldR, oldG, oldB, oldA = row:byte(offset, offset + 3)
            local newR, newG, newB, newA
            if mode == 'erase' then
                newR, newG, newB, newA = blendErase(oldR, oldG, oldB, oldA, amount * opacity)
            else
                newR, newG, newB, newA = blendPaint(oldR, oldG, oldB, oldA,
                    normalizedColor, amount * opacity * normalizedColor.a)
            end
            if oldR ~= newR or oldG ~= newG or oldB ~= newB or oldA ~= newA then
                rows[y + 1] = row:sub(1, offset - 1) .. string.char(newR, newG, newB, newA) ..
                    row:sub(offset + 4)
                markDirty(layer, x, y)
                changedPixels = changedPixels + 1
            end
        end
    end
    if changedPixels > 0 then
        layer.touched = true
        if not deferSync then M.syncBytes(layer) end
    end
    return {changedPixels = changedPixels, dirtyTiles = M.getDirtyTiles(layer)}
end

function M.applyStamp(layer, stamp, x, y, radius, hardness, opacity, deferSync, flipX, flipY, shape,
        scale, rotation)
    assertBuffer(layer, 'layer')
    assert(type(stamp) == 'table' and type(stamp.bytes) == 'string', 'stamp must contain bytes')
    integer(stamp.width, 'stamp.width')
    integer(stamp.height, 'stamp.height')
    assert(#stamp.bytes == stamp.width * stamp.height * M.BYTES_PER_PIXEL,
        'stamp.bytes has the wrong size')
    radius = math.max(0, finiteNumber(radius, stamp.radius or 0))
    hardness = clamp(finiteNumber(hardness, 1), 0, 1)
    opacity = clamp(finiteNumber(opacity, 1), 0, 1)
    shape = normalizeShape(shape)
    scale = math.max(.001, finiteNumber(scale, 1))
    rotation = finiteNumber(rotation, 0)
    x, y = finiteNumber(x, 0), finiteNumber(y, 0)
    local maximumSourceDimension = math.max(1, stamp.width - 1, stamp.height - 1)
    local halfWidth = radius * scale * math.max(1, stamp.width - 1) / maximumSourceDimension
    local halfHeight = radius * scale * math.max(1, stamp.height - 1) / maximumSourceDimension
    local cosine, sine = math.cos(rotation), math.sin(rotation)
    local extentX = math.sqrt(halfWidth * halfWidth * cosine * cosine +
        halfHeight * halfHeight * sine * sine)
    local extentY = math.sqrt(halfWidth * halfWidth * sine * sine +
        halfHeight * halfHeight * cosine * cosine)
    local left = math.max(0, math.floor(x - extentX))
    local right = math.min(layer.width - 1, math.ceil(x + extentX))
    local top = math.max(0, math.floor(y - extentY))
    local bottom = math.min(layer.height - 1, math.ceil(y + extentY))
    if left > right or top > bottom then return {changedPixels = 0, dirtyTiles = {}} end
    local rows = rowsOf(layer)
    local changedRows = {}
    for py = top, bottom do
        local line = rows[py + 1]
        local first = left * M.BYTES_PER_PIXEL + 1
        local last = (right + 1) * M.BYTES_PER_PIXEL
        changedRows[py] = stringToBytes(line, first, last)
    end
    local changedPixels = 0
    for py = top, bottom do
        for px = left, right do
            local offsetX, offsetY = px - x, py - y
            local localX = offsetX * cosine + offsetY * sine
            local localY = -offsetX * sine + offsetY * cosine
            local normalizedX = halfWidth > 0 and localX / halfWidth or 0
            local normalizedY = halfHeight > 0 and localY / halfHeight or 0
            if math.abs(normalizedX) <= 1 and math.abs(normalizedY) <= 1 then
                local normalizedDistance = shape == 'square' and
                    math.max(math.abs(normalizedX), math.abs(normalizedY)) or
                    math.sqrt(normalizedX * normalizedX + normalizedY * normalizedY)
                local distance = normalizedDistance * radius
                local amount = opacity * falloff(distance, radius, hardness)
                if amount > 0 then
                    if flipX then normalizedX = -normalizedX end
                    if flipY then normalizedY = -normalizedY end
                    local sourceX = normalizedX * (stamp.width - 1) * .5 + (stamp.width - 1) * .5
                    local sourceY = normalizedY * (stamp.height - 1) * .5 + (stamp.height - 1) * .5
                    local sourceR, sourceG, sourceB, sourceA = sampleStamp(stamp, sourceX, sourceY)
                    local color = {r=sourceR, g=sourceG, b=sourceB, a=sourceA}
                    local offset = (px - left) * M.BYTES_PER_PIXEL + 1
                    local row = changedRows[py]
                    local oldR, oldG, oldB, oldA = row[offset], row[offset + 1],
                        row[offset + 2], row[offset + 3]
                    local newR, newG, newB, newA = blendPaint(oldR, oldG, oldB, oldA,
                        color, amount * color.a)
                    if oldR ~= newR or oldG ~= newG or oldB ~= newB or oldA ~= newA then
                        row[offset], row[offset + 1], row[offset + 2], row[offset + 3] =
                            newR, newG, newB, newA
                        markDirty(layer, px, py)
                        changedPixels = changedPixels + 1
                    end
                end
            end
        end
    end
    if changedPixels > 0 then
        local prefixBytes = left * M.BYTES_PER_PIXEL
        local suffixStart = (right + 1) * M.BYTES_PER_PIXEL + 1
        for py = top, bottom do
            local row = changedRows[py]
            local oldRow = rows[py + 1]
            rows[py + 1] = oldRow:sub(1, prefixBytes) .. bytesToString(row) .. oldRow:sub(suffixStart)
        end
        layer.touched = true
        if not deferSync then M.syncBytes(layer) end
    end
    return {changedPixels = changedPixels, dirtyTiles = M.getDirtyTiles(layer)}
end

function M.dilate(layer, radius, deferSync)
    assertBuffer(layer, 'layer')
    assert(layer.tileSize, 'dilate requires a layer created by newLayer')
    radius = math.max(0, math.floor(finiteNumber(radius, 0) + .5))
    if radius == 0 or not layer.touched then
        return {changedPixels = 0, dirtyTiles = {}}
    end

    local bounds = layer.alphaBounds or M.refreshAlphaBounds(layer)
    if not bounds then
        return {changedPixels = 0, dirtyTiles = {}}
    end

    local sourceRows = rowsOf(layer)
    local left = math.max(0, bounds.minX - radius)
    local right = math.min(layer.width - 1, bounds.maxX + radius)
    local top = math.max(0, bounds.minY - radius)
    local bottom = math.min(layer.height - 1, bounds.maxY + radius)
    local changedRows = {}
    for y = top, bottom do
        changedRows[y] = stringToBytes(sourceRows[y + 1],
            left * M.BYTES_PER_PIXEL + 1, (right + 1) * M.BYTES_PER_PIXEL)
    end

    local offsets = {}
    for dy = -radius, radius do
        for dx = -radius, radius do
            local distance = dx * dx + dy * dy
            if distance > 0 and distance <= radius * radius then
                offsets[#offsets + 1] = {dx = dx, dy = dy, distance = distance}
            end
        end
    end
    table.sort(offsets, function(leftOffset, rightOffset)
        return leftOffset.distance < rightOffset.distance
    end)

    local changedPixels = 0
    for y = top, bottom do
        local outputRow = changedRows[y]
        for x = left, right do
            local outputOffset = (x - left) * M.BYTES_PER_PIXEL + 1
            if outputRow[outputOffset + 3] == 0 then
                local sourcePixel
                for _, offset in ipairs(offsets) do
                    local sourceX, sourceY = x + offset.dx, y + offset.dy
                    if sourceX >= 0 and sourceX < layer.width and sourceY >= 0 and sourceY < layer.height then
                        local sourceLine = sourceRows[sourceY + 1]
                        local sourceOffset = sourceX * M.BYTES_PER_PIXEL + 1
                        if sourceLine:byte(sourceOffset + 3) > 0 then
                            sourcePixel = {
                                sourceLine:byte(sourceOffset), sourceLine:byte(sourceOffset + 1),
                                sourceLine:byte(sourceOffset + 2), sourceLine:byte(sourceOffset + 3),
                            }
                            break
                        end
                    end
                end
                if sourcePixel then
                    outputRow[outputOffset], outputRow[outputOffset + 1] = sourcePixel[1], sourcePixel[2]
                    outputRow[outputOffset + 2], outputRow[outputOffset + 3] = sourcePixel[3], sourcePixel[4]
                    markDirty(layer, x, y)
                    changedPixels = changedPixels + 1
                end
            end
        end
    end

    if changedPixels > 0 then
        local rows = rowsOf(layer)
        local prefixBytes = left * M.BYTES_PER_PIXEL
        local suffixStart = (right + 1) * M.BYTES_PER_PIXEL + 1
        for y = top, bottom do
            local oldRow = rows[y + 1]
            rows[y + 1] = oldRow:sub(1, prefixBytes) .. bytesToString(changedRows[y]) ..
                oldRow:sub(suffixStart)
        end
        layer.touched = true
        if not deferSync then M.syncBytes(layer) end
    end
    return {changedPixels = changedPixels, dirtyTiles = M.getDirtyTiles(layer)}
end

function M.paint(layer, dab, radius, hardness, opacity, color)
    if type(dab) == 'table' then
        local options = {}
        for key, value in pairs(dab) do options[key] = value end
        options.mode = 'paint'
        return M.applyDab(layer, options)
    end
    return M.applyDab(layer, dab, radius, hardness, opacity, color, 'paint')
end

function M.erase(layer, dab, radius, hardness, opacity)
    if type(dab) == 'table' then
        local options = {}
        for key, value in pairs(dab) do options[key] = value end
        options.mode = 'erase'
        return M.applyDab(layer, options)
    end
    return M.applyDab(layer, dab, radius, hardness, opacity, {r = 0, g = 0, b = 0, a = 255}, 'erase')
end

function M.compose(base, layer)
    assertBuffer(base, 'base')
    assertBuffer(layer, 'layer')
    assert(base.width == layer.width and base.height == layer.height, 'base and layer dimensions differ')
    if not layer.touched then return M.newBuffer(base.width, base.height, base.bytes) end
    local output = M.newBuffer(base.width, base.height, base.bytes)
    return M.composeInto(base, layer, output, {{
        key = 'all', tileX = 0, tileY = 0, x = 0, y = 0,
        width = base.width, height = base.height,
    }})
end

function M.composeInto(base, layer, output, tiles)
    assertBuffer(base, 'base')
    assertBuffer(layer, 'layer')
    assert(base.width == layer.width and base.height == layer.height, 'base and layer dimensions differ')
    output = output or M.newBuffer(base.width, base.height, base.bytes)
    assertBuffer(output, 'output')
    assert(output.width == base.width and output.height == base.height, 'output dimensions differ')
    tiles = tiles or M.tilesForRect(layer, 0, 0, layer.width - 1, layer.height - 1)
    for _, tile in ipairs(tileList(tiles)) do composeTile(base, layer, output, tile) end
    M.syncBytes(output)
    return output
end

M.composite = M.compose

return M
