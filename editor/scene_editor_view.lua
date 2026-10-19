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
| the Software.                                                                                                        |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Projection and geometry adapters for the shared world scene editor.
]]--

return function(world)
    local M = {labels = {}}
    local is2d = world == '2dw'
    if is2d then
        M.labels = {
            scene_editor_3d='scene_editor_2d', welcome_scene_editor_3d='welcome_scene_editor_2d',
            load_3d_scene='load_2d_scene', save_3d_scene='save_2d_scene', save_3d_scene_as='save_2d_scene_as',
            scene_3d_loaded_ok='scene_2d_loaded_ok', scene_3d_saved_ok='scene_2d_saved_ok',
            scene_3d_load_failed='scene_2d_load_failed',
            grid_depth_z_height_y='grid_height_y', grid_count_z_height_y='grid_count_y',
            mesh_preview_orbit='scene_camera_2d', camera_distance_format='scene_zoom_distance_format',
        }
    end

    -- Shared editor geometry is XYZ. Native 2D bindings require XY pairs.
    -- Conversion only happens on creation or an explicit dirty geometry update.
    local function xy(points)
        if not is2d then return points end
        local result = {}
        for i=1,#points,3 do result[#result+1],result[#result+2] = points[i],points[i+1] end
        return result
    end

    function M.newLine(x,y,z)
        local object = line:new(world,x or 0,y or 0,z or 0)
        if is2d then
            local add, set = object.add, object.set
            object.add = function(self,points,...) return add(self,xy(points),...) end
            object.set = function(self,points,...) return set(self,xy(points),...) end
        end
        return object
    end

    function M.newShape(x,y,z)
        local object = shape:new(world,x or 0,y or 0,z or 0)
        if is2d then
            local create = object.create
            object.create = function(self,points,uv,name,...)
                local vertices = xy(points)
                if name:find('sphere_unit',1,true) then
                    vertices = {}
                    for i=0,31 do
                        local a,b = i*math.pi/16,(i+1)*math.pi/16
                        local n=#vertices
                        vertices[n+1],vertices[n+2]=0,0
                        vertices[n+3],vertices[n+4]=math.cos(a),math.sin(a)
                        vertices[n+5],vertices[n+6]=math.cos(b),math.sin(b)
                    end
                elseif name:find('highlight',1,true) or name:find('cube_unit',1,true) then
                    vertices={-0.5,-0.5, -0.5,0.5, 0.5,0.5, -0.5,-0.5, 0.5,0.5, 0.5,-0.5}
                end
                return create(self,vertices,uv,name..'_2dw',...)
            end
        end
        return object
    end

    -- Touch callbacks arrive divided by camera scale. Use that space directly;
    -- engine projection caches are refreshed on the next frame after a zoom edit.
    function M.screenToWorld(x,y)
        local camera=mbm.getCamera('2d')
        local w,h=mbm.getSizeScreen()
        return x-w*0.5+camera.x, h*0.5-y+camera.y
    end

    function M.hit(object,x,y)
        if not is2d then return object:collide(x,y) end
        local wx,wy=M.screenToWorld(x,y)
        local w,h=object:getAABB(true)
        local cx,cy=object:getAABBCenter()
        return math.abs(wx-cx)<=w*0.5 and math.abs(wy-cy)<=h*0.5
    end

    function M.pickRay(x,y)
        if not is2d then return mbm.getPickRay(x,y) end
        local wx,wy=M.screenToWorld(x,y)
        return wx,wy,-1000000,0,0,1
    end

    function M.applyCamera(camera,c)
        local scale=800/c.distance
        if camera.sx ~= scale or camera.sy ~= scale or camera.x ~= c.fx or camera.y ~= c.fy then
            local w,h=mbm.getRealSizeScreen()
            camera:scaleToScreen(w/scale,h/scale,"xy")
            camera.sx,camera.sy=scale,scale
            camera:setPos(c.fx,c.fy)
        end
    end

    function M.drawCamera(c)
        if tImGui.Button(tLang.L('reset_camera'),{x=110,y=0}) then c.fx,c.fy,c.distance=0,0,800 end
        tUtil.pushResponsiveItemWidth(72)
        local cx,x=tImGui.InputFloat('X##camera2d',c.fx,1,10,'%.1f')
        local cy,y=tImGui.InputFloat('Y##camera2d',c.fy,1,10,'%.1f')
        local cz,zoom=tImGui.InputFloat(tLang.L('scene_zoom'),800/c.distance,0.1,1,'%.2f')
        if cx then c.fx=x end
        if cy then c.fy=y end
        if cz then c.distance=800/math.max(0.01,math.min(100,zoom)) end
        tImGui.PopItemWidth()
    end

    function M.drawGameCamera(options,c)
        local pos=options.fSceneCamPos
        local cx,x=tImGui.InputFloat('X##game_camera2d',pos.x,1,10,'%.1f')
        local cy,y=tImGui.InputFloat('Y##game_camera2d',pos.y,1,10,'%.1f')
        local cz,zoom=tImGui.InputFloat(tLang.L('scene_zoom')..'##game_camera2d',options.fSceneCamScale,0.1,1,'%.2f')
        if cx then pos.x=x end
        if cy then pos.y=y end
        if cz then options.fSceneCamScale=math.max(0.01,math.min(100,zoom)) end
        if tImGui.Button(tLang.L('set_initial_camera_pos'),{x=-1,y=0}) then
            options.fSceneCamPos={x=c.fx,y=c.fy,z=0}
            options.fSceneCamScale=800/c.distance
        end
    end

    function M.loader(code)
        if not is2d then return code end
        code=code:gsub("'3d'","'2dw'"):gsub("mbm.getCamera%('2dw'%)","mbm.getCamera('2d')")
        code=code:gsub('    cam:setPos%(self.fCamPos.x, self.fCamPos.y, self.fCamPos.z%)',
            '    local w,h = mbm.getRealSizeScreen()\n    local zoom = self.fCamScale or 1\n    cam:scaleToScreen(w/zoom,h/zoom,"xy")\n    cam:setPos(self.fCamPos.x, self.fCamPos.y)')
        code=code:gsub('    cam:setFocus%(self.fCamFocus.x, self.fCamFocus.y, self.fCamFocus.z%)','')
        return code
    end

    local revision, selection, highlighted, hoverKey = 0, nil, nil, nil
    function M.invalidate() revision=revision+1 end
    function M.selection(objects)
        if selection and selection.revision==revision and selection.objects==objects and selection.count==#objects then return selection end
        selection={revision=revision,objects=objects,count=#objects,indices={},names={},layers={}}
        for i,p in ipairs(objects) do
            if p.isSelected then
                selection.indices[#selection.indices+1]=i
                selection.names[p.fileName]=true
                selection.layers[p.layerIndex]=(selection.layers[p.layerIndex] or 0)+1
            end
        end
        return selection
    end
    function M.highlightCandidates(objects,hovered)
        local selected=M.selection(objects)
        if highlighted and highlighted.selection==selected and highlighted.hovered==hovered then return highlighted.items end
        if highlighted then
            for _,p in pairs(highlighted.items) do
                if p.tHighlightShape then p.tHighlightShape.visible=false end
            end
        end
        highlighted={selection=selected,hovered=hovered,items={}}
        for _,i in ipairs(selected.indices) do highlighted.items[i]=objects[i] end
        if hovered then highlighted.items[hovered]=objects[hovered] end
        return highlighted.items
    end
    function M.hoverChanged(x,y,c,layer,tab,objects)
        local w,h=mbm.getRealSizeScreen()
        local key=hoverKey
        if key and key.revision==revision and key.x==x and key.y==y and key.fx==c.fx and key.fy==c.fy and
            key.distance==c.distance and key.layer==layer and key.tab==tab and key.objects==objects and
            key.count==#objects and key.w==w and key.h==h then return false end
        hoverKey={revision=revision,x=x,y=y,fx=c.fx,fy=c.fy,distance=c.distance,layer=layer,tab=tab,
            objects=objects,count=#objects,w=w,h=h}
        return true
    end

    function M.driver()
        return [[
local tScene = require "YOUR_SCENE3D"
local move = {x=0,y=0}
function onInitScene() tScene:load() end
function onKeyDown(key)
    if key==mbm.getKeyCode('W') or key==mbm.getKeyCode('up') then move.y=1
    elseif key==mbm.getKeyCode('S') or key==mbm.getKeyCode('down') then move.y=-1
    elseif key==mbm.getKeyCode('A') or key==mbm.getKeyCode('left') then move.x=-1
    elseif key==mbm.getKeyCode('D') or key==mbm.getKeyCode('right') then move.x=1 end
end
function onKeyUp(key)
    if key==mbm.getKeyCode('W') or key==mbm.getKeyCode('S') or key==mbm.getKeyCode('up') or key==mbm.getKeyCode('down') then move.y=0 end
    if key==mbm.getKeyCode('A') or key==mbm.getKeyCode('D') or key==mbm.getKeyCode('left') or key==mbm.getKeyCode('right') then move.x=0 end
end
function onLoop(delta)
    if move.x~=0 or move.y~=0 then
        local cam=mbm.getCamera('2d')
        cam:setPos(cam.x+move.x*400*delta,cam.y+move.y*400*delta)
    end
end
]]
    end
    return M
end
