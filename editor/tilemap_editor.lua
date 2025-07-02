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

   Tile Map Editor

   This is a script based on mbm engine.

   Tile Map Editor is a 2D level editor meant to help you develop the content of your game by little bricks.

   More info at: https://mbm-documentation.readthedocs.io/en/latest/editors.html#tilemap-editor

]]--

tImGui        =     require "ImGui"
tUtil         =     require "editor_utils"
tTile         =     require "tilemap"

if not mbm.get('USE_EDITOR_FEATURES') then
	mbm.messageBox('Missing features','Is necessary to compile using USE_EDITOR FEATURES to run this editor','ok','error',0)
	mbm.quit()
end

function onInitScene()
    
    tWindowsTitle        = {title_image_selector    = 'Image(s) selector', 
                            title_tile_map          = 'Tile Map Options',
                            title_layer_brick_option= 'Brick selector'}

    camera2d		     = mbm.getCamera("2d")
    tLineCenterX         = line:new("2dw",0,0,50)
    tLineCenterY         = line:new("2dw",0,0,50)
    tLineCenterX:add({-9999999,0, 9999999,0})
    tLineCenterY:add({0,-9999999, 0,9999999})
    tLineCenterX:setColor(1,0,0)
    tLineCenterY:setColor(0,1,0)
    bEnableMainTabBar           = true
    ImGuiWindowFlags_NoMove     = tImGui.Flags('ImGuiWindowFlags_NoMove')
    sFileNameTile               = ''
    tUtil.bRightSide            = true
    tUtil.sMessageOverlay       = 'Welcome to Tile Map Editor!'
    tTextureTileSet             = {}
    ImGuiTabItemFlags           = {}
    tPropertyTypes              = {'Text','Number', 'Boolean'}
    tPropertyMap                = {sText = '', bValue = false, fValue = 0, iSelectedCombo = 1, name = 'id-01'}
    tPropertyLayer              = {sText = '', bValue = false, fValue = 0, iSelectedCombo = 1, name = 'id-01'}
    tPropertyTile               = {sText = '', bValue = false, fValue = 0, iSelectedCombo = 1, name = 'id-01'}
    tPropertyBrick              = {sText = '', bValue = false, fValue = 0, iSelectedCombo = 1, name = 'id-01'}

    tEditorOptions               = {sDefaultTileSetName = 'tileset-1', 
                                    iIndexDrawTileSet = 1,
                                    iIndexDrawLayer   = 1,
                                    iIndexDrawBrick   = 1,
                                    iIndexSelectedBrickMenuLayer = 0,
                                    iBrickIdSelected       = 0,
                                    iDefaultTileSetWidth   = 256,
                                    iDefaultTileSetHeight  = 256,
                                    iWidth                 = 0,
                                    iHeight                = 0,
                                    iDefaultTileSetSpaceX  = 0,
                                    iDefaultTileSetSpaceY  = 0,
                                    iDefaultTileSetMarginX = 0,
                                    iDefaultTileSetMarginY = 0,
                                    iFineAdjust            = 100,
                                    iTileSetFilterBrick    = 1,
                                    iLayerFilterBrick      = 1,
                                    iExpandValue           = 0.0001,
                                    iSelectedTileSetPreview= 1,
                                    iBlockToMoveLayer      = 1,
                                    tSubTilesToImport      = {xStart = 0,yStart = 0,xEnd = 1024,yEnd = 768},
                                    bMakeSubTiles          = false,
                                    iSizeBrickOnSelector   = 70,
                                    }

    tAnimTypes    = {'PAUSED','GROWING','GROWING_LOOP', 'DECREASING', 'DECREASING_LOOP', 'RECURSIVE', 'RECURSIVE_LOOP'}
    tComboMapType = {'ORTHOGONAL', 'ISOMETRIC'}
    tComboMapTypeIndex = {ORTHOGONAL = 1, ISOMETRIC = 2}
    tCircleEditVuBrick = shape:new('2dw')
    tCircleEditVuBrick:create('circle',20)
    tCircleEditVuBrick.z = -100
    iClickedAndSelectedBrick = false
    tInitialBrickAdjustUv = {}
    tClicked = {x = 0, y = 0}
    tOverBrick = {x = 0, y = 0}
    tToolsBrickLayerSize = {x = 0, y = 0}
    sRenderWhat = ''
    bShowBrickBelows = true
    sLastRenderWhat = ''
    tPhysicsType =  {none = 1, rectangle = 2, circle = 3, triangle = 4 }
    tPhysicsCombo = {'none', 'rectangle' , 'circle' , 'triangle' }
    tObjectCombo  = {'none', 'rectangle' , 'circle' , 'triangle','line', 'point' }
    tComboType    =  {none = 1, rectangle = 2, circle = 3, triangle = 4, line = 5, point = 6 }
    tLinePhysics = {}
    tMapObjects   = {}
    tLineCenterX.visible = false
    tLineCenterY.visible = false
    bObjectsVisible = true
    bAddHistoryFlag = false
    keyControlPressed = false
    keyTabPressed = true
    keyShiftPressed = false
    isClickedMouseLeft = false
    colorStatusHistoric         = {r = 0.234, g = 0.863, b = 0.195}
    sFilterTextureNameTileSet = ''
    tTimeShowingStatusHistoric = timer:new(
        function(self)
            self:stop()
        end,
        0.5
    )

    tTimeToSave = timer:new(
        function(self)
            bAddHistoryFlag = true
            colorStatusHistoric = {r = 0.234, g = 0.863, b = 0.195}
            tTimeShowingStatusHistoric:stop()
            tTimeShowingStatusHistoric:start()
            self:stop()
        end,
        2 -- seconds to save historic
    )

    tTimeShowingStatusHistoric:stop()
    tTimeToSave:stop()

    tVec2Aux = vec2:new()
    tLineSelection = line:new('2ds')
    tLineSelection:add({0,0,0,0})
    tLineSelection.xStart = 0
    tLineSelection.yStart = 0
    tLineSelection:setColor(0,1,1)

    tLineRectTile = line:new('2dw',0,0,-12)
    tLineRectTile:add({0,0,0,0})
    tLineRectTile:setColor(0,1,1)
    tLineRectTile.visible = false
end

function onOpenTileBinary()
    local sFileName = mbm.openFile(sFileNameTile,'*.tile')
    if sFileName then
        if tTile:load(sFileName) then
            sFileNameTile = sFileName
            tUtil.showMessage('Tile Map Loaded Successfully!')
            tTile:clearHistory()
            tTile:addHistoric()
            ImGuiTabItemFlags = {map = tImGui.Flags('ImGuiTabItemFlags_SetSelected') }
        else
            tUtil.showMessageWarn('Failed to Load Tile Map File!')
        end
    end
end


function onSaveAsTileBinary()
    local sFileName = mbm.saveFile(sFileNameTile,'*.tile')
    if sFileName then
        if tTile:save(sFileName) then
            sFileNameTile = sFileName
            tUtil.showMessage('Tile Map Saved Successfully!')
        else
            tUtil.showMessageWarn('Failed to Save Tile Map File!')
        end
    end
end

function onSaveTileBinary()
    if sFileNameTile and sFileNameTile:len() > 0 then
        if tTile:save(sFileNameTile) then
            tUtil.showMessage('Tile Map Saved Successfully!')
        else
            tUtil.showMessageWarn('Failed to Save Tile Map File!')
        end
    else
        onSaveAsTileBinary()
    end
end

function formatIdProperty(tProperty)
    if tProperty.type == 'Number' then
        return string.format('##%s - Number:%g',tProperty.name, tProperty.value)
    elseif tProperty.type == 'Text' then
        return string.format('##%s - Text:%s',tProperty.name, tProperty.value)
    elseif tProperty.type == 'Boolean' then
        return string.format('##%s - Boolean:%s',tProperty.name, tostring(tProperty.value))
    else
        return string.format('##%s - Unknown:%s',tProperty.name, tProperty.value)
    end
end

function propertiesTreeView(tProperties,sTreeId,item_width,tPropertyOptions,fOnAddProperty)
    local flags = 0
    if tImGui.TreeNodeEx("Properties",flags,sTreeId) then
        tImGui.PushItemWidth(item_width - 20)
        tImGui.Text('New Property')
        local label            = '##' .. sTreeId .. 'Combo'
        local height_in_items  =  -1

        local ret, current_item, item_as_string = tImGui.Combo(label, tPropertyOptions.iSelectedCombo, tPropertyTypes, height_in_items)
        if ret then
            tPropertyOptions.iSelectedCombo = current_item
        end

        tImGui.Text('Name')
        local label            = '##' .. sTreeId .. 'InputTextPropertyName'
        local modified , sNewText = tImGui.InputText(label,tPropertyOptions.name,flags)
        if modified then
            tPropertyOptions.name = sNewText
        end
        
        tImGui.Text('Value')
        local sTextAdd
        if tPropertyOptions.iSelectedCombo == 1 then
            local label            = '##' .. sTreeId .. 'InputText'
            local modified , sNewText = tImGui.InputText(label,tPropertyOptions.sText,flags)
            if modified then
                tPropertyOptions.sText = sNewText
            end
            sTextAdd = 'Add Text'
        elseif tPropertyOptions.iSelectedCombo == 2 then
            local label      = '##' .. sTreeId .. 'InputFloat'
            local step       =  1.0
            local step_fast  =  10.0
            local format     = "%.3f"

            local result, fValue = tImGui.InputFloat(label, tPropertyOptions.fValue, step, step_fast, format, flags)
            if result then
                tPropertyOptions.fValue = fValue
            end
            sTextAdd = 'Add Number'
        else
            tPropertyOptions.bValue = tImGui.Checkbox('Value', tPropertyOptions.bValue)
            sTextAdd = 'Add Boolean'
        end

        if tImGui.Button(sTextAdd,{x=item_width - 20 ,y=0}) then
            if tPropertyOptions.name:len() == 0 then
                tUtil.showMessageWarn('Property Name Is Empty!')
            else
                local bFound = false
                for i=1, #tProperties do
                    local tProperty = tProperties[i]
                    if tProperty.name == tPropertyOptions.name then
                        bFound = true
                        break
                    end
                end
                if bFound == false then
                    fOnAddProperty(tPropertyOptions)
                else
                    tUtil.showMessageWarn(string.format('Property [%s] exists!',tPropertyOptions.name))
                end
            end
        end

        if #tProperties > 0 then
            tImGui.Separator()
        end

        tImGui.PushItemWidth(item_width - 40)
        for i=1, #tProperties do
            local tProperty = tProperties[i]
            local sNewId    = sTreeId .. tostring(i) .. 'property'
            if tImGui.TreeNodeEx(tProperty.name,flags,sNewId) then
                tImGui.Text('Type:' ..  tProperty.type)

                if tProperty.type == 'Text' then
                    local label            = '##' .. sNewId .. 'InputTextEdit'
                    local modified , sNewText = tImGui.InputText(label,tProperty.value,flags)
                    if modified then
                        tProperty.value = sNewText
                        tProperty.isUpdate = true
                        fOnAddProperty(tProperty)
                    end
                elseif tProperty.type == 'Number' then
                    local label      = '##' .. sNewId .. 'InputFloatEdit'
                    local step       =  1.0
                    local step_fast  =  10.0
                    local format     = "%.3f"

                    local result, fValue = tImGui.InputFloat(label, tProperty.value, step, step_fast, format, flags)
                    if result then
                        tProperty.value = fValue
                        tProperty.isUpdate = true
                        fOnAddProperty(tProperty)
                    end
                    
                elseif tProperty.type == 'Boolean' then
                    local bValue = tImGui.Checkbox('Value', tProperty.value)
                    if tProperty.value ~= bValue then
                        tProperty.value = bValue
                        tProperty.isUpdate = true
                        fOnAddProperty(tProperty)
                    end
                end
                tImGui.TreePop()
            end
        end
        tImGui.PopItemWidth()
        tImGui.PopItemWidth()
        tImGui.TreePop()
    end
end

function addHistoric()
    tTimeToSave:stop()
    tTimeToSave:start()
end

