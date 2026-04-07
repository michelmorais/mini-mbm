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

function onInitScene()
    
    tWindowsTitle        = {title_image_selector    = "title_image_selector",
                            title_texture_options   = "title_texture_options"}

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
    bPrintDebug          = false
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
                        bOnlySelectedTextures = true,
                        bGridVisibleX = true,
                        bGridVisibleY = true,
                        iMaxTileCount  = 0,
                        bAxisY     = false,
                        iIndexSortOption = 1,
                        bSortByName = false,
                        bSortBySizeAscending = false,
                        bSortBySizeDescending= false,
                        indexReferenceTexture = 1,
                        iCurrentAlgorithm = 1,
                        bFilter   = true,
                        bGridForceFitScale = false, 
                        bLastGridForceFitScaleWasEnabled = false,
                        scaleImage= 1,
                        sumScaleImageX=0,
                        sumScaleImageY=0,
                        tRgba     = {r=1,g=1,b=1,a=0},
                        bAlpha    = true
    }

    tRender = render2texture:new('2dw')
    tRender:setColor(tTextureOptions.tRgba.r,tTextureOptions.tRgba.g,tTextureOptions.tRgba.b,tTextureOptions.tRgba.a)
    tShape = shape:new('2dw')
    tShapeHoverImage = shape:new('2dw')
    tShapeHoverImage:create('rectangle',1,1)
    tShapeHoverImage.visible = false
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
    sFileNameTextureCfg = nil
    tComboAlgorithm = { 'Follow bigger Texture',        -- 1
                        'First-Fit (FF)',               -- 2
                        'Best-Fit (BF)' ,               -- 3
                        'Grid-based placement',         -- 4
                        'Grid-force fit placement',     -- 5
                        }
end

function onSaveTexture()
    local sFileName = mbm.saveFile(sFileNameTexture,'*.png')
    if sFileName then
        if tRender:save(sFileName) then
            sFileNameTexture = sFileName
            tUtil.showMessage(tLang.L("texture_saved_ok"))
        else
            tUtil.showMessageWarn(tLang.L("failed_to_save_texture"))
        end
    end
end

function onLoadTextureConfiguration()
    local sFileName = mbm.openFile(sFileNameTextureCfg or '','*.texturecfg')
    if sFileName then
        if mbm.include(sFileName) then
            sFileNameTextureCfg = sFileName
            adjustTextureSize()
            local tTexturesToEditorLoaded = tTexturesToEditor
            tTexturesToEditor = {}
            for i=1, #tTexturesToEditorLoaded do
                local tDesc = tTexturesToEditorLoaded[i]
                tTexturesToEditor = tUtil.loadInfoImagesToTable(tostring(tDesc.file_name),tTexturesToEditor)
            end
            for i=1, #tTexturesToEditor do
                local tDesc = tTexturesToEditor[i]
                tDesc.isSelected = true
                local tTex   = texture:new('2dw')
                tTex:load(tDesc.file_name,tDesc.width,tDesc.height)
                tTexturesToEditor[i].tTex = tTex
                tTexturesToEditor[i].isSelected = tTexturesToEditorLoaded[i].isSelected
                tTexturesToEditor[i].iOffsetPerTextureX = tTexturesToEditorLoaded[i].iOffsetPerTextureX or 0
                tTexturesToEditor[i].iOffsetPerTextureY = tTexturesToEditorLoaded[i].iOffsetPerTextureY or 0
                tTexturesToEditor[i].iAnglePerTextureRX = tTexturesToEditorLoaded[i].iAnglePerTextureRX or 0
                tTexturesToEditor[i].iAnglePerTextureRY = tTexturesToEditorLoaded[i].iAnglePerTextureRY or 0
                tTexturesToEditor[i].iAnglePerTextureRZ = tTexturesToEditorLoaded[i].iAnglePerTextureRZ or 0
                tTexturesToEditor[i].fScalePerTextureSX = tTexturesToEditorLoaded[i].fScalePerTextureSX or 0
                tTexturesToEditor[i].fScalePerTextureSY = tTexturesToEditorLoaded[i].fScalePerTextureSY or 0
                tRender:add(tTex)
            end
            bTextureViewOpened = true
            bViewTextureOptions = true
            tUtil.showMessage(tLang.L("texture_config_loaded_ok"))
        else
            tUtil.showMessageWarn(tLang.L("failed_to_load_texture_config"))
        end
    end
end

