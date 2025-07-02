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

   Texture Packer Editor

   This is a script based on mbm engine.

   Texture Packer Editor is a 2D level editor meant to join texture in a single file (sprite sheet)

   More info at: https://mbm-documentation.readthedocs.io/en/latest/editors.html#texture-packer-editor

]]--

tImGui        =     require "ImGui"
tUtil         =     require "editor_utils"

if not mbm.get('USE_EDITOR_FEATURES') then
	mbm.messageBox('Missing features','Is necessary to compile using USE_EDITOR FEATURES to run this editor','ok','error',0)
	mbm.quit()
end

function onInitScene()
    
    tWindowsTitle        = {title_image_selector    = 'Image(s) selector',
                            title_texture_options   = 'Texture options'}

    camera2d		     = mbm.getCamera("2d")
    tLineCenterX         = line:new("2dw",0,0,50)
    tLineCenterY         = line:new("2dw",0,0,50)
    tLineCenterX:add({-9999999,0, 9999999,0})
    tLineCenterY:add({0,-9999999, 0,9999999})
    tLineCenterX:setColor(1,0,0)
    tLineCenterY:setColor(0,1,0)
    ImGuiWindowFlags_NoMove     = tImGui.Flags('ImGuiWindowFlags_NoMove')
    tUtil.bRightSide            = true
    tUtil.sMessageOverlay       = 'Welcome to Texture Packer Editor!'
    tClicked = {x = 0, y = 0}
    tLineCenterX.visible = false
    tLineCenterY.visible = false
    keyControlPressed    = false
    keyShiftPressed      = false
    isClickedMouseLeft   = false
    tTexturesToEditor    = {}
    sLastTextureOpened  = ''
    bViewTextureOptions  = false
    bTextureViewOpened   = false
    scale                = 1

    tTextureOptions = { fWidth    = 1024, 
                        fHeight   = 1024,
                        bPowerOf2 = true,
                        iSpaceX   = 0,
                        iSpaceY   = 0,
                        iOffsetX  = 0,
                        iOffsetY  = 0,
                        iGridX    = 1,
                        iGridY    = 1,
                        bGridVisibleX = true,
                        bGridVisibleY = true,
                        iMaxTileCount  = 0,
                        bAxisY    = false,
                        bBiggerTex = true,
                        bFilter   = true,
                        scaleImage= 1,
                        sumScaleImageX=0,
                        sumScaleImageY=0,
                        tRgba     = {r=1,g=1,b=1,a=0},
                        bAlpha    = true
    }

    tRender = render2texture:new('2dw')
    tRender:setColor(tTextureOptions.tRgba.r,tTextureOptions.tRgba.g,tTextureOptions.tRgba.b,tTextureOptions.tRgba.a)
    tShape = shape:new('2dw')
    tLine  = line:new('2dw',0,0,-100)
    tLineGridX  = line:new('2dw',0,0,-100)
    tLineGridY  = line:new('2dw',0,0,-100)
    tLine:add({0,0,1,1})
    tLineGridX:add({0,0,1,1})
    tLineGridY:add({0,0,1,1})
    tLineGridX.visible = false
    tLineGridY.visible = false
    tLineGridX:setColor(1,1,0)
    tLineGridY:setColor(1,1,0)
    tLine.visible = false
    tRender.width = 0
    tRender.height = 0
    iNextNickName = 0
    tStatusMessageSize = {x=0,y=0}
    sFileNameTexture = ''
end

function onSaveTexture()
    local sFileName = mbm.saveFile(sFileNameTexture,'*.png')
    if sFileName then
        if tRender:save(sFileName) then
            sFileNameTexture = sFileName
            tUtil.showMessage('Texture Saved Successfully!')
        else
            tUtil.showMessageWarn('Failed to Save Texture File!')
        end
    end
end