function drawLayerTab(item_width)
    local flags = 0
    updatePhysicsLine({})
    tEditorOptions.iIndexDrawLayer = tTile:setRenderMode('layer',tEditorOptions.iIndexDrawLayer)
    
    if tImGui.Button('New Layer', {x=item_width,y=0}) then
        tTile:newLayer()
        addHistoric()
    end
    local iTotalLayer = tTile:getTotalLayer()
    for i=1, iTotalLayer do
        local tLayer = tTile:getLayer(i)
        local id  =  'Layer-' .. tostring(i)

        local flag_selected_node = 0
        if tEditorOptions.iIndexDrawLayer == i then
            flag_selected_node = tImGui.Flags('ImGuiTreeNodeFlags_Selected')
        end
        
        if tImGui.TreeNodeEx(id,flag_selected_node,id) then
            if tImGui.IsItemHovered(0) then
                tEditorOptions.iIndexDrawLayer = tTile:setRenderMode('layer',i)
            end
            if i > 1 then
                tImGui.Text('Bring Layer Up')
                tImGui.SameLine()
                tImGui.SetCursorPosX(item_width-10)
                if tImGui.ArrowButton('Layer UP',tImGui.Flags('ImGuiDir_Up')) then
                    tTile:moveLayerUp(i)
                    tImGui.TreePop()
                    addHistoric()
                    break
                end
            end
            if i < iTotalLayer then
                tImGui.Text('Bring Layer Down')
                tImGui.SameLine()
                tImGui.SetCursorPosX(item_width-10)
                if tImGui.ArrowButton('Layer Down',tImGui.Flags('ImGuiDir_Down')) then
                    tTile:moveLayerDown(i)
                    tImGui.TreePop()
                    addHistoric()
                    break
                end
            end
            tImGui.PushItemWidth(item_width - 20)
            local bVisible = tImGui.Checkbox('Visible',tLayer.visible)
            if tLayer.visible ~= bVisible then
                tLayer.visible = bVisible
                if tTile:updateLayer(i,tLayer) == false then
                    tUtil.showMessageWarn('Could not update the layer!')
                end
            end

            tImGui.Text('OffSet (X)')
            local step       =  1
            local step_fast  =  10
            local result, iValue = tImGui.InputInt('##LayerOffSetX'.. id, tLayer.offset.x, step, step_fast, flags)
            if result and iValue > -999999999 and iValue < 999999999 then
                tLayer.offset.x = iValue
                if tTile:updateLayer(i,tLayer) == false then
                    tUtil.showMessageWarn('Could not update offset on the layer!')
                else
                    addHistoric()
                end
            end

            tImGui.Text('OffSet (Y)')
            local result, iValue = tImGui.InputInt('##LayerOffSetY' .. id , tLayer.offset.y, step, step_fast, flags)
            if result and iValue > -999999999 and iValue < 999999999 then
                tLayer.offset.y = iValue
                if tTile:updateLayer(i,tLayer) == false then
                    tUtil.showMessageWarn('Could not update offset in the layer!')
                else
                    addHistoric()
                end
            end

            local sShaderName = tTile:getNameShaderLayer(i)
            if  sShaderName == 'tint.ps' then
                if tImGui.TreeNodeEx('Tint options',flag_selected_node,id .. '-tint') then

                    tImGui.PushItemWidth(item_width - 40)
                    tImGui.Text('Min Tint Color')
                    local clicked, tRgba = tImGui.ColorEdit3('##LayerMinTint' .. id, tLayer.minTint, flags)
                    if clicked then
                        tLayer.minTint = tRgba
                        if tTile:updateLayer(i,tLayer) == false then
                            tUtil.showMessageWarn('Could not update min tint in the layer!')
                        else
                            addHistoric()
                        end
                    end

                    tImGui.Text('Max Tint Color')
                    local clicked, tRgba = tImGui.ColorEdit3('##LayerMaxTint' .. id, tLayer.maxTint, flags)
                    if clicked then
                        tLayer.maxTint = tRgba
                        if tTile:updateLayer(i,tLayer) == false then
                            tUtil.showMessageWarn('Could not update max tint in the layer!')
                        else
                            addHistoric()
                        end
                    end

                    tImGui.Text('Type Animation')
                    local ret, current_item, item = tImGui.Combo('##ComboAnimPSTintLayer' .. id , tLayer.tintAnimType, tAnimTypes)
                    if ret then
                        tLayer.tintAnimType = current_item
                        if tTile:updateLayer(i,tLayer) == false then
                            tUtil.showMessageWarn('Could not update type animation tint in the layer!')
                        else
                            addHistoric()
                        end
                    end

                    local step       =  0.05
                    local step_fast  =  0.1
                    local format     = "%.3f"

                    tImGui.Text('Time Animation')
                    local result, fValue = tImGui.InputFloat('##tintTimeLayer-'.. id, tLayer.tintAnimTime, step, step_fast, format, flags)
                    if result and fValue >=0 then
                        tLayer.tintAnimTime = fValue
                        if tTile:updateLayer(i,tLayer) == false then
                            tUtil.showMessageWarn('Could not update time animation tint in the layer!')
                        else
                            addHistoric()
                        end
                    end

                    if tImGui.Button('Restart animation', {x=item_width - 40,y=0}) then
                        tTile:updateLayer(i,tLayer)
                    end

                    tImGui.PopItemWidth()
                    tImGui.TreePop()
                end
            else
                tImGui.TextDisabled('Shader [' .. sShaderName .. ']')
                tImGui.TextDisabled('not supported')
                tImGui.TextDisabled('by this editor')
                tImGui.TextDisabled('Use shader editor')
                tImGui.TextDisabled('to modify it!')
            end

            tImGui.Separator()

            local tProperties = tTile:getLayerProperties(i)
            propertiesTreeView(tProperties,'##LayerProperty' .. id,item_width-25,tPropertyLayer,
                function (tProperty)
                    if tProperty.isUpdate then
                        tTile:setLayerProperty(i,tProperty)
                    elseif tPropertyTypes[tProperty.iSelectedCombo] == 'Text' then
                        tTile:setLayerProperty(i, {name = tProperty.name, value = tProperty.sText})
                    elseif tPropertyTypes[tProperty.iSelectedCombo] == 'Number' then
                        tTile:setLayerProperty(i, {name = tProperty.name, value = tProperty.fValue})
                    elseif tPropertyTypes[tProperty.iSelectedCombo] == 'Boolean' then
                        tTile:setLayerProperty(i, {name = tProperty.name, value = tProperty.bValue})
                    else
                        tUtil.showMessageWarn('Invalid Property Selected')
                    end
                    addHistoric()
                end
            )

            tImGui.Separator()
            tImGui.TextDisabled('Delete Layer')
            tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), {r=1,g=0,b=0.3,a=1})
            tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r=0,g=0,b=0.3,a=0})
            tImGui.SameLine()
            tImGui.SetCursorPosX(item_width - 25)
            if tImGui.Button('X', {x=30,y=0}) then
                tTile:deleteLayer(i)
                addHistoric()

                tImGui.PopItemWidth()
                tImGui.PopStyleColor(2)
                tImGui.Separator()
                tImGui.TreePop()
                break
            end
            tImGui.PopItemWidth()
            tImGui.PopStyleColor(2)
            tImGui.Separator()
            tImGui.TreePop()

        else
            if tImGui.IsItemHovered(0) then
                tEditorOptions.iIndexDrawLayer = tTile:setRenderMode('layer',i)
            end
        end
    end

    if tTile:getTotalLayer() > 0 then
        bShowBrickBelows = tImGui.Checkbox('Brick selector separated',bShowBrickBelows)
        if bShowBrickBelows then
            local iTotalBrick  = tTile:getTotalBricks()
            tImGui.PushItemWidth(item_width - 20)
            tImGui.Text('Current Brick ID')
            local result, iValue = tImGui.InputInt('##selectedBrickAtLayer', tEditorOptions.iIndexSelectedBrickMenuLayer, 1, 10, flags)
            if result and iValue > 0 and iValue <= iTotalBrick then
                tEditorOptions.iIndexSelectedBrickMenuLayer = iValue
            end

            tImGui.Text('Size Brick')
            local result, iValue = tImGui.InputInt('##iSizeBrickOnSelector', tEditorOptions.iSizeBrickOnSelector, 1, 10, flags)
            if result and iValue > 30 and iValue <= 300 then
                tEditorOptions.iSizeBrickOnSelector = iValue
            end
            tImGui.PopItemWidth()
        end
    end

    if bShowBrickBelows == false and tTile:getTotalLayer() > 0 and tImGui.TreeNodeEx('Bricks',flags,'Brick2layer') then

        local iTotalBrick  = tTile:getTotalBricks()
        local tFilterBrick = {'All'}
        tImGui.PushItemWidth(item_width - 20)
        if iTotalBrick > 0 then
            tImGui.Text('Filter by TileSet')

            for i=1, tTile:getTotalTileSet() do
                table.insert(tFilterBrick,tTile:getTileSetName(i))
            end

            local height_in_items  =  -1
            local ret, current_item, item_as_string = tImGui.Combo('##FilTerBrickCombo', tEditorOptions.iLayerFilterBrick, tFilterBrick, height_in_items)
            if ret then
                tEditorOptions.iLayerFilterBrick = current_item
            end

            if tFilterBrick[tEditorOptions.iLayerFilterBrick] ~= 'All' then
                iTotalBrick = tTile:getTotalBricks(tFilterBrick[tEditorOptions.iLayerFilterBrick]) --filter total bricks
                if tEditorOptions.iIndexSelectedBrickMenuLayer > iTotalBrick then
                    tEditorOptions.iIndexSelectedBrickMenuLayer = 0
                end
            end

            tImGui.Text('Filter by Texture Name')
            local modified , sNewText = tImGui.InputText('##FilterTextureNameTileSet',sFilterTextureNameTileSet,flags)
            if modified then
                sFilterTextureNameTileSet = sNewText
            end
        end

        tImGui.Text('Current Brick ID')
        local result, iValue = tImGui.InputInt('##selectedBrickAtLayer', tEditorOptions.iIndexSelectedBrickMenuLayer, 1, 10, flags)
        if result and iValue > 0 and iValue <= iTotalBrick then
            tEditorOptions.iIndexSelectedBrickMenuLayer = iValue
        end

        local filterByTextureName = function(sTexture)
            if sFilterTextureNameTileSet:len() > 0 then
                local s,e = sTexture:find(sFilterTextureNameTileSet)
                if s and e then
                    return true
                end
                return false
            end
            return true
        end

        tImGui.PopItemWidth()
        for n=1, iTotalBrick do
            local tBrick
            tImGui.SetCursorPosX(30)
            if tFilterBrick[tEditorOptions.iLayerFilterBrick] == 'All' then
                tBrick = tTile:getBrick(n) -- no filter (use id)
            else
                tBrick = tTile:getBrick(n,tFilterBrick[tEditorOptions.iLayerFilterBrick]) -- using filter, the index is absolute inside the vector
            end
            local sBrickId = string.format('Brick-%d',tBrick.id)
            local flag_selected_node = 0
            if tEditorOptions.iIndexSelectedBrickMenuLayer == n then
                tEditorOptions.iBrickIdSelected = tBrick.id
                flag_selected_node = tImGui.Flags('ImGuiTreeNodeFlags_Selected')
            end

            if tBrick.texture and tBrick.texture:len() > 0 and filterByTextureName(tBrick.texture) then

                local new_width     = item_width - 30
                local sy            = new_width / tBrick.width  * tBrick.height
                local size          = {x=new_width,y=sy}
                local bg_col        = {r=1,g=1,b=1,a=1}
                local line_color    = {r=1,g=1,b=1,a=1} -- white color
                local uv1           = {x = tBrick.uv[2].x, y = tBrick.uv[2].y}
                local uv2           = {x = tBrick.uv[4].x, y = tBrick.uv[4].y}
                local uv3           = {x = tBrick.uv[3].x, y = tBrick.uv[3].y}
                local uv4           = {x = tBrick.uv[1].x, y = tBrick.uv[1].y}
                local winPos

                if tEditorOptions.iIndexSelectedBrickMenuLayer == n then
                    line_color        = {r=0,g=1,b=0,a=1}
                    winPos            = tImGui.GetCursorScreenPos()
                end

                tImGui.SetCursorPosX(30)
                
                tImGui.ImageQuad(tBrick.texture, size,uv1,uv2,uv3,uv4,bg_col,line_color)
                if tImGui.IsItemHovered(0) and tImGui.IsMouseDown(0) then
                    tEditorOptions.iIndexSelectedBrickMenuLayer = n
                end

                if tEditorOptions.iIndexSelectedBrickMenuLayer == n and winPos then

                    line_color              = {r=0,g=1,b=0,a=1}
                    local p_min             = {x = winPos.x,          y = winPos.y}
                    local p_max             = {x = winPos.x + size.x, y = size.y + winPos.y}
                    local rounding          =  0.0
                    local rounding_corners  =  0
                    local thickness         =  5.0

                    tImGui.AddRect(p_min, p_max, line_color, rounding, rounding_corners, thickness)
                end

                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(string.format('%s\nbrick ID:%d', tBrick.texture,tBrick.id))
                    tImGui.EndTooltip()
                end
            end
        end
        menuPopUpOptionToAddBrick()
        tImGui.TreePop()
    end
end