function onSaveTextureConfiguration()
    local prevNumericLocale = nil
    if os and os.setlocale then
        prevNumericLocale = os.setlocale(nil, 'numeric')
        os.setlocale('C', 'numeric')
    end
    local sFileName =sFileNameTextureCfg
    if sFileNameTextureCfg == nil then
        sFileName = mbm.saveFile(sFileNameTextureCfg,'*.texturecfg')
    end
    if sFileName then
        local fp = io.open(sFileName,"w")
        if fp == nil then
            tUtil.showMessageWarn(string.format(tLang.L("failed_to_save_config_fmt"), sFileName))
        else
            fp:write(string.format("tTextureOptions = {}\n"))
            fp:write(string.format("tTextureOptions.fWidth = %d\n",  tTextureOptions.fWidth))
            fp:write(string.format("tTextureOptions.fHeight = %d\n",  tTextureOptions.fHeight))
            fp:write(string.format("tTextureOptions.bPowerOf2 = %s\n",  tTextureOptions.bPowerOf2))
            fp:write(string.format("tTextureOptions.iSpaceX = %d\n",  tTextureOptions.iSpaceX))
            fp:write(string.format("tTextureOptions.iSpaceY = %d\n",  tTextureOptions.iSpaceY))
            fp:write(string.format("tTextureOptions.iOffsetX = %d\n",  tTextureOptions.iOffsetX))
            fp:write(string.format("tTextureOptions.iOffsetY = %d\n",  tTextureOptions.iOffsetY))
            fp:write(string.format("tTextureOptions.iGridX = %d\n",  tTextureOptions.iGridX))
            fp:write(string.format("tTextureOptions.iGridY = %d\n",  tTextureOptions.iGridY))
            fp:write(string.format("tTextureOptions.bGridVisibleX = %s\n",  tTextureOptions.bGridVisibleX))
            fp:write(string.format("tTextureOptions.bGridVisibleY = %s\n",  tTextureOptions.bGridVisibleY))
            fp:write(string.format("tTextureOptions.iMaxTileCount = %d\n",  tTextureOptions.iMaxTileCount))
            fp:write(string.format("tTextureOptions.bAxisY = %s\n",  tTextureOptions.bAxisY))
            fp:write(string.format("tTextureOptions.iIndexSortOption = %d\n",  tTextureOptions.iIndexSortOption))
            fp:write(string.format("tTextureOptions.bSortByName = %s\n",  tTextureOptions.bSortByName))
            fp:write(string.format("tTextureOptions.bSortBySizeAscending = %s\n",  tTextureOptions.bSortBySizeAscending))
            fp:write(string.format("tTextureOptions.bSortBySizeDescending = %s\n",  tTextureOptions.bSortBySizeDescending))
            fp:write(string.format("tTextureOptions.indexReferenceTexture = %d\n",  tTextureOptions.indexReferenceTexture))
            fp:write(string.format("tTextureOptions.iCurrentAlgorithm = %d\n",  tTextureOptions.iCurrentAlgorithm))
            fp:write(string.format("tTextureOptions.bFilter = %s\n",  tTextureOptions.bFilter))
            fp:write(string.format("tTextureOptions.bGridForceFitScale = %s\n",  tTextureOptions.bGridForceFitScale))
            fp:write(string.format("tTextureOptions.bLastGridForceFitScaleWasEnabled = %s\n",  tTextureOptions.bLastGridForceFitScaleWasEnabled))
            fp:write(string.format("tTextureOptions.scaleImage = %f\n",  tTextureOptions.scaleImage))
            fp:write(string.format("tTextureOptions.sumScaleImageX = %f\n",  tTextureOptions.sumScaleImageX))
            fp:write(string.format("tTextureOptions.sumScaleImageY = %f\n",  tTextureOptions.sumScaleImageY))
            fp:write(string.format("tTextureOptions.bOnlySelectedTextures = %s\n",  tTextureOptions.bOnlySelectedTextures))
            
            local stRgba = string.format("{ r = %f, g = %f, b = %f, a = %f}",
                                        tTextureOptions.tRgba.r,
                                        tTextureOptions.tRgba.g,
                                        tTextureOptions.tRgba.b,
                                        tTextureOptions.tRgba.a)
            fp:write(string.format("tTextureOptions.tRgba = %s\n", stRgba ))
            fp:write(string.format("tTextureOptions.bAlpha = %s\n",  tTextureOptions.bAlpha))
            fp:write(string.format("\n"))
            fp:write(string.format("tTexturesToEditor = {}\n\n"))
            
            for i=1, #tTexturesToEditor do
                local tTexDesc = tTexturesToEditor[i]
                fp:write(string.format("tTexturesToEditor[%d] = {}\n",i))
                fp:write(string.format("tTexturesToEditor[%d].file_name = \"%s\"\n", i, tTexDesc.file_name))
                fp:write(string.format("tTexturesToEditor[%d].isSelected = %s\n", i, tostring(tTexDesc.isSelected)))
                fp:write(string.format("tTexturesToEditor[%d].iOffsetPerTextureX = %d\n", i, tTexDesc.iOffsetPerTextureX or 0))
                fp:write(string.format("tTexturesToEditor[%d].iOffsetPerTextureY = %d\n", i, tTexDesc.iOffsetPerTextureY or 0))
                fp:write(string.format("tTexturesToEditor[%d].iAnglePerTextureRX = %d\n", i, tTexDesc.iAnglePerTextureRX or 0))
                fp:write(string.format("tTexturesToEditor[%d].iAnglePerTextureRY = %d\n", i, tTexDesc.iAnglePerTextureRY or 0))
                fp:write(string.format("tTexturesToEditor[%d].iAnglePerTextureRZ = %d\n", i, tTexDesc.iAnglePerTextureRZ or 0))
                fp:write(string.format("tTexturesToEditor[%d].fScalePerTextureSX = %f\n", i, tTexDesc.fScalePerTextureSX or 0))
                fp:write(string.format("tTexturesToEditor[%d].fScalePerTextureSY = %f\n", i, tTexDesc.fScalePerTextureSY or 0))
                fp:write(string.format("\n"))
            end
            fp:close()
            sFileNameTextureCfg = sFileName
            tUtil.showMessage(tLang.L("texture_config_saved_ok"))
        end
    end
    if os and os.setlocale and prevNumericLocale then
        os.setlocale(prevNumericLocale, 'numeric')
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
        local result, texture_name = tRender:create(tTextureOptions.fWidth,tTextureOptions.fHeight,tTextureOptions.bAlpha,getNextNickName())
        if result then
            tRender:enableFrame(false)
            tShapeHoverImage:destroy()
            tShapeHoverImage = shape:new('2dw')
            tShapeHoverImage:create('rectangle',1,1)
            tShapeHoverImage.visible = false
            tRender:add(tShapeHoverImage)
            tShape:destroy()
            tShape = shape:new('2dw')
            local half_width  = tTextureOptions.fWidth  * 0.5
            local half_height = tTextureOptions.fHeight * 0.5
            local tVertex     = {-half_width ,-half_height,  -half_width,half_height,  half_width,-half_height,  half_width,half_height}
            local tUv
            if mbm.get('USE_DIRECTX9') or mbm.get('USE_METAL') then
                tUv = {0,1, 0,0, 1,1, 1,0}
            else
                tUv = {0,0, 0,1, 1,0, 1,1}
            end
            local tIndex      = {1,2,3, 3,2,4 }
            
            tShape:createIndexed(tVertex,tIndex,tUv,getNextNickName())
            tShape:setTexture(texture_name)
            tShape:setScale(scale,scale)

            tLine.visible = true
            tLine:set({-half_width ,-half_height,  -half_width ,half_height,  half_width ,half_height,   half_width ,-half_height, -half_width ,-half_height  },1)
            tLine:setScale(scale,scale)
            tLine:setColor(1,1,0)
        else
            tUtil.showMessageWarn(tLang.L("failed_to_create_dynamic_texture"))
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

function getLowerTextureSize()
    local width, height = math.huge, math.huge
    for i=1, #tTexturesToEditor do
        local tTexture = tTexturesToEditor[i]
        local tTex     = tTexture.tTex
        if tTex and tTexture.isSelected then
            local w, h  = tTex:getSize()
            width  = math.min(w,width)
            height = math.min(h,height)
        end
    end
    return width, height
end

function findLowerTextureSize()
    local width, height = math.huge, math.huge
    for i=1, #tTexturesToEditor do
        local tTexture = tTexturesToEditor[i]
        local tTex     = tTexture.tTex
        if tTex and tTexture.isSelected then
            local w, h  = tTex:getSize()
            width  = math.min(w,width)
            height = math.min(h,height)
        end
    end
    return width, height
end

function draw_first_fit_algorithm()
    
    local leftBound   = -tRender.width * 0.5 + (tTextureOptions.iOffsetX or 0)
    local topBound    =  tRender.height * 0.5 - (tTextureOptions.iOffsetY or 0)
    local rightBound  =  tRender.width * 0.5
    local bottomBound = -tRender.height * 0.5
    
    local placed = {} -- list of placed rectangles (using inflated dims to account spacing)
    
    local step_w, step_h = findLowerTextureSize() -- scan resolution in pixels

    local width, height = 0,0
    if tTextureOptions.indexReferenceTexture == 1 then
        width, height = getBiggerTextureSize()
    elseif tTextureOptions.indexReferenceTexture == 2 then
        width, height = getLowerTextureSize()
    end

    for i=1, #tTexturesToEditor do
        local tTexture = tTexturesToEditor[i]
        local tTex     = tTexture.tTex
        if tTex and tTexture.isSelected then
            tRender:add(tTex)
            tTex.visible = true
            apply_scale_for_tex(i)

            if tTextureOptions.indexReferenceTexture == 3 then
                width, height  = tTex:getSize()
            end
            
            -- Include spacing when checking collisions, but center placement uses real size
            local checkW = width + (tTextureOptions.iSpaceX or 0)
            local checkH = height + (tTextureOptions.iSpaceY or 0)

            local placed_this = false
            local outer_break = false

            -- scan top (topBound) to bottom (bottomBound + checkH) stepping downwards
            for cy = topBound, (bottomBound + checkH), -step_h do
                if outer_break then break end
                -- scan left to right
                for cx = leftBound, (rightBound - checkW), step_w do
                    -- candidate inflated rect
                    local left  = cx
                    local right = cx + checkW
                    local top   = cy
                    local bottom= cy - checkH

                    -- check within bounds
                    if left >= leftBound and right <= rightBound and bottom >= bottomBound then
                        -- check collisions
                        local collide = false
                        for _, pr in ipairs(placed) do
                            if not (pr.right <= left or pr.left >= right or pr.bottom >= top or pr.top <= bottom) then
                                collide = true
                                break
                            end
                        end
                        if not collide then
                            -- place texture: compute center using real width/height and per-texture offsets
                            local center_x = left + (width * 0.5) + (tTexture.iOffsetPerTextureX or 0)
                            local center_y = top  - (height * 0.5) + (tTexture.iOffsetPerTextureY or 0)
                            tTex:setPos(center_x, center_y)

                            -- store inflated rect to reserve spacing area
                            table.insert(placed, { left = left, right = right, top = top, bottom = bottom })

                            placed_this = true
                            outer_break = true
                            break
                        end
                    end
                end
            end

            if placed_this == false then
                -- couldn't place: hide/remove
                tRender:remove(tTex)
                tTex.visible = false
            end
        elseif tTex then
            -- unselected textures should not be in render
            tRender:remove(tTex)
            tTex.visible = false
        end
    end
    return countTotalInOut()