function onOpenTextures()
    mbm.enableTextureFilter(tTextureOptions.bFilter)
    local file_name = mbm.openMultiFile(sLastTextureOpened or '',"png","jpeg","jpg","bmp","gif","psd","pic","pnm","hdr","tga","tif")
    if file_name then
        tTexturesToEditor = tUtil.loadInfoImagesToTable(file_name,tTexturesToEditor)
        bTextureViewOpened = true
        bViewTextureOptions = true
        if type(file_name) == 'string' then
            sLastTextureOpened = file_name
        elseif type(file_name) == 'table' and #file_name > 0 then
            sLastTextureOpened = file_name[1]
            adjustTextureSize()
            for i=1, #tTexturesToEditor do
                local tDesc = tTexturesToEditor[i]
                tDesc.isSelected = true
                local tTex   = texture:new('2dw')
                tTex:load(tDesc.file_name,tDesc.width,tDesc.height)
                tTexturesToEditor[i].tTex = tTex
                tRender:add(tTex)
            end
        end
    end
    mbm.enableTextureFilter(true)
end

function onOpenTexturesFromFolder()
    mbm.enableTextureFilter(tTextureOptions.bFilter)
    local file_name = mbm.openFolder(sLastTextureOpened)
    if file_name then
        if file_name then
            file_name = tUtil.loadInfoImagesFromFolderToTable(file_name,tTexturesToEditor)
            bTextureViewOpened = true
            bViewTextureOptions = true
            if type(file_name) == 'string' then
                sLastTextureOpened = file_name
            elseif type(file_name) == 'table' and #file_name > 0 then
                sLastTextureOpened = file_name[1]
                for i=1, #tTexturesToEditor do
                    local tDesc = tTexturesToEditor[i]
                    tDesc.isSelected = true
                    local tTex   = texture:new('2dw')
                    tTex:load(tDesc.file_name,tDesc.width,tDesc.height)
                    tTexturesToEditor[i].tTex = tTex
                    tRender:add(tTex)
                end
            end
        end
    end
    mbm.enableTextureFilter(true)
end

function getNextNickName()
    iNextNickName = iNextNickName + 1
    return string.format('dynamic-tex-%d',iNextNickName)
end

function adjustTextureSize()
    if  tRender.width ~= tTextureOptions.fWidth or 
        tRender.height ~= tTextureOptions.fHeight or
        tRender.alpha ~= tTextureOptions.bAlpha then

        tRender.width  = tTextureOptions.fWidth
        tRender.height = tTextureOptions.fHeight
        tRender.alpha  = tTextureOptions.bAlpha

        tRender:release()
        local result, texture_name, id = tRender:create(tTextureOptions.fWidth,tTextureOptions.fHeight,tTextureOptions.bAlpha,getNextNickName())
        if result then
            tRender:enableFrame(false)
            tShape:destroy()
            tShape = shape:new('2dw')
            local half_width  = tTextureOptions.fWidth  * 0.5
            local half_height = tTextureOptions.fHeight * 0.5
            local tVertex     = {-half_width ,-half_height,  -half_width,half_height,  half_width,-half_height,  half_width,half_height}
            local tUv         = {0,0,           0,1,      1,0,       1,1  }
            local tIndex      = {1,2,3, 3,2,4 }
            local tNormal     = nil
            
            tShape:createIndexed(tVertex,tIndex,tUv,tNormal,bAlpha,getNextNickName())
            tShape:setTexture(texture_name)
            tShape:setScale(scale,scale)

            tLine.visible = true
            tLine:set({-half_width ,-half_height,  -half_width ,half_height,  half_width ,half_height,   half_width ,-half_height, -half_width ,-half_height  },1)
            tLine:setScale(scale,scale)
            tLine:setColor(1,1,0)
        else
            tUtil.showMessageWarn('Failed to create dynamic texture\nTry to reduce the size of texture!')
        end
    end
end

