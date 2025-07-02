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

   Scene Editor 2d World and 2d Screen

   This is a script based on mbm engine.

   Scene Editor 2d meant to create your scene 2d easier.

   More info at: https://mbm-documentation.readthedocs.io/en/latest/editors.html#scene-editor-2d

]]--

tImGui        =     require "ImGui"
tUtil         =     require "editor_utils"

function onInitScene()
    camera2d		 = mbm.getCamera("2d")
    tLineCenterX     = line:new("2dw",0,0,50)
    tLineCenterY     = line:new("2dw",0,0,50)
    tLineScreen2d    = line:new("2dw")
    tLineCenterX:add({-9999999,0, 9999999,0})
    tLineCenterY:add({0,-9999999, 0,9999999})
    local xRes = 800
    local yRes = 600
    local tRectangleScreen = {-xRes/2,-yRes/2, -xRes/2,yRes/2, xRes/2,yRes/2, xRes/2,-yRes/2, -xRes/2,-yRes/2  }
    tLineScreen2d:add(tRectangleScreen)
    tLineScreen2d:setColor(0.7,0.7,0.7)
    tWindowsTitle    = {    title_image_selector    = 'Image(s) selector',
                            title_meshes            = 'Mesh(s)',
                            title_mesh_info         = 'Mesh Information',
                            title_loading           = 'loading',
                            title_adding_mesh       = 'Options when Adding Mesh',
                          }
    tLineCenterX:setColor(1,0,0)
    tLineCenterY:setColor(0,1,0)
    camera2d.iIteration         = 0
    bEnableMoveWorld            = true
    bClickedOverAnyMesh         = false
    bMovingAnyMesh              = false
    tUtil.sMessageOverlay       = 'Welcome to Scene Editor 2D!!!'
    ImGuiWindowFlags_NoMove     = tImGui.Flags('ImGuiWindowFlags_NoMove')
    ImGuiTreeNodeFlags_Selected = tImGui.Flags('ImGuiTreeNodeFlags_Selected')
    local sTextureFileName      = tUtil.createAlphaPattern(1024,768,32,{r=240,g=240,b=240},{r=125,g=125,b=125})
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

    tResolution = {
		{x = 800 , y = 600  , comment = 'XVGA'},
		{x = 1024, y = 768  , comment = ''},
		{x = 1280, y = 720  , comment = 'Standard High Density (HD)'},
		{x = 1280, y = 736  , comment = ''},
		{x = 1280, y = 752  , comment = ''},
		{x = 1280, y = 768  , comment = 'WXGA'},
		{x = 1334, y = 750  , comment = 'Apple phones only'},
		{x = 1280, y = 800  , comment = 'WXGA'},
		{x = 1920, y = 1080 , comment = 'Standard Full HD Display'},
		{x = 2560, y = 1440 , comment = 'Standard Quad HD Display'},
        {x = 3840, y = 2160 , comment = 'Standard Ultra HD Display'}}
    
    
    tFilter = {
        tWorld      = {'All', '2D World', '2D Screen'},
        tType       = {'All Type','Font', 'Gif', 'Mesh','Particle', 'Sprite', 'Texture', 'Tile' },
        tPhysicType = {'All Type','Static', 'Dynamic', 'Kinematic', 'Character' },
    }

    tPhysicEditor   = {
        tType       = {'None', 'Static', 'Dynamic', 'Kinematic', 'Character' },
        Static      = {type = 'static',     density = 0, friction = 0.3,                   scaleX = 1, scaleY = 1, sensor = false},
        Dynamic     = {type = 'dynamic',    density = 1, friction = 10, restitution = 0.1, scaleX = 1, scaleY = 1, sensor = false, bullet = false},
        Kinematic   = {type = 'kinematic',  density = 1, friction = 10, restitution = 0.1, scaleX = 1, scaleY = 1, sensor = false, bullet = false},
        Character   = {type = 'character',  density = 1, friction = 10, restitution = 0.1, scaleX = 1, scaleY = 1, sensor = false, bullet = false},
    }

    v1                      = vec2:new()
    sTextureShapeOver       = '#86FF8686'
    sTextureShapeSelected   = '#8686FF48'
    iLastNodeSelected       = 0
    iFiltered               = 0
    tWindowsArea            = tUtil.onNewAnyWindowsHovered()
    onNewSceneEditor()
end



function shiftAddedMeshAccordingToOptions(tObj)
    if not tOptionsEditor.bCenterOfScreen and tLastMeshAdded then
        local w,h = tOptionsEditor.initialDisplacement.x, tOptionsEditor.initialDisplacement.y
        if tOptionsEditor.tIncrementOnNewMesh.x then
            if tOptionsEditor.tDirectionIncrementOnNewMesh.bRight then
                tObj.x = tLastMeshAdded.x + w
                tObj.tShape.x = tObj.x
            else
                tObj.x = tLastMeshAdded.x - w
                tObj.tShape.x = tObj.x
            end
        else
            tObj.x = tLastMeshAdded.x
            tObj.tShape.x = tObj.x
        end
        if tOptionsEditor.tIncrementOnNewMesh.y then
            if tOptionsEditor.tDirectionIncrementOnNewMesh.bUp then
                tObj.y = tLastMeshAdded.y + h
                tObj.tShape.y = tObj.y
            else
                tObj.y = tLastMeshAdded.y - h
                tObj.tShape.y = tObj.y
            end
        else
            tObj.y = tLastMeshAdded.y
            tObj.tShape.y = tObj.y
        end
    end
    tLastMeshAdded = tObj
    tFollowCam = tObj
end

function initialSetUpForAddedMesh(tObj)
    table.insert(tAllMesh,tObj)
    tObj.index = #tAllMesh
    shiftAddedMeshAccordingToOptions(tObj)
end

function onAddMesh()
    local fileName = mbm.openMultiFile(sLastMeshAdd,"tile","spt","ptl","png","msh","fnt","jpeg","jpg","bmp","gif","psd","pic","pnm","hdr","tga","tif")
    if fileName then
		if type(fileName) == 'string' then
			local tMeshTmp = tUtil.onAddMeshToEditor(fileName,true,"2dw")
            if tMeshTmp then
                initialSetUpForAddedMesh(tMeshTmp)
				bShowMeshList = true
                sLastMeshAdd = fileName
			else
                tUtil.showMessageWarn('Failed to add mesh!')
            end
		elseif type(fileName) == 'table' then
			local width = 0
			for i=1, #fileName do
				local tMeshTmp = tUtil.onAddMeshToEditor(fileName[i],true,"2dw")
                if tMeshTmp then
					tMeshTmp.x = tMeshTmp.x + width
					local w,h = tMeshTmp:getSize()
					width = width + w
                    initialSetUpForAddedMesh(tMeshTmp)
					sLastMeshAdd = fileName
                    bShowMeshList = true
                else
                    tUtil.showMessageWarn('Failed to add mesh!!!')
				end
			end
		end
	end
end

function onUnSelectAll()
    for i=1, #tSelectedObjs do
        local tObj          = tSelectedObjs[i]
        tObj.isSelected     = false
        tObj.tShape.visible = false
    end
    tSelectedObjs = {}
end

function onSelectAll()
    tSelectedObjs = {}
    for i=1, #tAllMesh do
        local tObj = tAllMesh[i]
        if filter(tObj) and not tObj.isBlocked then
            tObj.isSelected     = true
            tObj.tShape.visible = true
            table.insert(tSelectedObjs,tObj)
        else
            tObj.isSelected     = false
            tObj.tShape.visible = false
        end
    end
end

function onInvertSelection()
    tSelectedObjs = {}
    for i=1, #tAllMesh do
        local tObj = tAllMesh[i]
        if filter(tObj) and not tObj.isBlocked then
            if tObj.isSelected then
                tObj.isSelected     = false
                tObj.tShape.visible = false
            else
                tObj.isSelected     = true
                tObj.tShape.visible = true
                table.insert(tSelectedObjs,tObj)
            end
        else
            tObj.isSelected     = false
            tObj.tShape.visible = false
        end
    end
end

function onDeleteSelected()
    for i=1, #tSelectedObjs do
        local tObj          = tSelectedObjs[i]
        for j=1, #tAllMesh do
            local that = tAllMesh[j]
            if that == tObj then
                table.remove(tAllMesh,j)
                that:destroy()
                that.tShape:destroy()
                break
            end
        end
    end
    tSelectedObjs = {}
end