end

function apply_scale_for_tex(i)
    local tTexture = tTexturesToEditor[i]
    local tTex     = tTexture.tTex
    if tTex and tTexture.isSelected then
        tTex:setScale((tTextureOptions.scaleImage or 1) + (tTextureOptions.sumScaleImageX or 0) + (tTexturesToEditor[i].fScalePerTextureSX or 0),
                      (tTextureOptions.scaleImage or 1) + (tTextureOptions.sumScaleImageY or 0) + (tTexturesToEditor[i].fScalePerTextureSY or 0))
    end
end

function countTotalInOut()
    -- need to re-count total in/selected in case some textures were hidden due to scaling
    local iTotalIn = 0
    local iTotalSelected = 0

    local leftBound   = -tRender.width * 0.5 + (tTextureOptions.iOffsetX or 0)
    local topBound    =  tRender.height * 0.5 - (tTextureOptions.iOffsetY or 0)
    local rightBound  =  tRender.width * 0.5
    local bottomBound = -tRender.height * 0.5
    -- small epsilon to absorb floating-point rounding when canvas size is not
    -- divisible by the grid count (e.g. 2048/5 = 409.6 is not exact in IEEE 754)
    local eps = 0.5

    for i=1, #tTexturesToEditor do
        local tTexture = tTexturesToEditor[i]
        local tTex     = tTexture.tTex
        if tTex and tTexture.isSelected then
            iTotalSelected = iTotalSelected + 1
            if tTex.visible then
                local w,h    = tTex:getSize()
                local x      = tTex.x
                local y      = tTex.y
                local left   = x - (w * 0.5)
                local right  = x + (w * 0.5)
                local top    = y + (h * 0.5)
                local bottom = y - (h * 0.5)
                if left >= (leftBound - eps) and right <= (rightBound + eps) and bottom >= (bottomBound - eps) and top <= (topBound + eps) then
                    iTotalIn = iTotalIn + 1
                    tTexture.isOutOfBounds = false
                else
                    tTexture.isOutOfBounds = true
                end
            end
        end
    end
    return iTotalIn, iTotalSelected
end

function draw_followed_by_bigger_or_lower_texture_algorithm()
    local x_initial,y_initial = tTextureOptions.iOffsetX - (tRender.width * 0.5),(tRender.height * 0.5) - tTextureOptions.iOffsetY
    local x_final,  y_final   = tRender.width * 0.5, tRender.height * -0.5
    local x,y                 = x_initial,y_initial
    local iCountMaxTile       = 0
    local bCheckTile          = tTextureOptions.iMaxTileCount > 0

    local width, height = 0,0

    if tTextureOptions.indexReferenceTexture == 1 then
        width, height = getBiggerTextureSize()
    elseif tTextureOptions.indexReferenceTexture == 2 then
        width, height = getLowerTextureSize()
    end

    if tTextureOptions.bAxisY then
        for i=1, #tTexturesToEditor do
            local tTexture = tTexturesToEditor[i]
            local tTex     = tTexture.tTex
            if tTex and tTexture.isSelected then
                tRender:add(tTex)
                tTex.visible = true
                apply_scale_for_tex(i)
                if tTextureOptions.indexReferenceTexture == 3 then
                    width, height = tTex:getSize()
                end
                if i == 1 then
                    local half_width_tex  = width  * 0.5
                    local half_height_tex = height * 0.5
                    x = x + half_width_tex
                    y = y - half_height_tex
                end

                tTex:setPos(x,y)

                if tTextureOptions.iMaxTileCount > 0 then
                    iCountMaxTile = iCountMaxTile + 1
                end

                y = y - height - tTextureOptions.iSpaceY

                if (bCheckTile and iCountMaxTile >= tTextureOptions.iMaxTileCount) or ((y - (height  * 0.5)) < y_final) then
                    x = x + width + tTextureOptions.iSpaceX
                    y = y_initial - (height  * 0.5)
                    iCountMaxTile = 0
                end
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
                apply_scale_for_tex(i)
                if tTextureOptions.indexReferenceTexture == 3 then
                    width, height = tTex:getSize()
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

                tTex:setPos(x,y)

                if tTextureOptions.iMaxTileCount > 0 then
                    iCountMaxTile = iCountMaxTile + 1
                end

                x = x + width + tTextureOptions.iSpaceX
            elseif tTex then
                tRender:remove(tTex)
                tTex.visible = false
            end
        end
    end

    for i=1, #tTexturesToEditor do
        local tTexture = tTexturesToEditor[i]
        local tTex     = tTexture.tTex
        x = tTex.x + (tTexture.iOffsetPerTextureX or 0)
        y = tTex.y + (tTexture.iOffsetPerTextureY or 0)
        tTex:setPos(x,y)
    end

    return countTotalInOut()
end

function draw_best_fit_algorithm()
    
    local leftBound   = -tRender.width * 0.5 + (tTextureOptions.iOffsetX or 0)
    local topBound    =  tRender.height * 0.5 - (tTextureOptions.iOffsetY or 0)
    local rightBound  =  tRender.width * 0.5
    local bottomBound = -tRender.height * 0.5

    local placed = {} -- list of placed rectangles (using inflated dims to account spacing)
    
    local step_w, step_h = findLowerTextureSize() -- scan resolution in pixels
    if step_w <= 0 then step_w = 1 end
    if step_h <= 0 then step_h = 1 end

    local width, height = 0,0
    if tTextureOptions.indexReferenceTexture == 1 then
        width, height = getBiggerTextureSize()
    elseif tTextureOptions.indexReferenceTexture == 2 then
        width, height = getLowerTextureSize()
    end

    for i=1, #tTexturesToEditor do
        local tTexture = tTexturesToEditor[i]
        local tTex     = tTexture.tTex
        if tTex and tTexture.isSelected then
            tRender:add(tTex)
            tTex.visible = true
            apply_scale_for_tex(i)

            if tTextureOptions.indexReferenceTexture == 3 then
                width, height = tTex:getSize()
            end
            
            local checkW = width + (tTextureOptions.iSpaceX or 0)
            local checkH = height + (tTextureOptions.iSpaceY or 0)

            local bestCandidate = nil
            local bestTop = -math.huge
            local bestLeft = math.huge

            -- evaluate all candidate positions (try to pick the one closest to top, then left)
            for cy = topBound, (bottomBound + checkH), -step_h do
                for cx = leftBound, (rightBound - checkW), step_w do
                    local left  = cx
                    local right = cx + checkW
                    local top   = cy
                    local bottom= cy - checkH

                    -- within bounds
                    if left >= leftBound and right <= rightBound and bottom >= bottomBound then
                        local collide = false
                        for _, pr in ipairs(placed) do
                            if not (pr.right <= left or pr.left >= right or pr.bottom >= top or pr.top <= bottom) then
                                collide = true
                                break
                            end
                        end
                        if not collide then
                            -- prefer candidate with highest top (closest to top bound),
                            -- tie-breaker: smallest left (closest to left bound)
                            if top > bestTop or (top == bestTop and left < bestLeft) then
                                bestTop = top
                                bestLeft = left
                                bestCandidate = { left = left, right = right, top = top, bottom = bottom }
                            end
                        end
                    end
                end
            end

            if bestCandidate then
                -- compute actual center using real width/height and per-texture offsets
                local center_x = bestCandidate.left + (width * 0.5) + (tTexture.iOffsetPerTextureX or 0)
                local center_y = bestCandidate.top  - (height * 0.5) + (tTexture.iOffsetPerTextureY or 0)
                tTex:setPos(center_x, center_y)

                table.insert(placed, bestCandidate)
            else
                -- couldn't place: hide/remove
                tRender:remove(tTex)
                tTex.visible = false
            end

        elseif tTex then
            -- unselected textures should not be in render
            tRender:remove(tTex)
            tTex.visible = false
        end
    end

    return countTotalInOut()
end