function getBiggerTextureSize()
    local width, height = 0,0
    for i=1, #tTexturesToEditor do
        local tTexture = tTexturesToEditor[i]
        local tTex     = tTexture.tTex
        if tTex and tTexture.isSelected then
            local w, h  = tTex:getSize()
            width  = math.max(w,width)
            height = math.max(h,height)
        end
    end
    return width, height
end

function drawSpriteSheet()
    if #tTexturesToEditor > 0 then

        adjustTextureSize()

        local x_initial,y_initial = tTextureOptions.iOffsetX - (tRender.width * 0.5),(tRender.height * 0.5) - tTextureOptions.iOffsetY
        local x_final,  y_final   = tRender.width * 0.5, tRender.height * -0.5
        local x,y                 = x_initial,y_initial
        local iTotalIn            = 0
        local iTotalSelected      = 0
        local iCountMaxTile       = 0
        local bCheckTile          = tTextureOptions.iMaxTileCount > 0

        if tTextureOptions.bAxisY then
            for i=1, #tTexturesToEditor do
                local tTexture = tTexturesToEditor[i]
                local tTex     = tTexture.tTex
                if tTex and tTexture.isSelected then
                    tRender:add(tTex)
                    tTex.visible = true
                    tTex:setScale(tTextureOptions.scaleImage + tTextureOptions.sumScaleImageX,tTextureOptions.scaleImage + tTextureOptions.sumScaleImageY)
                    local width, height  = tTex:getSize()

                    if tTextureOptions.bBiggerTex then
                        width, height = getBiggerTextureSize()
                    end
                    
                    if i == 1 then
                        local half_width_tex  = width  * 0.5
                        local half_height_tex = height * 0.5
                        x = x + half_width_tex
                        y = y - half_height_tex
                    end

                    x = x + (tTexture.iOffsetPerTextureX or 0)
                    y = y + (tTexture.iOffsetPerTextureY or 0)
                    tTex:setPos(x,y)

                    if tTextureOptions.iMaxTileCount > 0 then
                        iCountMaxTile = iCountMaxTile + 1
                    end

                    if (x + (width  * 0.5)) <= x_final and
                        not ((y + (height  * 0.5)) < y_final) then
                            iTotalIn = iTotalIn + 1
                    end
                    
                    y = y - height - tTextureOptions.iSpaceY

                    if (bCheckTile and iCountMaxTile >= tTextureOptions.iMaxTileCount) or ((y - (height  * 0.5)) < y_final) then
                        x = x + width + tTextureOptions.iSpaceX
                        y = y_initial - (height  * 0.5)
                        iCountMaxTile = 0
                    end
                    iTotalSelected = iTotalSelected + 1
                elseif tTex then
                    tRender:remove(tTex)
                    tTex.visible = false
                end
            end
        else
            for i=1, #tTexturesToEditor do
                local tTexture = tTexturesToEditor[i]
                local tTex     = tTexture.tTex
                if tTex and tTexture.isSelected then
                    tRender:add(tTex)
                    tTex.visible = true
                    tTex:setScale(tTextureOptions.scaleImage + tTextureOptions.sumScaleImageX,tTextureOptions.scaleImage + tTextureOptions.sumScaleImageY)
                    local width, height  = tTex:getSize()

                    if tTextureOptions.bBiggerTex then
                        width, height = getBiggerTextureSize()
                    end
                    
                    if (bCheckTile and iCountMaxTile >= tTextureOptions.iMaxTileCount) or ((x - (width  * 0.5) ) > x_final) then
                        local half_width_tex = width * 0.5
                        x = x_initial + half_width_tex
                        y = y - height - tTextureOptions.iSpaceY
                        iCountMaxTile = 0
                    end
                    if i == 1 then
                        local half_width_tex  = width * 0.5
                        local half_height_tex = height * 0.5
                        x = half_width_tex  + x
                        y = y - half_height_tex
                    end

                    x = x + (tTexture.iOffsetPerTextureX or 0)
                    y = y + (tTexture.iOffsetPerTextureY or 0)
                    tTex:setPos(x,y)

                    if tTextureOptions.iMaxTileCount > 0 then
                        iCountMaxTile = iCountMaxTile + 1
                    end

                    if (x + (width  * 0.5)) <= x_final and (y - (height  * 0.5)) >= y_final then
                        iTotalIn = iTotalIn + 1
                    end
                    iTotalSelected = iTotalSelected + 1
                    x = x + width + tTextureOptions.iSpaceX
                elseif tTex then
                    tRender:remove(tTex)
                    tTex.visible = false
                end
            end
        end
        tLine:setScale(scale,scale)
        showPendingTextureMessage(iTotalIn == iTotalSelected, 'Status of Texture',string.format('%d of %d are inside.\nTotal existent %d',iTotalIn,iTotalSelected,#tTexturesToEditor))
    end
end

function showTextureOptions()
    if bViewTextureOptions then
        local width = 220
        local x_pos, y_pos = 0, 0
        local max_width = 220
        local tSizeBtnAddSet   = {x=43,y=0} 
        tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_texture_options,x_pos,y_pos,width,max_width)
        local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_texture_options, true, ImGuiWindowFlags_NoMove)
        if is_opened then
            
            local step       =  1
            local step_fast  =  100
            local flags      =  0

            local getNextPowerOf2 = function(x)
                local l2       = math.log(x,2)
                local  nextnum = math.ceil(l2)
                local  result  = 2 ^ nextnum
                return  math.ceil(result)
            end

            local getPreviousPowerOf2 = function(x)
                local l2        = math.log(x,2)
                local  previous = math.ceil(l2)
                local  result   = 2 ^ (previous-1)
                return  math.ceil(result)
            end

            tImGui.Text('Width')
            local result, iValue = tImGui.InputInt('##WidthTexture', tTextureOptions.fWidth, step, step_fast, flags)
            if result and iValue > 0 then
                if tTextureOptions.bPowerOf2 then
                    if iValue > tTextureOptions.fWidth then
                        iValue = getNextPowerOf2(iValue)
                    else
                        iValue = getPreviousPowerOf2(iValue)
                    end
                end
                tTextureOptions.fWidth = iValue
            end

            tImGui.Text('Height')
            local result, iValue = tImGui.InputInt('##HeightTexture', tTextureOptions.fHeight, step, step_fast, flags)
            if result and iValue > 0 then
                if tTextureOptions.bPowerOf2 then
                    if iValue > tTextureOptions.fHeight then
                        iValue = getNextPowerOf2(iValue)
                    else
                        iValue = getPreviousPowerOf2(iValue)
                    end
                end
                tTextureOptions.fHeight = iValue
            end

            tImGui.Text('Space X')
            local result, iValue = tImGui.InputInt('##SpaceXTexture', tTextureOptions.iSpaceX, step, step_fast, flags)
            if result then
                tTextureOptions.iSpaceX = iValue
            end

            tImGui.Text('Space Y')
            local result, iValue = tImGui.InputInt('##SpaceYTexture', tTextureOptions.iSpaceY, step, step_fast, flags)
            if result then
                tTextureOptions.iSpaceY = iValue
            end

            tImGui.Text('Offset X')
            local result, iValue = tImGui.InputInt('##OffsetXTexture', tTextureOptions.iOffsetX, step, step_fast, flags)
            if result then
                tTextureOptions.iOffsetX = iValue
            end

            tImGui.Text('Offset Y')
            local result, iValue = tImGui.InputInt('##OffsetYTexture', tTextureOptions.iOffsetY, step, step_fast, flags)
            if result then
                tTextureOptions.iOffsetY = iValue
            end

            tImGui.Text('Max Tile Count')
            tImGui.SameLine()
            tImGui.HelpMarker('Zero means automatic')
            local result, iValue = tImGui.InputInt('##MaxTileCount', tTextureOptions.iMaxTileCount, step, step_fast, flags)
            if result and iValue >= 0 then
                tTextureOptions.iMaxTileCount = iValue
            end

            tImGui.Text('Grid X')
            tImGui.SameLine()
            tImGui.HelpMarker('Visual grid on X axis to help to preview the sprite sheet')
            tImGui.SameLine()
            tImGui.SetCursorPosX(130)
            tTextureOptions.bGridVisibleX = tImGui.Checkbox('##ShowGridX',tTextureOptions.bGridVisibleX)
            local result, iValue = tImGui.InputInt('##GridX', tTextureOptions.iGridX, step, step_fast, flags)
            if result and iValue >= 1 and iValue <= (tTextureOptions.fWidth /2) then
                tTextureOptions.iGridX = iValue
            end

            tImGui.Text('Grid Y')
            tImGui.SameLine()
            tImGui.HelpMarker('Visual grid on Y axis to help to preview the sprite sheet')
            tImGui.SameLine()
            tImGui.SetCursorPosX(130)
            tTextureOptions.bGridVisibleY = tImGui.Checkbox('##ShowGridY',tTextureOptions.bGridVisibleY)
            local result, iValue = tImGui.InputInt('##iGridY', tTextureOptions.iGridY, step, step_fast, flags)
            if result and iValue >= 1 and iValue < (tTextureOptions.fHeight / 2) then
                tTextureOptions.iGridY = iValue
            end

            tImGui.Text('Scale Image')
            local step       =  0.01
            local step_fast  =  0.02
            local format     = "%.3f"
            local result, fValue = tImGui.InputFloat('##ScaleImageRect', tTextureOptions.scaleImage, step, step_fast, format, flags)
            if result and fValue > 0 then
                tTextureOptions.scaleImage = fValue
            end

            tImGui.Text('Adjust scale on X')
            local step       =  0.001
            local step_fast  =  0.002
            local format     = "%.3f"
            local result, fValue = tImGui.InputFloat('##SumScaleImageRectX', tTextureOptions.sumScaleImageX, step, step_fast, format, flags)
            if result then
                if fValue <= -1.0 then
                    tTextureOptions.sumScaleImageX = -1.0
                elseif fValue >= 1.0 then
                    tTextureOptions.sumScaleImageX = 1.0
                else
                    tTextureOptions.sumScaleImageX = fValue
                end
            end

            tImGui.Text('Adjust scale on Y')
            local step       =  0.001
            local step_fast  =  0.002
            local format     = "%.3f"
            local result, fValue = tImGui.InputFloat('##SumScaleImageRectY', tTextureOptions.sumScaleImageY, step, step_fast, format, flags)
            if result then
                if fValue <= -1.0 then
                    tTextureOptions.sumScaleImageY = -1.0
                elseif fValue >= 1.0 then
                    tTextureOptions.sumScaleImageY = 1.0
                else
                    tTextureOptions.sumScaleImageY = fValue
                end
            end

            tImGui.Text('Background Color')
            local clicked, tRgba = tImGui.ColorEdit4('Select your color', tTextureOptions.tRgba, tImGui.Flags('ImGuiColorEditFlags_NoLabel'))
            if clicked then
                tTextureOptions.tRgba = tRgba
                tRender:setColor(tRgba.r,tRgba.g,tRgba.b,tRgba.a)
            end

            tTextureOptions.bPowerOf2 = tImGui.Checkbox('Power Of 2##P2',tTextureOptions.bPowerOf2)

            tTextureOptions.bAxisY    = tImGui.Checkbox('Axis Y## Axis Y',tTextureOptions.bAxisY)

            tTextureOptions.bBiggerTex    = tImGui.Checkbox('Follow bigger Texture## Bigger Tex',tTextureOptions.bBiggerTex)

            tImGui.NewLine()
            local step       =  1
            local step_fast  =  10

            if tImGui.TreeNode('id_OffsetPerTexture',"Manual Offset") then
                for i=1, #tTexturesToEditor do
                    local sShortName   = tUtil.getShortName(tTexturesToEditor[i].file_name)
                    if tImGui.TreeNode('id_OffsetPerTexture_' .. tostring(i),sShortName) then
                        local result, iValue = tImGui.InputInt('X##OffsetPerTextureX' .. tostring(i), tTexturesToEditor[i].iOffsetPerTextureX or 0, step, step_fast, flags)
                        if result then
                            tTexturesToEditor[i].iOffsetPerTextureX = iValue
                        end

                        local result, iValue = tImGui.InputInt('Y##OffsetPerTextureY' .. tostring(i), tTexturesToEditor[i].iOffsetPerTextureY or 0, step, step_fast, flags)
                        if result then
                            tTexturesToEditor[i].iOffsetPerTextureY = iValue
                        end
                        tImGui.TreePop()
                    end
                end
                tImGui.TreePop()
            end
            --tTextureOptions.bAlpha    = tImGui.Checkbox('Enable Alpha##AlphaTex',tTextureOptions.bAlpha)
        end
        if closed_clicked then
            bViewTextureOptions = false
        end
        tImGui.End()
    end
