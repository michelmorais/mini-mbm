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
local api={}; assert(loadfile('editor/mesh_maker_articulated.lua'))(api)
local init,loop=onInitScene,onLoop
local stage=0
local reads,writes=0,0
local get=mbm.getLightState
mbm.getLightState=function(...) reads=reads+1; return get(...) end
for _,name in ipairs({'setLightEnabled','setAmbientLight','setDirectionalLightColor','setDirectionalLightDirection','resetLight'}) do
    local fn=mbm[name]; mbm[name]=function(...) writes=writes+1; return fn(...) end
end
function onInitScene() init(); assert(api.state.lightState.enabled) end
function onLoop(delta)
    stage=stage+1
    local checkbox,color,gizmo,button=tImGui.Checkbox,tImGui.ColorEdit4,tUtil.drawOrbitGizmo,tImGui.Button
    tImGui.Checkbox=function(label,value,...)
        local result=checkbox(label,value,...)
        if stage==1 and label==tLang.L('light_enabled') then return false end
        return result
    end
    tImGui.ColorEdit4=function(label,value,...)
        local changed,out=color(label,value,...)
        if (stage==2 and label=='##ame_ambient') or (stage==3 and label=='##ame_directional') then
            return true,{r=0.3,g=0.4,b=0.5,a=1}
        end
        return changed,out
    end
    tUtil.drawOrbitGizmo=function(orbit,...)
        local changed=gizmo(orbit,...)
        if stage==4 and orbit==api.state.lightState.orbit then orbit.azimuth=0.8; orbit.elevation=0.2; return true end
        return changed
    end
    tImGui.Button=function(label,...)
        local clicked=button(label,...)
        return clicked or (stage==5 and label==tLang.L('reset_light'))
    end
    loop(delta)
    tImGui.Checkbox,tImGui.ColorEdit4,tUtil.drawOrbitGizmo,tImGui.Button=checkbox,color,gizmo,button
    local state=get('3d')
    if stage==1 then assert(not state.enabled)
    elseif stage==2 then assert(math.abs(state.ambientColor.r-0.3)<0.001)
    elseif stage==3 then assert(math.abs(state.directionalColor.b-0.5)<0.001)
    elseif stage==4 then
        local d=tUtil.dirFromOrbit(api.state.lightState.orbit)
        assert(math.abs(state.directionalDirection.x-d.x)<0.001)
    elseif stage==5 then
        assert(reads==2 and writes==6,'unexpected lighting updates')
        assert(api.state.lightState.enabled==state.enabled)
        local d=tUtil.dirFromOrbit(api.state.lightState.orbit)
        assert(math.abs(d.y-state.directionalDirection.y)<0.001,'reset gizmo mismatch')
    elseif stage==15 then
        assert(reads==2 and writes==6,'idle lighting work')
        print('ARTICULATED MESH LIGHT ENABLE / COLORS / GIZMO / RESET / IDLE OK'); mbm.quit()
    end
end