function draw_grid_based_placement_algorithm()
    
    local leftBound   = -tRender.width * 0.5 + (tTextureOptions.iOffsetX or 0)
    local topBound    =  tRender.height * 0.5 - (tTextureOptions.iOffsetY or 0)
    local rightBound  =  tRender.width * 0.5
    local bottomBound = -tRender.height * 0.5

    local iCountMaxTile = 0
    local bCheckTile = tTextureOptions.iMaxTileCount > 0

    local gx = math.max(1, tTextureOptions.iGridX or 1)
    local gy = math.max(1, tTextureOptions.iGridY or 1)

    local cell_w = tTextureOptions.fWidth  / gx
    local cell_h = tTextureOptions.fHeight / gy

    -- occupancy grid (rows 0..gy-1, cols 0..gx-1) false = free, true = occupied
    local occ = {}
    for r=0, gy-1 do
        occ[r] = {}
        for c=0, gx-1 do
            occ[r][c] = false
        end
    end

    local function fits_and_mark(row,col,needR,needC)
        -- check bounds
        if row < 0 or col < 0 or (row + needR) > gy or (col + needC) > gx then
            return false
        end
        -- check occupancy
        for rr = row, row + needR - 1 do
            for cc = col, col + needC - 1 do
                if occ[rr][cc] then
                    return false
                end
            end
        end
        -- mark occupied
        for rr = row, row + needR - 1 do
            for cc = col, col + needC - 1 do
                occ[rr][cc] = true
            end
        end
        return true
    end

    local width, height = 0,0
    if tTextureOptions.indexReferenceTexture == 1 then
        width, height = getBiggerTextureSize()
    elseif tTextureOptions.indexReferenceTexture == 2 then
        width, height = getLowerTextureSize()
    end

    -- choose ordering: when AxisY true fill columns top->bottom then left->right
    for i=1, #tTexturesToEditor do
        local tTexture = tTexturesToEditor[i]
        local tTex     = tTexture.tTex
        if tTex and tTexture.isSelected then
            tRender:add(tTex)
            tTex.visible = true
            apply_scale_for_tex(i)

            if tTextureOptions.indexReferenceTexture == 3 then
                width, height = tTex:getSize()
            end

            -- required cells to cover texture (ceil to allow spanning multiple cells)
            local needCellsX = math.max(1, math.ceil(width  / cell_w))
            local needCellsY = math.max(1, math.ceil(height / cell_h))

            -- if exceeds total cells capacity, cannot place
            if bCheckTile and iCountMaxTile >= tTextureOptions.iMaxTileCount then
                tRender:remove(tTex)
                tTex.visible = false
            else
                local placed = false

                if tTextureOptions.bAxisY then
                    -- iterate columns (left->right) and rows (top->bottom)
                    for col = 0, gx-1 do
                        if placed then break end
                        for row = 0, gy-1 do
                            -- top-left cell is (row, col), but row 0 is top
                            local startRow = row
                            local startCol = col
                            -- convert startRow/startCol to index where top is 0
                            -- check if block fits in grid and is free
                            if fits_and_mark(startRow, startCol, needCellsY, needCellsX) then
                                -- compute center of covered area in render coordinates
                                local left_cell_edge  = leftBound + (startCol * cell_w)
                                local top_cell_edge   = topBound  - (startRow * cell_h)
                                local covered_w = cell_w * needCellsX
                                local covered_h = cell_h * needCellsY
                                local center_x = left_cell_edge + (covered_w * 0.5)
                                local center_y = top_cell_edge  - (covered_h * 0.5)

                                -- apply per-texture manual offsets
                                center_x = center_x + (tTexture.iOffsetPerTextureX or 0)
                                center_y = center_y + (tTexture.iOffsetPerTextureY or 0)

                                -- final bounds check using actual texture size (should be OK since we reserved enough cells)
                                local left  = center_x - (width * 0.5)
                                local right = center_x + (width * 0.5)
                                local top   = center_y + (height * 0.5)
                                local bottom= center_y - (height * 0.5)

                                if left >= leftBound and right <= rightBound and bottom >= bottomBound and top <= topBound then
                                    tTex:setPos(center_x, center_y)
                                    iCountMaxTile = iCountMaxTile + 1
                                    placed = true
                                    break
                                else
                                    -- if bounds failed, unmark those cells
                                    for rr = startRow, startRow + needCellsY - 1 do
                                        for cc = startCol, startCol + needCellsX - 1 do
                                            occ[rr][cc] = false
                                        end
                                    end
                                end
                            end
                        end
                    end
                else
                    -- iterate rows (top->bottom) then cols (left->right)
                    for row = 0, gy-1 do
                        if placed then break end
                        for col = 0, gx-1 do
                            local startRow = row
                            local startCol = col
                            if fits_and_mark(startRow, startCol, needCellsY, needCellsX) then
                                local left_cell_edge  = leftBound + (startCol * cell_w)
                                local top_cell_edge   = topBound  - (startRow * cell_h)
                                local covered_w = cell_w * needCellsX
                                local covered_h = cell_h * needCellsY
                                local center_x = left_cell_edge + (covered_w * 0.5)
                                local center_y = top_cell_edge  - (covered_h * 0.5)

                                center_x = center_x + (tTexture.iOffsetPerTextureX or 0)
                                center_y = center_y + (tTexture.iOffsetPerTextureY or 0)

                                local left  = center_x - (width * 0.5)
                                local right = center_x + (width * 0.5)
                                local top   = center_y + (height * 0.5)
                                local bottom= center_y - (height * 0.5)

                                if left >= leftBound and right <= rightBound and bottom >= bottomBound and top <= topBound then
                                    tTex:setPos(center_x, center_y)
                                    iCountMaxTile = iCountMaxTile + 1
                                    placed = true
                                    break
                                else
                                    for rr = startRow, startRow + needCellsY - 1 do
                                        for cc = startCol, startCol + needCellsX - 1 do
                                            occ[rr][cc] = false
                                        end
                                    end
                                end
                            end
                        end
                    end
                end

                if not placed then
                    tRender:remove(tTex)
                    tTex.visible = false
                end
            end

        elseif tTex then
            tRender:remove(tTex)
            tTex.visible = false
        end
    end

    return countTotalInOut()
end

