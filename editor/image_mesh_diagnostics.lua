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

-- Camera and light inspection for image meshes. Reuses Mesh Debug's shared
-- orbit gizmo and direction conversions without depending on its scene globals.
local M={}
local Model=require 'image_mesh_model'
local function L(key) return tLang.L(key) end
local function lightState(target)
    local state=mbm.getLightState(target)
    state.orbit=tUtil.orbitFromDir(state.directionalDirection)
    return state
end
function M.init(E)
    E.lights={['3d']=lightState('3d')}
end
local function vector(label,v,prefix)
    tImGui.Text(label)
    local changed=false
    for _,axis in ipairs({'x','y','z'}) do
        local edited,value=tImGui.DragFloat(axis:upper()..'##'..prefix..axis,v[axis],1,0,0,'%.2f',0)
        if edited then v[axis]=value; changed=true end
    end
    return changed
end
local function cameraPanel(E,H)
    local c=E.orbit
    local mode=E.editMode and 0 or 1
    local selected=tImGui.RadioButton(L('ime_mode_edit')..'##ime_mode',mode,0)
    tImGui.SameLine(); selected=tImGui.RadioButton(L('ime_mode_view')..'##ime_mode',selected,1)
    if selected~=mode then H.setMode(selected==0) end
    if E.editMode then
        local camera=E.camera2d
        local rx,x=tImGui.DragFloat('X##ime_cam2',camera.x,1,0,0,'%.2f',0)
        local ry,y=tImGui.DragFloat('Y##ime_cam2',camera.y,1,0,0,'%.2f',0)
        if rx or ry then camera:setPos(x,y) end
        local changed,zoom=tImGui.InputFloat(L('ime_zoom'),E.zoom,0.1,1,'%.3f')
        if changed then H.zoom(E,Model.clampNumber(zoom,0.02,32,E.zoom)) end
        if tImGui.Button(L('reset_camera')) then H.fit(E) end
        tImGui.TextWrapped(L('ime_camera_pan_help'))
    else
        if tUtil.drawOrbitGizmo(c,{size=90}) then H.camera() end
        local changed,value=tImGui.InputFloat(L('cam_distance'),c.distance,10,100,'%.2f')
        if changed then c.distance=Model.clampNumber(value,0.01,math.huge,c.distance); H.camera() end
        if tImGui.CollapsingHeader(L('cam_position')..' / '..L('cam_focus')) then
            -- Orbit coordinates before the parallel viewport-centering offset.
            local p={x=c.fx+c.distance*math.cos(c.elevation)*math.sin(c.azimuth),
                y=c.fy+c.distance*math.sin(c.elevation),z=c.fz+c.distance*math.cos(c.elevation)*math.cos(c.azimuth)}
            if vector(L('ime_orbit_position'),p,'ime_pos') then
                local x,y,z=p.x-c.fx,p.y-c.fy,p.z-c.fz
                local distance=math.sqrt(x*x+y*y+z*z)
                if distance>0.001 then
                    c.distance=distance; c.azimuth=math.atan(x,z)
                    c.elevation=math.asin(math.max(-0.9999,math.min(0.9999,y/distance))); H.camera()
                end
            end
            local focus={x=c.fx,y=c.fy,z=c.fz}
            if vector(L('ime_orbit_focus'),focus,'ime_focus') then c.fx=focus.x; c.fy=focus.y; c.fz=focus.z; H.camera() end
        end
        if tImGui.Button(L('reset_camera')) then
            c.fx=0; c.fy=0; c.fz=0; c.azimuth=0.3; c.elevation=0.3
            c.distance=E.fitDistance or 300; H.camera()
        end
        tImGui.TextWrapped(L('ime_preview_help'))
    end
end
local function lightPanel(E)
    local target='3d'
    local state=E.lights[target]
    tImGui.Text(target)
    local enabled=tImGui.Checkbox(L('light_enabled'),state.enabled)
    if enabled~=state.enabled then state.enabled=enabled; mbm.setLightEnabled(target,enabled) end
    local flags=tImGui.Flags('ImGuiColorEditFlags_NoInputs')
    local changed,value=tImGui.ColorEdit4(L('ambient'),state.ambientColor,flags)
    if changed then state.ambientColor=value; mbm.setAmbientLight(target,value) end
    changed,value=tImGui.ColorEdit4(L('light_color'),state.directionalColor,flags)
    if changed then state.directionalColor=value; mbm.setDirectionalLightColor(target,value) end
    tImGui.Text(L('direction_label'))
    if tUtil.drawOrbitGizmo(state.orbit,{size=100}) then
        state.directionalDirection=tUtil.dirFromOrbit(state.orbit)
        mbm.setDirectionalLightDirection(target,state.directionalDirection)
    end
    if tImGui.CollapsingHeader(L('ime_light_vector')) then
        local d=state.directionalDirection
        if vector(L('direction_label'),d,'ime_direction') and d.x*d.x+d.y*d.y+d.z*d.z>0.000001 then
            mbm.setDirectionalLightDirection(target,d); state.orbit=tUtil.orbitFromDir(d)
        end
    end
    if tImGui.Button(L('reset_light')) then mbm.resetLight(target); E.lights[target]=lightState(target) end
    tImGui.TextWrapped(L('ime_light_scope'))
end
function M.draw(E,H)
    local width,height=mbm.getRealSizeScreen()
    local cameraHeight=math.min(370,math.floor((height-30)*0.47))
    tImGui.SetNextWindowPos({x=width-E.rightbar,y=25},E.flags.always)
    tImGui.SetNextWindowSize({x=E.rightbar,y=cameraHeight},E.flags.always)
    if tImGui.Begin(L('ime_view_panel')..'###ime_camera',false,E.flags.fixed) then
        tImGui.PushItemWidth(130); cameraPanel(E,H); tImGui.PopItemWidth()
    end
    tImGui.End()
    if E.editMode then return end
    tImGui.SetNextWindowPos({x=width-E.rightbar,y=25+cameraHeight},E.flags.always)
    tImGui.SetNextWindowSize({x=E.rightbar,y=height-25-cameraHeight},E.flags.always)
    if tImGui.Begin(L('light_panel')..'###ime_light',false,E.flags.fixed) then
        tImGui.PushItemWidth(130); lightPanel(E); tImGui.PopItemWidth()
    end
    tImGui.End()
end
return M