function drawBrickTab(item_width)
    local flags = 0
    local step       =  1
    local step_fast  =  10
    local flags      =  0
    tImGui.PushItemWidth(item_width - 35)
    tEditorOptions.iIndexDrawBrick = tTile:setRenderMode('brick',tEditorOptions.iIndexDrawBrick)
    tImGui.HelpMarker('Generally bricks do not need \nto be adjusted.\nHowever, it might be necessary. \nIt depends on the quality of \ntexture and division of tile.')

    local iTotalBrick = tTile:getTotalBricks()
    local tFilterBrick = {'All'}

    if iTotalBrick > 0 then
        tImGui.Text('Filter by TileSet')

        for i=1, tTile:getTotalTileSet() do
            table.insert(tFilterBrick,tTile:getTileSetName(i))
        end

        local height_in_items  =  -1
        local ret, current_item, item_as_string = tImGui.Combo('##FilTerBrickCombo', tEditorOptions.iTileSetFilterBrick, tFilterBrick, height_in_items)
        if ret then
            tEditorOptions.iTileSetFilterBrick = current_item
        end

        tImGui.Text('Fine Adjust UV')
        tImGui.SameLine()
        tImGui.HelpMarker('Higher is thinner \nAffects only UV')
        local result, iValue = tImGui.InputInt('##fineAdjustUVBrick', tEditorOptions.iFineAdjust, step, step_fast, flags)
        if result and iValue > 1 and iValue <= 1000 then
            tEditorOptions.iFineAdjust = iValue
        end
        if tFilterBrick[tEditorOptions.iTileSetFilterBrick] ~= 'All' then
            iTotalBrick = tTile:getTotalBricks(tFilterBrick[tEditorOptions.iTileSetFilterBrick]) --filter total bricks
        end
    end

    tCircleEditVuBrick.visible = false
    for n=1, iTotalBrick do
        local tBrick
        if tFilterBrick[tEditorOptions.iTileSetFilterBrick] == 'All' then
            tBrick = tTile:getBrick(n) -- no filter (use id)
        else
            tBrick = tTile:getBrick(n,tFilterBrick[tEditorOptions.iTileSetFilterBrick]) -- using filter, the index is absoulte inside the vector
        end
        
        local sBrickId = string.format('Brick-%d',tBrick.id)
        local flag_selected_node = 0
        if tEditorOptions.iIndexDrawBrick == n then
            flag_selected_node = tImGui.Flags('ImGuiTreeNodeFlags_Selected')
            local tPhysics     = tTile:getPhysicsBrick(tBrick.id)
            updatePhysicsLine(tPhysics)
        end
        if tImGui.TreeNodeEx(sBrickId,flag_selected_node,sBrickId) then
            if tImGui.IsItemHovered(0) then
                tEditorOptions.iIndexDrawBrick = tTile:setRenderMode('brick',tBrick.id)
            end
            tImGui.Text('Width')
            local result, iValue = tImGui.InputInt('##' .. sBrickId .. 'width', tBrick.width, step, step_fast, flags)
            if result and iValue > 1 and iValue <= 9999999 then
                tBrick.width = iValue
                tTile:updateBrick(tBrick.id,tBrick)
                addHistoric()
            end

            tImGui.Text('Height')
            local result, iValue = tImGui.InputInt('##' .. sBrickId .. 'height', tBrick.height, step, step_fast, flags)
            if result and iValue > 1 and iValue <= 9999999 then
                tBrick.height = iValue
                tTile:updateBrick(tBrick.id,tBrick)
                addHistoric()
            end

            tImGui.Text('Texture')
            if tBrick.texture and tBrick.texture:len() > 0 then
                local sTextureTemp = tUtil.getShortName(tBrick.texture)
                if sTextureTemp:len() > 12 then
                    sTextureTemp = sTextureTemp:sub(1,5) .. '...' .. sTextureTemp:sub(sTextureTemp:len() - 5)
                end
                tImGui.TextDisabled(sTextureTemp)
                tImGui.SameLine()
                tImGui.HelpMarker(tBrick.texture)
                tImGui.SameLine()
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), {r=1,g=0,b=0.3,a=1})
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r=0,g=0,b=0.3,a=0})
                tImGui.SetCursorPosX(item_width-35)
                if tImGui.Button('...##SetTexBrick' .. tostring(n), {x=30,y=0}) then
                    local sFileName = mbm.openFile(tBrick.texture,"png","jpeg","jpg","bmp","gif","psd","pic","pnm","hdr","tga","tif")
                    if sFileName then
                        tBrick.texture = sFileName
                        tTile:updateBrick(tBrick.id,tBrick)
                        addHistoric()
                    end
                end
                tImGui.PopStyleColor(2)
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text('Change Texture')
                    tImGui.EndTooltip()
                end
            else
                tImGui.TextDisabled('None')
            end
            
            tImGui.Separator()
            tImGui.PushItemWidth(item_width - 40)
            
            if tInitialBrickAdjustUv[tBrick.id] == nil then
                tInitialBrickAdjustUv[tBrick.id] = { u =  tBrick.uv[3].x - tBrick.uv[1].x, v = tBrick.uv[1].y - tBrick.uv[2].y}
            end

            local step_u      =  tInitialBrickAdjustUv[tBrick.id].u / tEditorOptions.iFineAdjust
            local step_fast_u =  step_u * 5
            local step_v      =  tInitialBrickAdjustUv[tBrick.id].v / tEditorOptions.iFineAdjust
            local step_fast_v =  step_u * 5
            local bOneUvOver  = false
            local format      = "%.7f"
            tImGui.Text('UV')
            for j=1, #tBrick.uv do
                local uv = tBrick.uv[j]
                local labelU     = string.format('U%d##U%d-%d',j,n,j)
                local labelV     = string.format('V%d##V%d-%d',j,n,j)
                
                local result, fValue = tImGui.InputFloat(labelU, uv.x, step_u, step_fast_u, format, flags)
                if result then
                    uv.x = fValue
                    tTile:updateBrick(tBrick.id,tBrick)
                    addHistoric()
                end

                if bOneUvOver == false and tEditorOptions.iIndexDrawBrick == n and tImGui.IsItemHovered(0) then
                    bOneUvOver = j
                end
                
                local result, fValue = tImGui.InputFloat(labelV, uv.y, step_v, step_fast_v, format, flags)
                if result then
                    uv.y = fValue
                    tTile:updateBrick(tBrick.id,tBrick)
                    addHistoric()
                end

                if bOneUvOver == false and tEditorOptions.iIndexDrawBrick == n and tImGui.IsItemHovered(0) then
                    bOneUvOver = j
                end
            end
            tImGui.Text('Expand UV')
            local sExpandUv = string.format('##ExpandUV-%s',sBrickId)
            local result, fValue = tImGui.InputFloat(sExpandUv, tEditorOptions.iExpandValue, 0.0001, 0.0001, format, flags)
            if result and fValue > 0 then
                tEditorOptions.iExpandValue = fValue
            end
            if tImGui.Button('Inside##UV', {x=(item_width - 45) / 2 - 2,y=0}) then
                tTile:expandBrick(tBrick.id,true,tEditorOptions.iExpandValue)
                addHistoric()
            end
            tImGui.SameLine()
            if tImGui.Button('Outside##UV', {x=(item_width - 45) / 2 - 2,y=0}) then
                tTile:expandBrick(tBrick.id,false,tEditorOptions.iExpandValue)
                addHistoric()
            end

            tImGui.Text('Expand U')
            if tImGui.Button('Inside##H', {x=(item_width - 45) / 2 - 2,y=0}) then
                tTile:expandHBrick(tBrick.id,true,tEditorOptions.iExpandValue)
                addHistoric()
            end
            tImGui.SameLine()
            if tImGui.Button('Outside##H', {x=(item_width - 45) / 2 - 2,y=0}) then
                tTile:expandHBrick(tBrick.id,false,tEditorOptions.iExpandValue)
                addHistoric()
            end

            tImGui.Text('Expand V')
            if tImGui.Button('Inside##V', {x=(item_width - 45) / 2 - 2,y=0}) then
                tTile:expandVBrick(tBrick.id,true,tEditorOptions.iExpandValue)
                addHistoric()
            end
            tImGui.SameLine()
            if tImGui.Button('Outside##V', {x=(item_width - 45) / 2 - 2,y=0}) then
                tTile:expandVBrick(tBrick.id,false,tEditorOptions.iExpandValue)
                addHistoric()
            end


            if bOneUvOver then
                local scale = tTile:getScale()
                if bOneUvOver == 1 then 
                    tCircleEditVuBrick:setPos(-tBrick.width / 2 * scale.x, -tBrick.height / 2 * scale.y) -- left down
                elseif bOneUvOver == 2 then
                    tCircleEditVuBrick:setPos(-tBrick.width / 2 * scale.x, tBrick.height / 2 * scale.y) -- left up
                elseif bOneUvOver == 3 then
                    tCircleEditVuBrick:setPos(tBrick.width / 2 * scale.x, -tBrick.height / 2 * scale.y) -- down right
                elseif bOneUvOver == 4 then
                    tCircleEditVuBrick:setPos(tBrick.width / 2 * scale.x, tBrick.height / 2 * scale.y) -- up right
                end
                tCircleEditVuBrick.visible = true
            end

            tImGui.Separator()
            if tImGui.Button(string.format('Restore Default##RestBrick%d',n), {x=item_width - 40,y=0}) then
                tTile:undoChangesBrick(tBrick.id)
                addHistoric()
            end
            tImGui.Separator()
            local tPhysics     = tTile:getPhysicsBrick(tBrick.id)
            tImGui.Text('Physics')
            if tImGui.Button('Add Physic##PhysicBrickN'.. tostring(n), {x=item_width - 40,y=0}) then
                table.insert(tPhysics,{type = 'rectangle', x = 0, y = 0, width = tBrick.width, height = tBrick.height })
                tTile:setPhysicsBrick(tBrick.id,tPhysics)
                addHistoric()
            end

            for j=1, #tPhysics do
                tImGui.Separator()
                local tPhysic = tPhysics[j]
                local current_item = tPhysicsType[tPhysic.type]
                local label            = '##PhysicsBrick-' .. tostring(n) .. '-' .. tostring(j)
                local height_in_items  =  -1

                local ret, current_item, item_as_string = tImGui.Combo(label, current_item, tPhysicsCombo, height_in_items)
                if ret then
                    if item_as_string ==  'none' then
                        table.remove(tPhysics,j)
                        tTile:setPhysicsBrick(tBrick.id,tPhysics)
                    elseif item_as_string ==  'rectangle' then
                        tPhysics[j] = {type = 'rectangle', x = tPhysic.x or 0, y = tPhysic.y or 0, width = tBrick.width, height = tBrick.height }
                        tTile:setPhysicsBrick(tBrick.id,tPhysics)
                    elseif item_as_string ==  'circle' then
                        tPhysics[j] = {type = 'circle', x = tPhysic.x or 0, y = tPhysic.y or 0, ray = tBrick.width * 0.5 }
                        tTile:setPhysicsBrick(tBrick.id,tPhysics)
                    elseif item_as_string ==  'triangle' then
                        local p1 = {x = tBrick.width * -0.5, y = tBrick.height * -0.5}
                        local p2 = {x = 0                  , y = tBrick.height *  0.5}
                        local p3 = {x = tBrick.width *  0.5, y = tBrick.height * -0.5}
                        tPhysics[j] = {type = 'triangle', p1 = p1, p2 = p2, p3 = p3}
                        tTile:setPhysicsBrick(tBrick.id,tPhysics)
                    end
                    addHistoric()
                end

                local step       =  1.0
                local step_fast  =  10.0
                local format     = "%.3f"
                if tPhysic.type == 'rectangle' or tPhysic.type == 'circle' then
                    
                    tImGui.Text('X')
                    local result, fValue = tImGui.InputFloat(string.format('##Brick%sPosX%d-%d',tPhysic.type,n,j), tPhysic.x, step, step_fast, format, flags)
                    if result then
                        tPhysic.x = fValue
                        tTile:setPhysicsBrick(tBrick.id,tPhysics)
                        addHistoric()
                    end

                    tImGui.Text('Y')
                    local result, fValue = tImGui.InputFloat(string.format('##Brick%sPosY%d-%d',tPhysic.type,n,j), tPhysic.y, step, step_fast, format, flags)
                    if result then
                        tPhysic.y = fValue
                        tTile:setPhysicsBrick(tBrick.id,tPhysics)
                        addHistoric()
                    end
                end

                if tPhysic.type == 'rectangle' then
                    tImGui.Text('Width')
                    local result, fValue = tImGui.InputFloat(string.format('##BrickRectPosWidth%d-%d',n,j), tPhysic.width, step, step_fast, format, flags)
                    if result then
                        tPhysic.width = fValue
                        tTile:setPhysicsBrick(tBrick.id,tPhysics)
                        addHistoric()
                    end

                    tImGui.Text('Height')
                    local result, fValue = tImGui.InputFloat(string.format('##BrickRectPosHeight%d-%d',n,j), tPhysic.height, step, step_fast, format, flags)
                    if result then
                        tPhysic.height = fValue
                        tTile:setPhysicsBrick(tBrick.id,tPhysics)
                        addHistoric()
                    end
                elseif tPhysic.type == 'circle' then
                    tImGui.Text('Ray')
                    local result, fValue = tImGui.InputFloat(string.format('##BrickCircleRay%d-%d',n,j), tPhysic.ray, step, step_fast, format, flags)
                    if result then
                        tPhysic.ray = fValue
                        tTile:setPhysicsBrick(tBrick.id,tPhysics)
                        addHistoric()
                    end
                elseif tPhysic.type == 'triangle' then
                    for k=1, 3 do
                        local p = 'p' .. tostring(k)
                        tImGui.Text('P' .. tostring(k))
                        local result, fValue = tImGui.InputFloat(string.format('##BrickTriangle%sx%d-%d',p,n,j), tPhysic[p].x, step, step_fast, format, flags)
                        if result then
                            tPhysic[p].x = fValue
                            tTile:setPhysicsBrick(tBrick.id,tPhysics)
                            addHistoric()
                        end

                        local result, fValue = tImGui.InputFloat(string.format('##BrickTriangle%sy%d-%d',p,n,j), tPhysic[p].y, step, step_fast, format, flags)
                        if result then
                            tPhysic[p].y = fValue
                            tTile:setPhysicsBrick(tBrick.id,tPhysics)
                            addHistoric()
                        end
                    end
                end
            end

            tImGui.Separator()

            local tProperties = tTile:getBrickProperties(tBrick.id)
            propertiesTreeView(tProperties,'##BrickProperty-' .. tostring(tBrick.id),item_width-40,tPropertyBrick,
                function (tProperty)
                    if tProperty.isUpdate then
                        tTile:setBrickProperty(tBrick.id,tProperty)
                    elseif tPropertyTypes[tProperty.iSelectedCombo] == 'Text' then
                        tTile:setBrickProperty(tBrick.id,{name = tProperty.name, value = tProperty.sText})
                    elseif tPropertyTypes[tProperty.iSelectedCombo] == 'Number' then
                        tTile:setBrickProperty(tBrick.id,{name = tProperty.name, value = tProperty.fValue})
                    elseif tPropertyTypes[tProperty.iSelectedCombo] == 'Boolean' then
                        tTile:setBrickProperty(tBrick.id,{name = tProperty.name, value = tProperty.bValue})
                    else
                        tUtil.showMessageWarn('Invalid Property Selected')
                    end
                    addHistoric()
                end
            )
            tImGui.Separator()
            tImGui.PopItemWidth()
            tImGui.TreePop()
        else
            if tImGui.IsItemHovered(0) then
                tEditorOptions.iIndexDrawBrick = tTile:setRenderMode('brick',tBrick.id)
            end
        end
    end
    tImGui.PopItemWidth()
end