function onDuplicated()
    if #tSelectedObjs > 0 then
        for i=1, #tSelectedObjs do
            tLastMeshAdded = tSelectedObjs[i]
            local tMeshTmp = tUtil.onAddMeshToEditor(tLastMeshAdded.fileName,true,"2dw",tLastMeshAdded.sText)
            if tMeshTmp then
                tMeshTmp:setAnim(select(2,tLastMeshAdded:getAnim()))
                if tLastMeshAdded.tPhysicInfo then
                    tMeshTmp.tPhysicInfo = tUtil.deepCopyTable(tLastMeshAdded.tPhysicInfo)
                end
                tMeshTmp.is2ds          = tLastMeshAdded.is2ds
                tMeshTmp.isRelative2ds  = tLastMeshAdded.isRelative2ds
                initialSetUpForAddedMesh(tMeshTmp)
                sLastMeshAdd = tLastMeshAdded.fileName
			end
        end
        if #tSelectedObjs > 1 then
            tUtil.showMessage(string.format('%d Meshes duplicated!',#tSelectedObjs))
        else
            tUtil.showMessage('Mesh duplicated!')
        end
    elseif tLastMeshAdded then
        local tMeshTmp = tUtil.onAddMeshToEditor(tLastMeshAdded.fileName,true,"2dw",tLastMeshAdded.sText)
        if tMeshTmp then
            tMeshTmp:setAnim(select(2,tLastMeshAdded:getAnim()))
            if tLastMeshAdded.tPhysicInfo then
                tMeshTmp.tPhysicInfo = tUtil.deepCopyTable(tLastMeshAdded.tPhysicInfo)
            end
            tMeshTmp.is2ds          = tLastMeshAdded.is2ds
            tMeshTmp.isRelative2ds  = tLastMeshAdded.isRelative2ds
            initialSetUpForAddedMesh(tMeshTmp)
            tUtil.showMessage('Mesh duplicated!')
        end
    else
        tUtil.showMessageWarn('There is no Mesh to Duplicate!')
    end
end

function drawBlockButton(isBlocked,axis)

    tImGui.SameLine()
    local label
    if isBlocked then
        label = 'Blocked##' .. axis
    else
        label = 'Free##' .. axis
    end
    if tImGui.SmallButton(label) then
        return not isBlocked
    end
    return isBlocked
end

function treeNodePosition(tObj)
    local flags  = 0
    if tImGui.TreeNodeEx('Position',flags, string.format("Position-%d",tObj.iIndex)) then
        local step       =  1.0
        local step_fast  =  10.0
        local format     = "%.3f"
        local flags      =  0
        tImGui.PushItemWidth(150)

        if tObj.is2ds then
            tObj.isRelative2ds = tImGui.Checkbox('Relative 2D Screen', tObj.isRelative2ds)
            tImGui.SameLine()
            tImGui.HelpMarker('This flag is only available for mesh 2D screen!\nIt forces to calc considering the size of mesh and how far it is from the border (closest border)\n\nIt is good when you want to make sure that the displacement of mesh regarding the aspect ratio.')
        end
        local result, fValue = tImGui.InputFloat('X##Mesh(s)', tObj.x, step, step_fast, format, flags)
        tObj.isBlockedX      = drawBlockButton(tObj.isBlockedX,'X')
        if result and not tObj.isBlockedX then
            tObj.x = fValue
            tObj.tShape.x = fValue
        end

        local result, fValue = tImGui.InputFloat('Y##Mesh(s)', tObj.y, step, step_fast, format, flags)
        tObj.isBlockedY      = drawBlockButton(tObj.isBlockedY, 'Y')
        if result and not tObj.isBlockedY then
            tObj.y = fValue
            tObj.tShape.y = fValue
        end

        local result, fValue = tImGui.InputFloat('Z##Mesh(s)', tObj.z, step, step_fast, format, flags)
        tObj.isBlockedZ      = drawBlockButton(tObj.isBlockedZ,'Z')
        if result and not tObj.isBlockedZ then
            tObj.z = fValue
            tObj.tShape.z = fValue - 1
        end
        tImGui.PopItemWidth()

        tImGui.TreePop()
    end
end

function treeNodeScale(tObj)
    local flags  = 0
    if tImGui.TreeNodeEx('Scale',flags, string.format("Scale-%d",tObj.iIndex)) then
        local step       =  0.02
        local step_fast  =  0.1
        local format     = "%.3f"
        local flags      =  0
        tImGui.PushItemWidth(150)
        local result, fValue = tImGui.InputFloat('SX##Mesh(s)', tObj.sx, step, step_fast, format, flags)
        if result then
            if fValue > 0 then
                tObj.sx = fValue
                local w,h,d = tObj:getSize()
                tObj.tShape.sx = w
            end
        end

        local result, fValue = tImGui.InputFloat('SY##Mesh(s)', tObj.sy, step, step_fast, format, flags)
        if result then
            if fValue > 0 then
                tObj.sy = fValue
                local w,h,d = tObj:getSize()
                tObj.tShape.sy = h
            end
        end

        local result, fValue = tImGui.InputFloat('SZ##Mesh(s)', tObj.sz, step, step_fast, format, flags)
        if result then
            if fValue > 0 then
                tObj.sz = fValue
                local w,h,d = tObj:getSize()
                if d then
                    tObj.tShape.sz = d
                end
            end
        end
        tImGui.PopItemWidth()

        tImGui.TreePop()
    end
end

function treeNodeAngle(tObj)
    local flags  = 0
    if tImGui.TreeNodeEx('Angle',flags, string.format("Angle-%d",tObj.iIndex)) then
        local step       =  1.0
        local step_fast  =  5.0
        local format     = "%.2f"
        local flags      =  0
        tImGui.PushItemWidth(150)
        local result, fValue = tImGui.InputFloat('AX##Mesh(s)', math.deg(tObj.ax), step, step_fast, format, flags)
        if result then
            if fValue >= -360 and fValue <= 360 then
                local radian   = math.rad(fValue)
                tObj.ax        = radian
                tObj.tShape.ax = radian
            end
        end

        local result, fValue = tImGui.InputFloat('AY##Mesh(s)', math.deg(tObj.ay), step, step_fast, format, flags)
        if result then
            if fValue >= -360 and fValue <= 360 then
                local radian   = math.rad(fValue)
                tObj.ay        = radian
                tObj.tShape.ay = radian
            end
        end

        local result, fValue = tImGui.InputFloat('AZ##Mesh(s)', math.deg(tObj.az), step, step_fast, format, flags)
        if result then
            if fValue >= -360 and fValue <= 360 then
                local radian   = math.rad(fValue)
                tObj.az        = radian
                tObj.tShape.az = radian
            end
        end
        tImGui.PopItemWidth()

        tImGui.TreePop()
    end
end

function showPropertiesForMesh(tObj)
    
    local isBlocked = tObj.isBlocked or false
    if not isBlocked then
        local tWorld = {'2D World', '2D Screen'}
        local iIndexWorldMesh = 1
        if tObj.is2ds then
            iIndexWorldMesh = 2
        end
        local ret, current_item, item = tImGui.Combo('##WorldObj' , iIndexWorldMesh, tWorld)
        if ret then
            if current_item == 1 then
                tObj.is2ds = false
                tObj.isRelative2ds = nil
            else
                tObj.is2ds = true
                tObj.isRelative2ds = true
            end
            if filter(tObj) then
                tObj.visible = true
            else
                tObj.visible = false
                tObj.tShape.visible = false
            end
        end
    
        local bSelected = tImGui.Checkbox('Selected',tObj.isSelected)

        if bSelected ~= tObj.isSelected then
            setSelectedObj(tObj,bSelected)
            if bSelected then
                tFollowCam = tObj
            end
        end
    end

    if isBlocked then
        local idx     = tImGui.Flags('ImGuiCol_Text')
        local color   = {r=1,g=0,b=0.3,a=1}
        tImGui.PushStyleColor(idx, color)
    end
    
    local bBlocked  = tImGui.Checkbox('Blocked',isBlocked)
    if bBlocked ~= isBlocked then
        tObj.isBlocked = bBlocked
        if bBlocked then
            setSelectedObj(tObj,false)
        end
    end
    if isBlocked then
        tImGui.PopStyleColor(1)
    end
    if not bBlocked then
        treeNodePosition(tObj)
        treeNodeScale(tObj)
        treeNodeAngle(tObj)
        treeNodeText(tObj)
        treeNodeAnimation(tObj)
        treeNodePhysics(tObj)
    end
end

function treeNodeText(tObj)
    local sText = tObj.sText
    if sText then
        local flags  = 0
        if tImGui.TreeNodeEx('Text',flags, string.format("Text-%d",tObj.iIndex)) then
            tImGui.Text('Text')
            local label      = string.format("##Font-Text-%d",tObj.iIndex)
            local size       = {x=-1,y=100}
            local flags      = 0

            local modified , sNewText = tImGui.InputTextMultiline(label,sText,size,flags)
            if modified then
                tObj.sText = sNewText
                tObj.text = sNewText
                local w,h,d = tObj:getSize(sNewText)
                tObj.tShape:setScale(w,h,d or 1)
            end
            tImGui.TreePop()
        end
    end
end


function treeNodePhysics(tObj)
    local flags  = 0
    if tImGui.TreeNodeEx('Physics',flags, string.format("Physics-%d",tObj.iIndex)) then
        local height_in_items  =  -1
        tImGui.Text('Type')

        local tPhysicInfo  = tObj.tPhysicInfo or {type = 'None'}
        
        local iType = 1
        for i=1, #tPhysicEditor.tType do
            if tPhysicEditor.tType[i]:lower() == tPhysicInfo.type then
                iType = i
                break
            end
        end

        local ret, current_item, item_as_string = tImGui.Combo(string.format("##TypePhysics-%d",tObj.iIndex), iType, tPhysicEditor.tType, height_in_items)
        if ret then
            if item_as_string == 'None' then
                tObj.tPhysicInfo = nil
            else
                tObj.tPhysicInfo = tUtil.deepCopyTable(tPhysicEditor[item_as_string])
            end
        end

        
        if tPhysicInfo.density then
            local step       =  1.0
            local step_fast  =  2.0
            local format     = "%.3f"
            tImGui.Text('Density')
            tImGui.SameLine()
            tImGui.TextDisabled(' (kg/m^2)')
            local result, fValue = tImGui.InputFloat(string.format('##density-%d',tObj.iIndex), tPhysicInfo.density, step, step_fast, format, flags)
            if result and fValue >= 0 then
                tPhysicInfo.density = fValue
            end
        end

        if tPhysicInfo.friction then
            local step       =  0.002
            local step_fast  =  0.02
            local format     = "%.7f"
            tImGui.Text('Friction')
            tImGui.SameLine()
            tImGui.TextDisabled(' (Coefficient [0,1])')
            local result, fValue = tImGui.InputFloat(string.format('##Friction-%d',tObj.iIndex), tPhysicInfo.friction, step, step_fast, format, flags)
            if result and fValue >= 0 and fValue <= 1 then
                tPhysicInfo.friction = fValue
            end
        end

        if tPhysicInfo.restitution then
            local step       =  0.002
            local step_fast  =  0.02
            local format     = "%.7f"
            tImGui.Text('Restitution')
            tImGui.SameLine()
            tImGui.TextDisabled(' (Elasticity [0,1])')
            local result, fValue = tImGui.InputFloat(string.format('##Restitution-%d',tObj.iIndex), tPhysicInfo.restitution, step, step_fast, format, flags)
            if result and fValue >= 0 and fValue <= 1 then
                tPhysicInfo.restitution = fValue
            end
        end

        if tPhysicInfo.scaleX then
            local step       =  0.02
            local step_fast  =  0.2
            local format     = "%.2f"
            tImGui.Text('Scale X')
            tImGui.SameLine()
            tImGui.TextDisabled(' (Scale of physics on X axis)')
            local result, fValue = tImGui.InputFloat(string.format('##Scale X-%d',tObj.iIndex), tPhysicInfo.scaleX, step, step_fast, format, flags)
            if result and fValue > 0 and fValue <= 1000 then
                tPhysicInfo.scaleX = fValue
            end
        end

        if tPhysicInfo.scaleY then
            local step       =  0.02
            local step_fast  =  0.2
            local format     = "%.2f"
            tImGui.Text('Scale Y')
            tImGui.SameLine()
            tImGui.TextDisabled(' (Scale of physics on Y axis)')
            local result, fValue = tImGui.InputFloat(string.format('##Scale Y-%d',tObj.iIndex), tPhysicInfo.scaleY, step, step_fast, format, flags)
            if result and fValue > 0 and fValue <= 1000 then
                tPhysicInfo.scaleY = fValue
            end
        end

        if type(tPhysicInfo.sensor) == 'boolean' then
            tPhysicInfo.sensor = tImGui.Checkbox(string.format('Sensor##%d',tObj.iIndex),tPhysicInfo.sensor)
        end

        if type(tPhysicInfo.bullet) == 'boolean' then
            tPhysicInfo.bullet = tImGui.Checkbox(string.format('Bullet##%d',tObj.iIndex),tPhysicInfo.bullet)
        end

        tImGui.TreePop()
    end
end

function treeNodeAnimation(tObj)
    local flags  = 0
    if tImGui.TreeNodeEx('Animation',flags, string.format("Animation-%d",tObj.iIndex)) then
        local label            = '##Animation' .. tostring(tObj.iIndex)
        local tAnimations      = {}
        for i=1, tObj:getTotalAnim() do
            table.insert(tAnimations, string.format('%d:  %s',i,select(1,tObj:getAnim(i))))
        end

        local current_item     = select(2,tObj:getAnim())
        local height_in_items  =  -1
        local ret, current_item, item_as_string = tImGui.Combo(label, current_item, tAnimations, height_in_items)
        if ret then
            tObj:setAnim(current_item)
        end
        tImGui.TreePop()
    end
end

function filter(tObj)
    if tOptionsEditor.iIndexWorldMesh > 1 then -- not all worlds
        if tOptionsEditor.iIndexWorldMesh == 2 then -- 2dw
            if tObj.is2ds then
                return false
            end
        elseif tOptionsEditor.iIndexWorldMesh == 3 then -- 2ds
            if not tObj.is2ds then
                return false
            end
        end
    end
    if tOptionsEditor.iIndexTypeMeshFilter > 1 then -- not all type of mesh
        local sType = tFilter.tType[tOptionsEditor.iIndexTypeMeshFilter]:lower()
        if tObj.type ~= sType then
            return false
        end
    end

    if tOptionsEditor.iIndexTypePhysicsFilter > 1 then -- not all type of physics
        local sType = tFilter.tPhysicType[tOptionsEditor.iIndexTypePhysicsFilter]:lower()
        if tObj.tPhysicInfo == nil or tObj.tPhysicInfo.type ~= sType then
            return false
        end
    end

    return true
end

function showAddingMeshOptions()
    
    if bShowAddingMesh then
        local width        = 300
        local tPosWin      = {x = 0, y = 0}
        tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_adding_mesh,tPosWin.x,tPosWin.y,width,width + 50)
        local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_adding_mesh, true, ImGuiWindowFlags_NoMove)
        if is_opened then
            tOptionsEditor.bAddObjAs2dw = tImGui.Checkbox('Add Mesh as 2D world',tOptionsEditor.bAddObjAs2dw)
            tImGui.SameLine()
            tImGui.HelpMarker('Disabled is 2D screen.')
            tOptionsEditor.bCenterOfScreen = tImGui.Checkbox('Add Mesh at center of screen',tOptionsEditor.bCenterOfScreen)
            tImGui.SameLine()
            tImGui.HelpMarker('Disabled means that the Mesh will have options for the initial position.')
            if not tOptionsEditor.bCenterOfScreen then
                local step       =  1.0
                local step_fast  =  10.0
                local format     = "%.3f"
                local flags      =  0

                tImGui.PushItemWidth(200)
                local result, fValue = tImGui.InputFloat('X##InitialDisplacementX', tOptionsEditor.initialDisplacement.x, step, step_fast, format, flags)
                if result and fValue >= 0 then
                    tOptionsEditor.initialDisplacement.x = fValue
                end
                local result, fValue = tImGui.InputFloat('Y##InitialDisplacementY', tOptionsEditor.initialDisplacement.y, step, step_fast, format, flags)
                if result and fValue >= 0 then
                    tOptionsEditor.initialDisplacement.y = fValue
                end
                tImGui.PopItemWidth()

                tOptionsEditor.tIncrementOnNewMesh.x = tImGui.Checkbox('Increment width on X',tOptionsEditor.tIncrementOnNewMesh.x)
                tImGui.SameLine()
                tImGui.HelpMarker('The initial position (X) will be set by the width of the last mesh added. Ctrl+R')
                tOptionsEditor.tDirectionIncrementOnNewMesh.bRight = tImGui.Checkbox('To the Right on X',tOptionsEditor.tDirectionIncrementOnNewMesh.bRight)
                tOptionsEditor.tIncrementOnNewMesh.y = tImGui.Checkbox('Increment height on Y',tOptionsEditor.tIncrementOnNewMesh.y)
                tImGui.SameLine()
                tImGui.HelpMarker('The initial position (Y) will be set by the height of the last mesh added.')
                tOptionsEditor.tDirectionIncrementOnNewMesh.bUp = tImGui.Checkbox('To Up on Y',tOptionsEditor.tDirectionIncrementOnNewMesh.bUp)

                if tImGui.Button('Get Mesh Size', {x=200,y=0}) then
                    if tLastMeshAdded then
                        tOptionsEditor.initialDisplacement.x, tOptionsEditor.initialDisplacement.y = tLastMeshAdded:getSize()
                    else
                        tOptionsEditor.initialDisplacement.x = 0
                        tOptionsEditor.initialDisplacement.y = 0
                    end
                end

            end

            local title_duplicated = 'Duplicate Last Mesh Added'
            if #tSelectedObjs > 0 then
                title_duplicated = 'Duplicate All Mesh Selected'
            end
            if tImGui.Button(title_duplicated, {x=200,y=0}) then
                onDuplicated()
            end
        end
        tWindowsArea:addThisWindow()
        tImGui.End()
        if closed_clicked then
            bShowAddingMesh = false
        end
    end
