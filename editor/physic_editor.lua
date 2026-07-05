--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2020      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

   Physic Editor

   This is a script based on mbm engine.

   Physic Editor meant to edit physic for sprite and mesh to be used in the engine.

   More info at: https://mbm-documentation.readthedocs.io/en/latest/editors.html#font-maker

]]--

tImGui        =     require "ImGui"
tUtil         =     require "editor_utils"
require "box2d"


function onInitScene()
    camera2d		     = mbm.getCamera("2d")
    camera2d:setPos(-150,0)
    camera3d             = mbm.getCamera("3d")
    tCam3d               = {azimuth=0, elevation=0.4, distance=600, fx=0, fy=0, fz=0}
    lastMouse3d          = {x=0, y=0}
    tLineCenterX         = line:new("2dw",0,0,50)
    tLineCenterY         = line:new("2dw",0,0,50)
    bEnableMoveWorld     = true
    tColorBackGround     = {r=0,g=0,b=0}
    bShowOrigin          = false
    bUseSolidColorBackGround = true
    tMesh                = nil
    tInfoPhysics         = nil
    bShowEditPhysics     = false
    bIs3d                = false
    setupPhysics         = nil
    scale                = 1
    tSimulate            = {}
    tMouseJoint          = nil --point to mouse joint when created
    tMeshJoint           = nil --point to mesh that we have selected (clicked)
    tJoint =
    {
        name                = 'mouse',
        target              = {x=0,y=0}, -- updated in the moment that we click on object and keep clicked
        maxForce            = 0,         -- updated according to mass
        frequencyHz         = 30.0,
        dampingRatio        = 0.7,
        collideConnected    = false,
    }
    tLineCenterX:add({-9999999,0, 9999999,0})
    tLineCenterY:add({0,-9999999, 0,9999999})
    tLineCenterX:setColor(1,0,0)
    tLineCenterY:setColor(0,1,0)
    tLineCenterX.visible = bShowOrigin
    tLineCenterY.visible = bShowOrigin
    vec_a = vec2:new()
    vec_b = vec2:new()
    vector_aux = vec2:new()
    tHighLightPoint = nil

    tUtil.sMessageOverlay= 'Welcome to Physic Editor!\n\nFirst add an existent Mesh / sprite!'
    tUtil.bRightSide = true
    local sTextureFileName = tUtil.createAlphaPattern(1024,768,32,{r=240,g=240,b=240},{r=125,g=125,b=125})
    if sTextureFileName ~= nil then
        local iW, iH      = mbm.getSizeScreen()
        tex_alpha_pattern  = texture:new('2dw')
        tex_alpha_pattern:load(sTextureFileName)
        tex_alpha_pattern:setSize(iW, iH)
        tex_alpha_pattern.z       = 99
        tex_alpha_pattern.visible = false
     else
        print('Could not create the alpha pattern!')
     end

     sLastEditorFileName = ''

     tWindowsTitle        = {title_edit_physics = "title_edit_physics"}
     tPhysicsOptions      = {
        iIndexPrimitiveType = 1,
        iPrimitivesRectangle  = 2,
        iPrimitivesCircle  = 5
        }
end



function onSaveMesh()
    if tInfoPhysics then
        if #tInfoPhysics > 0 then
            local tMeshDebug = meshDebug:new()
            if tMeshDebug:load(sLastEditorFileName) then
                tMeshDebug:setPhysics(tInfoPhysics)
                if tMeshDebug:save(sLastEditorFileName) then
                    tUtil.showMessage(tLang.L("physics_saved_ok"))
                else
                    tUtil.showMessageWarn(string.format(tLang.L("failed_to_save_mesh_fmt"), sLastEditorFileName))
                end
            else
                tUtil.showMessageWarn(string.format(tLang.L("failed_to_load_edit_fmt"), sLastEditorFileName))
            end
        else
            tUtil.showMessageWarn(tLang.L("no_physic_for_mesh"))
        end
    else
        tUtil.showMessageWarn(tLang.L("no_physics_load_first"))
    end
end

function updatePhysics(tPhysic)
    setupPhysics(tPhysic)
    tInfoPhysics:updateCircles()
end

function cam3dGetPos(c)
    local x = c.fx + c.distance * math.cos(c.elevation) * math.sin(c.azimuth)
    local y = c.fy + c.distance * math.sin(c.elevation)
    local z = c.fz + c.distance * math.cos(c.elevation) * math.cos(c.azimuth)
    return x,y,z
end

function applyCam3d(c)
    local x,y,z = cam3dGetPos(c)
    camera3d:setPos(x,y,z)
    camera3d:setFocus(c.fx,c.fy,c.fz)
end

function onRender(tShape, vertex, uv, index_read_only)
    if tShape.vertex == nil then
        tShape.vertex = vertex
    end
    vertex = tShape.vertex
    return vertex,uv
end


function setupPhysics2d(tInfoPhysic)
    local tShape = nil
    local tLine = nil
    if tInfoPhysic.tLine == nil then
        tLine             = line:new('2dw')
        tInfoPhysic.tLine = tLine
        tLine:add({0,0,0,0})
    else
        tLine = tInfoPhysic.tLine
    end

    if tInfoPhysic.destroy == nil then
        tInfoPhysic.destroy = function(self)
            self.tShape:destroy()
            self.tLine:destroy()

        end
    end

    if tInfoPhysic.type == 'cube' then
        tInfoPhysic.type_info = 'rectangle'
        local x,y,w,h = tInfoPhysic.x,tInfoPhysic.y,tInfoPhysic.width * 0.5, tInfoPhysic.height * 0.5
        local tPoints = {   (x - w) * scale, (y - h) * scale,
                            (x - w) * scale, (y + h) * scale,
                            (x + w) * scale, (y + h) * scale,
                            (x + w) * scale, (y - h) * scale,
                            (x - w) * scale, (y - h) * scale}

        tInfoPhysic.tPoints   = tPoints
                            
        
        if tInfoPhysic.tShape == nil then
            tShape               = shape:new('2dw')
            local sUniqueName    = tostring(os.time()) .. tostring(os.clock()) .. tostring(math.random())
            local dynamic_mode   = true
            local total_triangle = 2
            tInfoPhysic.tShape   = tShape
            tShape:create('RECTANGLE', tInfoPhysic.width, tInfoPhysic.height , total_triangle, dynamic_mode, sUniqueName)
            tShape:onRender(onRender)
            tShape:setColor(0,1,0,0.1)

            tInfoPhysic.isOver = function (self,x,y)
                local x,y = mbm.to2dw(x,y)
                if x < self.tPoints[1] then return false end
                if x > self.tPoints[5] then return false end
                if y < self.tPoints[2] then return false end
                if y > self.tPoints[4] then return false end
                return true
            end

            tInfoPhysic.moveFrame = function(self,x_diff,y_diff,x,y)
                local tPoints = self.tPoints
                local vertex  = self.tShape.vertex
                for i=1, #vertex do
                    vertex[i].x = vertex[i].x + x_diff
                    vertex[i].y = vertex[i].y - y_diff
                end
                tPoints[1] = vertex[1].x
                tPoints[2] = vertex[1].y
                tPoints[3] = vertex[2].x
                tPoints[4] = vertex[2].y
                tPoints[7] = vertex[3].x
                tPoints[8] = vertex[3].y
                tPoints[5] = vertex[4].x
                tPoints[6] = vertex[4].y
                tPoints[9] = tPoints[1]
                tPoints[10] = tPoints[2]

                tLine:set(tPoints,1)
                local width  = (vertex[3].x - vertex[1].x)
                local height = (vertex[2].y - vertex[1].y)
                self.width  = width  / scale
                self.height = height / scale

            end
    
            tInfoPhysic.editByCirclePosition = function(self,x,y,index_circle,tCircles)
                local x,y = mbm.to2dw(x,y)
                local vertex  = self.tShape.vertex
                local tPoints = self.tPoints