function drawTileSetTab(item_width)
    updatePhysicsLine({})
    local step       =  1
    local step_fast  =  10
    local flags      = 0
    tEditorOptions.iIndexDrawTileSet = tTile:setRenderMode('tileset',tEditorOptions.iIndexDrawTileSet)
    tImGui.PushItemWidth(item_width)
    tImGui.Text('Name')

    local modified , sNewText = tImGui.InputText('##TileSetNew',tEditorOptions.sDefaultTileSetName,flags)
    if modified then
        tEditorOptions.sDefaultTileSetName = sNewText
    end

    local anyChange = false

    tImGui.Text('Tile Width')
    local result, iValue = tImGui.InputInt('##NewTileSetWidth', tEditorOptions.iDefaultTileSetWidth , step, step_fast, flags)
    if result and iValue > 1 and iValue <= 4096 then
        tEditorOptions.iDefaultTileSetWidth = iValue
        anyChange = true
    end

    tImGui.Text('Tile Height')
    local result, iValue = tImGui.InputInt('##NewTileSetHeight', tEditorOptions.iDefaultTileSetHeight, step, step_fast, flags)
    if result and iValue > 1 and iValue <= 4096 then
        tEditorOptions.iDefaultTileSetHeight = iValue
        anyChange = true
    end

    if tTextureTileSet[tEditorOptions.iSelectedTileSetPreview] then

        if tImGui.Button('Set Image Size', {x=item_width,y=0}) then
            local width,height,id, has_alpha = mbm.loadTexture(tTextureTileSet[tEditorOptions.iSelectedTileSetPreview])
            if id ~= 0 then
                tEditorOptions.iDefaultTileSetWidth  = width
                tEditorOptions.iDefaultTileSetHeight = height
                anyChange = true
            end
        end
    end

    tImGui.Text('Tile Space X')
    local result, iValue = tImGui.InputInt('##NewTileSetSpaceX', tEditorOptions.iDefaultTileSetSpaceX, step, step_fast, flags)
    if result and iValue >= -4096 and iValue <= 4096 then
        tEditorOptions.iDefaultTileSetSpaceX = iValue
        anyChange = true
    end

    tImGui.Text('Tile Space Y')
    local result, iValue = tImGui.InputInt('##NewTileSetSpaceY', tEditorOptions.iDefaultTileSetSpaceY, step, step_fast, flags)
    if result and iValue >= -4096 and iValue <= 4096 then
        tEditorOptions.iDefaultTileSetSpaceY = iValue
        anyChange = true
    end

    tImGui.Text('Tile Margin X')
    local result, iValue = tImGui.InputInt('##NewTileSetMarginX', tEditorOptions.iDefaultTileSetMarginX, step, step_fast, flags)
    if result and iValue >= -4096 and iValue <= 4096 then
        tEditorOptions.iDefaultTileSetMarginX = iValue
        anyChange = true
    end

    tImGui.Text('Tile Margin Y')
    local result, iValue = tImGui.InputInt('##NewTileSetMarginY', tEditorOptions.iDefaultTileSetMarginY, step, step_fast, flags)
    if result and iValue >= -4096 and iValue <= 4096 then
        tEditorOptions.iDefaultTileSetMarginY = iValue
        anyChange = true
    end

    if tImGui.Button('Load Image(s)', {x=item_width,y=0}) then
        if tTile:existTileSet(tEditorOptions.sDefaultTileSetName) then
            tUtil.showMessageWarn(string.format('TileSet \n[%s]\nalready exists!\nPlease give another name!',tEditorOptions.sDefaultTileSetName))
        else
            local sFileName
            if type(tTextureTileSet) == 'table' then
                sFileName = tTextureTileSet[1]
            else
                sFileName = tTextureTileSet
            end
            sFileName = mbm.openMultiFile(sFileName,"png","jpeg","jpg","bmp","gif","psd","pic","pnm","hdr","tga","tif")
            if sFileName then
                tTextureTileSet = sFileName
                if type(tTextureTileSet) == 'table' then
                    tTile:showTileSetPreview(tTextureTileSet[1],
                                        tEditorOptions.iDefaultTileSetWidth,
                                        tEditorOptions.iDefaultTileSetHeight,
                                        tEditorOptions.iDefaultTileSetSpaceX,
                                        tEditorOptions.iDefaultTileSetSpaceY,
                                        tEditorOptions.iDefaultTileSetMarginX,
                                        tEditorOptions.iDefaultTileSetMarginY)
                    local width,height,id, has_alpha = mbm.loadTexture(tTextureTileSet[1])
                    tEditorOptions.iWidth  = width
                    tEditorOptions.iHeight = height
                    tEditorOptions.tSubTilesToImport      = {xStart = 0,yStart = 0,xEnd = width,yEnd = height}
                else
                    tTile:showTileSetPreview(tTextureTileSet,
                                        tEditorOptions.iDefaultTileSetWidth,
                                        tEditorOptions.iDefaultTileSetHeight,
                                        tEditorOptions.iDefaultTileSetSpaceX,
                                        tEditorOptions.iDefaultTileSetSpaceY,
                                        tEditorOptions.iDefaultTileSetMarginX,
                                        tEditorOptions.iDefaultTileSetMarginY)
                    local width,height,id, has_alpha = mbm.loadTexture(tTextureTileSet)
                    tEditorOptions.iWidth  = width
                    tEditorOptions.iHeight = height
                    tEditorOptions.tSubTilesToImport      = {xStart = 0,yStart = 0,xEnd = width,yEnd = height}
                end
            end
        end
    end

    if #tTextureTileSet > 0 then
        tImGui.Text('Texture(s) preview')
        for i=1, #tTextureTileSet do
            tImGui.Selectable(tUtil.getShortName(tTextureTileSet[i]), tEditorOptions.iSelectedTileSetPreview == i,0, {x=item_width,y=0} )
            if tImGui.IsItemClicked(0) then
                tEditorOptions.iSelectedTileSetPreview = i
                anyChange = true
            end
            if anyChange then
                tTile:showTileSetPreview(tTextureTileSet[tEditorOptions.iSelectedTileSetPreview] or 'no_tex_set',
                                        tEditorOptions.iDefaultTileSetWidth,
                                        tEditorOptions.iDefaultTileSetHeight,
                                        tEditorOptions.iDefaultTileSetSpaceX,
                                        tEditorOptions.iDefaultTileSetSpaceY,
                                        tEditorOptions.iDefaultTileSetMarginX,
                                        tEditorOptions.iDefaultTileSetMarginY)
            end
        end
        tImGui.Text('')

        tEditorOptions.bMakeSubTiles = tImGui.Checkbox("Create sub tile set",tEditorOptions.bMakeSubTiles)
        tLineRectTile.visible = tEditorOptions.bMakeSubTiles

        tImGui.NewLine()

        local tMinBound = { x = tEditorOptions.tSubTilesToImport.xStart, y = tEditorOptions.tSubTilesToImport.yStart}
        local tMaxBound = { x = tEditorOptions.tSubTilesToImport.xEnd,   y = tEditorOptions.tSubTilesToImport.yEnd}

        if tEditorOptions.bMakeSubTiles then
            local step,step_fast,flags = 1, 10, 0
            tImGui.Text('Min X')
            local result, iValue = tImGui.InputInt('##tMinBound.x', tEditorOptions.tSubTilesToImport.xStart, step, step_fast, flags)
            if result then
                tEditorOptions.tSubTilesToImport.xStart = iValue
            end

            tImGui.Text('Min Y')
            local result, iValue = tImGui.InputInt('##tMinBound.y', tEditorOptions.tSubTilesToImport.yStart, step, step_fast, flags)
            if result then
                tEditorOptions.tSubTilesToImport.yStart = iValue
            end

            tImGui.Text('Max X')
            local result, iValue = tImGui.InputInt('##tMaxBound.x', tEditorOptions.tSubTilesToImport.xEnd, step, step_fast, flags)
            if result then
                tEditorOptions.tSubTilesToImport.xEnd = iValue
            end

            tImGui.Text('Max Y')
            local result, iValue = tImGui.InputInt('##tMaxBound.y', tEditorOptions.tSubTilesToImport.yEnd, step, step_fast, flags)
            if result then
                tEditorOptions.tSubTilesToImport.yEnd = iValue
            end

            local size   =  {x=0,y=0}
            tImGui.Text('Move X')
            tImGui.SameLine()
            tImGui.SetNextItemWidth(20)
            local result, iValue = tImGui.InputInt('##XMinustLineRectTile', 0, step, step_fast, flags)
            if result then
                tEditorOptions.tSubTilesToImport.xStart = tEditorOptions.tSubTilesToImport.xStart + iValue
                tEditorOptions.tSubTilesToImport.xEnd   = tEditorOptions.tSubTilesToImport.xEnd   + iValue
            end
            tImGui.Text('Move Y')
            tImGui.SameLine()
            tImGui.SetNextItemWidth(20)
            local result, iValue = tImGui.InputInt('##YMinustLineRectTile', 0, step, step_fast, flags)
            if result then
                tEditorOptions.tSubTilesToImport.yEnd   = tEditorOptions.tSubTilesToImport.yEnd   - iValue
                tEditorOptions.tSubTilesToImport.yStart = tEditorOptions.tSubTilesToImport.yStart - iValue
            end
            
            local iHalfTextureWidth  = tEditorOptions.iWidth * 0.5
            local iHalfTextureHeight = tEditorOptions.iHeight * 0.5

            tLineRectTile:set({ tMinBound.x - iHalfTextureWidth,(tEditorOptions.iHeight - tMinBound.y) - iHalfTextureHeight,
                                tMinBound.x - iHalfTextureWidth,(tEditorOptions.iHeight - tMaxBound.y) - iHalfTextureHeight,
                                tMaxBound.x - iHalfTextureWidth,(tEditorOptions.iHeight - tMaxBound.y) - iHalfTextureHeight,
                                tMaxBound.x - iHalfTextureWidth,(tEditorOptions.iHeight - tMinBound.y) - iHalfTextureHeight,
                                tMinBound.x - iHalfTextureWidth,(tEditorOptions.iHeight - tMinBound.y) - iHalfTextureHeight},
                                1)
            local scale = tTile:getScale()
            tLineRectTile:setScale(scale.x,scale.y)
        end
        
        if tImGui.Button('Create Tile Set', {x=item_width,y=0}) then

            if tTile:existTileSet(tEditorOptions.sDefaultTileSetName) then
                tUtil.showMessageWarn(string.format('TileSet \n[%s]\nalready exists!\nPlease give another name!',tEditorOptions.sDefaultTileSetName))
            else
                
                local bResult = false
                
                if tEditorOptions.bMakeSubTiles then
                    bResult = tTile:newTileSet( tTextureTileSet,
                                                tEditorOptions.sDefaultTileSetName,
                                                tEditorOptions.iDefaultTileSetWidth,
                                                tEditorOptions.iDefaultTileSetHeight,
                                                tEditorOptions.iDefaultTileSetSpaceX,
                                                tEditorOptions.iDefaultTileSetSpaceY,
                                                tEditorOptions.iDefaultTileSetMarginX,
                                                tEditorOptions.iDefaultTileSetMarginY,
                                                tMinBound.x,tMinBound.y,
                                                tMaxBound.x,tMaxBound.y)
                else
                    bResult = tTile:newTileSet( tTextureTileSet,
                                                tEditorOptions.sDefaultTileSetName,
                                                tEditorOptions.iDefaultTileSetWidth,
                                                tEditorOptions.iDefaultTileSetHeight,
                                                tEditorOptions.iDefaultTileSetSpaceX,
                                                tEditorOptions.iDefaultTileSetSpaceY,
                                                tEditorOptions.iDefaultTileSetMarginX,
                                                tEditorOptions.iDefaultTileSetMarginY)

                end

                if tEditorOptions.bMakeSubTiles then
                    if bResult then
                        tUtil.showMessage('Sub Tile created!')
                    else
                        tUtil.showMessageWarn('An error occurred!')
                    end
                else
                    if bResult then
                        tTextureTileSet = {}
                    else
                        tTextureTileSet = {}
                        tUtil.showMessageWarn('An error occurred!')
                    end
                end
            end
        end
    else
        tLineRectTile.visible = false
    end


    local iTotalTileSet = tTile:getTotalTileSet()

    if iTotalTileSet > 0 then
        tImGui.Separator()
    end

    tImGui.PushItemWidth(item_width - 35)
    for i=1, iTotalTileSet do
        local size         =  {x=item_width-25,y=0}
        local sTileSetName = tTile:getTileSetName(i)

        local flag_selected_node = 0
        if tEditorOptions.iIndexDrawTileSet == i then
            flag_selected_node = tImGui.Flags('ImGuiTreeNodeFlags_Selected')
        end

        if tImGui.TreeNodeEx(sTileSetName,flag_selected_node,'##TileSet'.. tostring(i)) then
            
            if tImGui.IsItemHovered(0) then
                tEditorOptions.iIndexDrawTileSet = tTile:setRenderMode('tileset',i)
            end
            tImGui.Text('Name')
            local modified , sNewText = tImGui.InputText('##tileName' .. tostring(i),sTileSetName,flags)
            if modified then
                tTile:setTileSetName(i,sNewText)
            end

            tImGui.Text('Tile Width')
            tImGui.SameLine()
            tImGui.HelpMarker('Will affect all bricks which belong to this TileSet')
            local result, iValue = tImGui.InputInt('##TileSetWidth-' .. tostring(i), tTile:getTileSetWidth(i), step, step_fast, flags)
            if result and iValue > 1 and iValue <= 4096 then
                tTile:setTileSetWidth(i,iValue)
            end

            tImGui.Text('Tile Height')
            tImGui.SameLine()
            tImGui.HelpMarker('Will affect all bricks which belong to this TileSet')
            local result, iValue = tImGui.InputInt('##TileSetHeight-' .. tostring(i), tTile:getTileSetHeight(i), step, step_fast, flags)
            if result and iValue > 1 and iValue <= 4096 then
                tTile:setTileSetHeight(i,iValue)
            end
            tImGui.TreePop()
        else
            if tImGui.IsItemHovered(0) then
                tEditorOptions.iIndexDrawTileSet = tTile:setRenderMode('tileset',i)
            end
        end
    end
    tImGui.PopItemWidth()
    tImGui.PopItemWidth()
end

