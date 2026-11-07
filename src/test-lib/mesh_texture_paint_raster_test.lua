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

local source = debug.getinfo(1, 'S').source
local testDirectory = source:sub(1, 1) == '@' and source:sub(2):match('^(.*[/\\])') or ''
package.path = testDirectory .. '../../editor/?.lua;' .. package.path

local Raster = require 'mesh_texture_paint_raster'

local function pixel(buffer, x, y)
    local offset = (y * buffer.width + x) * 4 + 1
    return buffer.bytes:byte(offset, offset + 3)
end

local function assertPixel(buffer, x, y, r, g, b, a)
    local actualR, actualG, actualB, actualA = pixel(buffer, x, y)
    assert(actualR == r and actualG == g and actualB == b and actualA == a,
        string.format('pixel %d,%d was %d,%d,%d,%d, expected %d,%d,%d,%d',
            x, y, actualR, actualG, actualB, actualA, r, g, b, a))
end

local baseBytes = string.rep(string.char(20, 40, 60, 255), 25)
local base = Raster.newBuffer(5, 5, baseBytes)
local layer = Raster.newLayer(base, 2)
assert(layer.width == 5 and layer.height == 5 and #layer.bytes == 100)
assert(layer.bytes == string.rep('\0', 100), 'new layer must be transparent')
assert(base.bytes == baseBytes, 'layer creation changed the base')

local paintDab = {x = 2, y = 2, radius = 1, hardness = 1, opacity = 1, color = {255, 0, 0, 255}}
local tilesBefore = Raster.tilesForDab(layer, paintDab.x, paintDab.y, paintDab.radius)
local beforePaint = Raster.snapshotTiles(layer, tilesBefore)
local paintResult = Raster.paint(layer, paintDab)
assert(paintResult.changedPixels == 5, 'hard circular dab must affect the center and four neighbors')
assertPixel(layer, 2, 2, 255, 0, 0, 255)
assertPixel(layer, 1, 2, 255, 0, 0, 255)
assertPixel(layer, 0, 0, 0, 0, 0, 0)
assert(#Raster.getDirtyTiles(layer) == 3, 'dab must register every touched 2x2 tile')

local dirty = Raster.consumeDirtyTiles(layer)
assert(#dirty == 3 and #Raster.getDirtyTiles(layer) == 0, 'dirty tile consumption must clear the registry')
Raster.restoreTiles(layer, beforePaint)
assert(layer.bytes == string.rep('\0', 100), 'tile restoration must restore the exact snapshot')
assert(#Raster.getDirtyTiles(layer) == 4, 'restoration must mark restored tiles dirty')
Raster.clearDirtyTiles(layer)

local soft = Raster.applyDab(layer, {x = 2, y = 2, radius = 2, hardness = 0, opacity = 1, color = {0, 255, 0, 255}, mode = 'paint'})
assert(soft.changedPixels > 5, 'soft dab must cover more pixels than a hard unit dab')
local centerR, centerG, centerB, centerA = pixel(layer, 2, 2)
assert(centerR == 0 and centerG == 255 and centerB == 0 and centerA == 255, 'soft dab center is incorrect')
local edgeR, edgeG, edgeB, edgeA = pixel(layer, 1, 2)
assert(edgeA > 0 and edgeA < 255 and edgeG == 255 and edgeR == 0 and edgeB == 0, 'soft dab falloff is incorrect')

local composed = Raster.compose(base, layer)
assertPixel(base, 2, 2, 20, 40, 60, 255)
local composedR, composedG, composedB, composedA = pixel(composed, 2, 2)
assert(composedA == 255 and composedR == 0 and composedG == 255 and composedB == 0,
    'opaque layer paint must replace the base in the composed result')
local sampled = Raster.sample(composed, 2.4, 2.4)
assert(sampled.r == 0 and sampled.g == 255 and sampled.b == 0 and sampled.a == 255,
    'sample must return the nearest composed pixel')

local stampSource = Raster.newBuffer(3, 3, string.rep(string.char(0, 0, 255, 255), 9))
local stamp = Raster.extractStamp(stampSource, 1, 1, 1)
local stampLayer = Raster.newLayer(5, 5, 2)
local stampResult = Raster.applyStamp(stampLayer, stamp, 3, 3, 1, 1, 1)
assert(stampResult.changedPixels == 5, 'stamp must affect the center and four neighbors')
assert(Raster.sample(stamp, 0, 0).a == 0, 'stamp corners must be transparent outside the brush circle')
assertPixel(stampLayer, 3, 3, 0, 0, 255, 255)

local directionalSource = Raster.newBuffer(3, 3,
    string.rep(string.char(255, 0, 0, 255) .. string.char(0, 255, 0, 255) ..
        string.char(0, 0, 255, 255), 3))
local directionalStamp = Raster.extractStamp(directionalSource, 1, 1, 1)
local flippedLayer = Raster.newLayer(5, 5, 2)
Raster.applyStamp(flippedLayer, directionalStamp, 3, 3, 1, 1, 1, false, true, false)
assertPixel(flippedLayer, 2, 3, 0, 0, 255, 255)
assertPixel(flippedLayer, 4, 3, 255, 0, 0, 255)

local interpolatedLayer = Raster.newLayer(7, 7, 2)
Raster.applyStamp(interpolatedLayer, directionalStamp, 3, 3, 2, 1, 1)
local interpolatedR, interpolatedG = pixel(interpolatedLayer, 2, 3)
assert(interpolatedR > 0 and interpolatedG > 0 and interpolatedR < 255 and interpolatedG < 255,
    'stamp application must interpolate between source pixels')

local eraseResult = Raster.erase(layer, {x = 2, y = 2, radius = 0, hardness = 1, opacity = 1})
assert(eraseResult.changedPixels == 1, 'erase must change the selected pixel')
assertPixel(layer, 2, 2, 0, 0, 0, 0)
local erasedComposition = Raster.compose(base, layer)
assertPixel(erasedComposition, 2, 2, 20, 40, 60, 255)
assert(base.bytes == baseBytes, 'composition and erase must not mutate the base')

local partial = Raster.newLayer(2, 1, 2)
Raster.paint(partial, {x = 0, y = 0, radius = 0, hardness = 1, opacity = 0.5, color = {200, 100, 50, 255}})
local partialR, partialG, partialB, partialA = pixel(partial, 0, 0)
assert(partialR == 200 and partialG == 100 and partialB == 50 and partialA == 128, 'opacity must be represented in layer alpha')
local partialSnapshot = Raster.snapshotTiles(partial, Raster.tilesForDab(partial, 0, 0, 0))
Raster.erase(partial, {x = 0, y = 0, radius = 0, hardness = 1, opacity = 0.5})
local afterEraseR, afterEraseG, afterEraseB, afterEraseA = pixel(partial, 0, 0)
assert(afterEraseR == 200 and afterEraseG == 100 and afterEraseB == 50 and afterEraseA == 64, 'partial erase must reduce only layer alpha')
Raster.restoreTiles(partial, partialSnapshot)
assertPixel(partial, 0, 0, 200, 100, 50, 128)

local projected = Raster.newLayer(8, 8, 2)
local projectedSamples = {
    {x=2, y=2, amount=1},
    {x=2, y=2, amount=.25},
    {x=4, y=4, amount=.5},
}
local projectedResult = Raster.applyProjectedSamples(projected, projectedSamples, 1,
    {r=255, g=0, b=255, a=255}, false)
assert(projectedResult.changedPixels == 2, 'projected samples must merge duplicate destination pixels')
assertPixel(projected, 2, 2, 255, 0, 255, 255)
assertPixel(projected, 4, 4, 255, 0, 255, 128)
assert(#Raster.tilesForPixels(projected, projectedSamples) == 2,
    'projected samples must report unique dirty tiles')

local paddingSource = Raster.newLayer(5, 5, 2)
Raster.paint(paddingSource, {x = 2, y = 2, radius = 0, hardness = 1, opacity = 1,
    color = {40, 80, 120, 255}})
local padded = Raster.cloneLayer(paddingSource)
local paddingResult = Raster.dilate(padded, 1)
assert(paddingResult.changedPixels == 4, 'one-pixel dilation must fill the four adjacent texels')
assertPixel(padded, 2, 2, 40, 80, 120, 255)
assertPixel(padded, 1, 2, 40, 80, 120, 255)
assertPixel(padded, 3, 2, 40, 80, 120, 255)
assertPixel(padded, 2, 1, 40, 80, 120, 255)
assertPixel(padded, 2, 3, 40, 80, 120, 255)
assertPixel(paddingSource, 1, 2, 0, 0, 0, 0, 'dilation must not mutate the source layer')

print('mesh texture paint raster tests passed')