end

function showPendingTextureMessage (bAllIn, sMessageColored,sMessageGrayed)
    local iW, iH     = mbm.getRealSizeScreen()
    local flags = {'ImGuiWindowFlags_NoDecoration', 'ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoSavedSettings', 'ImGuiWindowFlags_NoFocusOnAppearing', 'ImGuiWindowFlags_NoNav'}
    tImGui.SetNextWindowBgAlpha(0.75);
    local window_pos = {x = iW - (tStatusMessageSize.x * 0.5) - (iW * 0.5), y = iH - tStatusMessageSize.y}
    local window_pos_pivot = {x = 0, y = 0}
    tImGui.SetNextWindowPos(window_pos, 0, window_pos_pivot);
    local is_opened, closed_clicked = tImGui.Begin('##StatusWindows', false,tImGui.Flags(flags) )
    if is_opened then
        if sMessageColored and sMessageColored:len() > 0 then
            if bAllIn then
                tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=1,b=0,a=0.8})
            else
                tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=0,b=0,a=0.8})
            end
            tImGui.Text(sMessageColored)
            tImGui.PopStyleColor(1)
        end
        if sMessageGrayed and sMessageGrayed:len() > 0 then
            tImGui.Text(sMessageGrayed)
        end
    end
    tStatusMessageSize      = tImGui.GetWindowSize()
    tImGui.End()