function draw_grid_force_fit_placement_algorithm()
    
    local leftBound   = -tRender.width * 0.5 + (tTextureOptions.iOffsetX or 0)
    local topBound    =  tRender.height * 0.5 - (tTextureOptions.iOffsetY or 0)
    local rightBound  =  tRender.width * 0.5
    local bottomBound = -tRender.height * 0.5

    local iCountMaxTile = 0
    local bCheckTile = tTextureOptions.iMaxTileCount > 0

    local gx = math.max(1, tTextureOptions.iGridX or 1)
    local gy = math.max(1, tTextureOptions.iGridY or 1)

    local cell_w = tTextureOptions.fWidth  / gx
    local cell_h = tTextureOptions.fHeight / gy

    local totalCells = gx * gy
    local nextCell = 0
    local fMinScale = math.huge

    -- iterate textures in the chosen order; each texture is forced into the next free cell
    for i=1, #tTexturesToEditor do
        local tTexture = tTexturesToEditor[i]
        local tTex     = tTexture.tTex
        if tTex and tTexture.isSelected then

            if bCheckTile and iCountMaxTile >= tTextureOptions.iMaxTileCount then
                -- reached max tile count: remove/hide remaining textures
                tRender:remove(tTex)
                tTex.visible = false
            else
                -- find next available cell
                if nextCell >= totalCells then
                    -- no more cells available
                    tRender:remove(tTex)
                    tTex.visible = false
                else
                    if tTextureOptions.bGridForceFitScale == false then
                        apply_scale_for_tex(i)
                    end

                    -- compute cell coords depending on AxisY option
                    local col, row
                    if tTextureOptions.bAxisY then
                        col = math.floor(nextCell / gy)
                        row = nextCell % gy
                    else
                        row = math.floor(nextCell / gx)
                        col = nextCell % gx
                    end

                    -- compute center of cell in render coordinates (cell center)
                    local center_x = leftBound + (col + 0.5) * cell_w
                    local center_y = topBound  - (row + 0.5) * cell_h

                    -- apply per-texture manual offsets
                    center_x = center_x + (tTexture.iOffsetPerTextureX or 0)
                    center_y = center_y + (tTexture.iOffsetPerTextureY or 0)

                    -- add to render and make visible
                    tRender:add(tTex)
                    tTex.visible = true

                    -- compute scaling to force-fit into cell (respect spacing)
                    local avail_w = math.max(1, cell_w - (tTextureOptions.iSpaceX or 0))
                    local avail_h = math.max(1, cell_h - (tTextureOptions.iSpaceY or 0))
                    local w,h = tTex:getSize()
                    
                    local fitScale = 1
                    if w > 0 and h > 0 then
                        fitScale = math.min(avail_w / w, avail_h / h)
                        if fitScale <= 0 then fitScale = 0.0001 end
                    end
                    
                    if tTextureOptions.bGridForceFitScale then
                        -- combine user scale settings (keep aspect by applying same factor to both axes)
                        local userScaleX = (tTextureOptions.scaleImage or 1) + (tTextureOptions.sumScaleImageX or 0)
                        local userScaleY = (tTextureOptions.scaleImage or 1) + (tTextureOptions.sumScaleImageY or 0)
                        -- choose average user scale to keep aspect ratio
                        local userAvgScale = (userScaleX + userScaleY) * 0.5

                        local finalScale = fitScale * userAvgScale
                        -- avoid negative or zero scale
                        if finalScale <= 0 then finalScale = 0.0001 end
                        fMinScale = math.min(fMinScale, finalScale)
                    end
                    tTex:setPos(center_x, center_y)

                    iCountMaxTile = iCountMaxTile + 1
                    nextCell = nextCell + 1
                end
            end
        elseif tTex then
            tRender:remove(tTex)
            tTex.visible = false
        end
    end

    if tTextureOptions.bGridForceFitScale then
        for i=1, #tTexturesToEditor do
            local tTexture = tTexturesToEditor[i]
            local tTex     = tTexture.tTex
            if tTex and tTexture.isSelected and tTex.visible then
                tTex:setScale(fMinScale, fMinScale)
            end
        end
        tTextureOptions.scaleImage = fMinScale
    end

    return countTotalInOut()
end

function drawSpriteSheet()
    if #tTexturesToEditor > 0 then
        local iTotalIn, iTotalSelected = 0,0

        adjustTextureSize()

        if tTextureOptions.bSortByName then
            table.sort(tTexturesToEditor, function(a, b)
                return a.file_name < b.file_name
            end)
        elseif tTextureOptions.bSortBySizeAscending then
                table.sort(tTexturesToEditor, function(a, b)
                    local aw, ah = 0,0
                    local bw, bh = 0,0
                    if a.tTex then aw, ah = a.tTex:getSize() end
                    if b.tTex then bw, bh = b.tTex:getSize() end
                    local aa = aw * ah
                    local bb = bw * bh
                    if aa ~= bb then
                        return aa < bb
                    end
                    return (a.file_name or '') < (b.file_name or '')
                end)
