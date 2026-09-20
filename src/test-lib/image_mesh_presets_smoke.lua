--[[---------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------]]

package.path='editor/?.lua;'..package.path
local Model=require 'image_mesh_model'
local Presets=require 'image_mesh_presets'
local IO=require 'image_mesh_io'
local api={}; assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local started,baseline
local function equal(a,b)
    if type(a)~=type(b) then return false end
    if type(a)~='table' then return a==b end
    for k,v in pairs(a) do if not equal(v,b[k]) then return false end end
    for k in pairs(b) do if a[k]==nil then return false end end
    return true
end
local function run()
    init()
    local pixels={}; for i=1,64*64*3 do pixels[i]=128 end
    local image='/tmp/ime_presets_source.png'
    assert(mbm.createTexture(pixels,64,64,3,'ime_presets_source',image))
    api.openImage(image)
    assert(api.action(function(p)
        Model.primitive(p,'rectangle',20,30)
        local r=Model.primitive(p,'triangle',30,20)
        r.heightEdits={{mode='raise',x=.5,y=.5,radius=.1,strength=.2,height=.5}}
        Model.primitive(p,'circle',15,15)
    end))
    api.select(1,false); api.select(2,true)
    local E=api.state
    local before=Model.copy(E.project)
    local settings=Model.copy(E.values)
    settings.depth=33; settings.relief=12; settings.twoLevels=true
    settings.simplify=true; settings.simplifyRatio=.8
    assert(api.action(function(p) Presets.store(p,'Grade',settings) end))
    assert(api.action(function(p)
        local updated=Model.copy(settings); updated.depth=34
        Presets.store(p,'Grade',updated,true)
    end))
    assert(E.project.presets[1].settings.depth==34,'update failed')
    api.undo(false); assert(E.project.presets[1].settings.depth==33,'update undo')
    api.select(1,false); api.select(2,true) -- Undo restores the primary selection only.
    settings.relief=99
    assert(E.project.presets[1].settings.relief==12,'aliased capture')
    assert(api.action(function(p) Presets.apply(p,1,E.selection,false) end))
    for i=1,2 do
        local r=E.project.regions[i]
        assert(Model.options(E.project,r).depth==33)
        assert(Model.options(E.project,r).simplifyRatio==.8)
        local expected=Model.copy(before.regions[i]); expected.overrides=Model.copy(r.overrides)
        assert(equal(r,expected),'preset changed shape, name, crop or painting')
    end
    assert(equal(E.project.regions[3],before.regions[3]),'unselected module changed')
    assert(equal(E.project.defaults,before.defaults),'defaults changed')
    local result=Model.copy(E.project)
    api.undo(false)
    assert(equal(E.project.regions,before.regions),'undo failed')
    api.undo(true)
    assert(equal(E.project,result),'redo failed')
    assert(api.action(function(p) Presets.rename(p,1,'Metal') end))
    assert(api.action(function(p) Presets.store(p,'Other',Model.defaults) end))
    local invalid=Model.copy(E.project)
    assert(not pcall(Presets.rename,invalid,2,'Metal'),'duplicate rename')
    assert(not pcall(Presets.store,invalid,'Metal',Model.defaults),'duplicate create')
    assert(not pcall(Presets.store,invalid,'  ',Model.defaults),'blank name')
    local bad=Model.copy(Model.defaults); bad.grooveThreshold=2
    assert(not pcall(Presets.store,invalid,'bad',bad),'invalid settings')
    assert(api.action(function(p) table.remove(p.presets,1) end))
    api.undo(false); assert(E.project.presets[1].name=='Metal','delete undo')
    api.saveProject('/tmp/ime_presets.imesh')
    local saved=IO.load('/tmp/ime_presets.imesh')
    assert(equal(saved,E.project),'project roundtrip')
    Presets.save(saved.presets[1],'/tmp/ime_metal.imeshpreset',tUtil.save)
    local imported=Presets.load('/tmp/ime_metal.imeshpreset')
    local file=assert(io.open('/tmp/ime_invalid.imeshpreset','wb'))
    file:write('return {version=99}'); file:close()
    assert(not pcall(Presets.load,'/tmp/ime_invalid.imeshpreset'),'invalid version accepted')
    local other=Model.new(image,64,64); Model.primitive(other,'circle',20,20)
    Presets.store(other,imported.name,imported.settings)
    Presets.apply(other,1,{[1]=true},false)
    assert(Model.options(other,other.regions[1]).depth==33,'cross-project import')
    assert(other.regions[1].shape=='ellipse','import changed shape')
    Presets.apply(other,1,{},true)
    assert(other.defaults.depth==33,'defaults application')
    other.presets=nil; IO.save(other,'/tmp/ime_presets_legacy.imesh',tUtil.save)
    assert(IO.load('/tmp/ime_presets_legacy.imesh').presets==nil,'legacy project failed')
    api.openProject('/tmp/ime_presets.imesh')
    assert(equal(E.project.presets,saved.presets),'reopen presets')
    api.select(1,false); E.presetUI={index=1,name='Metal'}
    print('PRESETS HISTORY / VALIDATION / MULTI-SELECTION / PERSISTENCE / IMPORT OK')
    started=mbm.getTimeRun()
end
function onInitScene()
    local ok,err=pcall(run)
    if not ok then print('PRESETS FAIL '..tostring(err)); mbm.quit() end
end
function onLoop(delta)
    if not started then return end
    local tree=tImGui.TreeNode
    tImGui.TreeNode=function(label,...)
        if label==tLang.L('ime_presets') then tImGui.SetNextItemOpen(true,0) end
        return tree(label,...)
    end
    loop(delta)
    tImGui.TreeNode=tree
    local E=api.state
    if mbm.getTimeRun()-started>1 and not baseline then
        baseline={names=E.presetUI.names,builds=E.builds,revision=E.revision}
    end
    if mbm.getTimeRun()-started>3 then
        assert(baseline.names==E.presetUI.names and baseline.builds==E.builds and baseline.revision==E.revision,'idle preset work')
        print('PRESETS EDITOR UI / IDLE OK'); mbm.quit()
    end
end