--[[
            2 vertex         4 vertex
    2 point +---------------+ 3 point
            |\              |
            |  \      2T    | 
            |    \          | 
            |      \        | 
            |        \      | 
            |   1T     \    | 
            |            \  | 
            |              \| 
   1 point +---------------+ 4 point
            1  vertex        3 vertex

            index = {1,2,3, 3,2,4} (apply to vertex only)
]]--
                local diff_x = 0
                local diff_y = 0
                if index_circle == 1 or index_circle == 2 then
                    diff_x = x - vertex[1].x
                else
                    diff_x = vertex[3].x - x
                end
                local x_left  = vertex[1].x + diff_x
                local x_right = vertex[3].x - diff_x
                if x_left < x_right then
                    vertex[1].x = vertex[1].x + diff_x
                    vertex[2].x = vertex[2].x + diff_x
                    
                    vertex[3].x = vertex[3].x - diff_x
                    vertex[4].x = vertex[4].x - diff_x

                    self.width = (vertex[3].x - vertex[1].x) / scale
                end
                if index_circle == 1 or index_circle == 4 then
                    diff_y  = y - vertex[1].y
                else
                    diff_y  = vertex[2].y - y
                end

                local y_down  = vertex[1].y + diff_y
                local y_up    = vertex[2].y - diff_y
                if y_down < y_up then
                    vertex[1].y = vertex[1].y + diff_y
                    vertex[3].y = vertex[3].y + diff_y
                    
                    vertex[2].y = vertex[2].y - diff_y
                    vertex[4].y = vertex[4].y - diff_y
                    
                    self.height = (vertex[2].y - vertex[1].y) / scale
                end

                tCircles[1]:setPos(vertex[1].x,vertex[1].y)
                tCircles[2]:setPos(vertex[2].x,vertex[2].y)
                tCircles[3]:setPos(vertex[4].x,vertex[4].y)
                tCircles[4]:setPos(vertex[3].x,vertex[3].y)

                tPoints[1] = vertex[1].x
                tPoints[2] = vertex[1].y
                tPoints[3] = vertex[2].x
                tPoints[4] = vertex[2].y
                tPoints[7] = vertex[3].x
                tPoints[8] = vertex[3].y
                tPoints[5] = vertex[4].x
                tPoints[6] = vertex[4].y
                tPoints[9] = tPoints[1]
                tPoints[10] = tPoints[2]

                tLine:set(tPoints,1)
            end
        else
            tShape              = tInfoPhysic.tShape
            local vertex        = tShape.vertex
            vertex[1].x         = tPoints[1]
            vertex[1].y         = tPoints[2]
            vertex[2].x         = tPoints[3]
            vertex[2].y         = tPoints[4]
            vertex[3].x         = tPoints[7]
            vertex[3].y         = tPoints[8]
            vertex[4].x         = tPoints[5]
            vertex[4].y         = tPoints[6]
        end

        tLine:set(tPoints,1)
        
    elseif tInfoPhysic.type == 'sphere' then
        tInfoPhysic.type_info = 'circle'

        local tPoints = {}
        for i=1, 361, 90 do
            local x = ((math.sin(math.rad(i)) * tInfoPhysic.ray) * scale) + (tInfoPhysic.x * scale) ;
            local y = ((math.cos(math.rad(i)) * tInfoPhysic.ray) * scale) + (tInfoPhysic.y * scale) ;
            table.insert(tPoints,x)
            table.insert(tPoints,y)
        end

        tInfoPhysic.tPoints   = tPoints

        if tInfoPhysic.tShape == nil then

            tInfoPhysic.getCircleLine = function(self)
                local tPointsLine = {}
                for i=1, 361 do
                    
                    local x = ((math.sin(math.rad(i)) * self.ray) * scale) + (self.x * scale) ;
                    local y = ((math.cos(math.rad(i)) * self.ray) * scale) + (self.y * scale) ;

                    table.insert(tPointsLine,x)
                    table.insert(tPointsLine,y)
                end
                return tPointsLine
            end

            tShape               = shape:new('2dw')
            local sUniqueName    = tostring(os.time()) .. tostring(os.clock()) .. tostring(math.random())
            local dynamic_mode   = true
            local total_triangle = 360
            tInfoPhysic.tShape   = tShape
            tShape:create('CIRCLE', tInfoPhysic.ray * 2.0, tInfoPhysic.ray * 2.0 , total_triangle, dynamic_mode, sUniqueName)
            tShape:onRender(onRender)
            tShape:setColor(0,1,0,0.1)

            tInfoPhysic.isOver = function (self,x,y)
                local x,y = mbm.to2dw(x,y)
                return (x - self.x * scale)^2 + (y - self.y * scale)^2 < (self.ray * scale) ^2
            end

            tInfoPhysic.moveFrame = function(self,x_diff,y_diff,x,y)
                local tPoints = self.tPoints
                local vertex  = self.tShape.vertex
                local index_line = 3
                tPoints[1] = tPoints[1] + x_diff
                tPoints[2] = tPoints[2] + y_diff
                for i=1, #vertex do
                    vertex[i].x = vertex[i].x + x_diff
                    vertex[i].y = vertex[i].y - y_diff
                    tPoints[index_line] = vertex[i].x
                    tPoints[index_line+1] = vertex[i].y
                    index_line = index_line + 2
                end
                if not self.isMovingFrame then
                    self.x_diff = x_diff
                    self.y_diff = y_diff
                    self.isMovingFrame = true
                end
            end

            tInfoPhysic.finishMoveFrame = function(self,x,y)
                if self.isMovingFrame then
                    --local xx,yy = mbm.to2dw(self.xMoving + self.x_diff,self.yMoving - self.y_diff)
                    local x,y  = mbm.to2dw(x,y)
                    -- TODO
                    --self.x = x - ((xx - self.x) * scale)
                    --self.y = y - ((yy - self.y) * scale)

                    self.x = x
                    self.y = y
                end
                self.isMovingFrame = false
            end
    
            tInfoPhysic.editByCirclePosition = function(self,x,y,index_circle,tCircles)
                local x,y = mbm.to2dw(x,y)
                vector_aux:set(x / scale,y / scale)
                vector_aux:sub(self.x / scale,self.y / scale)
                self.ray = vector_aux.len
                local tPointsLine = self:getCircleLine()
                self.tLine:set(tPointsLine,1)

                local j = 1
                local tPoints = {}
                for i=1, 360, 90 do
                    local x = ((math.sin(math.rad(i)) * self.ray) * scale) + self.x ;
                    local y = ((math.cos(math.rad(i)) * self.ray) * scale) + self.y ;

                    tCircles[j]:setPos(x,y)
                    j = j +1
                end
            end                    
        else
            local vertex        = tInfoPhysic.tShape.vertex
            vertex[1].x         = tInfoPhysic.x
            vertex[1].y         = tInfoPhysic.y
            local tPointsLine   = tInfoPhysic:getCircleLine()

            local j = 1
            for i=1, #vertex do
                local x = tPointsLine[j]
                local y = tPointsLine[j+1]
                vertex[i].x = x
                vertex[i].y = y
                j = j + 2
            end
        end

        local tPointsLine = tInfoPhysic:getCircleLine()
        tLine:set(tPointsLine,1)
        
    elseif tInfoPhysic.type == 'triangle' then

        tInfoPhysic.type_info = 'triangle'
        local tPoints = {   tInfoPhysic.a.x * scale, tInfoPhysic.a.y * scale,
                            tInfoPhysic.b.x * scale, tInfoPhysic.b.y * scale,
                            tInfoPhysic.c.x * scale, tInfoPhysic.c.y * scale,
                            tInfoPhysic.a.x * scale, tInfoPhysic.a.y * scale}

        tInfoPhysic.tPoints   = tPoints
                            
        
        if tInfoPhysic.tShape == nil then
            tShape               = shape:new('2dw')
            local sUniqueName    = tostring(os.time()) .. tostring(os.clock()) .. tostring(math.random())
            local dynamic_mode   = true
            tInfoPhysic.tShape   = tShape
            local tmpVertex = {tInfoPhysic.a.x,tInfoPhysic.a.y,tInfoPhysic.b.x,tInfoPhysic.b.y,tInfoPhysic.c.x,tInfoPhysic.c.y}
            tShape:create('TRIANGLE', tmpVertex , dynamic_mode, sUniqueName)
            tShape:onRender(onRender)
            tShape:setColor(0,1,0,0.1)
            tInfoPhysic.isOver = function (self,x,y)
                local px,py = mbm.to2dw(x,y)

                local function calTriangleArea(v1, v2, v3)
                    local det = ((v1.x - v3.x) * (v2.y - v3.y)) - ((v2.x - v3.x) * (v1.y - v3.y));
                    return (det * 0.5);
                end

                local  p1 = {x = self.tPoints[1],y = self.tPoints[2]}
                local  p2 = {x = self.tPoints[3],y = self.tPoints[4]}
                local  p3 = {x = self.tPoints[5],y = self.tPoints[6]}
                local area = calTriangleArea(p1,p2,p3)

                local s = 1/(2*area)*(p1.y*p3.x - p1.x*p3.y + (p3.y - p1.y)*px + (p1.x - p3.x)*py);
                local t = 1/(2*area)*(p1.x*p2.y - p1.y*p2.x + (p1.y - p2.y)*px + (p2.x - p1.x)*py);
                
                local a = (s > 0.0 and t > 0.0)
                local b = (1.0 - s - t) > 0.0
                return a and b

            end

            tInfoPhysic.moveFrame = function(self,x_diff,y_diff,x,y)
                local tPoints = self.tPoints
                local vertex  = self.tShape.vertex
                for i=1, #vertex do
                    vertex[i].x = vertex[i].x + x_diff
                    vertex[i].y = vertex[i].y - y_diff
                end
                tPoints[1] = vertex[1].x
                tPoints[2] = vertex[1].y
                tPoints[3] = vertex[2].x
                tPoints[4] = vertex[2].y
                tPoints[5] = vertex[3].x
                tPoints[6] = vertex[3].y
                tPoints[7] = vertex[1].x
                tPoints[8] = vertex[1].y

                tLine:set(tPoints,1)

                self.a.x = vertex[1].x / scale
                self.a.y = vertex[1].y / scale
                self.b.x = vertex[2].x / scale
                self.b.y = vertex[2].y / scale
                self.c.x = vertex[3].x / scale
                self.c.y = vertex[3].y / scale
            end
    
            tInfoPhysic.editByCirclePosition = function(self,x,y,index_circle,tCircles)
                local x,y = mbm.to2dw(x,y)
                
                local vertex  = self.tShape.vertex
                local tPoints = self.tPoints
                vertex[index_circle].x = x
                vertex[index_circle].y = y
                
                tPoints[1] = vertex[1].x
                tPoints[2] = vertex[1].y
                tPoints[3] = vertex[2].x
                tPoints[4] = vertex[2].y
                tPoints[5] = vertex[3].x
                tPoints[6] = vertex[3].y
                tPoints[7] = vertex[1].x
                tPoints[8] = vertex[1].y

                tLine:set(tPoints,1)

                tCircles[index_circle]:setPos(x,y)

                self.a.x = vertex[1].x / scale
                self.a.y = vertex[1].y / scale
                self.b.x = vertex[2].x / scale
                self.b.y = vertex[2].y / scale
                self.c.x = vertex[3].x / scale
                self.c.y = vertex[3].y / scale
        
            end                    
        else
            local vertex        = tInfoPhysic.tShape.vertex
            vertex[1].x         = tPoints[1]
            vertex[1].y         = tPoints[2]
            vertex[2].x         = tPoints[3]
            vertex[2].y         = tPoints[4]
            vertex[3].x         = tPoints[5]
            vertex[3].y         = tPoints[6]
        end

        tLine:set(tPoints,1)
    elseif tInfoPhysic.type == 'complex' then
        tInfoPhysic.type_info = 'complex'
        -- box2d (2D) only ever reads the front quad a,b,c,d; keep e..h mirrored to it
        -- so the data stays well-formed if this same physic is later used on a 3D mesh.
        local tPoints = {   tInfoPhysic.a.x * scale, tInfoPhysic.a.y * scale,
                            tInfoPhysic.b.x * scale, tInfoPhysic.b.y * scale,
                            tInfoPhysic.c.x * scale, tInfoPhysic.c.y * scale,
                            tInfoPhysic.d.x * scale, tInfoPhysic.d.y * scale,
                            tInfoPhysic.a.x * scale, tInfoPhysic.a.y * scale}

        tInfoPhysic.tPoints = tPoints

        if tInfoPhysic.tShape == nil then
            tShape               = shape:new('2dw')
            local sUniqueName    = tostring(os.time()) .. tostring(os.clock()) .. tostring(math.random())
            tInfoPhysic.tShape   = tShape
            local vertsXY = {  tInfoPhysic.a.x,tInfoPhysic.a.y, tInfoPhysic.b.x,tInfoPhysic.b.y,
                                tInfoPhysic.c.x,tInfoPhysic.c.y, tInfoPhysic.d.x,tInfoPhysic.d.y}
            local idx     = {1,2,3, 1,3,4}
            tShape:createDynamicIndexed(vertsXY, idx, nil, sUniqueName)
            tShape:onRender(onRender)
            tShape:setColor(0,1,0,0.1)

            tInfoPhysic.isOver = function(self,x,y)
                local x,y = mbm.to2dw(x,y)
                local minX = math.min(self.a.x,self.b.x,self.c.x,self.d.x) * scale
                local maxX = math.max(self.a.x,self.b.x,self.c.x,self.d.x) * scale
                local minY = math.min(self.a.y,self.b.y,self.c.y,self.d.y) * scale
                local maxY = math.max(self.a.y,self.b.y,self.c.y,self.d.y) * scale
                return x >= minX and x <= maxX and y >= minY and y <= maxY
            end

            tInfoPhysic.moveFrame = function(self,x_diff,y_diff,x,y)
                local vertex = self.tShape.vertex
                for i=1,4 do
                    vertex[i].x = vertex[i].x + x_diff
                    vertex[i].y = vertex[i].y - y_diff
                end
                self.a.x, self.a.y = vertex[1].x / scale, vertex[1].y / scale
                self.b.x, self.b.y = vertex[2].x / scale, vertex[2].y / scale
                self.c.x, self.c.y = vertex[3].x / scale, vertex[3].y / scale
                self.d.x, self.d.y = vertex[4].x / scale, vertex[4].y / scale
                self.e.x, self.e.y = self.a.x, self.a.y
                self.f.x, self.f.y = self.b.x, self.b.y
                self.g.x, self.g.y = self.c.x, self.c.y
                self.h.x, self.h.y = self.d.x, self.d.y

                local tPoints = {  vertex[1].x,vertex[1].y, vertex[2].x,vertex[2].y,
                                    vertex[3].x,vertex[3].y, vertex[4].x,vertex[4].y,
                                    vertex[1].x,vertex[1].y}
                self.tPoints = tPoints
                tLine:set(tPoints,1)
            end

            tInfoPhysic.editByCirclePosition = function(self,x,y,index_circle,tCircles)
                local x,y = mbm.to2dw(x,y)
                local vertex = self.tShape.vertex
                vertex[index_circle].x = x
                vertex[index_circle].y = y

                local tPoints = {  vertex[1].x,vertex[1].y, vertex[2].x,vertex[2].y,
                                    vertex[3].x,vertex[3].y, vertex[4].x,vertex[4].y,
                                    vertex[1].x,vertex[1].y}
                self.tPoints = tPoints
                tLine:set(tPoints,1)
                tCircles[index_circle]:setPos(x,y)

                self.a.x, self.a.y = vertex[1].x / scale, vertex[1].y / scale
                self.b.x, self.b.y = vertex[2].x / scale, vertex[2].y / scale
                self.c.x, self.c.y = vertex[3].x / scale, vertex[3].y / scale
                self.d.x, self.d.y = vertex[4].x / scale, vertex[4].y / scale
                self.e.x, self.e.y = self.a.x, self.a.y
                self.f.x, self.f.y = self.b.x, self.b.y
                self.g.x, self.g.y = self.c.x, self.c.y
                self.h.x, self.h.y = self.d.x, self.d.y
            end
        else
            local vertex = tInfoPhysic.tShape.vertex
            vertex[1].x, vertex[1].y = tInfoPhysic.a.x * scale, tInfoPhysic.a.y * scale
            vertex[2].x, vertex[2].y = tInfoPhysic.b.x * scale, tInfoPhysic.b.y * scale
            vertex[3].x, vertex[3].y = tInfoPhysic.c.x * scale, tInfoPhysic.c.y * scale
            vertex[4].x, vertex[4].y = tInfoPhysic.d.x * scale, tInfoPhysic.d.y * scale
        end

        tLine:set(tPoints,1)
    end

    if tInfoPhysic.finishMoveFrame == nil then
        tInfoPhysic.finishMoveFrame = function(self,x,y) end
    end