elseif tTextureOptions.bSortBySizeDescending then
                table.sort(tTexturesToEditor, function(a, b)
                    local aw, ah = 0,0
                    local bw, bh = 0,0
                    if a.tTex then aw, ah = a.tTex:getSize() end
                    if b.tTex then bw, bh = b.tTex:getSize() end
                    local aa = aw * ah
                    local bb = bw * bh
                    if aa ~= bb then
                        return aa > bb
                    end
                    return (a.file_name or '') < (b.file_name or '')
                end)
        end

        local sNameAlgorithm = ""
        if tTextureOptions.iCurrentAlgorithm == 1 then -- 'Follow bigger or lower Texture'
            iTotalIn, iTotalSelected = draw_followed_by_bigger_or_lower_texture_algorithm()
            sNameAlgorithm = tLang.L("follow_bigger_or_lower_texture")
        elseif tTextureOptions.iCurrentAlgorithm == 2 then -- 'First Fit algorithm'
            iTotalIn, iTotalSelected = draw_first_fit_algorithm()
            sNameAlgorithm = tLang.L("first_fit_algorithm")
        elseif tTextureOptions.iCurrentAlgorithm == 3 then -- 'Best Fit algorithm'
            iTotalIn, iTotalSelected = draw_best_fit_algorithm()
            sNameAlgorithm = tLang.L("best_fit_algorithm")
        elseif tTextureOptions.iCurrentAlgorithm == 4 then -- 'Grid-based placement'
            iTotalIn, iTotalSelected = draw_grid_based_placement_algorithm()
            sNameAlgorithm = tLang.L("grid_based_placement")
        elseif tTextureOptions.iCurrentAlgorithm == 5 then -- 'Grid-force fit placement'
            iTotalIn, iTotalSelected = draw_grid_force_fit_placement_algorithm()
            sNameAlgorithm = tLang.L("grid_force_fit_placement")
        elseif tTextureOptions.iCurrentAlgorithm > 5 then
            sNameAlgorithm = tLang.L("grid_force_fit_placement")
            tTextureOptions.iCurrentAlgorithm = 5
        elseif tTextureOptions.iCurrentAlgorithm < 1 then
            sNameAlgorithm = tLang.L("follow_bigger_or_lower_texture")
            tTextureOptions.iCurrentAlgorithm = 1
        end

        applyRotationToTextures()

        if bPrintDebug then
            bPrintDebug = false
            print(string.format("Algorithm: %s, Total In: %d, Total Selected: %d", sNameAlgorithm, iTotalIn, iTotalSelected))
            for i=1, #tTexturesToEditor do
                local tTexture = tTexturesToEditor[i]
                local tTex     = tTexture.tTex
                if tTex and tTexture.isSelected and tTex.visible then
                    local tDesc = tTexturesToEditor[i]
                    local w,h = tTex:getSize()
                    print(string.format("Texture [%s] %d position: (%.2f, %.2f), width: %d, height: %d, original width: %d, original height: %d scaled: (%.2f, %.2f) out of bounds: %s", 
                        tUtil.getShortName(tDesc.file_name), 
                        i, 
                        tTex.x, 
                        tTex.y, 
                        w, 
                        h, 
                        tDesc.width, 
                        tDesc.height, 
                        tTex.sx, 
                        tTex.sy,
                        tTexture.isOutOfBounds))
                elseif tTex and tTexture.isSelected and tTex.visible == false then
                    print(string.format("Texture [%s] %d is selected but not visible (probably couldn't fit in)", tUtil.getShortName(tTexture.file_name), i))
                end
            end
        end

        tLine:setScale(scale,scale)
        showPendingTextureMessage(iTotalIn == iTotalSelected, 'Status of Texture',string.format('%d of %d are inside.\nTotal existent %d',iTotalIn,iTotalSelected,#tTexturesToEditor))
    end
end

function applyRotationToTextures()
    for i=1, #tTexturesToEditor do
        local tTexture = tTexturesToEditor[i]
        local tTex     = tTexture.tTex
        if tTex and tTexture.isSelected then
            tTex:setAngle(math.rad(tTexture.iAnglePerTextureRX or 0),math.rad(tTexture.iAnglePerTextureRY or 0),math.rad(tTexture.iAnglePerTextureRZ or 0))
        end
    end
end


function showSortOptions()
    tImGui.Text(tLang.L("sort_textures_by"))
    tTextureOptions.iIndexSortOption = tImGui.RadioButton(tLang.L("sort_by_name"), tTextureOptions.iIndexSortOption, 1)
    tTextureOptions.iIndexSortOption = tImGui.RadioButton(tLang.L("sort_by_size_asc"), tTextureOptions.iIndexSortOption, 2)
    tTextureOptions.iIndexSortOption = tImGui.RadioButton(tLang.L("sort_by_size_desc"), tTextureOptions.iIndexSortOption, 3)

    tTextureOptions.bSortByName = false
    tTextureOptions.bSortBySizeAscending = false
    tTextureOptions.bSortBySizeDescending = false
    if tTextureOptions.iIndexSortOption == 1 then
        tTextureOptions.bSortByName = true
    elseif tTextureOptions.iIndexSortOption == 2 then
        tTextureOptions.bSortBySizeAscending = true
    elseif tTextureOptions.iIndexSortOption == 3 then
        tTextureOptions.bSortBySizeDescending = true
    end
end

function showTextureOptions()
    if bViewTextureOptions then
        local width = 220
        local x_pos, y_pos = 0, 0
        local max_width = 220
        local tSizeBtnAddSet   = {x=43,y=0} 
        tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_texture_options,x_pos,y_pos,width,max_width)
        local is_opened, closed_clicked = tImGui.Begin(tLang.L(tWindowsTitle.title_texture_options), true, ImGuiWindowFlags_NoMove)
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

            tImGui.Text(tLang.L("width"))
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

            tImGui.Text(tLang.L("height"))
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

            tTextureOptions.bPowerOf2 = tImGui.Checkbox(tLang.L("power_of_2") .. '##P2', tTextureOptions.bPowerOf2)

            tImGui.Text(tLang.L("space_x_label"))
            local result, iValue = tImGui.InputInt('##SpaceXTexture', tTextureOptions.iSpaceX, step, step_fast, flags)
            if result then
                tTextureOptions.iSpaceX = iValue
            end

            tImGui.Text(tLang.L("space_y_label"))
            local result, iValue = tImGui.InputInt('##SpaceYTexture', tTextureOptions.iSpaceY, step, step_fast, flags)
            if result then
                tTextureOptions.iSpaceY = iValue
            end

            tImGui.Text(tLang.L("offset_x_label"))
            local result, iValue = tImGui.InputInt('##OffsetXTexture', tTextureOptions.iOffsetX, step, step_fast, flags)
            if result then
                tTextureOptions.iOffsetX = iValue
            end

            tImGui.Text(tLang.L("offset_y_label"))
            local result, iValue = tImGui.InputInt('##OffsetYTexture', tTextureOptions.iOffsetY, step, step_fast, flags)
            if result then
                tTextureOptions.iOffsetY = iValue
            end

            tImGui.Text(tLang.L("max_tile_count"))
            tImGui.SameLine()
            tImGui.HelpMarker(tLang.L("help_zero_automatic"))
            local result, iValue = tImGui.InputInt('##MaxTileCount', tTextureOptions.iMaxTileCount, step, step_fast, flags)
            if result and iValue >= 0 then
                tTextureOptions.iMaxTileCount = iValue
            end

            local gx = math.max(1, tTextureOptions.iGridX or 1)
            local gy = math.max(1, tTextureOptions.iGridY or 1)
            local cell_w = tTextureOptions.fWidth  / gx
            local cell_h = tTextureOptions.fHeight / gy

            tImGui.Text(string.format('Grid X %dpx',cell_w))
            tImGui.SameLine()
            tImGui.HelpMarker(tLang.L("help_visual_grid_x"))
            tImGui.SameLine()
            tImGui.SetCursorPosX(130)
            tTextureOptions.bGridVisibleX = tImGui.Checkbox('##ShowGridX',tTextureOptions.bGridVisibleX)
            local result, iValue = tImGui.InputInt('##GridX', tTextureOptions.iGridX, step, step_fast, flags)
            if result and iValue >= 1 and iValue <= (tTextureOptions.fWidth /2) then
                tTextureOptions.iGridX = iValue
            end
            
            tImGui.Text(string.format('Grid Y %dpx',cell_h)    )
            tImGui.SameLine()
            tImGui.HelpMarker(tLang.L("help_visual_grid_y"))
            tImGui.SameLine()
            tImGui.SetCursorPosX(130)
            tTextureOptions.bGridVisibleY = tImGui.Checkbox('##ShowGridY',tTextureOptions.bGridVisibleY)
            local result, iValue = tImGui.InputInt('##iGridY', tTextureOptions.iGridY, step, step_fast, flags)
            if result and iValue >= 1 and iValue < (tTextureOptions.fHeight / 2) then
                tTextureOptions.iGridY = iValue
            end

            tImGui.Text(tLang.L("scale_image"))
            local step       =  0.01
            local step_fast  =  0.02
            local format     = "%.3f"
            local result, fValue = tImGui.InputFloat('##ScaleImageRect', tTextureOptions.scaleImage, step, step_fast, format, flags)
            if result and fValue > 0 then
                tTextureOptions.scaleImage = fValue
                tTextureOptions.bGridForceFitScale = false
            end

            tImGui.Text(tLang.L("adjust_scale_on_x"))
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

            tImGui.Text(tLang.L("adjust_scale_on_y"))
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

            tImGui.Text(tLang.L("background_color_text"))
            local clicked, tRgba = tImGui.ColorEdit4('Select your color', tTextureOptions.tRgba, tImGui.Flags('ImGuiColorEditFlags_NoLabel'))
            if clicked then
                tTextureOptions.tRgba = tRgba
                tRender:setColor(tRgba.r,tRgba.g,tRgba.b,tRgba.a)
            end

            tTextureOptions.bAxisY    = tImGui.Checkbox(tLang.L("axis_y_label") .. '## Axis Y', tTextureOptions.bAxisY)
            tTextureOptions.bAlpha    = tImGui.Checkbox(tLang.L("enable_alpha") .. '##AlphaTex', tTextureOptions.bAlpha)

            tImGui.NewLine()
            tImGui.Text(tLang.L("algorithm"))
            if tImGui.IsItemHovered(0) then
                tImGui.BeginTooltip()
                tImGui.Text(tLang.L("select_algorithm_note"))
                tImGui.EndTooltip()
            end
            local height_in_items  =  -1

            local sDirection = ''
            if tTextureOptions.indexReferenceTexture == 1 then
                sDirection = 'bigger'
            elseif tTextureOptions.indexReferenceTexture == 2 then
                sDirection = 'lower'
            else
                sDirection = 'texture size'
            end
            tComboAlgorithm[1] = string.format('Follow %s Texture',sDirection)
            local ret, current_item, item_as_string = tImGui.Combo('##Algorithm', tTextureOptions.iCurrentAlgorithm, tComboAlgorithm, height_in_items)
            if ret then
                tTextureOptions.iCurrentAlgorithm = current_item --number of item selected
                if tTextureOptions.iCurrentAlgorithm >= 2 then
                    tTextureOptions.bAxisY = false
                    tTextureOptions.indexReferenceTexture = 3 -- Texture size reference
                end
            end

            if tImGui.IsItemHovered(0) then
                tImGui.BeginTooltip()
                if tTextureOptions.iCurrentAlgorithm == 1 then -- 'Follow bigger or lower Texture'
                    tImGui.Text(string.format('Note: Textures are arranged following the size of the %s texture.',sDirection))
                elseif tTextureOptions.iCurrentAlgorithm == 2 then -- 'First Fit algorithm'
                    tImGui.Text(tLang.L("note_first_fit"))
                elseif tTextureOptions.iCurrentAlgorithm == 3 then
                    tImGui.Text(tLang.L("note_best_fit"))
                elseif tTextureOptions.iCurrentAlgorithm == 4 then
                    tImGui.Text(tLang.L("note_grid"))
                elseif tTextureOptions.iCurrentAlgorithm == 5 then
                    tImGui.Text(tLang.L("note_maxrects"))
                end
                tImGui.EndTooltip()
            end

            tImGui.NewLine()

            if tTextureOptions.iCurrentAlgorithm ~= 5 then
                tImGui.Text(tLang.L("reference_texture_size"))
                tTextureOptions.indexReferenceTexture = tImGui.RadioButton(tLang.L("bigger_texture_reference"), tTextureOptions.indexReferenceTexture or 1, 1)
                tTextureOptions.indexReferenceTexture = tImGui.RadioButton(tLang.L("lower_texture_reference"), tTextureOptions.indexReferenceTexture, 2)
                tTextureOptions.indexReferenceTexture = tImGui.RadioButton(tLang.L("texture_size_reference"), tTextureOptions.indexReferenceTexture, 3)
                if tTextureOptions.indexReferenceTexture < 1 then
                    tTextureOptions.indexReferenceTexture = 1
                elseif tTextureOptions.indexReferenceTexture > 3 then
                    tTextureOptions.indexReferenceTexture = 3
                end

                tImGui.NewLine()
            end
            -- Check the applicable flags for each algorithm
            if tTextureOptions.iCurrentAlgorithm == 1 then -- 'Follow bigger Texture'
                showSortOptions()
            elseif tTextureOptions.iCurrentAlgorithm == 2 then -- 'First Fit algorithm'
                showSortOptions()
            elseif tTextureOptions.iCurrentAlgorithm == 3 then -- 'Best Fit algorithm' 
                showSortOptions()
            elseif tTextureOptions.iCurrentAlgorithm == 4 then -- 'Grid-based placement'
                showSortOptions()
            elseif tTextureOptions.iCurrentAlgorithm == 5 then -- 'MaxRects algorithm'
                tTextureOptions.bGridForceFitScale = tImGui.Checkbox(tLang.L("auto_scale_to_fit") .. '##GridForceFitScale', tTextureOptions.bGridForceFitScale)
                if tTextureOptions.bGridForceFitScale == false and tTextureOptions.bLastGridForceFitScaleWasEnabled then
                    tTextureOptions.scaleImage = 1.0
                end
                if tTextureOptions.bGridForceFitScale then
                    tTextureOptions.bLastGridForceFitScaleWasEnabled = true
                else
                    tTextureOptions.bLastGridForceFitScaleWasEnabled = false
                end
                tImGui.NewLine()
                showSortOptions()
            end

            tImGui.NewLine()
            local step       =  1
            local step_fast  =  10

            local step_f       =  0.01
            local step_fast_f  =  0.02
            local format_f     = "%.3f"

            tShapeHoverImage.visible = false
            
            local function showTextureHoverImage(i)
                if tTexturesToEditor[i].isSelected == true and tImGui.IsItemHovered(0) then
                    local tDesc = tTexturesToEditor[i]
                    local tTex = tTexturesToEditor[i].tTex
                    local w,h = tTex:getSize()
                    tShapeHoverImage.visible = true
                    tShapeHoverImage:setScale(w,h)
                    tShapeHoverImage:setPos(tTex.x, tTex.y, tTex.z - 1)
                    tImGui.BeginTooltip()
                    tImGui.Text(string.format('Name: %s\nwidth: %d\nheight: %d',tUtil.getShortName(tDesc.file_name),tDesc.width, tDesc.height))
                    tImGui.EndTooltip()
                end
            end

            local tOutOfBoundsColor = {r=1,g=0.3,b=0.3,a=0.8}

            if tImGui.TreeNode('id_OffsetPerTexture', tLang.L("override_adjusts")) then
                local label  = tLang.L("only_selected_textures") .. '##OverrideAdjustsPerTexture'
                tTextureOptions.bOnlySelectedTextures = tImGui.Checkbox(label,tTextureOptions.bOnlySelectedTextures)
                for i=1, #tTexturesToEditor do
                    if tTextureOptions.bOnlySelectedTextures then
                        if tTexturesToEditor[i].isSelected == false then
                            goto continue
                        end
                    end
                    local sShortName   = tUtil.getShortName(tTexturesToEditor[i].file_name)
                    if tTexturesToEditor[i].isOutOfBounds then
                        tImGui.PushStyleColor('ImGuiCol_Text',tOutOfBoundsColor)
                    end
                    if tImGui.TreeNode('id_OffsetPerTexture_' .. tostring(i),sShortName) then
                        if tTexturesToEditor[i].isOutOfBounds then
                            tImGui.PopStyleColor(1)
                        end
                        showTextureHoverImage(i)
                        tImGui.Text(tLang.L("offset_per_texture"))
                        local result, iValue = tImGui.InputInt(tLang.L("axis_x") .. '##OffsetPerTextureX' .. tostring(i), tTexturesToEditor[i].iOffsetPerTextureX or 0, step, step_fast, flags)
                        if result then
                            tTexturesToEditor[i].iOffsetPerTextureX = iValue
                        end

                        local result, iValue = tImGui.InputInt(tLang.L("axis_y") .. '##OffsetPerTextureY' .. tostring(i), tTexturesToEditor[i].iOffsetPerTextureY or 0, step, step_fast, flags)
                        if result then
                            tTexturesToEditor[i].iOffsetPerTextureY = iValue
                        end

                        tImGui.Text(tLang.L("rotation_per_texture"))
                        local result, iValue = tImGui.InputInt(tLang.L("rotation_rx") .. '##RotationPerTextureX' .. tostring(i), tTexturesToEditor[i].iAnglePerTextureRX or 0, step, step_fast, flags)
                        if result then
                            if iValue >= 360 then
                                iValue = 360
                            elseif iValue <= -360 then
                                iValue = -360
                            end
                            tTexturesToEditor[i].iAnglePerTextureRX = iValue
                        end

                        local result, iValue = tImGui.InputInt(tLang.L("rotation_ry") .. '##RotationPerTextureY' .. tostring(i), tTexturesToEditor[i].iAnglePerTextureRY or 0, step, step_fast, flags)
                        if result then
                            if iValue >= 360 then
                                iValue = 360
                            elseif iValue <= -360 then
                                iValue = -360
                            end
                            tTexturesToEditor[i].iAnglePerTextureRY = iValue
                        end

                        local result, iValue = tImGui.InputInt(tLang.L("rotation_rz") .. '##RotationPerTextureZ' .. tostring(i), tTexturesToEditor[i].iAnglePerTextureRZ or 0, step, step_fast, flags)
                        if result then
                            if iValue >= 360 then
                                iValue = 360
                            elseif iValue <= -360 then
                                iValue = -360
                            end
                            tTexturesToEditor[i].iAnglePerTextureRZ = iValue
                        end

                        if tTextureOptions.iCurrentAlgorithm ~= 5 or tTextureOptions.bGridForceFitScale == false then
                        
                            tImGui.Text(tLang.L("scale_per_texture"))

                            local result, fValue = tImGui.InputFloat(tLang.L("scale_sx") .. '##ScalePerTextureX_' .. tostring(i), tTexturesToEditor[i].fScalePerTextureSX or 0, step_f, step_fast_f, format_f, flags)
                            if tTextureOptions.iIndexSortOption ~= 1 then
                                if tImGui.IsItemHovered(0) then
                                    tImGui.BeginTooltip()
                                    tImGui.Text(tLang.L("scale_per_texture_disabled"))
                                    tImGui.EndTooltip()
                                end
                            elseif result then
                                if fValue >= 10 then
                                    fValue = 10
                                elseif fValue <= -10 then
                                    fValue = -10
                                end
                                tTexturesToEditor[i].fScalePerTextureSX = fValue
                            end

                            local result, fValue = tImGui.InputFloat(tLang.L("scale_sy") .. '##ScalePerTextureY_' .. tostring(i), tTexturesToEditor[i].fScalePerTextureSY or 0, step_f, step_fast_f, format_f, flags)
                            if tTextureOptions.iIndexSortOption ~= 1 then
                                if tImGui.IsItemHovered(0) then
                                    tImGui.BeginTooltip()
                                    tImGui.Text(tLang.L("scale_per_texture_disabled"))
                                    tImGui.EndTooltip()
                                end
                            elseif result then
                                if fValue >= 10 then
                                    fValue = 10
                                elseif fValue <= -10 then
                                    fValue = -10
                                end
                                tTexturesToEditor[i].fScalePerTextureSY = fValue
                            end
                            
                        end
                        if tTexturesToEditor[i].isOutOfBounds then
                            tImGui.TextColored(tOutOfBoundsColor,'Texture is out of bounds!')
                        end
                        tImGui.TreePop()
                    else
                        if tTexturesToEditor[i].isOutOfBounds then
                            tImGui.PopStyleColor(1)
                        end
                    end
                    showTextureHoverImage(i)
                    ::continue::
                end
                tImGui.TreePop()
            end
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
        if tImGui.BeginMenu(tLang.L("menu_file")) then

            if mbm.is('Windows') then
                local pressed,checked = tImGui.MenuItem(tLang.L("load_texture_max32"), "Ctrl+I", false)
                if pressed then
                    onOpenTextures()
                end
            else
                local pressed,checked = tImGui.MenuItem(tLang.L("load_texture"), "Ctrl+I", false)
                if pressed then
                    onOpenTextures()
                end
            end

            local pressed,checked = tImGui.MenuItem(tLang.L("load_texture_from_folder"), nil , false)
            if pressed then
                onOpenTexturesFromFolder()
            end

            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem(tLang.L("save_texture_png"), nil, false)
            if pressed then
                if tRender:isLoaded() then
                    --TODO: 
                    onSaveTexture()
                else
                    tUtil.showMessageWarn(tLang.L("no_texture_loaded"))
                end
            end

            local pressed,checked = tImGui.MenuItem(tLang.L("generate_image_header"), nil, false)
            if pressed then
                if #tTexturesToEditor > 0 then
                    for i=1, #tTexturesToEditor do
                        local sFileNameTexture = tTexturesToEditor[i].file_name
                        if sFileNameTexture  and tTexturesToEditor[i].isSelected then
                            local sFileName = mbm.saveFile(tUtil.getShortName(sFileNameTexture):split('%.')[1] .. '.h','*.h')
                            if sFileName then
                                if not mbm.generateImageResourceHeaderFromPng(sFileNameTexture, sFileName) then
                                    tUtil.showMessageWarn(tLang.L("failed_to_generate_image_header"))
                                else
                                    tUtil.showMessage(string.format(tLang.L("image_header_generated_fmt"), sFileName))
                                end
                            end
                        end
                    end
                end
            end

            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem(tLang.L("save_texture_config"), "Ctrl+S", false)
            if pressed then
                if tRender:isLoaded() then
                    onSaveTextureConfiguration()
                else
                    tUtil.showMessageWarn(tLang.L("no_texture_loaded"))
                end
            end

            local pressed,checked = tImGui.MenuItem(tLang.L("load_texture_config"), "Ctrl+O", false)
            if pressed then 
                onLoadTextureConfiguration()
            end

            tImGui.Separator()
            if (tTextureOptions.bGridVisibleX or tTextureOptions.bGridVisibleY) and (tTextureOptions.iGridX > 1 or tTextureOptions.iGridY) then

                local pressed,checked = tImGui.MenuItem(tLang.L("save_texture_rectangles") .. "##SaveXRectangle", tLang.L("x_axis_orientation"), false)
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
                                            tUtil.showMessageWarn(tLang.L("failed_to_save_texture_rect"))
                                        end
                                        iCount = iCount + 1
                                    end
                                end
                            end
                            if iCount > 0 then
                                tUtil.showMessage(string.format(tLang.L("textures_saved_fmt"), iCount-1))
                            else
                                tUtil.showMessageWarn(tLang.L("no_texture_was_saved"))
                            end
                        end
                    else
                        tUtil.showMessageWarn(tLang.L("no_texture_loaded"))
                    end
                end

                local pressed,checked = tImGui.MenuItem(tLang.L("save_texture_rectangles") .. "##SaveYRectangle", tLang.L("y_axis_orientation"), false)
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
                                            tUtil.showMessageWarn(tLang.L("failed_to_save_texture_rect"))
                                        end
                                        iCount = iCount + 1
                                    end
                                end
                            end
                            if iCount > 0 then
                                tUtil.showMessage(string.format(tLang.L("textures_saved_fmt"), iCount-1))
                            else
                                tUtil.showMessageWarn(tLang.L("no_texture_was_saved"))
                            end
                        end
                    else
                        tUtil.showMessageWarn(tLang.L("no_texture_loaded"))
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

        if tImGui.BeginMenu(tLang.L("general_options")) then

            local pressed,checked = tImGui.MenuItem(tLang.L("view_image_list"), nil, false)
            if pressed then
                bTextureViewOpened = checked
            end
            tImGui.Separator()

            local pressed,checked = tImGui.MenuItem(tLang.L("enable_origin_lines"), true, tLineCenterX.visible)
            if pressed then
                tLineCenterX.visible = checked
                tLineCenterY.visible = checked
            end

            local pressed,checked = tImGui.MenuItem(tLang.L("pre_load_texture_filter"), true, tTextureOptions.bFilter)
            if pressed then
                tTextureOptions.bFilter = checked
            end

            local pressed,checked = tImGui.MenuItem(tLang.L("move_windows"), true, bEnableMoveWindow)
            if pressed then
                bEnableMoveWindow = checked
                if bEnableMoveWindow then
                    ImGuiWindowFlags_NoMove = 0
                else
                    ImGuiWindowFlags_NoMove = tImGui.Flags('ImGuiWindowFlags_NoMove')
                end
            end

            tLang.renderLanguageSubmenu()

            if tImGui.BeginMenu(tLang.L("background_color")) then
                local sz        = tImGui.GetTextLineHeight()
                
                local rounding  =  0
                local flags     =  0

                local colors    = { {'default',    tUtil.tColorBackground},
                                    {'white',      {r=1,g=1,b=1,a=1}},
                                    {'black',      {r=0,g=0,b=0,a=1}},
                                    {'red',        {r=1,g=0,b=0,a=1}},
                                    {'green',      {r=0,g=1,b=0,a=1}},
                                    {'blue',       {r=0,g=0,b=1,a=1}},
                                    {'cyan',       {r=0,g=1,b=1,a=1}},
                                    {'yellow',     {r=1,g=1,b=0,a=1}},
                                    {'magenta',    {r=1,g=0,b=1,a=1}}
                                  }
                
                for i=1, #colors do
                    local winPos  = tImGui.GetCursorScreenPos()
                    local p_max   = {x=winPos.x + sz,y=winPos.y + sz}
                    local name    = tLang.L(colors[i][1])
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

            if mbm.is('debug') then
                local label  = "Debug: print texture info with algorithm"
                local size   =  {x=0,y=0}
                if tImGui.Button(label, size) then
                    bPrintDebug = true
                end
            end

            tImGui.EndMenu()
        end

        if tImGui.BeginMenu(tLang.L("zoom")) then

            local label   = '##Scale'
            local v_min   = 0.2
            local v_max   = 10
            local format  = "Scale %.1f"
            local result, fValue = tImGui.SliderFloat(label, scale, v_min, v_max, format, tImGui.ImGuiSliderFlags_None)
            if result and fValue > 0 then
                scale = fValue
                tShape:setScale(scale,scale)
            end
            tImGui.TextDisabled(tLang.L("or_use_scroll"))
            tImGui.SameLine()
            if tImGui.SmallButton(tLang.L("default")) then
                scale = 1
                tShape:setScale(scale,scale)
            end
            tImGui.EndMenu()
        end


        if tImGui.BeginMenu(tLang.L("menu_about")) then
            local pressed,checked = tImGui.MenuItem(tLang.L("texture_packer_editor"), nil, false)
            if pressed then
                if mbm.is('windows') then
                    os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/editors.html#texture-packer-editor"')
                elseif mbm.is('linux') then
                    os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/editors.html#texture-packer-editor"')
                elseif mbm.is('macos') then
                    os.execute('open "https://mbm-documentation.readthedocs.io/en/latest/editors.html#texture-packer-editor"')
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
        isClickedMouseLeft = (key == 0)
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
            onSaveTextureConfiguration()
        elseif key == mbm.getKeyCode('O') then -- Ctrl+O
            onLoadTextureConfiguration()
        elseif key == mbm.getKeyCode('I') then -- Ctrl+I
            onOpenTextures()
        end
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = false
    end
end

function onLoop(delta)
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