end

function updateVisibilityByFilter()
    for i=1, #tAllMesh do
        local tObj = tAllMesh[i]
        if filter(tObj) then
            tObj.visible = true
        else
            tObj.visible = false
            tObj.tShape.visible = false
        end
    end
end

function showMeshList()
    if bShowMeshList then
        local width        = 300
        local tPosWin      = {x = 0, y = 0}
        tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_meshes,tPosWin.x,tPosWin.y,width,width + 50)
        local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_meshes, true, ImGuiWindowFlags_NoMove)
        if is_opened then
            local flags  = 0
            if tImGui.TreeNodeEx(string.format("Filter",#tAllMesh),flags,'##FilterMesh') then
                local bAnyChange = false
                tImGui.Text('World')
                local ret, current_item, item = tImGui.Combo('##WorldMeshFilter' , tOptionsEditor.iIndexWorldMesh, tFilter.tWorld)
                if ret then
                    tOptionsEditor.iIndexWorldMesh = current_item
                    bAnyChange = true
                end
                tImGui.Text('Mesh Type')
                local ret, current_item, item = tImGui.Combo('##TypeMeshFilter' , tOptionsEditor.iIndexTypeMeshFilter, tFilter.tType)
                if ret then
                    tOptionsEditor.iIndexTypeMeshFilter = current_item
                    bAnyChange = true
                end

                tImGui.Text('Mesh Physics')
                local ret, current_item, item = tImGui.Combo('##TypePhysicsFilter' , tOptionsEditor.iIndexTypePhysicsFilter, tFilter.tPhysicType)
                if ret then
                    tOptionsEditor.iIndexTypePhysicsFilter = current_item
                    bAnyChange = true
                end

                if bAnyChange then
                    updateVisibilityByFilter()
                end
                tImGui.TreePop()
            end
            if tImGui.TreeNodeEx(string.format("Objects (%d/%d)",iFiltered,#tAllMesh),flags,'##allMeshes') then
                iFiltered = 0
                local iNodeClicked = 0
                for i=1, #tAllMesh do
                    local tObj = tAllMesh[i]
                    if filter(tObj) then
                        iFiltered = iFiltered + 1
                        local flags_selected  = 0
                        if tObj.isSelected then
                            flags_selected = ImGuiTreeNodeFlags_Selected
                        end
                        if tObj.isBlocked then
                            local idx     = tImGui.Flags('ImGuiCol_Text')
                            local color   = {r=1,g=0,b=0.3,a=1}
                            tImGui.PushStyleColor(idx, color)
                        end
                        local node_open =  tImGui.TreeNodeEx(string.format("%s (%d)",tObj.type,i), flags_selected)
                        if (keyControlPressed or keyShiftPressed) and tImGui.IsItemClicked() then
                            iNodeClicked = i
                        end
                        if tObj.isBlocked then
                            tImGui.PopStyleColor(1)
                        end
                        if node_open then
                            showPropertiesForMesh(tObj)
                            tImGui.TreePop()
                        end
                    else
                        setSelectedObj(tObj,false)
                    end
                end
                if iNodeClicked > 0 then
                    local tObj = tAllMesh[iNodeClicked]
                    if keyControlPressed then
                        if tObj.isBlocked then
                            tUtil.showMessageWarn('Object is Blocked!\nCan not be selected!')
                        else
                            if tObj.isSelected then
                                tFollowCam = nil
                                iLastNodeSelected = 0
                                setSelectedObj(tObj,false)
                            else
                                tFollowCam = tObj
                                setSelectedObj(tObj,true)
                                iLastNodeSelected = iNodeClicked
                            end
                        end
                    elseif keyShiftPressed then
                        local sText = ''
                        local bAnyBlocked = false
                        if iLastNodeSelected > 0 then
                            if iNodeClicked > iLastNodeSelected then
                                for i = iLastNodeSelected, iNodeClicked do
                                    local tObj = tAllMesh[i]
                                    if tObj.isBlocked then
                                        bAnyBlocked = true
                                        sText = sText .. tostring(i) .. ', '
                                    else
                                        setSelectedObj(tObj,true)
                                        tFollowCam = tObj
                                    end
                                end
                            else
                                for i = iNodeClicked, iLastNodeSelected do
                                    local tObj = tAllMesh[i]
                                    if tObj.isBlocked then
                                        bAnyBlocked = true
                                        sText = sText .. tostring(i) .. ', '
                                    else
                                        setSelectedObj(tObj,true)
                                        tFollowCam = tObj
                                    end
                                end
                            end
                        else
                            tFollowCam = tObj
                            setSelectedObj(tObj,true)
                            iLastNodeSelected = iNodeClicked
                        end

                        if bAnyBlocked then
                            tUtil.showMessageWarn('Object(s):\n' .. sText .. '\nare Blocked!\nCan not be selected!')
                        end
                    end
                end
                tImGui.TreePop()
            else
                iFiltered = 0
            end
        end
        tWindowsArea:addThisWindow()
        tImGui.End()
        if closed_clicked then
            bShowMeshList = false
        end
    end
end

function onNewSceneEditor()
    if tSelectedObjs == nil then
        tSelectedObjs = {}
    end
    if tAllMesh == nil then
        tAllMesh = {}
    end
    onSelectAll()
    onDeleteSelected()
    tOptionsEditor = {
        iIndexResolution        = 1,
        bInvertResolution       = false,
        bDrawResolution         = true,
        fSceneCamPos            = {x = 0, y = 0},
        tColorBackground        = {r = tUtil.tColorBackground.r, g = tUtil.tColorBackground.g, b = tUtil.tColorBackground.b},
        iIndexWorldMesh         = 1,
        iIndexTypeMeshFilter    = 1,
        iIndexTypePhysicsFilter = 1,
        bAddObjAs2dw            = true,
        bCenterOfScreen         = false,
        initialDisplacement     = {x=0,y=0},
        tIncrementOnNewMesh     = {x = true,y = false},
        tDirectionIncrementOnNewMesh= {bRight = true,bUp = true},
        sScaleAxis              = 'y',
        sCurrentScriptExecution = '',
        sExtraScript            = ''
    }

    tOptionsLaunch = {
        iIndexResolution      = 1,
        bInvertResolution     = false,
    }
    sLastMeshAdd            = ''
    tSelectedObjs           = {}
    bShowMeshList           = false
    tLastMeshAdded          = nil
    tFollowCam              = nil
    sLastEditorFileName     = ''
    keyControlPressed       = false
    keyShiftPressed         = false
    tAllMesh                = {}
end

function setSelectedObj(tObj,bValue)
    if bValue then
        if not tObj.isSelected then
            tObj.bJustSelected = true
            tObj.tShape:setTexture(sTextureShapeSelected)
        else
            tObj.bJustSelected = false
        end
        tObj.isSelected              = true
        tObj.tShape.visible          = true

        local bAlreadyInTheList = false

        for i=1, #tSelectedObjs do
            local that = tSelectedObjs[i]
            if that == tObj then
                bAlreadyInTheList = true
                break
            end
        end
        if bAlreadyInTheList == false then
            table.insert(tSelectedObjs,tObj)
        end
    else
        tObj.isSelected     = false
        tObj.tShape.visible = false

        for i=1, #tSelectedObjs do
            local that = tSelectedObjs[i]
            if that == tObj then
                table.remove(tSelectedObjs,i)
                break
            end
        end
    end
end

function onTouchDown(key,x,y)
    if cCoroutineLoadScene then return end
    local anyObj           = false
    local anyWindowHovered = tWindowsArea:IsAnyWindowHovered(x,y)
    isClickedMouseLeft     = key == 1 and not anyWindowHovered
    camera2d.mx            = x
    camera2d.my            = y
    bEnableMoveWorld       = true
    bClickedOverAnyMesh    = false
    bMovingAnyMesh         = false
    tFollowCam             = nil
    if isClickedMouseLeft then
        local clickedX, clickedY = mbm.to2dw(x,y)
        for i =1, #tAllMesh do
            local tObj = tAllMesh[i]
            if not tObj.isBlocked and tObj.visible and tObj:isOver(x,y) then
                if keyControlPressed then
                    setSelectedObj(tObj,true)
                else
                    onUnSelectAll()
                    setSelectedObj(tObj,true)
                end
                bEnableMoveWorld             = false
                bClickedOverAnyMesh          = true
            end
        end
        --update positions to selected objects
        for i=1, #tSelectedObjs do
            local tObj      = tSelectedObjs[i]
            tObj.originx    = tObj.x
            tObj.originy    = tObj.y
            tObj.clickedX   = clickedX
            tObj.clickedY   = clickedY
        end
    end
end

function onTouchMove(key,x,y)
    if cCoroutineLoadScene then return end
    if bEnableMoveWorld then
        if isClickedMouseLeft then
            local px = (camera2d.mx - x) * camera2d.sx
            local py = (camera2d.my - y) * camera2d.sy
            camera2d.mx = x
            camera2d.my = y
            camera2d:setPos(camera2d.x + px,camera2d.y - py)
        end
    elseif key == 1 then
        local mx,my = mbm.to2dw(x,y)
        if bClickedOverAnyMesh then
            bMovingAnyMesh = true
            for i =1, #tSelectedObjs do
                local tObj = tSelectedObjs[i]
                v1:set(tObj.originx,tObj.originy)
                v1:sub(tObj.clickedX,tObj.clickedY)
                v1:add(mx,my)
                if tObj.isBlockedX then
                    v1.x = tObj.x
                end
                if tObj.isBlockedY then
                    v1.y = tObj.y
                end
                tObj:setPos(v1.x,v1.y)
            end
        elseif not tWindowsArea:IsAnyWindowHovered(x,y) then
            for i =1, #tAllMesh do
                local tObj = tAllMesh[i]
                if not tObj.isBlocked and tObj.visible and tObj:isOver(x,y) == true then
                    tObj.tShape:setTexture(sTextureShapeOver)
                    tObj.tShape.visible = true
                elseif tObj.isSelected then
                    tObj.tShape:setTexture(sTextureShapeSelected)
                    tObj.tShape.visible = true
                else
                    tObj.tShape.visible = false
                end
            end
        end
    end
end

function onTouchUp(key,x,y)
    if cCoroutineLoadScene then return end
    if key == 1 and isClickedMouseLeft and bEnableMoveWorld == false then
        if not bMovingAnyMesh and bClickedOverAnyMesh then
            for i=1, #tSelectedObjs do
                local tObj = tSelectedObjs[i]
                if not tObj.bJustSelected and tObj:isOver(x,y) then
                    setSelectedObj(tObj,false)
                    break
                end
            end
        end
    end
    isClickedMouseLeft  = false
    bEnableMoveWorld    = false
    bClickedOverAnyMesh = false
    camera2d.mx         = x
    camera2d.my         = y
end

function onTouchZoom(zoom)
    if cCoroutineLoadScene then return end
end

function onKeyDown(key)
    if cCoroutineLoadScene then return end
    if key == mbm.getKeyCode('control') then
        keyControlPressed = true
    elseif key == mbm.getKeyCode('shift') then
        keyShiftPressed = true
    elseif keyControlPressed then
        if key == mbm.getKeyCode('S') then -- Ctrl+S
            onSaveSceneEditor()
        elseif key == mbm.getKeyCode('O') then -- Ctrl+O
            onLoadScene()
        elseif key == mbm.getKeyCode('N') then -- Ctrl+N
            onNewSceneEditor()
        elseif key == mbm.getKeyCode('I') then -- Ctrl+I
            onInvertSelection()
        elseif key == mbm.getKeyCode('A') then -- Ctrl+A
            onSelectAll()
        elseif key == mbm.getKeyCode('M') then -- Ctrl+M
            onAddMesh()
        elseif key == mbm.getKeyCode('B') then -- Ctrl+B
            onSaveSpriteBinary()
        elseif key == mbm.getKeyCode('C') then -- Ctrl+C
            if #tSelectedObjs == 1 then
                tLastMeshAdded = tSelectedObjs[1]
            end
        elseif key == mbm.getKeyCode('D') or key == mbm.getKeyCode('V') then -- Ctrl+D
            onDuplicated()
        elseif key == mbm.getKeyCode('U') then -- Ctrl+U
            onUnSelectAll()
        elseif key == mbm.getKeyCode('R') then -- Ctrl+R
            tOptionsEditor.tDirectionIncrementOnNewMesh.bRight = not tOptionsEditor.tDirectionIncrementOnNewMesh.bRight
            if tOptionsEditor.tDirectionIncrementOnNewMesh.bRight then
                tUtil.showMessage('Duplicate Mesh to the Right on X ')
            else
                tUtil.showMessage('Duplicate Mesh to the Left on X ')
            end
        elseif key == mbm.getKeyCode('delete') then -- Delete
            onDeleteSelected()
        elseif key == mbm.getKeyCode('L') then -- Ctrl+L
            bShowMeshList = true
        end
    elseif key == mbm.getKeyCode('F5') then
        onPlay()
    elseif key == mbm.getKeyCode('esc') or mbm.getKeyName(key) == 'ESCAPE' then
        onUnSelectAll()
    end
end


function onKeyUp(key)
    if cCoroutineLoadScene then return end
    if key == mbm.getKeyCode('control') then
        keyControlPressed = false
    elseif key == mbm.getKeyCode('shift') then
        keyShiftPressed = false
    end
end

function onNewScene()
    onSelectAll()
    onDeleteSelected()
    camera2d.x              = 0
    camera2d.y              = 0
    bShowMeshList           = false
    tLastMeshAdded          = nil
    tFollowCam              = nil
    sLastEditorFileName     = ''
    bShowAddingMesh         = false
    bShowDetailOfMesh       = true
end

function onSaveUserData(name,value,tOut)
    
end

function getHeader(fileName)
    local sHeader = '' .. "--[[\n" .. [[
    Scene 2d - this file is meant to be used in the engine mbm
    More info at: https://mbm-documentation.readthedocs.io/en/latest/
	Scene Editor: https://mbm-documentation.readthedocs.io/en/latest/editors.html#scene-editor-2d

	how to:
	* in your scene do:

		tScene = require "SCENE_NAME"

		tScene:load()

	* To retrieve mesh(s) use get or getAll:

		local tMesh      = tScene:get('mesh_name.msh')
		local tLife      = tScene:get('life.spt')
		local tAllCircle = tScene:getAll('mario.png')

	* To load new mesh(s)(same information as used in the editor, position, scale, rotation, etc) use:

        local tNewLife   = tScene:addMesh('mike.spt')

    * Index (from the table tAllMeshInfo) might be used instead of fileName to retrieve a specific mesh:

        local tMesh      = tScene:get(2)
        local tNewLife   = tScene:addMesh(3)

]] .. "]]\n\n"
    sHeader = sHeader:gsub('SCENE_NAME',tUtil.getShortName(fileName,false))
    sHeader = sHeader:gsub('%.lua','')
    return sHeader
end

function getSceneLoaderCode(xCam,yCam,sScaleAxis)
    
    local sScene = [[
tScene.updateCamera = (
    function (self) 
        local camera2d = mbm.getCamera('2d')
        camera2d:setPos(POS_CAM_X,POS_CAM_Y)
        camera2d:scaleToScreen(self.iExpectedWidth,self.iExpectedHeight,"AXIS_SCALE_CAM")
        self.iSizeScreenWidth, self.iSizeScreenHeight  = mbm.getSizeScreen()
        self.scale_cam_x = camera2d.sx
        self.scale_cam_y = camera2d.sy
    end
    )

tScene.load = (
    function(self,onProgress)
        local bEnableCoroutine = type(onProgress) == 'function'
        self:updateCamera()
        local iYieldForEach      = 60
        local iByYield,stepYield = 0,0
        if bEnableCoroutine then 
            if #self.tAllMeshInfo > iYieldForEach then
                iByYield = math.ceil(#self.tAllMeshInfo / iYieldForEach)
            end
        end
        for i=1, #self.tAllMeshInfo do
            local tInfo    = self.tAllMeshInfo[i]
            local tMeshTmp = self:_addMesh(tInfo)
            if bEnableCoroutine then
                stepYield = stepYield + 1
                if stepYield >= iByYield then
                    stepYield = 0
                    if onProgress and type(onProgress) == 'function' then
                        onProgress( i / #self.tAllMeshInfo * 100.0)
                    end
                    coroutine.yield(#self.tMeshesLoaded,#self.tAllMeshInfo)
                end
            end
        end
        if self.tPhysics then
            self.tPhysics:start()
        end
    end
    )

tScene._addMesh = (
    function(self,tInfo)
        local tMeshTmp = nil
        local sWorld = '2dw'
        if tInfo.is2ds then
            sWorld = '2ds'
        end
        local fileName = tInfo.fileName
        if tInfo.type == 'mesh' then
            tMeshTmp = mesh:new(sWorld)
            if not tMeshTmp:load(fileName) then
                print("failed to load mesh:["..fileName.."]")
                return nil
            end
        elseif tInfo.type == 'sprite' then
            tMeshTmp = sprite:new(sWorld)
            if not tMeshTmp:load(fileName) then
                print("failed to load sprite:["..fileName.."]")
                return nil
            end
        elseif tInfo.type == 'particle' then
            tMeshTmp = particle:new(sWorld)
            if not tMeshTmp:load(fileName) then
                print("failed to load particle:["..fileName.."]")
                return nil
            end
        elseif tInfo.type == 'tile' then
            tMeshTmp = tile:new(sWorld)
            if not tMeshTmp:load(fileName) then
                print("failed to load tile:["..fileName.."]")
                return nil
            end
        elseif tInfo.type == 'texture' then
            tMeshTmp = texture:new(sWorld)
            if not tMeshTmp:load(fileName) then
                print("failed to load texture:["..fileName.."]")
                return nil
            end
        elseif tInfo.type == 'gif' then
            tMeshTmp = gif:new(sWorld)
            if not tMeshTmp:load(fileName) then
                print("failed to load gif:["..fileName.."]")
                return nil
            end
        elseif tInfo.type == 'font' then
            local tMyFonts = self.tMyFonts
            if tMyFonts and tMyFonts[fileName] then
                local tFont = tMyFonts[fileName]
                tMeshTmp = tFont:add(tInfo.sText or "My text",sWorld)
                tMeshTmp.tFont = tFont
            else
                local heightFont 	  = tInfo.heightFont or 50
                local spaceFont	      = tInfo.spaceFont  or 5
                local spaceHeightFont = tInfo.spaceHeightFont or 5
                local tFont           = font:new(fileName,heightFont,spaceFont,spaceHeightFont)
                if not tFont then
                    print("failed to load font:["..fileName.."]")
                    return nil
                end
                tMeshTmp = tFont:add(tInfo.sText or "My text",sWorld)
                tMeshTmp.tFont = tFont
                if self.tMyFonts == nil then
                    self.tMyFonts = {}
                end
                self.tMyFonts[fileName] = tFont
            end
        else
            print("Only mesh 2d:["..fileName.."] \nMesh type:"..(tInfo.type or "nil"))
            return nil
        end
        tMeshTmp:setScale(tInfo.sx,tInfo.sy,tInfo.sz)
        tMeshTmp:setAngle(tInfo.ax,tInfo.ay,tInfo.az)

        if tInfo.is2ds then
            local ew_half  = self.iExpectedWidth  * 0.5
            local eh_half  = self.iExpectedHeight * 0.5
            if tInfo.isRelative2ds then
                local x
                local y
                local w,h          = tMeshTmp:getSize()
                local w_half_mesh  = w * 0.5
                local h_half_mesh  = h * 0.5
                if tInfo.x == 0 then
                    x = ew_half
                elseif tInfo.x > 0 then
                    local xDiff = ew_half - (tInfo.x + w_half_mesh)
                    x = self.iSizeScreenWidth - xDiff - w_half_mesh
                else
                    local xDiff = (ew_half + tInfo.x - w_half_mesh)
                    x = (xDiff + w_half_mesh)
                end

                if tInfo.y == 0 then
                    y = eh_half
                elseif tInfo.y > 0 then
                    local yDiff = eh_half - (tInfo.y + h_half_mesh)
                    y = yDiff + h_half_mesh
                else
                    local yDiff = eh_half + (tInfo.y - h_half_mesh)
                    y = self.iSizeScreenHeight - (yDiff + h_half_mesh)
                end

                tMeshTmp:setPos(x,y,tInfo.z)
            else
                local x    =  ((tInfo.x + ew_half) / self.iExpectedWidth  * self.iSizeScreenWidth )
                local y    =  self.iSizeScreenHeight - (((tInfo.y + eh_half) / self.iExpectedHeight * self.iSizeScreenHeight))
                tMeshTmp:setPos(x,y,tInfo.z)
            end
        else
            tMeshTmp:setPos(tInfo.x,tInfo.y,tInfo.z)
        end
        
        if tInfo.tPhysicInfo then
            self:addPhysics(tMeshTmp,tInfo.tPhysicInfo)
        end
        if tInfo.iAnim and tInfo.iAnim > 1 then
            tMeshTmp:setAnim(tInfo.iAnim)
        end
        table.insert(self.tMeshesLoaded,tMeshTmp)
        return tMeshTmp
    end
    )
    
tScene.addMesh = (
    function(self,fileNameOrIndex)
        
        if type(fileNameOrIndex) == 'number' then
            if fileNameOrIndex >= 1 and fileNameOrIndex <= #self.tAllMeshInfo then
                local tInfo = self.tAllMeshInfo[fileNameOrIndex]
                return self:_addMesh(tInfo)
            end
        else
            local indexByName = self.tAllMeshInfo[fileNameOrIndex]
            if indexByName then
                local tInfo = self.tAllMeshInfo[indexByName]
                return self:_addMesh(tInfo)
            end
            for i=1, #self.tAllMeshInfo do
                local tInfo = self.tAllMeshInfo[i]
                if tInfo.fileName == fileNameOrIndex then
                    self.tAllMeshInfo[fileNameOrIndex] = i
                    return self:_addMesh(tInfo)
                end
            end
            return nil
        end
    end
    )

tScene.add = tScene.addMesh
    
tScene.getAll = (--return a table for all already loaded meshes
    function(self,fileName)
        local tAll = {}
        for i=1, #self.tAllMeshInfo do
            local tInfo = self.tAllMeshInfo[i]
            if tInfo.fileName == fileName or select(1,tInfo.fileName:find(fileName)) then
                local tMesh = self.tMeshesLoaded[i]
                table.insert(tAll,tMesh)
            end
        end
        return tAll
    end
    )
    
tScene.get = (
    function(self,fileNameOrIndex)--get already loaded
        if type(fileNameOrIndex) == 'number' then
            if fileNameOrIndex >= 1 and fileNameOrIndex <= #self.tMeshesLoaded then
                return self.tMeshesLoaded[fileNameOrIndex]
            end
        else
            local tMesh = self.tMeshesLoadedDictionary[fileNameOrIndex]
            if tMesh then
                return tMesh
            end
            for i=1, #self.tAllMeshInfo do
                local tInfo = self.tAllMeshInfo[i]
                if tInfo.fileName == fileNameOrIndex or select(1,tInfo.fileName:find(fileNameOrIndex)) then
                    local tMesh = self.tMeshesLoaded[i]
                    self.tMeshesLoadedDictionary[fileNameOrIndex] = tMesh
                    return tMesh
                end
            end
        end
        return nil
    end
    )
    EXTRA_SCRIPT
return tScene
]]

    sScene = sScene:gsub('POS_CAM_X',tostring(xCam))
    sScene = sScene:gsub('POS_CAM_Y',tostring(yCam))
    sScene = sScene:gsub('AXIS_SCALE_CAM',sScaleAxis)
    if tOptionsEditor.sExtraScript and tOptionsEditor.sExtraScript:len() > 0 then
        sScene = sScene:gsub('EXTRA_SCRIPT',string.format('\nif not mbm.include(%q) then\n    print("error on execute script",%q)\nend\n',tOptionsEditor.sExtraScript,tOptionsEditor.sExtraScript))
    else
        sScene = sScene:gsub('EXTRA_SCRIPT','\n')
    end
    return sScene
end

function getPhysicsFunction()

    local sPhysics = [[

tScene.addPhysics = function (self,tObj,tPhysicInfo)

    if self.tPhysics == nil then
        local gravity_x           = 0
        local gravity_y           = -90.8
        local scale_box_2d        = 10
        local velocityIterations  = 10
        local positionIterations  = 3
        local multiplyStep        = 1
        self.tPhysics = box2d:new(gravity_x,gravity_y,scale_box_2d,velocityIterations,positionIterations,multiplyStep)
    end

    if tPhysicInfo.type == 'static' then
        self.tPhysics:addStaticBody(tObj,tPhysicInfo.density,tPhysicInfo.friction,tPhysicInfo.scaleX,tPhysicInfo.scaleY,tPhysicInfo.sensor)
    elseif tPhysicInfo.type == 'dynamic' then
        self.tPhysics:addDynamicBody(tObj,tPhysicInfo.density,tPhysicInfo.friction,tPhysicInfo.restitution,tPhysicInfo.scaleX,tPhysicInfo.scaleY,tPhysicInfo.sensor,tPhysicInfo.bullet)
    elseif tPhysicInfo.type == 'kinematic' then
        self.tPhysics:addKinematicBody(tObj,tPhysicInfo.density,tPhysicInfo.friction,tPhysicInfo.restitution,tPhysicInfo.scaleX,tPhysicInfo.scaleY,tPhysicInfo.sensor,tPhysicInfo.bullet)
    elseif tPhysicInfo.type == 'character' then
        self.tPhysics:addDynamicBody(tObj,tPhysicInfo.density,tPhysicInfo.friction,tPhysicInfo.restitution,tPhysicInfo.scaleX,tPhysicInfo.scaleY,tPhysicInfo.sensor,tPhysicInfo.bullet)
        self.tPhysics:setFixedRotation(tObj,true)
        self.tPhysics:setSleepingAllowed(tObj,false)
    else
        print('error','Not found type of physic:' .. tostring(tPhysicInfo.type))
    end
end

]]
    return sPhysics
end

function getPhysicInfo4Save(tPhysicInfo)
    if tPhysicInfo then
        local sRet = {}
        if tPhysicInfo.type then
            table.insert(sRet,string.format('type=%q',tPhysicInfo.type))
        end
        if tPhysicInfo.density then
            table.insert(sRet,string.format('density=%g',tPhysicInfo.density))
        end

        if tPhysicInfo.friction then
            table.insert(sRet,string.format('friction=%g',tPhysicInfo.friction))
        end

        if tPhysicInfo.restitution then
            table.insert(sRet,string.format('restitution=%g',tPhysicInfo.restitution))
        end

        if tPhysicInfo.scaleX then
            table.insert(sRet,string.format('scaleX=%g',tPhysicInfo.scaleX))
        end

        if tPhysicInfo.scaleY then
            table.insert(sRet,string.format('scaleY=%g',tPhysicInfo.scaleY))
        end

        if type(tPhysicInfo.sensor) == 'boolean' then
            table.insert(sRet,string.format('sensor=%s',tostring(tPhysicInfo.sensor)))
        end

        if type(tPhysicInfo.bullet) == 'boolean' then
            table.insert(sRet,string.format('bullet=%s',tostring(tPhysicInfo.bullet)))
        end
        return ',tPhysicInfo={' .. table.concat(sRet,',') .. '}'
    else
        return ''
    end
end

function getText4Font4Save(sText)
    if sText then
        return ',sText=[[' .. sText .. ']]'
    end
    return ''
end

function getIndependentCalCam4Save(tObj)
    if tObj.is2ds and tObj.isRelative2ds then
        return ',isRelative2ds=true'
    else
        return ''
    end
end

function getBlockFlag4Save(tObj)
    if tObj.isBlocked then
        return ',isBlocked=true'
    else
        return ''
    end
end


function getIs2ds4Save(tObj)
    if tObj.is2ds then
        return ',is2ds=true'
    else
        return ''
    end
end

function onSaveScene(sFileName)

    local fp = io.open(sFileName,"w")
    if fp then
        local tOptionsOut = {}

        fp:write(getHeader(sFileName))

        local bHasPhysics = false
        for i=1, #tAllMesh do
            local tObj = tAllMesh[i]
            if type(tObj.tPhysicInfo) == 'table' then
                bHasPhysics = true
                break
            end
        end

        if bHasPhysics then
            fp:write('require "box2d"\n\n')
        end

        fp:write('local tScene = {}\n')


        if bHasPhysics then
            fp:write(getPhysicsFunction())
        end

        fp:write('tScene.getOptionsEditor = function()\n    local')
        tUtil.save('tOptionsEditor',tOptionsEditor,  tOptionsOut,  onSaveUserData)
        
        for i=1, #tOptionsOut do
            local sLine = tOptionsOut[i]
            fp:write('    ' ..  sLine .. '\n')
        end

        fp:write('    return tOptionsEditor\nend\n\n')

        tOptionsOut = {}
        fp:write('tScene.getOptionsLaunch = function()\n    local')
        tUtil.save('tOptionsLaunch',tOptionsLaunch,  tOptionsOut,  onSaveUserData)

        for i=1, #tOptionsOut do
            local sLine = tOptionsOut[i]
            fp:write('    ' ..  sLine .. '\n')
        end

        fp:write('    return tOptionsLaunch\nend\n\n')

        local tPaths = mbm.getAllPaths()
        for i=1, #tPaths do
            fp:write(string.format('mbm.addPath(%q)\n', tPaths[i]))
        end

        local xRes, yRes
        if tOptionsEditor.bInvertResolution then
            xRes = tResolution[tOptionsEditor.iIndexResolution].y
            yRes = tResolution[tOptionsEditor.iIndexResolution].x
        else
            xRes = tResolution[tOptionsEditor.iIndexResolution].x
            yRes = tResolution[tOptionsEditor.iIndexResolution].y
        end

        fp:write(string.format('\ntScene.iExpectedWidth   = %d',xRes))
        fp:write(string.format('\ntScene.iExpectedHeight  = %d\n',yRes))

        fp:write('\ntScene.tAllMeshInfo = {')
        for i=1, #tAllMesh do
            local tObj = tAllMesh[i]
            fp:write(string.format('\n[%d]={fileName = %s,x=%g,y=%g,z=%g,sx=%g,sy=%g,sz=%g,ax=%g,ay=%g,az=%g,type=%q,iAnim=%d%s%s%s%s%s},',
            i,tUtil.getShortName(tObj.fileName,true),
            tObj.x,tObj.y,tObj.z,
            tObj.sx,tObj.sy,tObj.sz,
            tObj.ax,tObj.ay,tObj.az,
            tObj.type,
            select(2,tObj:getAnim()),
            getIs2ds4Save(tObj),
            getPhysicInfo4Save(tObj.tPhysicInfo),
            getBlockFlag4Save(tObj),
            getText4Font4Save(tObj.sText),
            getIndependentCalCam4Save(tObj)))
        end
        fp:write('}\n\n')

        fp:write('tScene.tMeshesLoaded = {}\n')
        fp:write('tScene.tMeshesLoadedDictionary = {}\n\n')
        fp:write(getSceneLoaderCode(tOptionsEditor.fSceneCamPos.x,tOptionsEditor.fSceneCamPos.y,tOptionsEditor.sScaleAxis))
        fp:close()
        return true
    else
        print('error',string.format('Could not open the file [%s] for write',sFileName))
        tUtil.showMessageWarn(string.format('Could not open the file [%s] for write',sFileName))
        return false
    end
end

function onSetScript()
    local fileName = mbm.openFile(tOptionsEditor.sExtraScript,"*.lua")
    if fileName then
        dofile(fileName)
        tUtil.showMessage('Script executed!')
        tOptionsEditor.sExtraScript = fileName
    end
end

function onLoadScene()
    local fileName = mbm.openFile(sLastEditorFileName,"*.lua")
    if fileName then
        onNewScene()
        local tScene = dofile(fileName)
        if tScene and type(tScene.getOptionsEditor) == 'function' then
            tOptionsEditor = tScene:getOptionsEditor()
            mbm.setColor(tOptionsEditor.tColorBackground.r,tOptionsEditor.tColorBackground.g,tOptionsEditor.tColorBackground.b)
            updateRectangleLine()
        else
            tUtil.showMessageWarn('Not found getOptionsEditor function!')
        end
        if tScene and type(tScene.getOptionsLaunch) == 'function' then
            tOptionsLaunch = tScene:getOptionsLaunch()
        else
            tUtil.showMessageWarn('Not found getOptionsLaunch function!')
        end
        if tScene and type(tScene.tAllMeshInfo) == 'table' then
            cCoroutineLoadScene = coroutine.create(
            function()
                local iYieldForEach            = 60
                local bOldOption               = tOptionsEditor.bCenterOfScreen
                tOptionsEditor.bCenterOfScreen = true
                local iByYield,stepYield       = 0,0
                if #tScene.tAllMeshInfo > iYieldForEach then
                    iByYield = math.ceil(#tScene.tAllMeshInfo / iYieldForEach)
                end
                for i=1, #tScene.tAllMeshInfo do
                    local tInfo = tScene.tAllMeshInfo[i]
                    local tMeshTmp = tUtil.onAddMeshToEditor(tInfo.fileName,false,'2dw',tInfo.sText)
                    if tMeshTmp then
                        if tInfo.iAnim and tInfo.iAnim > 1 then
                            tMeshTmp:setAnim(tInfo.iAnim)
                        end
                        tMeshTmp.tPhysicInfo = tInfo.tPhysicInfo
                        initialSetUpForAddedMesh(tMeshTmp)
                        tMeshTmp:setScale(tInfo.sx,tInfo.sy,tInfo.sz)
                        tMeshTmp:setAngle(tInfo.ax,tInfo.ay,tInfo.az)
                        tMeshTmp:setPos(tInfo.x,tInfo.y,tInfo.z)
                        tMeshTmp.is2ds          = tInfo.is2ds
                        tMeshTmp.isRelative2ds  = tInfo.isRelative2ds
                        tMeshTmp.isBlocked      = tInfo.isBlocked
                    end
                    stepYield = stepYield + 1
                    if stepYield >= iByYield then
                        stepYield = 0
                        coroutine.yield(i,#tScene.tAllMeshInfo)
                    end
                end
                tOptionsEditor.bCenterOfScreen = bOldOption
                sLastEditorFileName = fileName
                updateVisibilityByFilter()
            end)
        else
            tUtil.showMessageWarn('Not found tScene or table!')
        end
    end
end

function onSaveSceneEditor()
    if sLastEditorFileName:len() == 0 then
        local fileName = mbm.saveFile(sLastEditorFileName,'*.lua')
        if fileName then
            if onSaveScene(fileName) then
                sLastEditorFileName = fileName
                tUtil.showMessage('Scene \n' .. sLastEditorFileName .. '\nSaved Successfully!!')
            else
                tUtil.showMessageWarn('Failed to Save Scene!')
            end
        end
    else
        if onSaveScene(sLastEditorFileName) then
            tUtil.showMessage('Scene \n' .. sLastEditorFileName .. '\nSaved Successfully!!')
        else
            tUtil.showMessageWarn('Failed to Save Scene!')
        end
    end
end

function onPlay()
    local width, height
    if tOptionsLaunch.bInvertResolution then
        width  = tResolution[tOptionsLaunch.iIndexResolution].y
        height = tResolution[tOptionsLaunch.iIndexResolution].x
    else
        width  = tResolution[tOptionsLaunch.iIndexResolution].x
        height = tResolution[tOptionsLaunch.iIndexResolution].y
    end

    local expected_width, expected_height
    if tOptionsEditor.bInvertResolution then
        expected_width  = tResolution[tOptionsEditor.iIndexResolution].y
        expected_height = tResolution[tOptionsEditor.iIndexResolution].x
    else
        expected_width  = tResolution[tOptionsEditor.iIndexResolution].x
        expected_height = tResolution[tOptionsEditor.iIndexResolution].y
    end

    if tOptionsEditor.sCurrentScriptExecution and tOptionsEditor.sCurrentScriptExecution:len() > 0 then
        tUtil.newInstance(width, height, expected_width, expected_height, tOptionsEditor.sCurrentScriptExecution)
    else
        tUtil.newInstance(width, height, expected_width, expected_height, sLastEditorFileName)
    end
end

function main_menu_scene_editor_2d()
   if (tImGui.BeginMainMenuBar()) then
       if tImGui.BeginMenu("File") then
           
           local pressed,checked = tImGui.MenuItem("New Scene", "Ctrl+N", false)
           if pressed then
               onNewScene()
           end

           local pressed,checked = tImGui.MenuItem("Load Scene", "Ctrl+O", false)
           if pressed then
               onLoadScene()
           end

           tImGui.Separator()
           local pressed,checked = tImGui.MenuItem("Set Extra Script ", 'Extension', false)
           if pressed then
               onSetScript()
           end
           tImGui.SameLine()
           tImGui.HelpMarker('This script will be part of scene (even when loading). \nThis is useful when for example we have some extra shaders to specific mesh which is not part of the engine!\n\nThe script will be executed when load the scene!\n\nCurrent:' .. tostring(tOptionsEditor.sExtraScript))

           tImGui.Separator()
           local pressed,checked = tImGui.MenuItem("Save Scene", "Ctrl+S", false)
           if pressed then
               onSaveSceneEditor()
           end

           if sLastEditorFileName and sLastEditorFileName:len() > 0 then
                tImGui.SameLine()
                tImGui.HelpMarker(sLastEditorFileName)
           end

           local pressed,checked = tImGui.MenuItem("Save Scene As ...", nil, false)
           if pressed then
                local sEditorFileName = sLastEditorFileName
                sLastEditorFileName =  ''
                onSaveSceneEditor()
                if sLastEditorFileName == '' then
                    sLastEditorFileName = sEditorFileName
                end
           end

           tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("Quit", "Alt+F4", false)
            if pressed then
                mbm.quit()
            end
            
           tWindowsArea:addThisWindow()
           tImGui.EndMenu();
       end
        if tImGui.BeginMenu("Mesh") then
            local pressed,checked = tImGui.MenuItem("Add Mesh", 'Ctrl+M', false)
            if pressed then
               onAddMesh()
            end
            tImGui.Separator()
            local title_duplicated = 'Duplicate Last Mesh Added'
            if #tSelectedObjs > 0 then
                title_duplicated = 'Duplicate All Mesh Selected'
            end
            local pressed,checked = tImGui.MenuItem(title_duplicated, 'Ctrl+D', false)
            if pressed then
               onDuplicated()
            end
            local pressed,checked = tImGui.MenuItem('Select All Mesh (Considers filter)', 'Ctrl+A', false)
            if pressed then
                onSelectAll()
            end

            local pressed,checked = tImGui.MenuItem('Invert Selected Mesh (Considers filter)', 'Ctrl+I', false)
            if pressed then
                onInvertSelection()
            end

            local pressed,checked = tImGui.MenuItem('Unselect All Mesh', 'Ctrl+U', false)
            if pressed then
               onUnSelectAll()
            end

            local pressed,checked = tImGui.MenuItem('Delete Selected Mesh', 'Ctrl+Delete', false)
            if pressed then
                onDeleteSelected()
            end
            
            tImGui.Separator()
           
            local pressed,checked = tImGui.MenuItem("View Mesh List", 'Ctrl+L', false)
            if pressed then
               bShowMeshList  = true
            end

            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("View Options When Adding Mesh", nil, false)
            if pressed then
                bShowAddingMesh  = true
            end

            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("View Detail Of Selected Mesh", true, bShowDetailOfMesh)
            if pressed then
                bShowDetailOfMesh  = checked
            end

            tWindowsArea:addThisWindow()
           tImGui.EndMenu()
        end

        if tImGui.BeginMenu("World") then
            tImGui.Text('Resolution Expected')
            tOptionsEditor.bInvertResolution = tImGui.Checkbox('Invert Width / Height',tOptionsEditor.bInvertResolution)
            local tResolutionString = {}
            for i=1 , #tResolution do
                if tOptionsEditor.bInvertResolution then
                    table.insert(tResolutionString,string.format('%d x %d %s',tResolution[i].y,tResolution[i].x, tResolution[i].comment))
                else
                    table.insert(tResolutionString,string.format('%d x %d %s',tResolution[i].x,tResolution[i].y, tResolution[i].comment))
                end
            end
            local ret, current_item, item = tImGui.Combo('##ComboResolution' , tOptionsEditor.iIndexResolution, tResolutionString)
            if ret then
                tOptionsEditor.iIndexResolution = current_item
            end

            tImGui.Text('Axis of camera scale')
            local indexAxis
            if tOptionsEditor.sScaleAxis == 'x' then
                indexAxis  = 1
            elseif tOptionsEditor.sScaleAxis == 'y' then
                indexAxis  = 2
            else
                indexAxis  = 3
            end

            local index_activated = tImGui.RadioButton('X', indexAxis, 1)
            tImGui.SameLine()
            index_activated       = tImGui.RadioButton('Y', index_activated, 2)
            tImGui.SameLine()
            index_activated       = tImGui.RadioButton('Stretched', index_activated, 3)
            tImGui.SameLine()
            tImGui.HelpMarker('Default value is Y. See "scaleToScreen"')

            if index_activated == 1 then
                tOptionsEditor.sScaleAxis = 'x'
            elseif index_activated == 2 then
                tOptionsEditor.sScaleAxis = 'y'
            else
                tOptionsEditor.sScaleAxis = 'xy'
            end

            tOptionsEditor.bDrawResolution = tImGui.Checkbox('Draw Resolution Rectangle',tOptionsEditor.bDrawResolution)
            updateRectangleLine()
            
            tImGui.Separator()
            tImGui.Text('Camera Position')

            local step       =  1.0
            local step_fast  =  10.0
            local format     = "%.2f"
            local flags      =  0

            local result, fValue = tImGui.InputFloat('X##XCamera', camera2d.x, step, step_fast, format, flags)
            if result then
                camera2d.x = fValue
            end

            local result, fValue = tImGui.InputFloat('Y##YCamera', camera2d.y, step, step_fast, format, flags)
            if result then
                camera2d.y = fValue
            end

            if tImGui.Button('Set Initial Camera Position', {x=-1,y=0}) then
                tOptionsEditor.fSceneCamPos.x = camera2d.x
                tOptionsEditor.fSceneCamPos.y = camera2d.y
            end

            tImGui.Text('Initial Scene Position')
            tImGui.TextDisabled(string.format('X:%.2f',tOptionsEditor.fSceneCamPos.x))
            tImGui.TextDisabled(string.format('Y:%.2f',tOptionsEditor.fSceneCamPos.y))

            tWindowsArea:addThisWindow()
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu("Options") then
            local pressed,checked = tImGui.MenuItem("Move Windows", true, bEnableMoveWindow)
            if pressed then
               bEnableMoveWindow = checked
               if bEnableMoveWindow then
                   ImGuiWindowFlags_NoMove = 0
               else
                   ImGuiWindowFlags_NoMove = tImGui.Flags('ImGuiWindowFlags_NoMove')
               end
            end

            local pressed,checked = tImGui.MenuItem("Show Alpha Pattern", true, tex_alpha_pattern.visible)
            if pressed then
               tex_alpha_pattern.visible = checked
            end

            tImGui.Separator()
            if tImGui.BeginMenu("Background Color") then
                local sz        = tImGui.GetTextLineHeight()

                local rounding  =  0
                local flags     =  0

                local colors    = { {'Default',    tUtil.tColorBackground},
                                        {'White',      {r=1,g=1,b=1,a=1}},
                                        {'Black',      {r=0,g=0,b=0,a=1}},
                                        {'Red',        {r=1,g=0,b=0,a=1}},
                                        {'Green',      {r=0,g=1,b=0,a=1}},
                                        {'Blue',       {r=0,g=0,b=1,a=1}},
                                        {'Cyan',       {r=0,g=1,b=1,a=1}},
                                        {'Yellow',     {r=1,g=1,b=0,a=1}},
                                        {'Magenta',    {r=1,g=0,b=1,a=1}}
                                    }

                for i=1, #colors do
                    local winPos  = tImGui.GetCursorScreenPos()
                    local p_max   = {x=winPos.x + sz,y=winPos.y + sz}
                    local name    = colors[i][1]
                    local color   = colors[i][2]
                    tImGui.AddRectFilled(winPos, p_max, color, rounding, flags)
                    tImGui.Dummy({x =sz, y = sz})
                    tImGui.SameLine()
                    local pressed,checked = tImGui.MenuItem(name)
                    if pressed then
                        mbm.setColor(color.r,color.g,color.b)
                        tOptionsEditor.tColorBackground = color
                    end
                end
                tImGui.EndMenu()
            end
            tWindowsArea:addThisWindow()
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu("Run") then
            tImGui.Text('Resolution')
            tOptionsLaunch.bInvertResolution = tImGui.Checkbox('Invert Width / Height',tOptionsLaunch.bInvertResolution)
            local tResolutionString = {}
            for i=1 , #tResolution do
                if tOptionsLaunch.bInvertResolution then
                    table.insert(tResolutionString,string.format('%d x %d %s',tResolution[i].y,tResolution[i].x, tResolution[i].comment))
                else
                    table.insert(tResolutionString,string.format('%d x %d %s',tResolution[i].x,tResolution[i].y, tResolution[i].comment))
                end
            end
            local ret, current_item, item = tImGui.Combo('##ComboResolution' , tOptionsLaunch.iIndexResolution, tResolutionString)
            if ret then
                tOptionsLaunch.iIndexResolution = current_item
            end

            if tImGui.Button('Play', {x=200,y=0}) then
                onPlay()
            end
            tImGui.SameLine()
            tImGui.TextDisabled('F5')

            tImGui.Text('Execute this script')
            tImGui.SameLine()
            tImGui.HelpMarker('Will execute the script instead of the current scene!\nThis is a way to test the logic of your scene \n(loading from the script)')
            if tImGui.Button('...', {x=30,y=0}) then
                local fileName = mbm.openFile(tOptionsEditor.sCurrentScriptExecution,"*.lua")
                if fileName then
                    tOptionsEditor.sCurrentScriptExecution = fileName
                end
            end

            if tOptionsEditor.sCurrentScriptExecution:len() == 0 then
                tImGui.SameLine()
                if tImGui.Button('Create it for me', {x=160,y=0}) then
                    if tOptionsEditor.sCurrentScriptExecution and tOptionsEditor.sCurrentScriptExecution:len() > 0 then
                        createBasicScriptForScene(tOptionsEditor.sCurrentScriptExecution)
                    elseif sLastEditorFileName and sLastEditorFileName:len() > 0 then
                        createBasicScriptForScene(sLastEditorFileName)
                    else
                        tUtil.showMessageWarn('There is no scene loaded to create a script!')
                    end
                end
            end

            if tOptionsEditor.sCurrentScriptExecution and tOptionsEditor.sCurrentScriptExecution:len() > 0 then
                tImGui.TextDisabled(tUtil.getShortName(tOptionsEditor.sCurrentScriptExecution))
                tImGui.SameLine()
                tImGui.HelpMarker(tOptionsEditor.sCurrentScriptExecution)
                tImGui.SameLine()
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), {r=1,g=0,b=0.3,a=1})
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r=0,g=0,b=0.3,a=0})
                if tImGui.Button('X', {x=40,y=0}) then
                    tOptionsEditor.sCurrentScriptExecution = ''
                end
                tImGui.PopStyleColor(2)
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text('Clear the script')
                    tImGui.EndTooltip()
                end

            end
            tImGui.EndMenu()
            tWindowsArea:addThisWindow()
        end

        if tImGui.BeginMenu("About") then
            local pressed,checked = tImGui.MenuItem(string.format("Scene Editor 2D"), nil, false)
            if pressed then
                if mbm.is('windows') then
                   os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/editors.html#scene-editor-2d"')
                elseif mbm.is('linux') then
                   os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/editors.html#scene-editor-2d"')
                end
            end
            local pressed,checked = tImGui.MenuItem("Mbm Engine", nil, false)
            if pressed then
                if mbm.is('windows') then
                   os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/"')
                elseif mbm.is('linux') then
                   os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/"')
                end
            end

            if tImGui.BeginMenu("Version") then
                tImGui.TextDisabled(string.format('%s\nIMGUI: %s', mbm.get('version'),tImGui.GetVersion()))
                tImGui.EndMenu()
            end

            tImGui.EndMenu()
            tWindowsArea:addThisWindow()
        end
        tImGui.EndMainMenuBar()
    end
end

function updateRectangleLine()
    local xRes, yRes
    if tOptionsEditor.bInvertResolution then
        xRes = tResolution[tOptionsEditor.iIndexResolution].y
        yRes = tResolution[tOptionsEditor.iIndexResolution].x
    else
        xRes = tResolution[tOptionsEditor.iIndexResolution].x
        yRes = tResolution[tOptionsEditor.iIndexResolution].y
    end
    local tRectangleScreen = {-xRes/2,-yRes/2, -xRes/2,yRes/2, xRes/2,yRes/2, xRes/2,-yRes/2, -xRes/2,-yRes/2  }
    tLineScreen2d:set(tRectangleScreen,1)
end

function cameraFollowing()
    if tFollowCam then
        local iSpeedCam = 1000
        local bAnyFollow = false
        local iW,iH = mbm.getSizeScreen()
        local w,h = tFollowCam:getSize()
        if (w > iW or h > iH) and tFollowCam:isOnScreen() then
            tFollowCam = nil
            return
        end
        if camera2d.x + (iW * 0.5)  < (tFollowCam.x + w * 0.5) then
            camera2d:move(iSpeedCam,0)
            bAnyFollow = true
        elseif camera2d.x - (iW * 0.5)  > (tFollowCam.x - w * 0.5) then
            camera2d:move(-iSpeedCam,0)
            bAnyFollow = true
        end
        if camera2d.y + (iH * 0.5)  < (tFollowCam.y + h * 0.5) then
            camera2d:move(0,iSpeedCam)
            bAnyFollow = true
        elseif camera2d.y - (iH * 0.5)  > (tFollowCam.y - h * 0.5) then
            camera2d:move(0,-iSpeedCam)
            bAnyFollow = true
        end

        if bAnyFollow == false and w < (iW * 0.15) and  h < (iH * 0.15) then
            camera2d.iIteration = camera2d.iIteration + 1
            iSpeedCam = 200
            if camera2d.x + (iW * 0.15)  < (tFollowCam.x + w * 0.5) then
                camera2d:move(iSpeedCam,0)
                bAnyFollow = true
            elseif camera2d.x - (iW * 0.15)  > (tFollowCam.x - w * 0.5) then
                camera2d:move(-iSpeedCam,0)
                bAnyFollow = true
            end
            if camera2d.y + (iH * 0.15)  < (tFollowCam.y + h * 0.5) then
                camera2d:move(0,iSpeedCam)
                bAnyFollow = true
            elseif camera2d.y - (iH * 0.15)  > (tFollowCam.y - h * 0.5) then
                camera2d:move(0,-iSpeedCam)
                bAnyFollow = true
            end
            if camera2d.iIteration > 120 then --2 seconds
                camera2d.iIteration = 0
                bAnyFollow = nil
            end
        end
    end
end

function createBasicScriptForScene(sFullSceneName)

    local tDefaultScene = [[
tScene = require "YOUR_SCENE"

local tLogicScene = {}

tLogicScene.onProgress = function(self,percent)
    print(string.format('Loading your scene %.1f',percent))
end

tLogicScene.onInitScene = function(self)
    camera2d    = mbm.getCamera("2d")
    camera2d.mx = 0
    camera2d.my = 0
    mbm.setColor(1,1,1)
    tScene:load(onProgress)
    bEnableMoveCamera  = true
    isClickedMouseLeft = false
    
    --tScene:get() -- get any mesh to do something
end

tLogicScene.onTouchDown = function(self,key,x,y)
    isClickedMouseLeft = key == 1
    camera2d.mx = x
    camera2d.my = y
end

tLogicScene.onTouchMove = function(self,key,x,y)
    if isClickedMouseLeft and bEnableMoveCamera then
        local px = (camera2d.mx - x) * camera2d.sx
        local py = (camera2d.my - y) * camera2d.sy
        camera2d.mx = x
        camera2d.my = y
        camera2d:setPos(camera2d.x + px,camera2d.y - py)
    end
end

tLogicScene.onTouchUp = function(self,key,x,y)
    isClickedMouseLeft = false
    camera2d.mx = x
    camera2d.my = y
end

tLogicScene.loop = function(self,delta)
    -- your logic here
end

return tLogicScene
]]

    local sProjectName = sFullSceneName:gsub("\\",'/')
    local tProjectName = sProjectName:split('/')
    sProjectName       = tProjectName[#tProjectName]:gsub('%..*$','')
    tDefaultScene      = tDefaultScene:gsub('YOUR_SCENE',string.format('%s', sProjectName))
    local sTemp        = tProjectName[1] or ''
    local sSeparator   = '/'
    if mbm.is('windows') then
        sSeparator = '\\'
    end
    for i=2, (#tProjectName - 1) do
        sTemp = sTemp .. sSeparator .. tProjectName[i]
    end

    local sFileToSave = string.format('%s%s%s-logic.lua',sTemp, sSeparator, sProjectName)

    sFileToSave = mbm.saveFile(sFileToSave,'lua')
    if sFileToSave then
        local fp = io.open(sFileToSave,"w")
        if fp then
            fp:write(tDefaultScene)
            fp:close()
            tOptionsEditor.sCurrentScriptExecution = sFileToSave
            tUtil.showMessage('File created successfully!')
        else
            print('error',string.format('Could not open the file [%s] for write',sFileToSave))
            tUtil.showMessageWarn(string.format('Could not open the file [%s] for write',sFileToSave))
        end
    end
end

function showDetailOfMesh()
    if bShowDetailOfMesh and #tSelectedObjs == 1 then
        local tObj  = tSelectedObjs[1]
        local flags = { 'ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoSavedSettings', 'ImGuiWindowFlags_NoFocusOnAppearing'}
        tImGui.SetNextWindowBgAlpha(0.75);
        local window_pos     = {x = 300, y = 25}
        local window_pos_pivot = {x = 0, y = 0}
        tImGui.SetNextWindowPos(window_pos, tImGui.Flags('ImGuiCond_Once'), window_pos_pivot);
        local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_mesh_info, true,tImGui.Flags(flags) )
        if is_opened then
            local sWorld = '2d World'
            if tObj.is2ds then
                sWorld = '2d Screen'
            end
            tImGui.Text(string.format('Type:%s (%s)(%d)',tObj.type, sWorld,tObj.iIndex))
            tImGui.Text(string.format('File:%s',tUtil.getShortName(tObj.fileName,false)))
            tImGui.Text(string.format('Position: X:%g Y:%g Z:%g',tObj.x,tObj.y,tObj.z))
            tImGui.Text(string.format('Scale   : X:%g Y:%g Z:%g',tObj.sx,tObj.sy,tObj.sz))
            tImGui.Text(string.format('Angle   : X:%g Y:%g Z:%g',tObj.ax,tObj.ay,tObj.az))

            if tObj.sText then
                tImGui.Text('Text')
                tImGui.Text(tObj.sText)
            end
            if tObj.tPhysicInfo then
                local tPhysicInfo = tObj.tPhysicInfo
                tImGui.Text('Physics:')
                if tPhysicInfo.type then
                    tImGui.Text(string.format('Type (%s)',tPhysicInfo.type))
                end
                if tPhysicInfo.density then
                    tImGui.Text(string.format('Density (%g)',tPhysicInfo.density))
                end
        
                if tPhysicInfo.friction then
                    tImGui.Text(string.format('Friction (%g)',tPhysicInfo.friction))
                end
        
                if tPhysicInfo.restitution then
                    tImGui.Text(string.format('Restitution (%g)',tPhysicInfo.restitution))
                end
        
                if tPhysicInfo.scaleX then
                    tImGui.Text(string.format('Scale X:%g Y:%g',tPhysicInfo.scaleX,tPhysicInfo.scaleY))
                end
        
                if type(tPhysicInfo.sensor) == 'boolean' then
                    tImGui.Text(string.format('Sensor <%s>',tostring(tPhysicInfo.sensor)))
                end
        
                if type(tPhysicInfo.bullet) == 'boolean' then
                    tImGui.Text(string.format('Bullet  <%s>',tostring(tPhysicInfo.bullet)))
                end
            end
        end
        if closed_clicked then
            bShowDetailOfMesh = false
        end
        tWindowsArea:addThisWindow()
        tImGui.End()
    end
end

function onProgress(fPercent)
    local flags             = {'ImGuiWindowFlags_NoDecoration', 'ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoSavedSettings', 'ImGuiWindowFlags_NoFocusOnAppearing', 'ImGuiWindowFlags_NoNav'}
    local iW, iH            = mbm.getRealSizeScreen()
    local window_pos        = {x = 0, y = 0}
    local window_pos_pivot  = {x = 0, y = 0}
    tImGui.SetNextWindowBgAlpha(0.85);
    tImGui.SetNextWindowPos(window_pos, tImGui.Flags('ImGuiCond_Always'), window_pos_pivot);
    tImGui.SetNextWindowSize({x = iW, y = iH},tImGui.Flags('ImGuiCond_Always'))
    local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_loading, false,tImGui.Flags(flags) )
    if is_opened then
        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_FrameBg') , {r=0,g=0,b=0.5,a=0.7})
        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_PlotHistogram'), {r=0,g=1,b=0,a=1})
        local size_arg  = {x = -1, y = 0}
        local pos       = {x = 0, y = (iH * 0.5)}
        local overlay   =  string.format('Loading %.1f',fPercent)
        tImGui.SetCursorScreenPos(pos)
        tImGui.ProgressBar(fPercent * 0.01, size_arg, '')
        tImGui.PopStyleColor(2)
        tImGui.SetCursorScreenPos({x = iW * 0.48, y = (iH * 0.5) + 3})
        tImGui.Text(overlay)
    end
    tImGui.End()
end

--[[
width_camera = 1024 
height_camera = 768
function showSetScaleCam()
    local width        = 300
    local tPosWin      = {x = 0, y = 0}
    tUtil.setInitialWindowPositionLeft('Camera adjust',tPosWin.x,tPosWin.y,width,width + 50)
    local is_opened, closed_clicked = tImGui.Begin('Camera adjust', true,0 )
    if is_opened then
        tImGui.Text(string.format('Camera  scale %f %f ',camera2d.sx,camera2d.sy))

        local step       =  1
        local step_fast  =  10
        local flags      =  0

        local result, iValue = tImGui.InputInt('Width', width_camera, step, step_fast, flags)
        if result then
            width_camera = iValue
        end

        local result, iValue = tImGui.InputInt('Height', height_camera, step, step_fast, flags)
        if result then
            height_camera = iValue
        end

        camera2d:scaleToScreen(width_camera,height_camera,'y')
    end
    tImGui.End()
end
]]--

function loop(delta)
    
    if cCoroutineLoadScene then
        local sStatus = coroutine.status (cCoroutineLoadScene)
        if sStatus == 'suspended' or  sStatus == 'normal' then
            local bRet, iCurrent, iTotal = coroutine.resume(cCoroutineLoadScene)
            if  bRet == false then --error
                print('error',iCurrent)
                tUtil.showMessageWarn(tostring(iCurrent))
            else
                if iCurrent and iTotal then -- end of routine
                    iTotal = iTotal + 1 --avoid call 100% twice
                    onProgress(iCurrent / iTotal * 100)
                end
            end
        elseif sStatus == 'dead' then
            cCoroutineLoadScene = nil
            local tSplash = mbm.getSplash()
            if tSplash then
                tSplash.visible = false
            end
            mbm.setFakeFps(120,60)
            onProgress(100)
        end
    else
        tWindowsArea = tUtil.onNewAnyWindowsHovered()
        main_menu_scene_editor_2d()
        for i=1, #tAllMesh do
            local tObj  = tAllMesh[i]
            tObj.iIndex = i
        end
        tex_alpha_pattern:setPos(camera2d.x,camera2d.y)
        tLineScreen2d.visible = tOptionsEditor.bDrawResolution

        --showSetScaleCam()
        showMeshList()
        showAddingMeshOptions()
        showDetailOfMesh()
        
        tUtil.showOverlayMessage()

        cameraFollowing()
    end
end