end

local BOX_LETTERS = {'a','b','c','d','e','f','g','h'}

-- 8 corners of an axis-aligned box, using the engine's own CUBE_COMPLEX convention
-- (front face a,b,c,d @ +halfDepth, back face e,f,g,h @ -halfDepth), see include/core_mbm/shapes.h
function boxCorners(hw,hh,hd)
    return {
        {x=-hw,y=-hh,z= hd}, {x=-hw,y= hh,z= hd}, {x= hw,y= hh,z= hd}, {x= hw,y=-hh,z= hd},
        {x=-hw,y=-hh,z=-hd}, {x=-hw,y= hh,z=-hd}, {x= hw,y= hh,z=-hd}, {x= hw,y=-hh,z=-hd},
    }
end

-- 12 triangles (2 per face) covering a box defined by its 8 corners (a..h order)
function boxTriangleFaces(corners)
    local a,b,c,d,e,f,g,h = corners[1],corners[2],corners[3],corners[4],corners[5],corners[6],corners[7],corners[8]
    return {
        {a,b,c},{a,c,d}, -- front
        {h,g,f},{h,f,e}, -- back
        {e,f,b},{e,b,a}, -- left
        {d,c,g},{d,g,h}, -- right
        {b,f,g},{b,g,c}, -- top
        {e,a,d},{e,d,h}, -- bottom
    }
end

-- Builds an invisible 3D box shape (used both as translucent highlight fill and as the
-- native :collide(x,y) mouse hit-test target) sized to the given center/extents.
function makeBoxShape3d(cx,cy,cz,w,h,d,name)
    local hw = math.max(w,0.01) * 0.5
    local hh = math.max(h,0.01) * 0.5
    local hd = math.max(d,0.01) * 0.5
    local corners = boxCorners(hw,hh,hd)
    local faces   = boxTriangleFaces(corners)
    local verts   = {}
    for _,tri in ipairs(faces) do
        for _,p in ipairs(tri) do
            table.insert(verts,p.x); table.insert(verts,p.y); table.insert(verts,p.z)
        end
    end
    local tShape = shape:new('3d',cx,cy,cz)
    tShape:create(verts, nil, name)
    return tShape
end

function setupPhysics3d(tInfoPhysic)
    local sUniqueName = tostring(os.time()) .. tostring(os.clock()) .. tostring(math.random())

    if tInfoPhysic.destroy == nil then
        tInfoPhysic.destroy = function(self)
            if self.tShape then self.tShape:destroy() end
            if self.tLine then self.tLine:destroy() end
        end
    end

    tInfoPhysic.isOver = function(self,x,y)
        return self.tShape ~= nil and self.tShape:collide(x,y) or false
    end

    if tInfoPhysic.tShape then
        tInfoPhysic.tShape:destroy()
        tInfoPhysic.tShape = nil
    end

    if tInfoPhysic.type == 'cube' then
        tInfoPhysic.type_info = 'cube'
        tInfoPhysic.tShape = makeBoxShape3d(tInfoPhysic.x,tInfoPhysic.y,tInfoPhysic.z,
                                             tInfoPhysic.width,tInfoPhysic.height,tInfoPhysic.depth or tInfoPhysic.width,
                                             sUniqueName)
    elseif tInfoPhysic.type == 'sphere' then
        tInfoPhysic.type_info = 'sphere'
        local diameter = tInfoPhysic.ray * 2.0
        tInfoPhysic.tShape = makeBoxShape3d(tInfoPhysic.x,tInfoPhysic.y,tInfoPhysic.z, diameter,diameter,diameter, sUniqueName)
    elseif tInfoPhysic.type == 'triangle' then
        tInfoPhysic.type_info = 'triangle'
        local a,b,c = tInfoPhysic.a, tInfoPhysic.b, tInfoPhysic.c
        local minX = math.min(a.x,b.x,c.x); local maxX = math.max(a.x,b.x,c.x)
        local minY = math.min(a.y,b.y,c.y); local maxY = math.max(a.y,b.y,c.y)
        local minZ = math.min(a.z or 0,b.z or 0,c.z or 0); local maxZ = math.max(a.z or 0,b.z or 0,c.z or 0)
        local cx,cy,cz = (minX+maxX)*0.5, (minY+maxY)*0.5, (minZ+maxZ)*0.5
        tInfoPhysic.tShape = makeBoxShape3d(cx,cy,cz, maxX-minX, maxY-minY, maxZ-minZ, sUniqueName)
    elseif tInfoPhysic.type == 'complex' then
        tInfoPhysic.type_info = 'complex'
        local minX,maxX,minY,maxY,minZ,maxZ =  math.huge,-math.huge,math.huge,-math.huge,math.huge,-math.huge
        for _,l in ipairs(BOX_LETTERS) do
            local p = tInfoPhysic[l]
            minX = math.min(minX,p.x); maxX = math.max(maxX,p.x)
            minY = math.min(minY,p.y); maxY = math.max(maxY,p.y)
            minZ = math.min(minZ,p.z); maxZ = math.max(maxZ,p.z)
        end
        local cx,cy,cz = (minX+maxX)*0.5, (minY+maxY)*0.5, (minZ+maxZ)*0.5
        tInfoPhysic.tShape = makeBoxShape3d(cx,cy,cz, maxX-minX, maxY-minY, maxZ-minZ, sUniqueName)
    end

    if tInfoPhysic.tShape then
        tInfoPhysic.tShape:setColor(0,1,0,0.15)
        tInfoPhysic.tShape.visible = false

        if tInfoPhysic.tLine == nil then
            tInfoPhysic.tLine = line:new('3d')
        end
        tInfoPhysic.tLine:drawBounding(tInfoPhysic.tShape,false)
        tInfoPhysic.tLine:setColor(0,1,0)
    end

    if tInfoPhysic.finishMoveFrame == nil then
        tInfoPhysic.finishMoveFrame = function(self,x,y) end
    end
