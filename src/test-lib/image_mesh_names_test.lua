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

package.path='editor/?.lua;'..package.path
local Model=require 'image_mesh_model'
local Presets=require 'image_mesh_presets'
local Areas=require 'image_mesh_areas'
local originalLocale=os.setlocale(nil,'ctype')
local originalImGui,originalLang=tImGui,tLang
local function rejected(fn,key)
    local ok,err=pcall(fn)
    assert(not ok and tostring(err):find(key,1,true),tostring(err))
end
local function check()
    local name='Área 1'
    local p=Model.new('source.png',64,64)
    local r=Model.add(p,'rectangle',0,0,64,64)
    local a={{x=.2,y=.2},{x=.8,y=.2},{x=.8,y=.8},{x=.2,y=.8},
        name=name,shape='rectangle',enabled=true,height=1,transition=.02}
    r.heightAreas={a}
    r.curvedNodes={}
    Model.curved.add(r,0,'target','point',8)
    r.curvedNodes[1].name=name
    Presets.store(p,name,p.defaults)
    assert(Model.validate(p))
    assert(p.presets[1].name==name)
    for byte=0,127 do
        if byte<32 or byte==127 then
            local invalid='Area'..string.char(byte)..'1'
            a.name=invalid
            rejected(function() Model.validate(p) end,'ime_areas_invalid_name')
            a.name=name
            r.curvedNodes[1].name=invalid
            rejected(function() Model.validate(p) end,'ime_curved_nodes_invalid')
            r.curvedNodes[1].name=name
            rejected(function() Presets.store(p,invalid,p.defaults) end,'ime_invalid_name')
            p.presets[1].name=invalid
            rejected(function() Model.validate(p) end,'ime_preset_invalid')
            p.presets[1].name=name
        end
    end
    a.name=string.rep('a',129)
    rejected(function() Model.validate(p) end,'ime_areas_invalid_name')
    a.name=name
    a[1].x=-.1
    rejected(function() Model.validate(p) end,'ime_areas_invalid')
    a[1].x=.2
    -- Exercise the actual rename widget: it must remove controls, not UTF-8 bytes.
    tLang={L=function(key) return key end}
    local noop=function() end
    tImGui={Separator=noop,Text=noop,TextWrapped=noop,SameLine=noop,
        Combo=function() return false end,InputFloat=function() return false end,
        SliderFloat=function() return false end,IsItemHovered=function() return false end,
        Checkbox=function(_,value) return value end,Button=function() return false end,
        InputText=function() return true,name..string.char(0,9,31,127) end}
    local E={draft=r,areaIndex=1,values={heightSource='mixed',relief=8}}
    Areas.panel(E,function() error('unexpected action') end,function() error('unexpected apply') end)
    assert(a.name==name,'rename corrupted UTF-8')
    assert(Model.validate(p))
end
local ok,err=xpcall(function()
    for _,locale in ipairs({'C','Portuguese'}) do
        local selected=os.setlocale(locale,'ctype')
        if selected then check();print('IMAGE MESH UTF8 NAMES OK: '..selected)
        else print('IMAGE MESH UTF8 NAMES locale unavailable: '..locale) end
    end
end,debug.traceback)
os.setlocale(originalLocale,'ctype')
tImGui,tLang=originalImGui,originalLang
assert(ok,err)
