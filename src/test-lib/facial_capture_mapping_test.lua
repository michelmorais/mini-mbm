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
local Mapping = require 'facial_capture_mapping'

assert(Mapping.normalizeName(' Eye_Blink.Left ') == 'eyeblinkleft')
assert(Mapping.normalizeName('') == nil)

local index = Mapping.buildTargetIndex({
    'jawOpen', 'eyeBlinkLeft', 'mouthSmileLeft', 'mouthSmileRight', 'cheekPuff'
}, {requiredTargets = {'jawOpen', 'eyeBlinkRight'}})
assert(#index.targets == 5)
assert(#index.missingTargets == 1 and index.missingTargets[1] == 'eyeBlinkRight')
assert(Mapping.resolveSourceName(index, 'jaw_open') == 'jawOpen')
assert(Mapping.resolveSourceName(index, 'EyeBlink_L') == 'eyeBlinkLeft')
assert(Mapping.resolveSourceName(index, 'eyeBlinkRight') == nil)

local mesh = {weights = {}, calls = 0}
function mesh:setFacialWeight(name, value)
    self.weights[name] = value
    self.calls = self.calls + 1
    return true
end
function mesh:resetFacialWeights()
    self.weights = {}
    return true
end

local result = Mapping.apply(mesh, index, {
    jaw_open = 1.4,
    eyeBlink_L = 0.25,
    eyeBlinkLeft = 0.5,
    mouthSmile_L = -0.3,
    mouthSmile_R = 0 / 0,
    unknown = 0.4,
})
assert(result.mapped == 3 and result.changed == 3 and result.clamped == 2)
assert(#result.unknownSource == 1 and result.unknownSource[1] == 'unknown')
assert(result.rejected == 1 and mesh.weights.jawOpen == 1 and mesh.weights.eyeBlinkLeft == .5)
assert(mesh.weights.mouthSmileLeft == 0)
assert(#result.duplicateSource == 1)

local before = mesh.calls
result = Mapping.apply(mesh, index, {jawOpen = .5, eyeBlink_L = .5, mouthSmile_L = 0}, {epsilon = .001})
assert(result.changed == 1 and result.unchanged == 2 and mesh.calls == before + 1)

result = Mapping.apply(mesh, index, {jawOpen = .2}, {clearMissing = true})
assert(result.changed == 3 and mesh.weights.jawOpen == .2 and mesh.weights.eyeBlinkLeft == 0 and mesh.weights.mouthSmileLeft == 0)
assert(Mapping.reset(mesh, index) and next(index.lastWeights) == nil)

local sourceMesh = {names = {'jawOpen', 'eyeBlinkLeft'}}
function sourceMesh:getTotalFacialTargets() return #self.names end
function sourceMesh:getFacialTargetName(position) return self.names[position] end
local built = Mapping.build(sourceMesh)
assert(#built.targets == 2 and Mapping.resolveSourceName(built, 'blink_l') == 'eyeBlinkLeft')
local faceitNamed = Mapping.buildTargetIndex({'eyeBlink_L'})
assert(Mapping.resolveSourceName(faceitNamed, 'eyeBlinkLeft') == 'eyeBlink_L')

print('FACIAL CAPTURE MAPPING PASS')