end

function onLoadMesh()
    local file_name = mbm.openFile(sLastEditorFileName,'*.spt','*.msh')
    if file_name then
        if tMesh then
            tMesh:destroy()
            tMesh = nil
            tInfoPhysics = nil
        end
        if file_name:lower():match('%.spt$') then
            setupPhysics = setupPhysics2d
            tMesh = sprite:new('2dw')
            tHighLightPoint = shape:new('2dw')
            tHighLightPoint:create('circle',25,25,18,false,string.format('tHighLightPoint_%d',os.time()))
            tHighLightPoint.z = -100
            bIs3d = false
        elseif file_name:lower():match('%.msh$') then
            tMesh = mesh:new('3d')
            setupPhysics = setupPhysics3d
            tHighLightPoint = shape:new('3d')
            tHighLightPoint:create('circle',25,25,18,false,string.format('tHighLightPoint_%d',os.time()))
            bIs3d = true
            local w,h,d = tMesh:getSize()
            tCam3d.fx, tCam3d.fy, tCam3d.fz = 0,0,0
            tCam3d.distance = math.max(w or 300, h or 300, (d or 0)) * 2.5
            tCam3d.azimuth, tCam3d.elevation = 0, 0.4
        end
        tHighLightPoint:setColor(0,1,0)
        tHighLightPoint.visible = false

        if tMesh:load(file_name) then
            bShowEditPhysics = true
            sLastEditorFileName = file_name
            tInfoPhysics = tMesh:getPhysics()
            camera2d:setPos(-150,0)
            if bIs3d then
                applyCam3d(tCam3d)
            end

            for i=1, #tInfoPhysics do
                tInfoPhysics[i].selectable = true
            end

            do

                tInfoPhysics.highLightPoint = function(self,index,value)
                    if index == nil then --disable high light
                        tHighLightPoint.visible = false
                    else
                        tHighLightPoint.visible = true
                        local tInfoPhysic = self[index]
                        if bIs3d then
                            if tInfoPhysic.type_info == 'cube' or tInfoPhysic.type_info == 'sphere' then
                                tHighLightPoint:setPos(tInfoPhysic.x,tInfoPhysic.y,tInfoPhysic.z)
                            elseif tInfoPhysic.type_info == 'triangle' then
                                if value == 0 then
                                    tHighLightPoint:setPos(tInfoPhysic.x,tInfoPhysic.y,tInfoPhysic.z or 0)
                                elseif value == 1 then
                                    tHighLightPoint:setPos(tInfoPhysic.a.x,tInfoPhysic.a.y,tInfoPhysic.a.z or 0)
                                elseif value == 2 then
                                    tHighLightPoint:setPos(tInfoPhysic.b.x,tInfoPhysic.b.y,tInfoPhysic.b.z or 0)
                                elseif value == 3 then
                                    tHighLightPoint:setPos(tInfoPhysic.c.x,tInfoPhysic.c.y,tInfoPhysic.c.z or 0)
                                end
                            elseif tInfoPhysic.type_info == 'complex' then
                                local l = BOX_LETTERS[value or 0]
                                if l then
                                    local p = tInfoPhysic[l]
                                    tHighLightPoint:setPos(p.x,p.y,p.z)
                                end
                            end
                        else
                            if tInfoPhysic.type_info == 'rectangle' then
                                tHighLightPoint:setPos(tInfoPhysic.x * scale,tInfoPhysic.y * scale)
                            elseif tInfoPhysic.type_info == 'triangle' then
                                if value == 0 then
                                    tHighLightPoint:setPos(tInfoPhysic.x * scale,tInfoPhysic.y * scale)
                                elseif value == 1 then
                                    tHighLightPoint:setPos(tInfoPhysic.a.x * scale,tInfoPhysic.a.y * scale)
                                elseif value == 2 then
                                    tHighLightPoint:setPos(tInfoPhysic.b.x * scale,tInfoPhysic.b.y * scale)
                                elseif value == 3 then
                                    tHighLightPoint:setPos(tInfoPhysic.c.x * scale,tInfoPhysic.c.y * scale)
                                end
                            elseif tInfoPhysic.type_info == 'complex' then
                                local l = BOX_LETTERS[value or 0]
                                if l then
                                    local p = tInfoPhysic[l]
                                    tHighLightPoint:setPos(p.x * scale,p.y * scale)
                                end
                            end
                        end
                    end
                end

                tInfoPhysics.indexIsOver = function(self,x,y) -- 0 to none
                    for i=1, #self do
                        local tInfoPhysic = self[i]
                        if tInfoPhysic.selectable and tInfoPhysic:isOver(x,y) then
                            return i
                        end
                    end
                    return 0
                end

                tInfoPhysics.setTouchDown = function(self,x,y)
                    self.x_touch = x
                    self.y_touch = y
                end


                tInfoPhysics.setHighLight = function(self,index) -- 0 to none
                    
                    for j=1, #self do
                        local tShape = self[j].tShape
                        tShape.visible = j == index
                        tShape:setColor(1,0,1,0.1)
                    end

                    if self.iIndexSelected ~= 0 then
                        local tShape = self[self.iIndexSelected].tShape
                        tShape.visible = true
                        tShape:setColor(0,1,0,0.1)
                    end
                end

                tInfoPhysics.updateCircles = function(self)
                    for i=1, #self.tCircles do
                        self.tCircles[i].visible = false
                    end
                    if bIs3d then
                        return -- no corner-drag handles in 3D; shapes are edited via numeric fields
                    end
                    if self.iIndexSelected ~= 0 then
                        local tSelectedPoints = self[self.iIndexSelected].tPoints
                        for j=1, #self do
                            if self[j].bSelected then
                                local tPoints      = self[j].tPoints
                                local iTotalPoints = (#tPoints / 2) - 1 --the last one is to close the geometry
                                for i=1, iTotalPoints do
                                    local iindex  = (i -1) * 2 + 1
                                    local tCircle = self.tCircles[i]
                                    if tCircle == nil then
                                        tCircle          = shape:new('2dw')
                                        self.tCircles[i] = tCircle
                                        tCircle:create('circle',20,20,18,false,string.format('circle_%d%d',i,os.time()))
                                        tCircle:setColor(1,0,0)
                                    end
                                    local x,y = tSelectedPoints[iindex], tSelectedPoints[iindex+1]
                                    tCircle:setPos(x ,y )
                                    tCircle.visible = true
                                end
                                break
                            end
                        end
                    end
                end
                
                tInfoPhysics.setSelected = function(self,index) -- 0 to none
                    for j=1, #self do
                        local tShape = self[j].tShape
                        tShape.visible    = j == index
                        self[j].bSelected = j == index
                    end
                    
                    self.iIndexSelected = 0
                    if index ~= 0 and index <= #self then
                        self.iIndexSelected  = index
                    end

                    self:updateCircles()
                end

                tInfoPhysics.getIndexSelectedFrame = function(self) -- 0 to none selected
                    for j=1, #self do
                        if self[j].bSelected then
                            return j
                        end
                    end
                    return 0
                end

                tInfoPhysics.setScale = function(self,x,y,z)
                    for j=1, #self do
                        setupPhysics(self[j])
                    end
                    local index = self:getIndexSelectedFrame()
                    if index ~= 0 then
                        self:setSelected(index)
                    end
                    self:updateCircles()
                end

                tInfoPhysics.isOverCircle = function (self,x,y)
                    for j=1, #self.tCircles do
                        local tCircle = self.tCircles[j]
                        if tCircle.visible and tCircle:isOver(x,y) then
                            return j
                        end
                    end
                    return 0
                end

                tInfoPhysics.setSelectedCircle = function(self,i_circle)
                    for i=1, #self.tCircles do
                        local tCircle = self.tCircles[i]
                        tCircle.iSelected = false
                        tCircle:setColor(1,0,0)
                    end
                    if i_circle > 0 and i_circle <= #self.tCircles then
                        local tCircle = self.tCircles[i_circle]
                        tCircle.iSelected = true
                        tCircle:setColor(1,1,0)
                    end
                end

                tInfoPhysics.getSelectedCircle = function(self)
                    for i=1, #self.tCircles do
                        if self.tCircles[i].iSelected then
                            return i
                        end
                    end
                    return 0
                end

                tInfoPhysics.unSelectCircles = function(self)
                    for i=1, #self.tCircles do
                        local tCircle = self.tCircles[i]
                        tCircle.iSelected = false
                        tCircle:setColor(1,0,0)
                    end
                end

                tInfoPhysics.unSelectFrame = function(self)
                    self:setSelected(0)
                    self:unSelectCircles()
                end

                tInfoPhysics.editByCircleSelected = function(self,x,y)
                    if self.iIndexSelected > 0 and self.iIndexSelected <= #self then
                        for i=1, #self.tCircles do
                            local tCircle = self.tCircles[i]
                            if tCircle.iSelected then
                                self[self.iIndexSelected]:editByCirclePosition(x,y,i,self.tCircles)
                                break
                            end
                        end
                    end
                end

                tInfoPhysics.moveFrameSelected = function(self,x,y)
                    local x_diff = x - self.x_touch
                    local y_diff = y - self.y_touch
                    if self.iIndexSelected ~= 0 then
                        self[self.iIndexSelected]:moveFrame(x_diff,y_diff,x,y)
                        self.x_touch = x
                        self.y_touch = y
                    end
                end

                tInfoPhysics.finishMovingFrame = function(self,x,y)
                    if self.iIndexSelected ~= 0 then
                        self[self.iIndexSelected]:finishMoveFrame(x,y)
                    end
                end

                tInfoPhysics.tCircles = {}
                tInfoPhysics.iIndexSelected = 0
                tInfoPhysics.x_touch = 0
                tInfoPhysics.y_touch = 0

                for i=1, #tInfoPhysics do
                    setupPhysics(tInfoPhysics[i])
                end
            end
            tUtil.showMessage(tLang.L("mesh_loaded_ok"))
        else
            bShowEditPhysics = false
            tUtil.showMessageWarn(string.format(tLang.L("failed_to_load_file_fmt"), file_name))
        end
    end
end

function main_menu_physic_editor()
    if (tImGui.BeginMainMenuBar()) then
        if tImGui.BeginMenu(tLang.L("menu_file")) then
            
            local pressed,checked = tImGui.MenuItem(tLang.L("load_mesh"), "Ctrl+O", false)
            if pressed then
                onLoadMesh()
            end
            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem(tLang.L("save_mesh"), "Ctrl+S", false)
            if pressed then
                onSaveMesh()
            end

            tImGui.Separator()

            local pressed,checked = tImGui.MenuItem(tLang.L("add_shader"), nil, false)
            if pressed then
                local file_name = mbm.openFile(nil,"lua")
                if file_name then
                    if mbm.include(file_name) then
                        tUtil.showMessage(tLang.L("file_added_ok"))
                    else
                        tUtil.showMessageWarn(string.format(tLang.L("failed_to_add_file_fmt"), file_name))
                    end
                end
            end

            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem(tLang.L("menu_quit"), "Alt+F4", false)
            if pressed then
                mbm.quit()
            end
            tImGui.EndMenu();
        end
        
        if tImGui.BeginMenu(tLang.L("menu_physics")) then
            local pressed,checked = tImGui.MenuItem(tLang.L("show_physics_options"), 'Ctrl+P', false)
            if pressed then
                bShowEditPhysics = true
            end

            if tImGui.BeginMenu(tLang.L("simulate")) then
                local xCam = 100000
                if tInfoPhysics and #tInfoPhysics > 0 then
                    local pressed,checked = tImGui.MenuItem(tLang.L("add_mesh"), nil, false)
                    if pressed and sLastEditorFileName then
                        camera2d:setPos(xCam,0)

                        if #tSimulate == 0 then
                            local iW, iH              = mbm.getSizeScreen()
                            local gravity_x           = 0
                            local gravity_y           = -90.8
                            local scale_box_2d        = 10
                            local velocityIterations  = 10
                            local positionIterations  = 3
                            local multiplyStep        = 1
                           
                            tPhysicSimulate = box2d:new(gravity_x,gravity_y,scale_box_2d,velocityIterations,positionIterations,multiplyStep)
                            local x,y       = mbm.to2dw(0,iH)
                            local tShape    = shape:new('2dw',xCam,y)
                            tShape:create('rectangle',xCam-iW,200)
                            tPhysicSimulate:addStaticBody(tShape)
                            table.insert(tSimulate,tShape)

                            camera2d.mx,camera2d.my = xCam,0
                        end

                        local tMeshDebug = meshDebug:new()
                        if tMeshDebug:load(sLastEditorFileName) then
                            tMeshDebug:setPhysics(tInfoPhysics)
                            local tmpname = os.tmpname () .. '.xxx'
                            if tMeshDebug:save(tmpname) then
                                local x,y = mbm.to2dw(0,0)
                                local spt = sprite:new('2dw',xCam,y)
                                spt:load(tmpname)
                                local current_item     = select(2,tMesh:getAnim())
                                spt:setAnim(current_item)
                                table.insert(tSimulate,spt)
                                tPhysicSimulate:addDynamicBody(spt)
                            else
                                tUtil.showMessageWarn(string.format(tLang.L("failed_to_save_mesh_fmt"), tmpname))
                            end
                        else
                            tUtil.showMessageWarn(string.format(tLang.L("failed_to_load_edit_fmt"), sLastEditorFileName))
                        end
                    end
                    if #tSimulate > 0 then
                        local yCam       = select(2,mbm.to2dw(0,0))
                        tImGui.Separator()
                        if select(1,tImGui.MenuItem(tLang.L("add_rectangle"), nil, false)) then
                            local tShape = shape:new('2dw',xCam,yCam)
                            tShape:create('Rectangle',100,100)
                            tShape:setColor(1,1,1,0.3)
                            table.insert(tSimulate,tShape)
                            tPhysicSimulate:addDynamicBody(tShape)
                        end
                        if select(1,tImGui.MenuItem(tLang.L("add_circle"), nil, false)) then
                            local tShape = shape:new('2dw',xCam,yCam)
                            tShape:create('Circle',100,100)
                            tShape:setColor(1,1,0,0.3)
                            table.insert(tSimulate,tShape)
                            tPhysicSimulate:addDynamicBody(tShape)
                        end
                        if select(1,tImGui.MenuItem(tLang.L("add_triangle"), nil, false)) then
                            local tShape = shape:new('2dw',xCam,yCam)
                            tShape:create('Triangle',100,100)
                            tShape:setColor(0,1,1,0.3)
                            table.insert(tSimulate,tShape)
                            tPhysicSimulate:addDynamicBody(tShape)
                        end
                        tImGui.Separator()
                        if select(1,tImGui.MenuItem(tLang.L("stop_simulation"), nil, false)) then
                            camera2d:setPos(0,0)
                            for i=1, #tSimulate do
                                tPhysicSimulate:destroyBody(tSimulate[i])
                                tSimulate[i]:destroy()
                            end
                            tPhysicSimulate     = nil
                            tSimulate   = {}
                            tMouseJoint = nil
                            tMeshJoint  = nil
                        end
                    end
                else
                    tImGui.MenuItem(tLang.L("no_physics_available"), nil, false)
                end
                tImGui.EndMenu()
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu(tLang.L("menu_options")) then
            local pressed,checked = tImGui.MenuItem(tLang.L("show_alpha_pattern"), true, tex_alpha_pattern.visible)
            if pressed then
                tex_alpha_pattern.visible = checked
                bUseSolidColorBackGround = not checked
            end

            bShowOrigin = tImGui.Checkbox(tLang.L("show_origin"), bShowOrigin)
            tLineCenterX.visible = bShowOrigin
            tLineCenterY.visible = bShowOrigin
            bUseSolidColorBackGround = tImGui.Checkbox(tLang.L("background_solid_color"), bUseSolidColorBackGround)

            if bUseSolidColorBackGround then
                tex_alpha_pattern.visible = false
                local label    = 'Background'
                local flags    = 0
                local clicked, tRgb = tImGui.ColorEdit3(label, tUtil.tColorBackground, flags)
                if clicked then
                    tUtil.tColorBackground = tRgb
                    mbm.setColor(tUtil.tColorBackground.r,tUtil.tColorBackground.g,tUtil.tColorBackground.b)
                end
            else
                tex_alpha_pattern.visible = true
            end

            tLang.renderLanguageSubmenu()

            tImGui.EndMenu()
        end

        if tImGui.BeginMenu(tLang.L("menu_about")) then
            local pressed,checked = tImGui.MenuItem(tLang.L("physic_editor"), nil, false)
            if pressed then
                if mbm.is('windows') then
                    os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/editors.html#physic-editor"')
                elseif mbm.is('linux') then
                    os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/editors.html#physic-editor"')
                elseif mbm.is('macos') then
                    os.execute('open "https://mbm-documentation.readthedocs.io/en/latest/editors.html#physic-editor"')
                end
            end
            local pressed,checked = tImGui.MenuItem(tLang.L("mbm_engine"), nil, false)
            if pressed then
                if mbm.is('windows') then        
                    os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/"')
                elseif mbm.is('linux') then
                    os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/"')
                elseif mbm.is('macos') then
                    os.execute('open "https://mbm-documentation.readthedocs.io/en/latest/"')
                end
            end

            if tImGui.BeginMenu(tLang.L("menu_version")) then
                tImGui.TextDisabled(string.format('%s\nIMGUI: %s\nBox2d:%s', mbm.get('version'),tImGui.GetVersion(),box2d:getVersion()))
                tImGui.EndMenu()
            end
            
            tImGui.EndMenu()
        end

        
        tImGui.EndMainMenuBar()
    end
end

function showEditPhysics()
    if tMesh == nil then
        tUtil.showMessageWarn(tLang.L("no_mesh_to_edit_physics"))
        bShowEditPhysics = false
        return
    end

    local width = 250
    local x_pos, y_pos = 0, 0
    local max_width = 250
    local tSizeBtn   = {x=width - 20,y=0} -- size button
    tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_edit_physics,x_pos,y_pos,width,max_width)
    local is_opened, closed_clicked = tImGui.Begin(tLang.L(tWindowsTitle.title_edit_physics), true, 0)
    
    if is_opened then
        
        tImGui.Text(tLang.L("primitive_type"))
        local indexPrimitive = tImGui.RadioButton(tLang.L("rectangle"), tPhysicsOptions.iIndexPrimitiveType, 1)
        indexPrimitive       = tImGui.RadioButton(tLang.L("rectangle_triangle"), indexPrimitive, 2)
        if indexPrimitive == 2 then
            tImGui.SameLine()
            tUtil.pushResponsiveItemWidth(70)
            local step,step_fast,flags = 2,2,0
            local result, iValue = tImGui.InputInt('##PrimitivesRect',tPhysicsOptions.iPrimitivesRectangle,step,step_fast,flags)
            if result  and iValue > 1 and iValue < 1000 and iValue % 2 == 0 then
                tPhysicsOptions.iPrimitivesRectangle = iValue
            end
            tImGui.PopItemWidth()
        end

        indexPrimitive       = tImGui.RadioButton(tLang.L("circle"),          indexPrimitive, 3)
        indexPrimitive       = tImGui.RadioButton(tLang.L("circle_triangle"), indexPrimitive, 4)
        if indexPrimitive == 4 then
            tImGui.SameLine()
            tUtil.pushResponsiveItemWidth(70)
            local step,step_fast,flags = 1,1,0
            local result, iValue = tImGui.InputInt('##PrimitivesCircle',tPhysicsOptions.iPrimitivesCircle,step,step_fast,flags)
            if result  and iValue > 3 and iValue < 1000 then
                tPhysicsOptions.iPrimitivesCircle = iValue
            end
            tImGui.PopItemWidth()
        end
        indexPrimitive       = tImGui.RadioButton(tLang.L("triangle"), indexPrimitive, 5)
        indexPrimitive       = tImGui.RadioButton(tLang.L("complex_cube"), indexPrimitive, 6)
        indexPrimitive       = tImGui.RadioButton(tLang.L("complex_cube_triangle"), indexPrimitive, 7)
        local tSizeBtn       = {x=width - 20,y=0} -- size button

        local color             = {r=1,g=1,b=0.4,a=1}
        local thickness         =  5.0
        local winPos            = tImGui.GetCursorScreenPos()
        if indexPrimitive == 1 then
            local p_min             = {x = winPos.x + 75,  y = winPos.y + 15}
            local p_max             = {x = winPos.x + 125, y = winPos.y + 65}
            local rounding          =  2.0
            local rounding_corners  =  tImGui.Flags('ImDrawFlags_RoundCornersAll')
            tImGui.AddRect(p_min, p_max, color, rounding, rounding_corners, thickness)
        elseif indexPrimitive == 2 then
            local p_min             = {x = winPos.x + 75,  y = winPos.y + 15}
            local p_max             = {x = winPos.x + 125, y = winPos.y + 65}
            local rounding          =  2.0
            local rounding_corners  =  tImGui.Flags('ImDrawFlags_RoundCornersAll')
            tImGui.AddRect(p_min, p_max, color, rounding, rounding_corners, thickness)
            tImGui.AddLine(p_min,p_max,color,thickness)
            if tPhysicsOptions.iPrimitivesRectangle > 2 then
                local width = p_max.x - p_min.y
                local p_min             = {x = winPos.x + 75  - width - 10,  y = winPos.y + 15}
                local p_max             = {x = winPos.x + 125 - width - 10, y = winPos.y + 65}
                tImGui.AddRect(p_min, p_max, color, rounding, rounding_corners, thickness)
                tImGui.AddLine(p_min,p_max,color,thickness)
            end
        elseif indexPrimitive == 3 then
            local center        = {x=winPos.x + 100,y=winPos.y + 25 + 7.5}
            local radius        = 25
            tImGui.AddCircle(center, radius, color, 18, thickness)
        elseif indexPrimitive == 4 then
            local center        = {x=winPos.x + 100,y=winPos.y + 25 + 7.5}
            local radius        = 25
            local num_segments  = tPhysicsOptions.iPrimitivesCircle
            tImGui.AddNgon(center, radius, color, num_segments, thickness)
        elseif indexPrimitive == 5 then
            local p1     = {x=0 +  winPos.x + 75,y=50 + winPos.y + 15}
            local p2     = {x=25 + winPos.x + 75,y=0  + winPos.y + 15}
            local p3     = {x=50 + winPos.x + 75,y=50 + winPos.y + 15}
            tImGui.AddTriangle(p1, p2, p3, color, thickness + 5)
        elseif indexPrimitive == 6 or indexPrimitive == 7 then
            -- simple isometric-cube icon: two offset squares joined by 4 diagonals
            local off  = 18
            local p_min = {x = winPos.x + 75,       y = winPos.y + 15 + off}
            local p_max = {x = winPos.x + 125,      y = winPos.y + 65 + off}
            local q_min = {x = winPos.x + 75 + off, y = winPos.y + 15}
            local q_max = {x = winPos.x + 125 + off,y = winPos.y + 65}
            tImGui.AddRect(p_min, p_max, color, 0, 0, thickness)
            tImGui.AddRect(q_min, q_max, color, 0, 0, thickness)
            tImGui.AddLine({x=p_min.x,y=p_min.y}, {x=q_min.x,y=q_min.y}, color, thickness)
            tImGui.AddLine({x=p_max.x,y=p_min.y}, {x=q_max.x,y=q_min.y}, color, thickness)
            tImGui.AddLine({x=p_min.x,y=p_max.y}, {x=q_min.x,y=q_max.y}, color, thickness)
            tImGui.AddLine({x=p_max.x,y=p_max.y}, {x=q_max.x,y=q_max.y}, color, thickness)
            if indexPrimitive == 7 then
                tImGui.AddLine({x=p_min.x,y=p_min.y}, {x=p_max.x,y=p_max.y}, color, thickness)
            end
        end

        tPhysicsOptions.iIndexPrimitiveType = indexPrimitive

        local step       =  1
        local step_fast  =  10
        
        winPos.y = winPos.y + 100
        tImGui.SetCursorScreenPos(winPos)
        
        if tImGui.Button(tLang.L("add_physic_btn"), tSizeBtn) then
            local width,height,depth = tMesh:getSize()
            if depth == nil then
                depth = 1
            end
            local tInfoPhysicsInner = {x=0,y=0,z=0}
            if indexPrimitive == 1 then --cube
                
                tInfoPhysicsInner.type = 'cube'
                tInfoPhysicsInner.width  = width
                tInfoPhysicsInner.height = height
            elseif indexPrimitive == 2 then --Triangle/Rectangle
                local half_width = width / 2
                local half_height = height / 2
                local step_div = width / (tPhysicsOptions.iPrimitivesRectangle / 2.0);
                local step = -half_width
                for i=1, tPhysicsOptions.iPrimitivesRectangle / 2 do
                    local tInfoPhysicsTriangle = {x=0,y=0,z=0,type = 'triangle'}
                    tInfoPhysicsTriangle.a = {x = step,            y = -half_height}
                    tInfoPhysicsTriangle.b = {x = step,            y =  half_height}
                    tInfoPhysicsTriangle.c = {x = step + step_div, y = -half_height}
                    addPhysics(tInfoPhysicsTriangle)
                    step = step + step_div
                    local tOld = tInfoPhysicsTriangle
                    tInfoPhysicsTriangle = {x=0,y=0,z=0,type = 'triangle'}

                    tInfoPhysicsTriangle.a = {x = tOld.c.x, y = tOld.c.y}
                    tInfoPhysicsTriangle.b = {x = tOld.b.x, y = tOld.b.y}
                    tInfoPhysicsTriangle.c = {x = step,     y = half_height}
                    addPhysics(tInfoPhysicsTriangle)
                end
                tInfoPhysicsInner = nil
                
            elseif indexPrimitive == 3 then --sphere
                tInfoPhysicsInner.type = 'sphere'
                tInfoPhysicsInner.ray  = width * 0.5
            elseif indexPrimitive == 4 then --Triangle/Circle
                local index      = 1
                local degree     = math.rad(360) / tPhysicsOptions.iPrimitivesCircle;
                local ray_width  = width * 0.5;
			    local ray_height = height * 0.5;
                local pVertex = {[1]=math.sin(0) * ray_width,[2]=math.cos(0) * ray_height}
                for i=1, tPhysicsOptions.iPrimitivesCircle do
                    table.insert(pVertex,math.sin(degree * i) * ray_width)
                    table.insert(pVertex,math.cos(degree * i) * ray_height)
                end

                local index = 3
                for i=1, tPhysicsOptions.iPrimitivesCircle do
                    local tInfoPhysicsTriangle = {x=0,y=0,z=0,type = 'triangle'}

                    tInfoPhysicsTriangle.a = {x = 0, y = 0}
                    tInfoPhysicsTriangle.b = {x = pVertex[index-2], y = pVertex[index-1]}
                    tInfoPhysicsTriangle.c = {x = pVertex[index+0], y = pVertex[index+1]}
                    index = index + 2
                    addPhysics(tInfoPhysicsTriangle)
                end

                tInfoPhysicsInner = nil
            elseif indexPrimitive == 5 then --triangle
                tInfoPhysicsInner.type = 'triangle'
                tInfoPhysicsInner.a = {x = width * -0.25, y = height * -0.25}
                tInfoPhysicsInner.b = {x = 0, y = height * 0.25}
                tInfoPhysicsInner.c = {x = width * 0.25, y = height * -0.25}
            elseif indexPrimitive == 6 then --complex (single 8-point box)
                local corners = boxCorners(width * 0.5, height * 0.5, depth * 0.5)
                tInfoPhysicsInner.type = 'complex'
                for i,l in ipairs(BOX_LETTERS) do
                    tInfoPhysicsInner[l] = {x=corners[i].x, y=corners[i].y, z=corners[i].z}
                end
            elseif indexPrimitive == 7 then --complex (box decomposed into 12 triangles)
                local corners = boxCorners(width * 0.5, height * 0.5, depth * 0.5)
                local faces   = boxTriangleFaces(corners)
                for _,tri in ipairs(faces) do
                    local tInfoPhysicsTriangle = {x=0,y=0,z=0,type='triangle'}
                    tInfoPhysicsTriangle.a = {x=tri[1].x, y=tri[1].y, z=tri[1].z}
                    tInfoPhysicsTriangle.b = {x=tri[2].x, y=tri[2].y, z=tri[2].z}
                    tInfoPhysicsTriangle.c = {x=tri[3].x, y=tri[3].y, z=tri[3].z}
                    addPhysics(tInfoPhysicsTriangle)
                end
                tInfoPhysicsInner = nil
            end
            if tInfoPhysicsInner then
                addPhysics(tInfoPhysicsInner)
            end
        end

        tImGui.Separator()

        if tImGui.Button(tLang.L("enable_select_all"), tSizeBtn) then
            for i=1, #tInfoPhysics do
                local tPhysic       = tInfoPhysics[i]
                tPhysic.selectable = true
            end
        end

        if tImGui.Button(tLang.L("disable_select_all"), tSizeBtn) then
            for i=1, #tInfoPhysics do
                local tPhysic       = tInfoPhysics[i]
                tPhysic.selectable = false
            end
        end

        tImGui.Separator()

        local height_in_items  =  -1
        local items            = {}
        for i=1, tMesh:getTotalAnim() do
            local str = select(1,tMesh:getAnim(i))
            table.insert(items,str)
        end
        
        local current_item     = select(2,tMesh:getAnim())
        local ret, current_item, item_as_string = tImGui.Combo('Animation', current_item, items, height_in_items)
        if ret then
            tMesh:setAnim(current_item)
        end

        tImGui.Separator()
        
        local step       =  1.0
        local step_fast  =  10.0
        local format     = "%.3f"
        local flags      =  0
        tUtil.pushResponsiveItemWidth(150)
        if tImGui.TreeNode('##physics_tree', tLang.L("physics")) then
            tInfoPhysics:highLightPoint(nil) --disable high light before each loop
            for i=1, #tInfoPhysics do
                local tPhysic       = tInfoPhysics[i]
                local id_node       = '##physics_' .. tostring(i) .. '_' .. tPhysic.type
                local flag_node     = 0
                
                if tImGui.TreeNodeEx(tPhysic.type_info .. string.format('[%d]',i),flag_node,id_node) then
                    tPhysic.selectable = tImGui.Checkbox(tLang.L("selectable") .. '##ESelectable' .. tostring(i), tPhysic.selectable)

                    if tPhysic.type_info == 'rectangle' then
                        local label    = tLang.L("axis_x") .. string.format('##rectangle_%d_x', i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.x, step, step_fast, format, flags)
                        if result then
                            tPhysic.x = fValue
                            updatePhysics(tPhysic)
                        end
                        if tImGui.IsItemHovered(0) then
                            tInfoPhysics:highLightPoint(i)
                        end

                        local label    = tLang.L("axis_y") .. string.format('##rectangle_%d_y', i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.y, step, step_fast, format, flags)
                        if result then
                            tPhysic.y = fValue
                            updatePhysics(tPhysic)
                        end
                        if tImGui.IsItemHovered(0) then
                            tInfoPhysics:highLightPoint(i)
                        end

                        local label    = tLang.L("width") .. string.format('##rectangle_%d_width', i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.width, step, step_fast, format, flags)
                        if result then
                            if fValue > 0 then
                                tPhysic.width = fValue
                                updatePhysics(tPhysic)
                            end
                        end
                        
                        local label    = tLang.L("height") .. string.format('##rectangle_%d_height', i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.height, step, step_fast, format, flags)
                        if result then
                            if fValue > 0 then
                                tPhysic.height = fValue
                                updatePhysics(tPhysic)
                            end
                        end
                        
                    elseif tPhysic.type_info == 'cube' then
                            local label    = tLang.L("axis_x") .. string.format('##cube_%d_x', i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.x, step, step_fast, format, flags)
                            if result then
                                tPhysic.x = fValue
                                updatePhysics(tPhysic)
                            end
                            
                            local label    = tLang.L("axis_y") .. string.format('##cube_%d_y', i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.y, step, step_fast, format, flags)
                            if result then
                                tPhysic.y = fValue
                                updatePhysics(tPhysic)
                            end
                            
                            local label    = tLang.L("width") .. string.format('##cube_%d_width', i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.width, step, step_fast, format, flags)
                            if result then
                                if fValue > 0 then
                                    tPhysic.width = fValue
                                    updatePhysics(tPhysic)
                                end
                            end
                            
                            local label    = tLang.L("height") .. string.format('##cube_%d_height', i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.height, step, step_fast, format, flags)
                            if result then
                                if fValue > 0 then
                                    tPhysic.height = fValue
                                    updatePhysics(tPhysic)
                                end
                            end
                            --if tImGui.IsItemHovered(0) then
                            --    tLinesPhysics:setColor(1,0,1)
                            --end
                    elseif tPhysic.type == 'sphere' then
                        local label    = tLang.L("axis_x") .. string.format('##sphere_%d_x', i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.x, step, step_fast, format, flags)
                        if result then
                            tPhysic.x = fValue
                            updatePhysics(tPhysic)
                        end
                        if tImGui.IsItemHovered(0) then
                            tInfoPhysics:highLightPoint(i)
                        end

                        local label    = tLang.L("axis_y") .. string.format('##sphere_%d_y', i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.y, step, step_fast, format, flags)
                        if result then
                            tPhysic.y = fValue
                            updatePhysics(tPhysic)
                        end
                        if tImGui.IsItemHovered(0) then
                            tInfoPhysics:highLightPoint(i)
                        end

                        local label    = tLang.L("ray") .. string.format('##sphere_%d_ray', i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.ray, step, step_fast, format, flags)
                        if result then
                            if fValue > 0 then
                                tPhysic.ray = fValue
                                updatePhysics(tPhysic)
                            end
                        end
                        if tImGui.IsItemHovered(0) then
                            tInfoPhysics:highLightPoint(i)
                        end

                    elseif tPhysic.type == 'triangle' then
                        local label    = tLang.L("axis_x") .. string.format('##triangle_%d_x', i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.x, step, step_fast, format, flags)
                        if result then
                            tPhysic.x = fValue
                            updatePhysics(tPhysic)
                        end
                        if tImGui.IsItemHovered(0) then
                            tInfoPhysics:highLightPoint(i,0)
                        end

                        local label    = tLang.L("axis_y") .. string.format('##triangle_%d_y', i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.y, step, step_fast, format, flags)
                        if result then
                            tPhysic.y = fValue
                            updatePhysics(tPhysic)
                        end
                        if tImGui.IsItemHovered(0) then
                            tInfoPhysics:highLightPoint(i,0)
                        end

                        local label    = string.format('#%dX##triangle_%d_x',1,i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.a.x, step, step_fast, format, flags)
                        if result then
                            tPhysic.a.x = fValue
                            updatePhysics(tPhysic)
                        end
                        if tImGui.IsItemHovered(0) then
                            tInfoPhysics:highLightPoint(i,1)
                        end

                        local label    = string.format('#%dY##triangle_%d_y',1,i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.a.y, step, step_fast, format, flags)
                        if result then
                            tPhysic.a.y = fValue
                            updatePhysics(tPhysic)
                        end
                        if tImGui.IsItemHovered(0) then
                            tInfoPhysics:highLightPoint(i,1)
                        end

                        local label    = string.format('#%dX##triangle_%d_x',2,i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.b.x, step, step_fast, format, flags)
                        if result then
                            tPhysic.b.x = fValue
                            updatePhysics(tPhysic)
                        end
                        if tImGui.IsItemHovered(0) then
                            tInfoPhysics:highLightPoint(i,2)
                        end

                        local label    = string.format('#%dY##triangle_%d_y',2,i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.b.y, step, step_fast, format, flags)
                        if result then
                            tPhysic.b.y = fValue
                            updatePhysics(tPhysic)
                        end
                        if tImGui.IsItemHovered(0) then
                            tInfoPhysics:highLightPoint(i,2)
                        end

                        local label    = string.format('#%dX##triangle_%d_x',3,i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.c.x, step, step_fast, format, flags)
                        if result then
                            tPhysic.c.x = fValue
                            updatePhysics(tPhysic)
                        end
                        if tImGui.IsItemHovered(0) then
                            tInfoPhysics:highLightPoint(i,3)
                        end

                        local label    = string.format('#%dY##triangle_%d_y',3,i)
                        local result, fValue = tImGui.InputFloat(label, tPhysic.c.y, step, step_fast, format, flags)
                        if result then
                            tPhysic.c.y = fValue
                            updatePhysics(tPhysic)
                        end
                        if tImGui.IsItemHovered(0) then
                            tInfoPhysics:highLightPoint(i,3)
                        end
                    elseif tPhysic.type == 'complex' then
                        for pi,l in ipairs(BOX_LETTERS) do
                            local p = tPhysic[l]
                            local label    = string.upper(l) .. tLang.L("axis_x") .. string.format('##complex_%d_%s_x', i, l)
                            local result, fValue = tImGui.InputFloat(label, p.x, step, step_fast, format, flags)
                            if result then
                                p.x = fValue
                                updatePhysics(tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tInfoPhysics:highLightPoint(i,pi)
                            end

                            local label    = string.upper(l) .. tLang.L("axis_y") .. string.format('##complex_%d_%s_y', i, l)
                            local result, fValue = tImGui.InputFloat(label, p.y, step, step_fast, format, flags)
                            if result then
                                p.y = fValue
                                updatePhysics(tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tInfoPhysics:highLightPoint(i,pi)
                            end

                            local label    = string.upper(l) .. tLang.L("axis_z") .. string.format('##complex_%d_%s_z', i, l)
                            local result, fValue = tImGui.InputFloat(label, p.z, step, step_fast, format, flags)
                            if result then
                                p.z = fValue
                                updatePhysics(tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tInfoPhysics:highLightPoint(i,pi)
                            end
                        end
                    end
                    if tImGui.Button(tLang.L("delete_physic"), {x=0,y=0}) then
                        table.remove(tInfoPhysics,i)
                        tPhysic:destroy()
                        tInfoPhysics:unSelectFrame()
                        tImGui.TreePop()
                        break
                    end
                    tImGui.TreePop()
                end
            end
            tImGui.TreePop()
        end
        tImGui.PopItemWidth()
    end
    if closed_clicked then
        bShowEditPhysics = false
    end
    tImGui.End()
end

function onKeyDown(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = true
    elseif key == mbm.getKeyCode('esc') then
        if tInfoPhysics then
            tInfoPhysics:unSelectFrame()
        end
    elseif key == mbm.getKeyCode('delete') then
        if tInfoPhysics and tInfoPhysics.iIndexSelected <= #tInfoPhysics and tInfoPhysics.iIndexSelected > 0  then
            local tPhysic = tInfoPhysics[tInfoPhysics.iIndexSelected]
            table.remove(tInfoPhysics,tInfoPhysics.iIndexSelected)
            tPhysic:destroy()
            tInfoPhysics:unSelectFrame()
        end
    elseif keyControlPressed then
        if key == mbm.getKeyCode('S') then -- Ctrl+S
            onSaveMesh()
        elseif key == mbm.getKeyCode('O') then -- Ctrl+O
            onLoadMesh()
        elseif key == mbm.getKeyCode('P') then -- Ctrl+P
            bShowEditPhysics = true
        end
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = false
    end
end

function setTarget(x,y)
    x , y = x / tPhysicSimulate:getScale(),y / tPhysicSimulate:getScale() -- we have to consider the scale
    if tMouseJoint then
        tMouseJoint:setTarget(x,y)
    end
    tJoint.target.x,tJoint.target.y = x,y
end


function onTouchDown(key,x,y)
    local IsAnyWindowHovered = tImGui.IsAnyWindowHovered()
    isClickedMouseLeft = key == 0
    camera2d.mx = x
    camera2d.my = y
    lastMouse3d.x = x
    lastMouse3d.y = y

    if not IsAnyWindowHovered and tInfoPhysics then
        tInfoPhysics:setTouchDown(x,y)
        local i_circle     =  tInfoPhysics:isOverCircle(x,y)
        local index_isOver =  tInfoPhysics:indexIsOver(x,y)
        if index_isOver == 0 and i_circle == 0 then
            tInfoPhysics:unSelectFrame()
        end
        if index_isOver ~= 0 then
            tInfoPhysics:setSelected(index_isOver)
        end
        local index_sel    =  tInfoPhysics:getIndexSelectedFrame()
        if index_sel == 0 then
            if index_isOver ~= 0 then
                bEnableMoveWorld = false
            else
                tInfoPhysics:unSelectFrame()
                bEnableMoveWorld = true
            end
        else
            local index_isOver =  tInfoPhysics:indexIsOver(x,y)
            if index_isOver ~= 0 or i_circle ~= 0 then
                bEnableMoveWorld = false
            else
                bEnableMoveWorld = true
            end
            if i_circle ~= 0 then
                tInfoPhysics:setSelectedCircle(i_circle)
            end
        end

        if not bIs3d then
            x,y = mbm.to2dw(x,y)
            for i = 2, #tSimulate do
                local tMesh = tSimulate[i]
                if tPhysicSimulate:testPoint(tMesh,x,y) and tMouseJoint == nil then
                    tJoint.maxForce =  1000 * tPhysicSimulate:getMass(tMesh)
                    setTarget(x,y) -- update the target
                    if tPhysicSimulate:joint(tMesh,tMesh,tJoint) > 0 then
                        tMouseJoint = tPhysicSimulate:getJoint(tMesh)
                        tMeshJoint  = tSimulate[i]
                        bEnableMoveWorld = false
                        break
                    end
                end
            end
        end
    end
end

function onTouchMove(key,x,y)
    local IsAnyWindowHovered = tImGui.IsAnyWindowHovered()

    if bIs3d then
        if isClickedMouseLeft and not IsAnyWindowHovered then
            local dx = x - lastMouse3d.x
            local dy = y - lastMouse3d.y
            tCam3d.azimuth   = tCam3d.azimuth - dx * 0.005
            tCam3d.elevation = tCam3d.elevation + dy * 0.005
            local limit = math.pi * 0.49
            if tCam3d.elevation >  limit then tCam3d.elevation =  limit end
            if tCam3d.elevation < -limit then tCam3d.elevation = -limit end
            applyCam3d(tCam3d)
        elseif not IsAnyWindowHovered and tInfoPhysics then
            local index_isOver =  tInfoPhysics:indexIsOver(x,y)
            tInfoPhysics:setHighLight(index_isOver,x,y)
        end
        lastMouse3d.x = x
        lastMouse3d.y = y
        return
    end

    if bEnableMoveWorld and isClickedMouseLeft and not bMovingAnyPoint and not IsAnyWindowHovered then
        local px = (camera2d.mx - x) * camera2d.sx
        local py = (camera2d.my - y) * camera2d.sy
        camera2d.mx = x
        camera2d.my = y
        camera2d:setPos(camera2d.x + px,camera2d.y - py)
    end

    if not IsAnyWindowHovered and tInfoPhysics then
        if isClickedMouseLeft then
            local index_sel    =  tInfoPhysics:getIndexSelectedFrame()
            if index_sel ~= 0 then
                local index_circle = tInfoPhysics:getSelectedCircle()
                if index_circle ~= 0 then
                    tInfoPhysics:editByCircleSelected(x,y)
                else
                    tInfoPhysics:moveFrameSelected(x,y)
                end
            end

            if tMouseJoint then
                x , y = mbm.to2dw(x,y)
                setTarget(x,y)-- update the target
            end
        else
            local index_isOver =  tInfoPhysics:indexIsOver(x,y)
            tInfoPhysics:setHighLight(index_isOver,x,y)
        end
    end
end

function onTouchUp(key,x,y)
    isClickedMouseLeft = false
    bMovingAnyPoint = false
    bEnableMoveWorld = true
    if tInfoPhysics then
        tInfoPhysics:finishMovingFrame(x,y)
        tInfoPhysics:unSelectCircles()
        if not bIs3d then
            tInfoPhysics:setScale(scale,scale,scale)
        end
    end
    camera2d.mx = x
    camera2d.my = y

    if tMouseJoint then
        tPhysicSimulate:destroyJoint(tMeshJoint) --no long are holding the object, destroy it
        tMouseJoint = nil --mark as nil
    end
end


function onTouchZoom(zoom)
    local IsAnyWindowHovered = tImGui.IsAnyWindowHovered()
    if tMesh and not IsAnyWindowHovered then
        if bIs3d then
            tCam3d.distance = tCam3d.distance - (zoom * 20)
            tCam3d.distance = math.max(tCam3d.distance, 10)
            tCam3d.distance = math.min(tCam3d.distance, 20000)
            applyCam3d(tCam3d)
        else
            scale = tMesh.sx
            scale = scale + (zoom * 0.1)
            scale = math.max(scale,0.1)
            scale = math.min(scale,100)
            tMesh:setScale(scale,scale,scale)
            if tInfoPhysics then
                tInfoPhysics:setScale(scale,scale,scale)
            end
        end
    end
end

function addPhysics(tInfoPhysicsInner)
    tInfoPhysicsInner.selectable = true
    table.insert(tInfoPhysics,tInfoPhysicsInner)
    updatePhysics(tInfoPhysicsInner)
end

function onLoop(delta)
    main_menu_physic_editor()
    tex_alpha_pattern:setPos(camera2d.x,camera2d.y)
    
    if tGlobalFont then
        updateText(tGlobalFont.tText,false)
    end

    tUtil.showOverlayMessage()
    tex_alpha_pattern:setPos(camera2d.x,camera2d.y)

    if bIs3d and tMesh then
        applyCam3d(tCam3d)
        tImGui.Begin(tLang.L('mesh_preview_orbit') .. '##physicsOrbit3d', true, tImGui.Flags({'ImGuiWindowFlags_NoTitleBar','ImGuiWindowFlags_NoResize'}))
        tUtil.drawOrbitGizmo(tCam3d, {size = 110})
        tImGui.End()
    end

    if tMesh and tInfoPhysics then
        local strSelected = ''
        for i=1, #tInfoPhysics do
            if tInfoPhysics[i].bSelected then
                strSelected = 'Selected: ' .. tostring(i)
            end
        end

        tUtil.showStatusMessage('Info',string.format('Scale: %2.2f\nPhysics %d\n%s',tMesh.sx,#tInfoPhysics,strSelected))
    end

    if #tSimulate == 0 then
        if bShowEditPhysics then
            showEditPhysics()
        end
    end
end