function drawMapTab(item_width)
    updatePhysicsLine({})
    tTile:setRenderMode('map')
    local flags = 0
    local sMapName = tUtil.getShortName(sFileNameTile)
    tImGui.Text('File')
    tImGui.TextDisabled(sMapName)

    tImGui.Text('Type')

    local ret, current_item, item_as_string = tImGui.Combo('##MapType', tComboMapTypeIndex[tTile:getMapType()],tComboMapType, -1)
    if ret then
        tTile:setMapType(tComboMapType[current_item])
    end

    local tComboOrderRenderLeftRight = {'Left to Right', 'Right to Left'}
    local tComboOrderRenderTopDown   = {'Top to Down'  , 'Down to Top'}
    local left_to_right, top_to_down = tTile:getDirectionMapRender()
    local indexRender = 1
    if left_to_right == false then
        indexRender = 2
    end

    tImGui.Text('Render Direction')

    local ret, current_item, item_as_string = tImGui.Combo('##MapRenderLeftRight', indexRender, tComboOrderRenderLeftRight, -1)
    if ret then
        left_to_right = current_item == 1
        tTile:setDirectionMapRender(left_to_right,top_to_down)
    end

    indexRender = 1
    if top_to_down == false then
        indexRender = 2
    end

    local ret, current_item, item_as_string = tImGui.Combo('##MapRenderTopDown', indexRender, tComboOrderRenderTopDown, -1)
    if ret then
        top_to_down = current_item == 1
        tTile:setDirectionMapRender(left_to_right,top_to_down)
    end

    

    local label      = 'input int'
    local step       =  1
    local step_fast  =  1
    local flags      =  0

    tImGui.Text('Max Tile To Render')
    tImGui.SameLine()
    tImGui.HelpMarker('This is only for debug purpose\n set it to 0 to render all!')
    local result, iValue = tImGui.InputInt('##MaxTile2Render', tTile:getMaxTileToRender(), step, 1, flags)
    if result and iValue > -1 and iValue <= (tTile:getMapCountWidth() * tTile:getMapCountHeight()) then
        tTile:setMaxTileToRender(iValue)
    end
    
    tImGui.Text('Width (count tiles)')
    local step       =  2
    local step_fast  =  2
    local result, iValue = tImGui.InputInt('##MapCountWidth', tTile:getMapCountWidth(), step, step_fast, flags)
    if result and iValue > 1 and iValue < 999999999 then
        if iValue > tTile:getMapCountWidth() then
            if iValue % 2 ~= 0 then
                iValue = iValue + 1
            end
        else
            if iValue % 2 ~= 0 then
                iValue = iValue - 1
                if iValue < 2 then iValue = 2 end
            end
        end
        tTile:setMapCountWidth(iValue)
        tTile:setMaxTileToRender(0)
    end

    tImGui.Text('Height (count tiles)')
    local result, iValue = tImGui.InputInt('##MapCountHeight', tTile:getMapCountHeight(), step, step_fast, flags)
    if result and iValue > 1 and iValue < 999999999 then
        if iValue > tTile:getMapCountHeight() then
            if iValue % 2 ~= 0 then
                iValue = iValue + 1
            end
        else
            if iValue % 2 ~= 0 then
                iValue = iValue - 1
                if iValue < 2 then iValue = 2 end
            end
        end
        tTile:setMapCountHeight(iValue)
        tTile:setMaxTileToRender(0)
    end

    local step       =  1
    step_fast        =  1
    tImGui.Text('Tile Width (pixel)')
    local result, iValue = tImGui.InputInt('##MapTileWidth', tTile:getMapTileWidth(), step, step_fast, flags)
    if result and iValue > 1 and iValue < 999999999 then
        tTile:setMapTileWidth(iValue)
    end

    tImGui.Text('Tile Height (pixel)')
    local result, iValue = tImGui.InputInt('##MapTileHeight', tTile:getMapTileHeight(), step, step_fast, flags)
    if result and iValue > 1 and iValue < 999999999 then
        tTile:setMapTileHeight(iValue)
    end

    tImGui.Text('Background Color')
    tImGui.SameLine()
    tImGui.HelpMarker('Use alpha > 0 otherwise won\'t be visible')
    local clicked, tRgba = tImGui.ColorEdit4('##MapBackGroundColor', tTile:getMapBackgroundColor(), flags)
    if clicked then
        tTile:setMapBackgroundColor(tRgba)
    end

    tImGui.Text('Background Texture')
    tImGui.SameLine()
    tImGui.SetCursorPosX(item_width-20)
    local sTextureBackground = tTile:getMapBackgroundTexture()
    if tImGui.Button('...', {x=30,y=0}) then
        local fileName = mbm.openFile(sTextureBackground,"png","jpeg","jpg","bmp","gif","psd","pic","pnm","hdr","tga","tif")
        if fileName then
            tTile:setMapBackgroundTexture(fileName)
        end
    end

    if sTextureBackground and sTextureBackground:len() > 0 then
        local sTextureTemp = tUtil.getShortName(sTextureBackground)
        if sTextureTemp:len() > 15 then
            sTextureTemp = sTextureTemp:sub(1,7) .. '...' .. sTextureTemp:sub(sTextureTemp:len() - 7)
        end
        tImGui.TextDisabled(sTextureTemp)
        tImGui.SameLine()
        tImGui.HelpMarker(sTextureBackground)
        tImGui.SameLine()
        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), {r=1,g=0,b=0.3,a=1})
        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r=0,g=0,b=0.3,a=0})
        tImGui.SetCursorPosX(item_width-20)
        if tImGui.Button('X', {x=30,y=0}) then
            tTile:setMapBackgroundTexture(nil)
        end
        tImGui.PopStyleColor(2)
        if tImGui.IsItemHovered(0) then
            tImGui.BeginTooltip()
            tImGui.Text('Remove Background image.')
            tImGui.EndTooltip()
        end

    end

    tImGui.Separator()
    local flags = 0
    local bMoveLayerOpened = false
    if tImGui.TreeNodeEx("Move Layers",flags) then
        bMoveLayerOpened = true
    
        local winPos  = tImGui.GetCursorScreenPos()
        local size_triangle = 50
        local ray_triangle = size_triangle / 2
        local color  = {r=17/255,g=101/255,b=112/255,a=1}
        local colorHover  = {r=1,g=1,b=1,a=1}
        local mousePos  = tImGui.GetMousePos()

        local pos_left_right = 75 + winPos.y
        local pos_up_down    = 20 + winPos.x
        local is_repeated  = false

        -- left
        local pl1     = {x=pos_up_down                 , y=size_triangle/2 + pos_left_right}
        local pl2     = {x=size_triangle  + pos_up_down, y=0               + pos_left_right}
        local pl3     = {x=size_triangle  + pos_up_down, y=size_triangle   + pos_left_right}
        local iBlockToMoveLayer      = tEditorOptions.iBlockToMoveLayer
        

        tVec2Aux:set(pl1.x + size_triangle / 2, pl1.y)
        tVec2Aux:sub(mousePos.x,mousePos.y)
        if tVec2Aux.len <= ray_triangle then
            tImGui.AddTriangleFilled(pl1, pl2, pl3, colorHover)
            if tImGui.IsMouseClicked(0,is_repeated) then
                tTile:moveWholeLayerTo(-iBlockToMoveLayer,0)
                addHistoric()
            end
        else
            tImGui.AddTriangleFilled(pl1, pl2, pl3, color)
        end

        -- right
        local pr1     = {x=pl3.x + size_triangle, y=0               + pos_left_right}
        local pr2     = {x=pr1.x + size_triangle, y=size_triangle/2 + pos_left_right}
        local pr3     = {x=pr1.x,                 y=size_triangle   + pos_left_right}
        
        tVec2Aux:set(pr1.x + size_triangle / 2, pr2.y)
        tVec2Aux:sub(mousePos.x,mousePos.y)
        if tVec2Aux.len <= ray_triangle then
            tImGui.AddTriangleFilled(pr1, pr2, pr3, colorHover)
            if tImGui.IsMouseClicked(0,is_repeated) then
                tTile:moveWholeLayerTo(iBlockToMoveLayer,0)
                addHistoric()
            end
        else
            tImGui.AddTriangleFilled(pr1, pr2, pr3, color)
        end

        -- up
        local pu1     = {x=pl3.x                     , y= pos_left_right}
        local pu2     = {x=pu1.x    + size_triangle/2, y= pos_left_right - size_triangle}
        local pu3     = {x=pu1.x    + size_triangle,   y= pos_left_right}
        
        tVec2Aux:set(pu1.x + size_triangle / 2, pu1.y - size_triangle / 2)
        tVec2Aux:sub(mousePos.x,mousePos.y)
        if tVec2Aux.len <= ray_triangle then
            tImGui.AddTriangleFilled(pu1, pu2, pu3, colorHover)
            if tImGui.IsMouseClicked(0,is_repeated) then
                tTile:moveWholeLayerTo(0,iBlockToMoveLayer)
                addHistoric()
            end
        else
            tImGui.AddTriangleFilled(pu1, pu2, pu3, color)
        end
        

        --down
        local pd1     = {x=pl3.x                    , y= pr3.y}
        local pd2     = {x=pd1.x   + size_triangle/2, y= pr3.y + size_triangle}
        local pd3     = {x=pd1.x   + size_triangle,   y= pr3.y}

        tVec2Aux:set(pd1.x + size_triangle / 2, pd2.y - size_triangle / 2)
        tVec2Aux:sub(mousePos.x,mousePos.y)
        if tVec2Aux.len <= ray_triangle then
            tImGui.AddTriangleFilled(pd1, pd2, pd3, colorHover)
            if tImGui.IsMouseClicked(0,is_repeated) then
                tTile:moveWholeLayerTo(0,-iBlockToMoveLayer)
                addHistoric()
            end
        else
            tImGui.AddTriangleFilled(pd1, pd2, pd3, color)
        end

        tImGui.SetCursorPosX(winPos.x)
        tImGui.SetCursorPosY(winPos.y + 160)

        local label      = '##Blocks to move'
        local step       =  1
        local step_fast  =  100
        local flags      =  0

        tImGui.Text('Blocks to move')
        local result, iValue = tImGui.InputInt(label, tEditorOptions.iBlockToMoveLayer, step, step_fast, flags)
        if result then
            tEditorOptions.iBlockToMoveLayer = iValue
        end

        tImGui.SetCursorPosX(winPos.x)
        tImGui.SetCursorPosY(winPos.y + 200)

        tImGui.Separator()

        local tProperties = tTile:getMapProperties()
        propertiesTreeView(tProperties,'##MapProperty',item_width,tPropertyMap,
            function (tProperty)
                if tProperty.isUpdate then
                    tTile:setMapProperty(tProperty)
                elseif tPropertyTypes[tProperty.iSelectedCombo] == 'Text' then
                    tTile:setMapProperty({name = tProperty.name, value = tProperty.sText})
                elseif tPropertyTypes[tProperty.iSelectedCombo] == 'Number' then
                    tTile:setMapProperty({name = tProperty.name, value = tProperty.fValue})
                elseif tPropertyTypes[tProperty.iSelectedCombo] == 'Boolean' then
                    tTile:setMapProperty({name = tProperty.name, value = tProperty.bValue})
                else
                    tUtil.showMessageWarn('Invalid Property Selected')
                end
            end
        )
    end


    tImGui.Separator()

    local iTotalObjectMap     = tTile:getTotalObjectMap()
    tImGui.Text('Object Options')
    local bValue = tImGui.Checkbox("All Objects Visible##Objects",bObjectsVisible)
    if bValue ~= bObjectsVisible then
        for i= 1, #tMapObjects do
            local tShape = tMapObjects[i]
            tShape.bObjectsVisible = bValue
        end
        bObjectsVisible = bValue
    end

    if tImGui.Button('Add Object', {x=item_width - 40,y=0}) then
        local tObj = {type = 'rectangle',name = 'no_name' , x = 0, y = 0, width = tTile:getMapTileWidth(), height = tTile:getMapTileHeight() }
        tTile:addObjectMap(tObj)
    end

        
    local flags = 0
    tImGui.Separator()
    
    tImGui.PushItemWidth(item_width - 20)
    if bMoveLayerOpened then
        flags = 0 -- TODO: collapse TreeNodeEX here
    end
    for j=1, iTotalObjectMap do
        local strId = string.format('Object-%d',j)
        if tImGui.TreeNodeEx(strId,flags,strId) then
            local tObj             = tTile:getObjectMap(j)
            local label            = '##ObjectMap-' .. tostring(j)
            local height_in_items  =  -1

            if j <= #tMapObjects then
                tMapObjects[j].bObjectsVisible = tImGui.Checkbox("Visible##Obj-" .. tostring(j),tMapObjects[j].bObjectsVisible)
            end

            tImGui.Text('Change Type:')
            local ret, current_item, item_as_string = tImGui.Combo(label, tComboType[tObj.type], tObjectCombo, height_in_items)
            if ret then
                if item_as_string ==  'none' then
                    tTile:deleteObjectMap(j)
                    tImGui.TreePop()
                    tMapObjects[j]:destroy()
                    table.remove(tMapObjects,j)
                    break
                elseif item_as_string ==  'rectangle' then
                    tObj = {name = tObj.name or '', type = 'rectangle', x = tObj.x or 0, y = tObj.y or 0, width = tTile:getMapTileWidth(), height = tTile:getMapTileHeight() }
                    tTile:setObjectMap(j,tObj)
                elseif item_as_string ==  'circle' then
                    tObj = {name = tObj.name or '',type = 'circle', x = tObj.x or 0, y = tObj.y or 0, ray = tTile:getMapTileWidth() * 0.5 }
                    tTile:setObjectMap(j,tObj)
                elseif item_as_string ==  'triangle' then
                    local p1 = {x = tTile:getMapTileWidth() * -0.5, y = tTile:getMapTileHeight() * -0.5}
                    local p2 = {x = 0                  , y = tTile:getMapTileHeight() *  0.5}
                    local p3 = {x = tTile:getMapTileWidth() *  0.5, y = tTile:getMapTileHeight() * -0.5}
                    tObj = {name = tObj.name or '',type = 'triangle', x = tObj.x or 0, y = tObj.y or 0, p1 = p1, p2 = p2, p3 = p3}
                    tTile:setObjectMap(j,tObj)
                elseif item_as_string ==  'point' then
                    tObj = {name = tObj.name or '',type = 'point', x = tObj.x or 0, y = tObj.y or 0}
                    tTile:setObjectMap(j,tObj)
                elseif item_as_string ==  'line' then
                    local name = tObj.name or ''
                    tObj = {0,0,tTile:getMapTileWidth(),tTile:getMapTileHeight()}
                    tObj.type = 'line'
                    tObj.name = name
                    tTile:setObjectMap(j,tObj)
                end
            end

            tImGui.Text('Name')
            local flags      = 0
            local modified , sNewText = tImGui.InputText(string.format('##Obj-%d-Name',j) ,tObj.name or 'nop',flags)
            if modified then
                tObj.name = sNewText
                tTile:setObjectMap(j,tObj)
            end

            local step       =  1.0
            local step_fast  =  10.0
            local format     = "%.3f"
            if tObj.type == 'rectangle' or tObj.type == 'circle' or tObj.type == 'point'  or tObj.type == 'triangle'  then
                
                tImGui.Text('X')
                local result, fValue = tImGui.InputFloat(string.format('##ObjectMap%sPosX-%d',tObj.type,j), tObj.x, step, step_fast, format, flags)
                if result then
                    tObj.x = fValue
                    tTile:setObjectMap(j,tObj)
                end

                tImGui.Text('Y')
                local result, fValue = tImGui.InputFloat(string.format('##ObjectMap%sPosY-%d',tObj.type,j), tObj.y, step, step_fast, format, flags)
                if result then
                    tObj.y = fValue
                    tTile:setObjectMap(j,tObj)
                end
            end

            if tObj.type == 'rectangle' then
                tImGui.Text('Width')
                local result, fValue = tImGui.InputFloat(string.format('##ObjectMapRectPosWidth-%d',j), tObj.width, step, step_fast, format, flags)
                if result then
                    tObj.width = fValue
                    tTile:setObjectMap(j,tObj)
                end

                tImGui.Text('Height')
                local result, fValue = tImGui.InputFloat(string.format('##ObjectMapRectPosHeight-%d',j), tObj.height, step, step_fast, format, flags)
                if result then
                    tObj.height = fValue
                    tTile:setObjectMap(j,tObj)
                end
            elseif tObj.type == 'circle' then
                tImGui.Text('Ray')
                local result, fValue = tImGui.InputFloat(string.format('##ObjectMapCircleRay-%d',j), tObj.ray, step, step_fast, format, flags)
                if result then
                    tObj.ray = fValue
                    tTile:setObjectMap(j,tObj)
                end
            -- TODO make triangle options work here
            --elseif tObj.type == 'triangle' then
            --    for k=1, 3 do
            --        local p = 'p' .. tostring(k)
            --        tImGui.Text('P' .. tostring(k))
            --        local result, fValue = tImGui.InputFloat(string.format('##ObjectMapTriangle%sx-%d',p,j), tObj[p].x, step, step_fast, format, flags)
            --        if result then
            --            tObj[p].x = fValue
            --            tTile:setObjectMap(j,tObj)
            --        end

            --        local result, fValue = tImGui.InputFloat(string.format('##ObjectMapTriangle%sy-%d',p,j), tObj[p].y, step, step_fast, format, flags)
            --        if result then
            --            tObj[p].y = fValue
            --            tTile:setObjectMap(j,tObj)
            --        end
            --    end
            elseif tObj.type == 'line' then
                if tImGui.Button('Add Point', {x=item_width - 40,y=0}) then
                    
                    local x = tObj[#tObj -1]
                    local y = tObj[#tObj -0]
                    if #tObj >= 4 then
                        local x1 = tObj[#tObj -3]
                        local y1 = tObj[#tObj -2]
                        tVec2Aux:set(x,y)
                        tVec2Aux:sub(x1,y1)
                        tVec2Aux:normalize()
                        tVec2Aux:mul(tTile:getMapTileWidth(),tTile:getMapTileHeight())
                        tVec2Aux:add(x,y)
                        x,y = tVec2Aux.x, tVec2Aux.y
                    else
                        x = x + tTile:getMapTileWidth()
                        y = y + tTile:getMapTileHeight()
                    end
                    table.insert(tObj,x)
                    table.insert(tObj,y)
                    tTile:setObjectMap(j,tObj)

                end
                for k=1, #tObj, 2 do
                    local x = tObj[k]
                    local y = tObj[k+1]
                    tImGui.Text('X-' .. tostring(k))
                    local result, fValue = tImGui.InputFloat(string.format('##ObjectMap%sLinePosX-%d-%d',tObj.type,j,k), x, step, step_fast, format, flags)
                    if result then
                        tObj[k] = fValue
                        tTile:setObjectMap(j,tObj)
                    end

                    tImGui.Text('Y-' .. tostring(k))
                    local result, fValue = tImGui.InputFloat(string.format('##ObjectMap%ssLinePosY-%d-%d',tObj.type,j,k), y, step, step_fast, format, flags)
                    if result then
                        tObj[k+1] = fValue
                        tTile:setObjectMap(j,tObj)
                    end
                end
            end
            tImGui.TreePop()
        end
    end
    tImGui.PopItemWidth()
end


function setSelectedBrickOnLayerMenuByBrickId(id)
    tEditorOptions.iIndexSelectedBrickMenuLayer = 0
    tEditorOptions.iBrickIdSelected = 0

    local iTotalBrick  = tTile:getTotalBricks()
    for n=1, iTotalBrick do
        local tBrick = tTile:getBrick(n)
        if tBrick.id == id then
            tEditorOptions.iIndexSelectedBrickMenuLayer = n
            tEditorOptions.iBrickIdSelected = tBrick.id
            break
        end
    end
end

function showTileTools()
    local item_width = 150
    local flags = {'ImGuiWindowFlags_NoDecoration', 'ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoSavedSettings', 'ImGuiWindowFlags_NoFocusOnAppearing', 'ImGuiWindowFlags_NoNav'}
    tImGui.SetNextWindowBgAlpha(0.75);
    local iW, iH           = mbm.getRealSizeScreen()
    local window_pos       = {x = iW - tToolsBrickLayerSize.x, y = iH - tToolsBrickLayerSize.y}
    local window_pos_pivot = {x = 0, y = 0}
    tImGui.SetNextWindowPos(window_pos, 0, window_pos_pivot);
    local is_opened, closed_clicked = tImGui.Begin('##ToolsBrickLayer', false,tImGui.Flags(flags) )
    if is_opened then
        local tLayer          = tTile:getLayer(tEditorOptions.iIndexDrawLayer)
        local sLayer          = string.format('Layer-%d',tEditorOptions.iIndexDrawLayer)
        tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=1,b=0,a=0.8})
        tImGui.Text(sLayer)
        tImGui.PopStyleColor(1)

        local indexLastOver = tTile:getIndexTileIdOver() --last over
        local ID_Brick      = tTile:getBrickID(indexLastOver)
        tImGui.Text(string.format('Tile ID:%d\nBrick ID %d\nScale %g',indexLastOver,ID_Brick,tTile:getScale().x))
        local tSelectedTileIDs = tTile:getSelectedTileIDs()
        if #tSelectedTileIDs > 0 then
            tImGui.Separator()
            if #tSelectedTileIDs > 1 then
                tImGui.Text(string.format('Total Selected (%d)',#tSelectedTileIDs))
            else
                tImGui.Text('Total Selected (1)')
            end
            tImGui.SameLine()
            tImGui.HelpMarker('Tile ID selected:\n' .. table.concat(tSelectedTileIDs,','))
            if tImGui.Button('Rotate Right', {x=item_width,y=0}) then
                local id = tTile:rotate('right')
                setSelectedBrickOnLayerMenuByBrickId(id)
            end
            if tImGui.Button('Rotate Left', {x=item_width,y=0}) then
                local id = tTile:rotate('left')
                setSelectedBrickOnLayerMenuByBrickId(id)
            end
            if tImGui.Button('Flip', {x=item_width,y=0}) then
                local id = tTile:flip()
                setSelectedBrickOnLayerMenuByBrickId(id)
            end
            if tImGui.Button('Delete', {x=item_width,y=0}) then
                tTile:deleteSelectedBricks()
            end
        end

    end
    tToolsBrickLayerSize      = tImGui.GetWindowSize()
    tImGui.End()
end

function updatePhysicsLine(tPhysics)
    local scale = tTile:getScale()
    if #tPhysics == #tLinePhysics then
        for i=1, #tLinePhysics do
            local tPhysic = tPhysics[i]
            local tLine   = tLinePhysics[i]
            if tPhysic.type == 'rectangle' then
                tLine:setPos(tPhysic.x,tPhysic.y)
                local hw = tPhysic.width * 0.5
                local hh = tPhysic.height * 0.5
                local t = {-hw,-hh, -hw,hh, hw,hh, hw,-hh, -hw,-hh}
                tLine:set(t,1)
            elseif tPhysic.type == 'circle' then
                tLine:setPos(tPhysic.x,tPhysic.y)
                local t = {}
                for i=0, 360, 3 do
                    local x = math.sin(math.rad(i)) * tPhysic.ray
                    local y = math.cos(math.rad(i)) * tPhysic.ray
                    table.insert(t,x)
                    table.insert(t,y)
                end
                tLine:set(t,1)
            elseif tPhysic.type == 'triangle' then
                tLine:setPos(0,0)
                local t = {tPhysic.p1.x, tPhysic.p1.y, tPhysic.p2.x, tPhysic.p2.y, tPhysic.p3.x, tPhysic.p3.y, tPhysic.p1.x, tPhysic.p1.y}
                tLine:set(t,1)
            end

            tLine:setScale(scale.x,scale.y)

            tLine.visible = true
            tLine.z = -100
        end
    elseif #tPhysics < #tLinePhysics then
        for i= #tPhysics + 1, #tLinePhysics do
            local tLine = tLinePhysics[i]
            tLine.visible = false
        end
    else
        while #tLinePhysics < #tPhysics do
            local tLine = line:new('2dw')
            tLine:add({0,0,100,100})
            tLine:setColor(1,1,0)
            table.insert(tLinePhysics,tLine)
        end
    end
end

function isOverAnyObjectMap(x,y)
    if sRenderWhat == 'map' then
        local scale  = tTile:getScale()
        for i=1, #tMapObjects do
            local tShape = tMapObjects[i]
            if type(tShape.x) == 'number' then -- it is a shape?
                if tShape:isOver(x,y) then
                    tShape.index  = i
                    return tShape
                end
            end
        end
    end
    return nil
end

index_nick_name = 0
function getNickNameShape(name)
    index_nick_name = index_nick_name + 1
    return name .. tostring(index_nick_name)
end

function updateObjectsOnMap()
    if sRenderWhat == 'map' then
        local scale  = tTile:getScale()
        local iTotal = tTile:getTotalObjectMap()
        if iTotal == #tMapObjects then
            for i=1, #tMapObjects do
                local tObj    = tTile:getObjectMap(i)
                local tShape  = tMapObjects[i]
                tShape.tObj   = tObj
                if type(tShape.collide) == 'function' and tShape:collide(tOverBrick.x,tOverBrick.y) then
                    tImGui.BeginTooltip()
                    tImGui.Text(string.format('Object-%d\n%s',i,tObj.name))
                    tImGui.EndTooltip()
                end
                if tObj.type == 'rectangle' then
                    if tShape.type ~= 'rectangle' then
                        tShape:destroy()
                        local bObjectsVisible  = tShape.bObjectsVisible
                        tShape                 = shape:new('2dw')
                        tShape.bObjectsVisible = bObjectsVisible
                        tShape.type            = 'rectangle'
                        tShape:create('rectangle',1,1)
                        tMapObjects[i]         = tShape
                    end
                    tShape:setScale(tObj.width * scale.x, tObj.height * scale.y)
                    tShape:setPos(tObj.x * scale.x,tObj.y * scale.y)
                elseif tObj.type == 'circle' then
                    if tShape.type ~= 'circle' then
                        tShape:destroy()
                        local bObjectsVisible  = tShape.bObjectsVisible
                        tShape                 = shape:new('2dw')
                        tShape.bObjectsVisible = bObjectsVisible
                        tShape.type            = 'circle'
                        tShape:create('circle',1)
                        tMapObjects[i]         = tShape
                    end
                    tShape:setScale(tObj.ray * scale.x, tObj.ray * scale.y)
                    tShape:setPos(tObj.x * scale.x,tObj.y * scale.y)
                elseif tObj.type == 'triangle' then
                    if tShape.type ~= 'triangle' then
                        local tVertex   = {tObj.p1.x, tObj.p1.y, tObj.p2.x, tObj.p2.y, tObj.p3.x, tObj.p3.y}
                        local bObjectsVisible  = tShape.bObjectsVisible
                        tShape:destroy()
                        tShape                 = shape:new('2dw')
                        tShape.bObjectsVisible = bObjectsVisible
                        tShape.type            = 'triangle'
                        local dynamic          = false
                        local nickName         = getNickNameShape('triangle')
                        tShape:create('triangle',tVertex,dynamic,nickName)
                        tMapObjects[i]         = tShape
                    end
                    -- TODO change triangle size here
                    tShape:setScale(scale.x, scale.y)
                    tShape:setPos(tObj.x * scale.x,tObj.y * scale.y)
                elseif tObj.type == 'point' then
                    if tShape.type ~= 'point' then
                        tShape:destroy()
                        local bObjectsVisible  = tShape.bObjectsVisible
                        tShape                 = shape:new('2dw')
                        tShape.bObjectsVisible = bObjectsVisible
                        tShape.type            = 'point'
                        tShape:create('circle',1)
                        tMapObjects[i]         = tShape
                    end
                    local iTileWidth = tTile:getMapTileWidth()
                    tShape:setScale(iTileWidth * scale.x * 0.2, iTileWidth * scale.y * 0.2)
                    tShape:setPos(tObj.x * scale.x,tObj.y * scale.y)
                elseif tObj.type == 'line' then
                    if tShape.type ~= 'line' then
                        tShape:destroy()
                        local bObjectsVisible  = tShape.bObjectsVisible
                        local tLine = {indexOver = 0, x = 0,y=0, tObj = tObj, tTheLine = line:new('2dw')} -- is shape 'x'
                        tLine.tTheLine:add({0,0,0,0})
                        tLine.tTheLine.z = -100
                        tLine.tTheLine:setColor(1.0,0.0,1.0,0.7)

                        tLine.destroy = function(self)
                            for i=1, #self.tShapeCircles do
                                local tShapeCircles = self.tShapeCircles[i]
                                if tShapeCircles then
                                    tShapeCircles:destroy()
                                end
                            end
                            self.tTheLine:destroy()
                            self.tShapeCircles = nil
                        end

                        tLine.collide = function(self,x,y)
                            for i=1, #self.tShapeCircles do
                                local tShapeCircles = self.tShapeCircles[i]
                                if tShapeCircles then
                                    if tShapeCircles:collide(x,y) then 
                                        return true 
                                    end
                                end
                            end
                            return false
                        end

                        tLine.setPos = function(self,x,y)
                            if self.indexOver ~= 0 and self.indexOver < #self.tShapeCircles then
                                local tShapePoint = self.tShapeCircles[self.indexOver]
                                local index = self.indexOver * 2
                                tShapePoint:setPos(x,y)
                                self.tObj[index-1] = x
                                self.tObj[index]   = y
                            end
                        end

                        tLine.isOver = function(self,x,y)
                            for j=1, #self.tShapeCircles do
                                local tShapePoint = self.tShapeCircles[j]
                                if tShapePoint.visible and tShapePoint:isOver(x,y) then
                                    if self.indexOver ~= j then
                                        self.indexOver = j
                                        self.x = tShapePoint.x
                                        self.y = tShapePoint.y
                                    end
                                    return true
                                end
                            end
                            return false
                        end

                        tLine.setScale = function(self,sx,sy)
                            for j=1, #self.tObj, 2 do
                                local x = self.tObj[j]
                                local y = self.tObj[j+1]
                                local index = (j+1) / 2
                                local tShapePoint = self.tShapeCircles[index]
                                tShapePoint:setScale(sx,sy)
                                tShapePoint:setPos(x * sx, y * sy)
                            end
                            return false
                        end

                        tLine.bObjectsVisible = bObjectsVisible
                        tLine.type            = 'line'
                        tLine.tShapeCircles   = {}
                        
                        tShape                 = tLine
                        tMapObjects[i]         = tShape
                    end
                    
                    if #tShape.tShapeCircles * 2 ~= #tObj then
                        local iTileWidth = tTile:getMapTileWidth()
                        for j=1, #tObj, 2 do
                            local index = (j+1) / 2
                            if tShape.tShapeCircles[index] == nil then
                                local x = tObj[j]
                                local y = tObj[j+1]
                                local tShapePoint = shape:new('2dw')
                                tShapePoint:create('circle',iTileWidth * 0.1)
                                tShapePoint:setPos(x,y,-100)
                                table.insert(tShape.tShapeCircles,tShapePoint)
                            end
                        end
                    end

                    for i=1, #tShape.tShapeCircles do
                        tShape.tShapeCircles[i].visible = tShape.bObjectsVisible
                    end

                    tShape.tTheLine.visible = tShape.bObjectsVisible
                    tShape.tTheLine:set(tObj,1)
                    tShape.tTheLine:setScale(scale.x, scale.y)

                    tShape:setScale(scale.x, scale.y)
                end

                tShape.visible = tShape.bObjectsVisible
                tShape.z = -100
            end
        elseif iTotal < #tMapObjects then
            for i= iTotal + 1, #tMapObjects do
                local tShape = tMapObjects[i]
                tShape.visible = false
                tShape.type = 'none'
                if tShape.tShapeCircles then
                    tShape:destroy()
                end
            end
        else
            while #tMapObjects < iTotal do
                local tShape = {destroy = function(self) end, type = 'None', bObjectsVisible = true}
                table.insert(tMapObjects,tShape)
            end
        end
    else
        for i= 1, #tMapObjects do
            local tShape = tMapObjects[i]
            tShape.visible = false
            if tShape.tShapeCircles then
                tShape.tTheLine.visible = false
                for i=1, #tShape.tShapeCircles do
                    tShape.tShapeCircles[i].visible = false
                end
            end
        end
    end
end

function drawBrickSelector(xStart)
    local flags        =  0
    local YPercentage  = 0.3
    local xRight       = 180 -- toolTip
    tUtil.setInitialWindowPositionDown(tWindowsTitle.title_layer_brick_option,xStart+25,YPercentage,xRight)
    local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_layer_brick_option, true, 0)--
    if is_opened then
        local iTotalBrick = tTile:getTotalBricks()
        local xWinLastPos = tImGui.GetCursorPosX()
        local windowSize  = tImGui.GetWindowSize()
        bIsOverSizeBrickSelector = tImGui.IsWindowHovered(flags)
        for n=1, iTotalBrick do
            local tBrick = tTile:getBrick(n) -- no filter (use id)
            local sBrickId = string.format('Brick-%d',tBrick.id)
            if tEditorOptions.iIndexSelectedBrickMenuLayer == n then
                tEditorOptions.iBrickIdSelected = tBrick.id
            end

            if tBrick.texture and tBrick.texture:len() > 0 then

                local new_width     = tEditorOptions.iSizeBrickOnSelector
                local sy            = new_width / tBrick.width  * tBrick.height
                local size          = {x=new_width,y=sy}
                local bg_col        = {r=1,g=1,b=1,a=1}
                local line_color    = {r=1,g=1,b=1,a=1} -- white color
                local uv1           = {x = tBrick.uv[2].x, y = tBrick.uv[2].y}
                local uv2           = {x = tBrick.uv[4].x, y = tBrick.uv[4].y}
                local uv3           = {x = tBrick.uv[3].x, y = tBrick.uv[3].y}
                local uv4           = {x = tBrick.uv[1].x, y = tBrick.uv[1].y}
                local winPos

                if tEditorOptions.iIndexSelectedBrickMenuLayer == n then
                    line_color        = {r=0,g=1,b=0,a=1}
                    winPos            = tImGui.GetCursorScreenPos()
                end

                tImGui.ImageQuad(tBrick.texture, size,uv1,uv2,uv3,uv4,bg_col,line_color)
                if tImGui.IsItemHovered(0) and tImGui.IsMouseDown(0) then
                    tEditorOptions.iIndexSelectedBrickMenuLayer = n
                end

                if tEditorOptions.iIndexSelectedBrickMenuLayer == n and winPos then

                    line_color              = {r=0,g=1,b=0,a=1}
                    local p_min             = {x = winPos.x,          y = winPos.y}
                    local p_max             = {x = winPos.x + size.x, y = size.y + winPos.y}
                    local rounding          =  0.0
                    local rounding_corners  =  0
                    local thickness         =  5.0

                    tImGui.AddRect(p_min, p_max, line_color, rounding, rounding_corners, thickness)
                end

                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(string.format('%s\nbrick ID:%d', tBrick.texture,tBrick.id))
                    tImGui.EndTooltip()
                end
                if (xWinLastPos + (size.x * 2.2)) < windowSize.x then
                    tImGui.SameLine()
                end
                xWinLastPos = tImGui.GetCursorPosX()
            end
        end
        menuPopUpOptionToAddBrick()
    end
    tImGui.End()
    return closed_clicked
end

function menuPopUpOptionToAddBrick()
    if tEditorOptions.iBrickIdSelected ~= 0 then
        local mouse_button = 1
        if tImGui.BeginPopupContextVoid('##Options to add brick to layer :)', mouse_button) then
            if tImGui.Selectable("Fill layer with brick ID: " .. tostring(tEditorOptions.iBrickIdSelected)) then
                local total = tTile:getMapCountWidth() * tTile:getMapCountHeight()
                for i=1, total do
                    tTile:setBrickToLayer(tEditorOptions.iBrickIdSelected,i)
                end
            else
                local tSelectedTileIDs = tTile:getSelectedTileIDs()
                if #tSelectedTileIDs > 0 and tImGui.Selectable(string.format("Copy Selected Tiles (%d) here",#tSelectedTileIDs)) then
                    tTile:duplicateSelectedTiles(tClicked.x,tClicked.y)
                end
            end
            tImGui.EndPopup()
        end
    end
end

function main_tab_bar()
    local flags        =  0
    local x_pos, y_pos = 0, 0
    local width        = 230
    local item_width   = 200
    tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_tile_map,x_pos,y_pos,width,width)
    local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_tile_map, true, ImGuiWindowFlags_NoMove)
    bIsOverSizeBrickSelector = false
    if is_opened then
        tImGui.PushItemWidth(item_width)
        if tImGui.BeginTabBar('##TabBar_id', flags) then
            if tTile:getTotalLayer() > 0 and tImGui.BeginTabItem("Map",nil,ImGuiTabItemFlags['map'] or 0 ) then
                ImGuiTabItemFlags['map'] = 0
                sRenderWhat  = 'map'
                drawMapTab(item_width)
                tLineRectTile.visible = false
                tImGui.EndTabItem()
            end
            
            if tImGui.BeginTabItem("TileSet",nil,ImGuiTabItemFlags['tileset'] or 0) then
                ImGuiTabItemFlags['tileset'] = 0
                sRenderWhat  = 'tileset'
                drawTileSetTab(item_width)
                tImGui.EndTabItem()
            end
            
            if tTile:getTotalTileSet() > 0 then
                if tImGui.BeginTabItem("Layer",nil,ImGuiTabItemFlags['layer'] or 0) then
                    ImGuiTabItemFlags['layer'] = 0
                    sRenderWhat  = 'layer'
                    tLineRectTile.visible = false
                    drawLayerTab(item_width)
                    tImGui.EndTabItem()
                end
                if tImGui.BeginTabItem("Bricks",nil,ImGuiTabItemFlags['brick'] or 0) then
                    ImGuiTabItemFlags['brick'] = 0
                    sRenderWhat  = 'brick'
                    tLineRectTile.visible = false
                    drawBrickTab(item_width)
                    tImGui.EndTabItem()
                end
            end
            tImGui.PopItemWidth()
            tImGui.EndTabBar()
        end
        if sRenderWhat == 'layer' and bShowBrickBelows == true  then
            if drawBrickSelector(item_width) then
                bShowBrickBelows = false
            end
        end
    else
        tLineRectTile.visible = false
    end
    if closed_clicked then
        bEnableMainTabBar = false
    end

    tImGui.End()
end

function main_menu_tiled()
    if tImGui.BeginMainMenuBar() then
        if tImGui.BeginMenu("File") then

            local pressed,checked = tImGui.MenuItem("Load Tile Map", "Ctrl+O", false)
            if pressed then
                onOpenTileBinary()
            end
            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("Save Tile Map", "Ctrl+S", false)
            if pressed then
                onSaveTileBinary()
            end

            local pressed,checked = tImGui.MenuItem("Save Tile Map as", nil, false)
            if pressed then
                onSaveAsTileBinary()
            end

            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("Quit", "Alt+F4", false)
            if pressed then
                mbm.quit()
            end

            tImGui.EndMenu();
        end

        if tImGui.BeginMenu("Layer Options") then
            if sRenderWhat == 'layer' then
                local pressed,checked = tImGui.MenuItem("Select All Bricks", "Ctrl+A", false)
                if pressed then
                    tTile:selectAllBricks()
                end

                local pressed,checked = tImGui.MenuItem("Invert Selected Bricks", "Ctrl+I", false)
                if pressed then
                    tTile:invertSelectedBricks()
                end

                local pressed,checked = tImGui.MenuItem("Unselect All Bricks", "Ctrl+U / Esc", false)
                if pressed then
                    tTile:unselectAllBricks()
                end

                local pressed,checked = tImGui.MenuItem("Delete Selected Bricks", "Delete", false)
                if pressed then
                    tTile:deleteSelectedBricks()
                end

            else
                tImGui.TextDisabled('Please select a layer!')
            end
            tImGui.EndMenu();
        end

        if tImGui.BeginMenu("Image") then

            local pressed,checked = tImGui.MenuItem("Set Texture Path", nil, false)
            if pressed then
                local sFileName = mbm.openMultiFile(tTextureTileSet[1] or '',"png","jpeg","jpg","bmp","gif","psd","pic","pnm","hdr","tga","tif")
                if sFileName then
                    local IDTexture = 0
                    if type(sFileName) == 'table' then
                        local width,height,id, has_alpha = mbm.loadTexture(sFileName[1])
                        IDTexture = id
                        sFileName = sFileName[1]
                    else
                        local width,height,id, has_alpha = mbm.loadTexture(sFileName)
                        IDTexture = id
                    end
                    tUtil.showMessage('Path of texture:\n' .. sFileName .. '\nid:'.. tostring(IDTexture) .. '\n\nadded to the engine!\nThe next tile which depends on that path will know where to search.')
                end
            end
            tImGui.EndMenu();
        end

        if tImGui.BeginMenu("General Options") then

            local pressed,checked = tImGui.MenuItem("Undo", "Ctrl+Z", false)
            if pressed then
                onUndo()
            end

            local pressed,checked = tImGui.MenuItem("Re-do", "Ctrl+Y", false)
            if pressed then
                onRedo()
            end

            tImGui.Separator()

            local pressed,checked = tImGui.MenuItem("Show Map Options", false, bEnableMainTabBar)
            if pressed then
                bEnableMainTabBar = true
            end

            tImGui.Separator()

            local pressed,checked = tImGui.MenuItem("Enable Origin Lines", true, tLineCenterX.visible)
            if pressed then
                tLineCenterX.visible = checked
                tLineCenterY.visible = checked
            end

            local pressed,checked = tImGui.MenuItem("Move Windows", true, bEnableMoveWindow)
            if pressed then
                bEnableMoveWindow = checked
                if bEnableMoveWindow then
                    ImGuiWindowFlags_NoMove = 0
                else
                    ImGuiWindowFlags_NoMove = tImGui.Flags('ImGuiWindowFlags_NoMove')
                end
            end

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
                        tColorBackgroundGlobal = color
                    end
                end
                tImGui.EndMenu()
            end

            tImGui.EndMenu()
        end

        if tImGui.BeginMenu("Zoom") then

            local label   = '##Scale'
            local v_min   = 0.2
            local v_max   = 10
            local format  = "Scale %.1f"
            local power   = 1.0
            local result, fValue = tImGui.SliderFloat(label, tTile:getScale().x, v_min, v_max, format,power)
            if result then
                tTile:setScale({x=fValue,y=fValue})
            end
            tImGui.TextDisabled("Or use Scroll")
            tImGui.SameLine()
            if tImGui.SmallButton("Default") then
                tTile:setScale({x=1,y=1})
            end
            tImGui.EndMenu()
        end


        if tImGui.BeginMenu("About") then
            local pressed,checked = tImGui.MenuItem("Tile Map Editor", nil, false)
            if pressed then
                if mbm.is('windows') then
                    os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/editors.html#tile-map-editor"')
                elseif mbm.is('linux') then
                    os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/editors.html#tile-map-editor"')
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
                tImGui.TextDisabled(string.format('%s\nTile Map Editor: %s \nIMGUI: %s', mbm.get('version'),tTile.version(),tImGui.GetVersion()))
                tImGui.EndMenu()
            end
            
            tImGui.EndMenu()
        end

        tImGui.EndMainMenuBar()
    end
end

function onTouchDown(key,x,y)
    tLineSelection.xStart = x
    tLineSelection.yStart = y
    tLineSelection.visible = keyShiftPressed
    tMovingObjectMap = isOverAnyObjectMap(x,y)
    if tMovingObjectMap then
        tMovingObjectMap.mx = x
        tMovingObjectMap.my = y
    elseif not tImGui.IsAnyWindowHovered() then
        isClickedMouseLeft = (key == 1)
        camera2d.mx = x
        camera2d.my = y
        tClicked = {x = x, y = y}
    end
end

function onTouchMove(key,x,y)
    
    local isOver = tMovingObjectMap ~= nil or tImGui.IsAnyWindowHovered()
    if isOver then
        tTile:setOverBrick(-x,-y)
    else
        tOverBrick = {x = x, y = y}
        tTile:setOverBrick(x,y)
    end

    if tMovingObjectMap then
        local scale  = tTile:getScale()
        if tMovingObjectMap.type == 'line' then
            x,y = mbm.to2dw(x,y)
            local tShapePoint = tMovingObjectMap.tShapeCircles[tMovingObjectMap.indexOver]
            local index = tMovingObjectMap.indexOver * 2
            tMovingObjectMap.tObj[index-1] = x / scale.x
            tMovingObjectMap.tObj[index]   = y / scale.y
            tTile:setObjectMap(tMovingObjectMap.index,tMovingObjectMap.tObj)
        else
            local px = (tMovingObjectMap.mx - x)
            local py = (tMovingObjectMap.my - y)
            tMovingObjectMap.mx = x
            tMovingObjectMap.my = y
            tMovingObjectMap:setPos(tMovingObjectMap.x - px,tMovingObjectMap.y + py)
            tMovingObjectMap.tObj.x = tMovingObjectMap.x / scale.x
            tMovingObjectMap.tObj.y = tMovingObjectMap.y / scale.y
            tTile:setObjectMap(tMovingObjectMap.index,tMovingObjectMap.tObj)
        end
    end
    
    if not isOver then
        if keyShiftPressed then
            local tLines = {tLineSelection.xStart, tLineSelection.yStart, 
                            x, tLineSelection.yStart, 
                            x, y, 
                            tLineSelection.xStart,y, 
                            tLineSelection.xStart, tLineSelection.yStart}
            tLineSelection:set(tLines,1)
        elseif isClickedMouseLeft and keyControlPressed then
            local index = tTile:getIndexTileIdOver(x,y)
            if index > 0 and tEditorOptions.iBrickIdSelected ~= 0 then
                tTile:setBrickToLayer(tEditorOptions.iBrickIdSelected)
                addHistoric()
            end
        elseif isClickedMouseLeft and not isOver then
            local px = (camera2d.mx - x) * camera2d.sx
            local py = (camera2d.my - y) * camera2d.sy
            camera2d.mx = x
            camera2d.my = y
            camera2d:setPos(camera2d.x + px,camera2d.y - py)
        end
    end
end

function setBoundsSubTiles(tBound)
    if tLineRectTile.visible then
        local tMinBound          = {x=0,y=0}
        local tMaxBound          = {x=0,y=0}
        tMinBound.x, tMinBound.y = mbm.to2dw(tBound.xStart, tBound.yStart)
        tMaxBound.x, tMaxBound.y = mbm.to2dw(tBound.xEnd,   tBound.yEnd)
        local scale              = tTile:getScale()
        local iHalfTextureWidth  = tEditorOptions.iWidth  * 0.5 * scale.x
        local iHalfTextureHeight = tEditorOptions.iHeight * 0.5 * scale.y
        local iHeight            = tEditorOptions.iHeight * scale.y
        tEditorOptions.tSubTilesToImport.xStart, tEditorOptions.tSubTilesToImport.yStart = (tMinBound.x + iHalfTextureWidth), ((iHeight - tMinBound.y) - iHalfTextureHeight)
        tEditorOptions.tSubTilesToImport.xEnd,   tEditorOptions.tSubTilesToImport.yEnd   = (tMaxBound.x + iHalfTextureWidth), ((iHeight - tMaxBound.y) - iHalfTextureHeight)
        local width   = tEditorOptions.tSubTilesToImport.xEnd - tEditorOptions.tSubTilesToImport.xStart
        local height  = tEditorOptions.tSubTilesToImport.yEnd - tEditorOptions.tSubTilesToImport.yStart
        local frac_x  = (((tEditorOptions.tSubTilesToImport.xStart) / (scale.x * tEditorOptions.iWidth))) 
        local frac_y  = (((tEditorOptions.tSubTilesToImport.yStart) / (scale.y * tEditorOptions.iHeight)))
        tEditorOptions.tSubTilesToImport.xStart = frac_x * tEditorOptions.iWidth
        tEditorOptions.tSubTilesToImport.yStart = frac_y * tEditorOptions.iHeight
        tEditorOptions.tSubTilesToImport.xEnd = tEditorOptions.tSubTilesToImport.xStart + (width  / scale.x)
        tEditorOptions.tSubTilesToImport.yEnd = tEditorOptions.tSubTilesToImport.yStart + (height / scale.y)
    end
end

function onTouchUp(key,x,y)
    tLineSelection.visible = false
    tMovingObjectMap = nil
    iClickedAndSelectedBrick = false
    if keyShiftPressed then
        local tLines = {tLineSelection.xStart, tLineSelection.yStart, 
                        x, tLineSelection.yStart, 
                        x, y, 
                        tLineSelection.xStart,y, 
                        tLineSelection.xStart, tLineSelection.yStart}
        if x > tLineSelection.xStart then
            local xStart,yStart = tLineSelection.xStart,tLineSelection.yStart
            local xEnd,yEnd     = x,y
            tTile:selectTiles({x = xStart, y = yStart},{x = xEnd,y = yEnd})
            setBoundsSubTiles({xStart = xStart,yStart = yStart,xEnd = xEnd,yEnd = yEnd})
        else
            local xStart,yStart = x,y
            local xEnd,yEnd     = tLineSelection.xStart,tLineSelection.yStart
            tTile:selectTiles({x = xStart, y = yStart},{x = xEnd,y = yEnd})
            setBoundsSubTiles({xStart = xStart,yStart = yStart,xEnd = xEnd,yEnd = yEnd})
        end
    end
    if isClickedMouseLeft and tClicked.x == x and tClicked.y == y then
        local index = tTile:getIndexTileIdOver(x,y)
        if index > 0 and tEditorOptions.iBrickIdSelected ~= 0 then
            if keyControlPressed or keyShiftPressed then
                local unique = false
                local iTotalSelected = tTile:selectBrick(index,unique)
                if iTotalSelected == 1 then
                    local ID = tTile:getFirstSelectedBrick()
                    if ID ~= 0 then
                        tEditorOptions.iBrickIdSelected = ID
                        tEditorOptions.iIndexSelectedBrickMenuLayer = ID
                        tEditorOptions.iLayerFilterBrick = 1
                    end
                end
            else
                local isSelected = tTile:isBrickSelected(index)
                if isSelected then
                    tTile:unselectBrick(index)
                else
                    tTile:unselectAllBricks()
                end
                tTile:setBrickToLayer(tEditorOptions.iBrickIdSelected)
                addHistoric()
            end
        end
    end
    isClickedMouseLeft = false
    camera2d.mx = x
    camera2d.my = y
end

function onTouchZoom(zoom)
    if not tImGui.IsAnyWindowHovered() then
        local scale = tTile:getScale()
        scale.x = zoom * 0.02 +  scale.x
        if scale.x < 0.1 then
            scale.x = 0.1
        end
        scale.y = scale.x
        tTile:setScale(scale)
    elseif keyControlPressed and bIsOverSizeBrickSelector then
        tEditorOptions.iSizeBrickOnSelector = math.max(math.min(tEditorOptions.iSizeBrickOnSelector + (zoom * 2),300),30)
    end
end

function onUndo()
    if tTile:unDo() == true then
        tTimeToSave:stop()
        colorStatusHistoric = {r = 1.0, g = 0.596, b = 0.085}
        tTimeShowingStatusHistoric:stop()
        tTimeShowingStatusHistoric:start()
    else
        tUtil.showMessageWarn('There is no more commando to undo!')
    end
end

function onRedo()
    if tTile:redDo() == true then
        tTimeToSave:stop()
        colorStatusHistoric = {r=0.174,g=0.192,b=0.789,a=1}
        tTimeShowingStatusHistoric:stop()
        tTimeShowingStatusHistoric:start()
    else
        tUtil.showMessageWarn('There is no more commando to re-do!')
    end
end

function onKeyDown(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = true
    elseif key == mbm.getKeyCode('shift') then
        keyShiftPressed = true
    elseif keyControlPressed then
        if key == mbm.getKeyCode('S') then -- Ctrl+S
            onSaveTileBinary()
        elseif key == mbm.getKeyCode('O') then -- Ctrl+O
            onOpenTileBinary()
        elseif key == mbm.getKeyCode('A') then -- Ctrl+A
            tTile:selectAllBricks()
        elseif key == mbm.getKeyCode('I') then -- Ctrl+I
            tTile:invertSelectedBricks()
        elseif key == mbm.getKeyCode('U') then -- Ctrl+U
            tTile:unselectAllBricks()
        elseif key == mbm.getKeyCode('Z') then -- Ctrl+Z
            onUndo()
        elseif key == mbm.getKeyCode('Y') then -- Ctrl+Y
            onRedo()
        elseif key == mbm.getKeyCode('V') then -- Ctrl+V
            tTile:duplicateSelectedTiles(tOverBrick.x,tOverBrick.y)
        end
    elseif keyShiftPressed then
        if key == mbm.getKeyCode('INSERT') then -- Ctrl+V
            tTile:duplicateSelectedTiles(tOverBrick.x,tOverBrick.y)
        end
    elseif key == mbm.getKeyCode('delete') then
        tTile:deleteSelectedBricks()
    elseif key == mbm.getKeyCode('esc') then
        tTile:unselectAllBricks()
    elseif key == mbm.getKeyCode('Z') or key == mbm.getKeyCode('Q') then -- Q
        onUndo()
    elseif key == mbm.getKeyCode('Y') then -- Y
        onRedo()
    elseif key == mbm.getKeyCode('Tab') then -- Tab
        keyTabPressed = true
    end
end

function onKeyUp(key)
    keyControlPressed = false
    keyShiftPressed = false
end

function loop(delta)
    main_menu_tiled()
    if bEnableMainTabBar then
        main_tab_bar()
    end
    tUtil.showOverlayMessage()
    if sRenderWhat == 'brick' then
        if tEditorOptions.iIndexDrawBrick > 0 and tEditorOptions.iIndexDrawBrick <=  tTile:getTotalBricks() then
            local tBrick = tTile:getBrick(tEditorOptions.iIndexDrawBrick)
            local sBrickId = string.format('Brick id [%d]',tBrick.id)
            local sBrick = string.format('%s\nwidth %d\nheight %d\nScale %g',tBrick.texture,tBrick.width,tBrick.height,tTile:getScale().x)
            tUtil.showStatusMessage(sBrickId,sBrick)
        end
    elseif sRenderWhat == 'tileset' then
        if tEditorOptions.iIndexDrawTileSet > 0 and tEditorOptions.iIndexDrawTileSet <=  tTile:getTotalTileSet() then
            local sName        = tTile:getTileSetName(tEditorOptions.iIndexDrawTileSet)
            local iWidth       = tTile:getTileSetWidth(tEditorOptions.iIndexDrawTileSet)
            local iHeight      = tTile:getTileSetHeight(tEditorOptions.iIndexDrawTileSet)
            local iTotalBricks = tTile:getTotalBricks(sName)
            local sTileSet     = string.format('TileSet [%s]',sName)
            local sMessage     = string.format('Bricks [%d]\nwidth %d\nheight %d\nScale %g',iTotalBricks,iWidth,iHeight,tTile:getScale().x)
            tUtil.showStatusMessage(sTileSet,sMessage)
        end
        tCircleEditVuBrick.visible = false
    elseif sRenderWhat == 'layer' then
        if tEditorOptions.iIndexDrawLayer > 0 and tEditorOptions.iIndexDrawLayer <=  tTile:getTotalLayer() then
            showTileTools()
        end
    elseif sRenderWhat == 'map' then
        local sMapText     = 'Map'
        local sMessage     = string.format('Scale %g',tTile:getScale().x)
        tUtil.showStatusMessage(sMapText,sMessage)
    else
        tCircleEditVuBrick.visible = false
    end
    updateObjectsOnMap()

    if bAddHistoryFlag then
        bAddHistoryFlag = false
        tTile:addHistoric()
    end

    if tTimeShowingStatusHistoric:isRunning() then
        tImGui.SetImDrawListToForeground(true)
        local iW, iH        = mbm.getRealSizeScreen()
        local center        = {x=iW -30,y=iH -30}
        local radius        = 15
        local num_segments  =  12

        tImGui.AddCircleFilled(center, radius, colorStatusHistoric, num_segments)
        tImGui.SetImDrawListToForeground(false)
    end

    if keyTabPressed and tImGui.IsAnyItemActive() == false then
        if sRenderWhat == 'map' then
            ImGuiTabItemFlags = {tileset = tImGui.Flags('ImGuiTabItemFlags_SetSelected') }
        elseif sRenderWhat == 'tileset' then
            ImGuiTabItemFlags = {layer = tImGui.Flags('ImGuiTabItemFlags_SetSelected') }
        elseif sRenderWhat == 'layer' then
            ImGuiTabItemFlags = {brick = tImGui.Flags('ImGuiTabItemFlags_SetSelected') }
        elseif sRenderWhat == 'brick' then
            ImGuiTabItemFlags = {map = tImGui.Flags('ImGuiTabItemFlags_SetSelected') }
        end
    end
    keyTabPressed = false
end