end

function main_menu_texture_packer()
    if tImGui.BeginMainMenuBar() then
        if tImGui.BeginMenu("File") then

            if mbm.is('Windows') then
                local pressed,checked = tImGui.MenuItem("Load Texture (Max 32 files)", "Ctrl+O", false)
                if pressed then
                    onOpenTextures()
                end
            else
                local pressed,checked = tImGui.MenuItem("Load Texture", "Ctrl+O", false)
                if pressed then
                    onOpenTextures()
                end
            end

            local pressed,checked = tImGui.MenuItem("Load Texture From Folder", nil , false)
            if pressed then
                onOpenTexturesFromFolder()
            end

            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("Save Texture (png)", "Ctrl+S", false)
            if pressed then
                if tRender:isLoaded() then
                    onSaveTexture()
                else
                    tUtil.showMessageWarn('There is no texture loaded!')
                end
            end

            tImGui.Separator()
            if (tTextureOptions.bGridVisibleX or tTextureOptions.bGridVisibleY) and (tTextureOptions.iGridX > 1 or tTextureOptions.iGridY) then

                local pressed,checked = tImGui.MenuItem("Save Texture (png) Rectangles##SaveXRectangle", "X axis orientation ", false)
                if pressed then
                    if tRender:isLoaded() then
                        local iCount       = 1
                        local fx           = tTextureOptions.fWidth  / tTextureOptions.iGridX
                        local fy           = tTextureOptions.fHeight / tTextureOptions.iGridY
                        local sFileName    = mbm.saveFile(sFileNameTexture,'*.png')
                        if sFileName then
                            local sShortName   = tUtil.getShortName(sFileName)
                            local sFolder      = sFileName:sub(1,sFileName:len() - sShortName:len())
                            sShortName         = sShortName:sub(1,sShortName:len()-4)
                            
                            for x = 0, tTextureOptions.fWidth, fx do
                                for y = 0, tTextureOptions.fHeight, fy do
                                    if (x + fx) <= tTextureOptions.fWidth and (y + fy) <= tTextureOptions.fHeight then
                                        local sFullFileName = string.format('%s%s_%d.png',sFolder,sShortName,iCount)
                                        if not tRender:save(sFullFileName, x, y, fx, fy) then
                                            tUtil.showMessageWarn('Failed to Save Texture Rectangle!')
                                        end
                                        iCount = iCount + 1
                                    end
                                end
                            end
                            if iCount > 0 then
                                tUtil.showMessage(string.format('%d textures saved successfully!',iCount-1))
                            else
                                tUtil.showMessageWarn('No one texture was saved!')
                            end
                        end
                    else
                        tUtil.showMessageWarn('There is no texture loaded!')
                    end
                end

                local pressed,checked = tImGui.MenuItem("Save Texture (png) Rectangles##SaveYRectangle", "Y axis orientation ", false)
                if pressed then
                    if tRender:isLoaded() then
                        local iCount       = 1
                        local fx           = tTextureOptions.fWidth  / tTextureOptions.iGridX
                        local fy           = tTextureOptions.fHeight / tTextureOptions.iGridY
                        local sFileName    = mbm.saveFile(sFileNameTexture,'*.png')
                        if sFileName then
                            local sShortName   = tUtil.getShortName(sFileName)
                            local sFolder      = sFileName:sub(1,sFileName:len() - sShortName:len())
                            sShortName         = sShortName:sub(1,sShortName:len()-4)
                            
                            for y = 0, tTextureOptions.fHeight, fy do
                                for x = 0, tTextureOptions.fWidth, fx do
                                    if (x + fx) <= tTextureOptions.fWidth and (y + fy) <= tTextureOptions.fHeight then
                                        local sFullFileName = string.format('%s%s_%d.png',sFolder,sShortName,iCount)
                                        if not tRender:save(sFullFileName, x, y, fx, fy) then
                                            tUtil.showMessageWarn('Failed to Save Texture Rectangle!')
                                        end
                                        iCount = iCount + 1
                                    end
                                end
                            end
                            if iCount > 0 then
                                tUtil.showMessage(string.format('%d textures saved successfully!',iCount-1))
                            else
                                tUtil.showMessageWarn('No one texture was saved!')
                            end
                        end
                    else
                        tUtil.showMessageWarn('There is no texture loaded!')
                    end
                end
            end

            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("Quit", "Alt+F4", false)
            if pressed then
                mbm.quit()
            end

            tImGui.EndMenu();
        end

        if tImGui.BeginMenu("General Options") then

            local pressed,checked = tImGui.MenuItem("View Image List", nil, false)
            if pressed then
                bTextureViewOpened = checked
            end
            tImGui.Separator()

            local pressed,checked = tImGui.MenuItem("Enable Origin Lines", true, tLineCenterX.visible)
            if pressed then
                tLineCenterX.visible = checked
                tLineCenterY.visible = checked
            end

            local pressed,checked = tImGui.MenuItem("Pre load texture (enable filter)", true, tTextureOptions.bFilter)
            if pressed then
                tTextureOptions.bFilter = checked
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
            local result, fValue = tImGui.SliderFloat(label, scale, v_min, v_max, format,power)
            if result and fValue > 0 then
                scale = fValue
                tShape:setScale(scale,scale)
            end
            tImGui.TextDisabled("Or use Scroll")
            tImGui.SameLine()
            if tImGui.SmallButton("Default") then
                scale = 1
                tShape:setScale(scale,scale)
            end
            tImGui.EndMenu()
        end


        if tImGui.BeginMenu("About") then
            local pressed,checked = tImGui.MenuItem("Texture Packer Editor", nil, false)
            if pressed then
                if mbm.is('windows') then
                    os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/editors.html#texture-packer-editor"')
                elseif mbm.is('linux') then
                    os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/editors.html#texture-packer-editor"')
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
        end

        tImGui.EndMainMenuBar()
    end
