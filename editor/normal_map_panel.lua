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

local M={}
function M.draw(ui,L,o,processedHeight)
    local dirty=false
    local function combo(key,values,labels)
        local index=1;for i,v in ipairs(values) do if o[key]==v then index=i end end
        ui.SetNextItemWidth(145)
        local changed,value=ui.Combo(L(key),index,labels or values)
        if changed then o[key]=values[value];dirty=true end
    end
    if not processedHeight then combo('channel',{'luminance','r','g','b','a'},{L('luminance'),'R','G','B',L('alpha')}) end
    local controls=processedHeight and {{'blur',0,32},{'strength',0,16}} or
        {{'black',0,.99},{'white',.01,1},{'curve',.1,8},{'blur',0,32},{'strength',0,16}}
    for _,spec in ipairs(controls) do
        local key=spec[1]
        ui.SetNextItemWidth(145)
        local changed,value=ui.SliderFloat(L(key),o[key],spec[2],spec[3],'%.3f')
        if changed then
            if key=='black' then value=math.min(value,o.white-.001) end
            if key=='white' then value=math.max(value,o.black+.001) end
            o[key]=value;dirty=true
        end
    end
    -- This engine's Checkbox returns only the checked state.
    if not processedHeight then
        local value=ui.Checkbox(L('invert'),o.invert)
        if value~=o.invert then o.invert=value;dirty=true end
    end
    combo('convention',{'+Y','-Y'})
    combo('edge',{'clamp','repeat'},{L('clamp'),L('repeat')})
    if not processedHeight then ui.TextWrapped(L('height_help')) end
    return dirty
end
return M