end

function onTouchDown(key,x,y)
    if not tImGui.IsAnyWindowHovered() then
        isClickedMouseLeft = (key == 1)
        camera2d.mx = x
        camera2d.my = y
        tClicked = {x = x, y = y}
    end
end

function onTouchMove(key,x,y)
    
    if isClickedMouseLeft and not tImGui.IsAnyWindowHovered() then
        local px = (camera2d.mx - x) * camera2d.sx
        local py = (camera2d.my - y) * camera2d.sy
        camera2d.mx = x
        camera2d.my = y
        camera2d:setPos(camera2d.x + px,camera2d.y - py)
    end
end

function onTouchUp(key,x,y)
    isClickedMouseLeft = false
    camera2d.mx = x
    camera2d.my = y
end

function onTouchZoom(zoom)
    if not tImGui.IsAnyWindowHovered() then
        scale = scale + (zoom * 0.2)
        if scale <= 0.2 then
            scale = 0.2
        end
        tShape:setScale(scale,scale)
    end
end

function onKeyDown(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = true
    elseif keyControlPressed then
        if key == mbm.getKeyCode('S') then -- Ctrl+S
            onSaveTexture()
        elseif key == mbm.getKeyCode('O') then -- Ctrl+O
            onOpenTextures()
        end
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = false
    end
end

function loop(delta)
    main_menu_texture_packer()

    if bTextureViewOpened then
        local closed_clicked = tUtil.showTextureAssets(tWindowsTitle.title_image_selector,tTexturesToEditor,0,0,ImGuiWindowFlags_NoMove == 0)
        if closed_clicked then
            bTextureViewOpened = false
        end
    end

    if bViewTextureOptions then
        showTextureOptions()
    end

    tUtil.showOverlayMessage()

    drawSpriteSheet()

    if tTextureOptions.iGridX > 1 and tTextureOptions.bGridVisibleX then
        local fx           = tTextureOptions.fWidth  / tTextureOptions.iGridX
        local half_width   = tTextureOptions.fWidth  * 0.5
        local half_height  = tTextureOptions.fHeight * 0.5
        local tLinesX      = {}
        for x=(-half_width)+fx, half_width, fx do
            table.insert(tLinesX,x)
            table.insert(tLinesX,-half_height)
            table.insert(tLinesX,x)
            table.insert(tLinesX,half_height)
            table.insert(tLinesX,x)
            table.insert(tLinesX,-half_height)
        end
        tLineGridX.visible = true
        tLineGridX:set(tLinesX,1)
        tLineGridX:setScale(scale,scale)
    else
        tLineGridX.visible = false
    end

    if tTextureOptions.iGridY > 1 and tTextureOptions.bGridVisibleY then
        local fy           = tTextureOptions.fHeight / tTextureOptions.iGridY
        local half_width   = tTextureOptions.fWidth  * 0.5
        local half_height  = tTextureOptions.fHeight * 0.5
        local tLinesY     = {}
        for y=(-half_height)+fy, half_height, fy do
            table.insert(tLinesY,-half_width)
            table.insert(tLinesY,y)
            table.insert(tLinesY,half_width)
            table.insert(tLinesY,y)
            table.insert(tLinesY,-half_width)
            table.insert(tLinesY,y)
        end
        tLineGridY.visible = true
        tLineGridY:set(tLinesY,1)
        tLineGridY:setScale(scale,scale)
    else
        tLineGridY.visible = false
    end
end