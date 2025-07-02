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

   Font Maker Editor

   This is a script based on mbm engine.

   Font Maker meant to extract fonts from TrueType Font files and convert it to binary file to be used in the engine.

   More info at: https://mbm-documentation.readthedocs.io/en/latest/editors.html#font-maker

]]--

tImGui        =     require "ImGui"
tUtil         =     require "editor_utils"

if not mbm.get('USE_EDITOR_FEATURES') then
	mbm.messageBox('Missing features','Is necessary to compile using USE_EDITOR FEATURES to run this editor','ok','error',0)
	mbm.quit()
end


function onInitScene()
    camera2d		     = mbm.getCamera("2d")
    camera2d:setPos(-150,0)
    tLineCenterX         = line:new("2dw",0,0,50)
    tLineCenterY         = line:new("2dw",0,0,50)
    bEnableMoveWorld     = true
    bMovingAnyPoint      = false
    isClickedMouseLeft   = false
    tUtil.bRightSide     = true
    sFontSelected        = nil
    tGlobalFont          = nil
    iHeightFont          = 50
    iSpace               = 0
    iSpaceHeight         = 0
    tFontHeight          = {}
    sAnimationName       = ''
    --defaultText          = "\nSymbols:'\"\\/;.,<>|+-_!@#$%¨&*()?' '\nNumber:0123456789\nabcdefghijklmnopqrstuvxyz\nABCDEFGHIJKLMNOPQRSTUVXYZ\nSpace Space, Tab\tTab\nàèìòù ÀÈÌÒÙ áéíóú ÁÉÍÓÚ\nãõÃÕ âêîôû ÂÊÎÔÛ çÇ\nTime:"
    defaultText          = "\nSymbols:'\"\\/;.,<>|+-_!@#$%¨&*()?' '\nNumber:0123456789\nabcdefghijklmnopqrstuvxyz\nABCDEFGHIJKLMNOPQRSTUVXYZ\nSpace Space,\tTab\nTime:"
    sAdditionalText       = ''
    selectedLetter       = ''
    tAnimTypes           = {'PAUSED','GROWING','GROWING_LOOP', 'DECREASING', 'DECREASING_LOOP', 'RECURSIVE', 'RECURSIVE_LOOP'}
    tColorFont           = {r=0,g=0,b=0}
    tColorBackGround     = {r=0,g=0,b=0}
    bShowOrigin          = false
    bUseSolidColorBackGround = true
    tLineCenterX:add({-9999999,0, 9999999,0})
    tLineCenterY:add({0,-9999999, 0,9999999})
    tLineCenterX:setColor(1,0,0)
    tLineCenterY:setColor(0,1,0)
    tUtil.sMessageOverlay= 'Welcome to Font Maker!\n\nFirst add a TrueType Font!'
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
end

function updateText(tText,forceCalcRender)
    tText.text  = tText.defaultText .. os.date(" %H:%M:%S") .. '\n' .. sAdditionalText
    local x,y   = tText:getSize(forceCalcRender)
    tText:setPos(x * -0.5, y * 0.5)
end

function getHashFontName(fileName)
    local sHash = string.format('%s:%d',fileName,iHeightFont)
    return sHash
end

function copyShader(tSource,tDest)
    while tDest:getTotalAnim() < tSource:getTotalAnim() do
        tDest:addAnim(sAnimationName,mbm.GROWING,1,1,0)
        local index = tDest:getTotalAnim()
        tDest:setAnim(index)
        local tShader = tDest:getShader()
        if not tShader:load('font.ps',nil,mbm.GROWING,1.0,mbm.PAUSED,0.0) then
            tUtil.showMessageWarn('Failed to load shader ...')
        end
    end
    local indexAnim = select(2,tSource:getAnim())
    for i=1, tSource:getTotalAnim() do
        tSource:setAnim(i)
        tDest:setAnim(i)
        local tShaderSource = tSource:getShader()
        local tShaderDest   = tDest:getShader()
        local sourcePixel,sourceVertex = tShaderSource:getVars()
        for j=1, #sourcePixel do
            local var = sourcePixel[j]
            local sType,iType = tShaderSource:getPStype()
            tShaderDest:setPStype(iType)
            tShaderDest:setPSmin(var.name,  var.min[1],    var.min[2],    var.min[3],    var.min[4])
            tShaderDest:setPSmax(var.name,  var.max[1],    var.max[2],    var.max[3],    var.max[4])
            tShaderDest:setPS(var.name,     var.value[1],  var.value[2],  var.value[3],  var.value[4])
        end

        for j=1, #sourceVertex do
            local var = sourceVertex[j]
            local sType,iType = tShaderSource:getVStype()
            tShaderDest:setVStype(iType)
            tShaderDest:setVSmin(var.name,  var.min[1],    var.min[2],    var.min[3],    var.min[4])
            tShaderDest:setVSmax(var.name,  var.max[1],    var.max[2],    var.max[3],    var.max[4])
            tShaderDest:setVS(var.name,     var.value[1],  var.value[2],  var.value[3],  var.value[4])
        end
    end
    tDest:setAnim(indexAnim)
end

function loadFromCache(fileName)
    local sHash = getHashFontName(fileName)
    if tFontHeight[sHash] then
        if tGlobalFont then
            tGlobalFont.tText.visible = false
        end
        sFontSelected = fileName
        tNewFont      = tFontHeight[sHash]
        iHeightFont   = tNewFont.iHeightFont
        iSpace        = tNewFont:getSpace('x')
        iSpaceHeight  = tNewFont:getSpace('y')
        tNewFont.tText.visible = true
        local index   = select(2,tNewFont.tText:getAnim())
        if tGlobalFont then
            copyShader(tGlobalFont.tText,tNewFont.tText)
        end
        tGlobalFont   =  tNewFont
        return true
    else
        return false
    end
end

function setInitialColorFont(tNewFont)
    local tShader        = tNewFont.tText:getShader()
    local sPixel,sVertex = tShader:getNames()
    if sPixel and sPixel == 'font.ps' then
        local index   = select(2,tNewFont.tText:getAnim())
        tShader:setPSall('colorFont',tColorFont.r,tColorFont.g,tColorFont.b)
    end
end

function setInitialParameters(fileName,tNewFont)
    local sHash           = getHashFontName(fileName)
    local strText         = " Font:" .. tUtil.getShortName(fileName) .. defaultText
    local tText           = tNewFont:add(strText,"2dw")
    tNewFont.tText        = tText
    tText.defaultText     = strText
    tNewFont.iHeightFont  = iHeightFont
    iSpace                = math.floor(iHeightFont / 10)
    tNewFont.iSpace       = iSpace
    iSpaceHeight          = tNewFont:getSpace('y')
    tFontHeight[sHash]    = tNewFont
    tNewFont:setSpace('x',iSpace)
    setInitialColorFont(tNewFont)
    loadFromCache(fileName)
end

function loadNewFont(sNewFileName)
    local fileName = sNewFileName or mbm.openFile(sFontSelected,"*.ttf","fnt","true-font")
    if not loadFromCache(fileName) and fileName then
        if isBinaryFont(fileName) then
            local tNewFont        = font:new(fileName)
            setInitialParameters(fileName,tNewFont)
            tUtil.showMessage("Font Successfully Loaded!",5)
        else
            local saveAsPng       = true
            local tNewFont        = font:new(fileName,iHeightFont,iSpace,iSpaceHeight,saveAsPng)
            setInitialParameters(fileName,tNewFont)
            tNewFont.sTextureName = mbm.getFullPath(tNewFont:getTexture())
            tNewFont.sTmpFile     = string.format('%sFNT.png',os.tmpname ())
            if mbm.is('linux') then
                os.execute(string.format('mv %q %q',tNewFont.sTextureName, tNewFont.sTmpFile))
            elseif mbm.is('windows') then
                os.execute(string.format('MOVE %q %q',tNewFont.sTextureName, tNewFont.sTmpFile))
            end
            if sNewFileName == nil then
                tUtil.showMessage("Font Successfully Loaded!",5)
            end
        end
        tex_alpha_pattern.visible = true
    end
end

function onSaveFont()
    local meshD      = meshDebug:new()
    local fileName   = tUtil.getShortName(sFontSelected):split('%.')[1]
    fileName         = mbm.saveFile(string.format('%s-%d.fnt',fileName,iHeightFont),'fnt')
    if fileName then
        if meshD:load(tGlobalFont.tText) then
            if meshD:copyAnimationsFromMesh(tGlobalFont.tText) then --copy animations created
                if meshD:save(fileName) then
                    tUtil.showMessage("Successfully saved!")
                    if mbm.is('linux') then
                        local tDestTexture = fileName:split('/')
                        tDestTexture[#tDestTexture] = nil
                        local sTextureName = string.format('%s/%s',table.concat(tDestTexture,'/'),tUtil.getShortName(tGlobalFont.sTextureName))
                        os.execute(string.format('mv %q %q',tGlobalFont.sTmpFile,sTextureName))
                    elseif mbm.is('windows') then
                        local tDestTexture = fileName:split('\\')
                        tDestTexture[#tDestTexture] = nil
                        local sTextureName = string.format('%s\\%s',table.concat(tDestTexture,'\\'),tUtil.getShortName(tGlobalFont.sTextureName))
                        os.execute(string.format('MOVE %q %q',tGlobalFont.sTmpFile,sTextureName))
                    end
                else
                    tUtil.showMessageWarn("Failed to save font!")
                end
            else
                tUtil.showMessageWarn("Failed to apply shader!")
            end
        else
            tUtil.showMessageWarn("Failed to load font!")
        end
    end
end

function extractMatch(sLine,sPattern)
    local s,e = sLine:match(sPattern)
    return sLine:sub(s,e-1)
end


function createLetterFace(tLetter,width_texture,height_texture)

    local halfWidth  = tLetter.width
    local halfHeight = tLetter.height
    if halfWidth == 0 then
        halfWidth = 1
    end
    if halfHeight == 0 then
        halfHeight = 1
    end

    halfWidth  = halfWidth * 0.5
    halfHeight = halfHeight * 0.5

    local tVertex = {  {x = -halfWidth, y = -halfHeight, u = tLetter.x / width_texture,                   v = (tLetter.y + tLetter.height) / height_texture }, 
                       {x = -halfWidth, y =  halfHeight, u = tLetter.x / width_texture,                   v = tLetter.y  / height_texture },
                       {x =  halfWidth, y = -halfHeight, u = (tLetter.x + tLetter.width) / width_texture, v = (tLetter.y + tLetter.height) / height_texture }, 
                       {x =  halfWidth, y =  halfHeight, u = (tLetter.x + tLetter.width) / width_texture, v = tLetter.y  / height_texture },
                    }

    return tVertex
 end
 
 function createBinaryFont(sFileName,tFontStructure)
 
     --meshDebug is used to create dynamically mesh in the engine.
     local stride      = 2 --stride only can be 3 or 2. it means (x,y,z) or (x,y)
     local tMesh       = meshDebug:new() --new mesh debug to store the information about our mesh

     local tIndex      = {1,2,3, 3,2,4}
     for i=1, #tFontStructure.tLetters do
        local tLetter     = tFontStructure.tLetters[i]
        local indexFrame  = tMesh:addFrame(stride) -- Add one frame with stride 2 (x,y)
        local tVertex     = createLetterFace(tLetter, tFontStructure.scaleW,tFontStructure.scaleH )
        tLetter.index     = i - 1 -- index 0 based
        
        --To add vertex, first we need to add a subset
        local indexSubset     = tMesh:addSubSet(indexFrame) --add one subset for the first frame
    
        -- The table expected is : {{x,y,z,u,v},{x,y,z,u,v},{x,y,z,u,v}, ...}
        if not tMesh:addVertex(indexFrame,indexSubset,tVertex) then 
            tUtil.showMessageWarn("Error on add vertex buffer")
            return false
        end
        
        if not tMesh:addIndex(indexFrame,indexSubset,tIndex) then 
            tUtil.showMessageWarn("Error on add index buffer")
            return false
        end
    
        --apply the texture to frame / subset
        if not tMesh:setTexture(indexFrame,indexSubset,tFontStructure.sTextureFile) then
            tUtil.showMessageWarn("Error on set texture!")
            return false
        end
    end
 
     tMesh:setType('font')  -- set it to mesh type
     tMesh:setDetail({  letters   = tFontStructure.tLetters, 
                        font_name = tUtil.getShortName(sFileName),
                        height    = tFontStructure.iSize,
                        space_x   = 0,
                        space_y   = 0,
                    })
 
     local calcNormal,calcUv = true,false
     if tMesh:save(sFileName,calcNormal,calcUv) then
        tUtil.showMessage("Mesh created successfully ")
         return true
     else
        tUtil.showMessageWarn("Failed to create Mesh!")
         return false
     end
     
 end

function onParseFont(sFontSelected)
    local fileName = sFontSelected or mbm.openFile(sFontSelected,"txt")
    if fileName then
        local fp = io.open(fileName,"r")
        if fp then
            local tFontStructure = {sTextureFile = nil,tLetters = {}}
            local sPattern = 'char id=[%d]+%s.+x=[%d]+%s.+y=[%d]+%s.+width=[%d]+%s.+height=[%d]+%s.+xoffset=%-?[%d]+%s.+yoffset=%-?[%d]+%s.+xadvance=%-?[%d]+%s'
            for sLine in fp:lines() do
                sLine = sLine:gsub('\r','')
                if sLine:find('file="[%w_-%s%.]+"') then
                    local s,e = sLine:match('file="()[%w_-%s%.]+()"')
                    tFontStructure.sTextureFile = sLine:sub(s,e-1)
                elseif sLine:find('size=[%d]+') then
                    local s,e = sLine:match('size=()[%d]+()')
                    tFontStructure.iSize = tonumber(sLine:sub(s,e-1))
                elseif sLine:find('scaleW=[%d]+%s+scaleH=[%d]+') then
                    tFontStructure.scaleW = tonumber(extractMatch(sLine,'scaleW=()[%d]+()%s'))
                    tFontStructure.scaleH = tonumber(extractMatch(sLine,'scaleH=()[%d]+()%s'))
                elseif sLine:find(sPattern) then
                    local tLetter = {}
                    tLetter.char_id   = tonumber(extractMatch(sLine,'char id=()[%d]+()%s'))
                    tLetter.x         = tonumber(extractMatch(sLine,'%sx=()[%d]+()%s'))
                    tLetter.y         = tonumber(extractMatch(sLine,'%sy=()[%d]+()%s'))
                    tLetter.width     = tonumber(extractMatch(sLine,'%swidth=()[%d]+()%s'))
                    tLetter.height    = tonumber(extractMatch(sLine,'%sheight=()[%d]+()%s'))
                    tLetter.xoffset   = tonumber(extractMatch(sLine,'%sxoffset=()%-?[%d]+()%s'))
                    tLetter.yoffset   = tonumber(extractMatch(sLine,'%syoffset=()%-?[%d]+()%s'))
                    tLetter.xadvance  = tonumber(extractMatch(sLine,'%sxadvance=()%-?[%d]+()%s'))
                    table.insert(tFontStructure.tLetters,tLetter)
                end

            end
            if tFontStructure.sTextureFile == nil then
                tUtil.showMessageWarn('Texture not found in the txt file!')
            elseif tFontStructure.iSize == nil then
                tUtil.showMessageWarn('Size not defined in the txt file!')
            elseif #tFontStructure.tLetters == 0 then
                tUtil.showMessageWarn('Letters not defined in the txt file!')
            else
                if tFontStructure.scaleW == nil then
                    local width,height,id, has_alpha = mbm.loadTexture(tFontStructure.sTextureFile)
                    tFontStructure.scaleW = width
                    tFontStructure.scaleH = height
                end
                if tFontStructure.scaleW == nil then
                    tUtil.showMessageWarn('Texture size not defined in the txt file and could not load the texture!')
                else
                    tFontStructure.font_name = tUtil.getShortName(tFontStructure.sTextureFile):split('%.')[1]
                    local sFileName          = mbm.saveFile(string.format('%s.fnt',tFontStructure.font_name),'fnt')
                    if sFileName then
                        if createBinaryFont(sFileName,tFontStructure) then
                            tUtil.showMessageWarn('File [' .. tFontStructure.font_name .. ']  parsed successfully!')
                            loadNewFont(sFileName)
                        else
                            tUtil.showMessageWarn('Failed to parse file [' .. tFontStructure.font_name .. ']!')
                        end
                    end
                end
            end
            fp:close()
        end
    end
end

function renderMainMenu(delta)
    local width        = 300
    local iW, iH       = mbm.getRealSizeScreen()
    local height       = iH
    local tPosWin      = {x = 0, y = 0}
    local tSizeButton  = {x = 200, y = 0}
    local itemWidth    = 200
    local title        = 'True Type Font Import Options'
    tImGui.SetNextWindowSize({x = width, y = height},tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowPos(tPosWin , tImGui.Flags('ImGuiCond_Once'))
    local is_opened, closed_clicked = tImGui.Begin(title, false, tImGui.Flags('ImGuiWindowFlags_NoMove'))
    if is_opened then
        if tImGui.Button('Load Font...',tSizeButton) then
            loadNewFont(nil)
        end
        if tImGui.Button('Parse Font ... ',tSizeButton) then
            onParseFont()
        end
        if sFontSelected and tGlobalFont then
            tImGui.Text(tUtil.getShortName(sFontSelected))
            if tImGui.Button('Save Binary Font...',tSizeButton) then
                onSaveFont()
            end
        else
            tImGui.Text('No font loaded')
        end
        local step       =  1
        local step_fast  =  10
        tImGui.PushItemWidth(itemWidth)
        local clicked, iValue = tImGui.InputInt('Height', iHeightFont, step, step_fast)
        if clicked then
            if iValue > 0 and iValue < 1000 then
                iHeightFont = iValue
                if tGlobalFont and sFontSelected then
                    loadNewFont(sFontSelected)
                end
            end
        end

        local clicked, iValue = tImGui.InputInt('Space X', iSpace, step, step_fast)
        if clicked then
            if iValue > -1000 and iValue < 1000 then
                iSpace = iValue
                if tGlobalFont then
                    local tText = tGlobalFont.tText
                    tGlobalFont:setSpace('x',iSpace)
                    updateText(tGlobalFont.tText,true)
                end
            end
        end

        local clicked, iValue = tImGui.InputInt('Space Y', iSpaceHeight, step, step_fast)
        if clicked then
            if iValue > -1000 and iValue < 1000 then
                iSpaceHeight = iValue
                if tGlobalFont then
                    tGlobalFont:setSpace('y',iSpaceHeight)
                    updateText(tGlobalFont.tText,true)
                end
            end
        end

        local size       = {x = itemWidth, y =50}
        local flags      = 0
        local modified , sNewText = tImGui.InputTextMultiline('Your Text',sAdditionalText,size,flags)
        if modified then
            sAdditionalText = sNewText
        end

        bShowOrigin = tImGui.Checkbox('Show Origin',bShowOrigin)
        tLineCenterX.visible = bShowOrigin
        tLineCenterY.visible = bShowOrigin
        bUseSolidColorBackGround = tImGui.Checkbox('Background solid color',bUseSolidColorBackGround)

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

        if tImGui.TreeNode('id_master_anim',"Animation") then
            if tGlobalFont then
                local tText      = tGlobalFont.tText
                local label      = '##AddAnim'
                local hint       = '<name>'
                local flags      = 0
                tImGui.Text('Add:')
                tImGui.SameLine()
                tImGui.PushItemWidth(itemWidth-40)
                local modified , sNewText = tImGui.InputTextWithHint(label,sAnimationName,hint,flags)
                if modified then
                    sAnimationName = sNewText
                end
                tImGui.PopItemWidth()
                tImGui.SameLine()
                if tImGui.Button('+',{x=0,y=0}) then
                    if sAnimationName:len() > 0 then
                        tText:addAnim(sAnimationName,mbm.GROWING,1,1,0)
                        local index = tText:getTotalAnim()
                        tText:setAnim(index)
                        local tShader = tText:getShader()
                        if not tShader:load('font.ps',nil,mbm.GROWING,1.0,mbm.PAUSED,0.0) then
                            tUtil.showMessageWarn('Failed to load shader ...')
                         end
                    else
                        tUtil.showMessageWarn('A name is required to add animation!')
                    end
                end
                
                for i=1, tText:getTotalAnim() do
                    local sName           = select(1,tText:getAnim(i))
                    local iIndexAnim      = select(2,tText:getAnim())
                    local flag_node               = 0
                    if iIndexAnim == i then
                        flag_node  = tImGui.Flags('ImGuiTreeNodeFlags_Selected')
                    end
                    if iIndexAnim == i then
                        local is_open  = true
                        tImGui.SetNextItemOpen(is_open, tImGui.Flags('ImGuiCond_Always'))
                    else
                        local is_open  = false
                        tImGui.SetNextItemOpen(is_open, tImGui.Flags('ImGuiCond_Always'))
                    end
                    if tImGui.TreeNodeEx(sName, flag_node,'id_anim_' .. tostring(i)) then
                        if iIndexAnim ~= i then
                            local is_open  = true
                            tText:setAnim(i)
                        end
                        tImGui.Text('Type:')
                        local tShader        = tText:getShader()
                        local sPixel,sVertex = tShader:getNames()
                        if sPixel and sPixel == 'font.ps' then
                            local sType,iType    = tShader:getPStype()
                            local ret, current_item, item = tImGui.Combo('##ComboAnim' , iType + 1, tAnimTypes)
                            if ret then
                                iTypeAnim = current_item
                                tShader:setPStype(iTypeAnim-1)
                                tText:restartAnim()
                            end
                            local r,g,b = tShader:getPSmin('colorFont')
                            tImGui.Text('Color Min:')
                            local clicked, tRgb = tImGui.ColorEdit3('##color_min' .. tostring(1), {r=r,g=g,b=b}, flags)
                            if clicked then
                                tShader:setPSmin('colorFont',tRgb.r,tRgb.g,tRgb.b)
                                tText:restartAnim()
                            end
                            local r,g,b = tShader:getPSmax('colorFont')
                            tImGui.Text('Color Max:')
                            local clicked, tRgb = tImGui.ColorEdit3('##color_max' .. tostring(1), {r=r,g=g,b=b}, flags)
                            if clicked then
                                tShader:setPSmax('colorFont',tRgb.r,tRgb.g,tRgb.b)
                                tText:restartAnim()
                            end
                            local iTime      = tShader:getPStime()
                            local iMinTime   = tShader:getPSmin('colorFont')
                            local iMaxTime   = tShader:getPSmax('colorFont')
                            local step       =  0.01
                            local step_fast  =  0.1
                            local format     = "%.3f"
                            local flags      =  0
                            local result, fValue = tImGui.InputFloat('##TimeAnim' .. tostring(i), iTime, step, step_fast,format,flags)
                            if result then
                                if fValue >=0 and fValue <= 999999 then
                                    tShader:setPStime(fValue)
                                end
                            end
                            if tImGui.Button('Restart Animation',tSizeButton) then
                                tText:restartAnim()
                            end
                        else
                            tImGui.Text('Shader:' .. tostring(sPixel) .. '\nnot supported for edition!')    
                        end
                        tImGui.TreePop()
                    end
                end
            end
            tImGui.TreePop()

        end
        if tImGui.TreeNode('id_adjust_letters',"Letters Position") then
            if tGlobalFont then
                
                local step           =  0.5
                local step_fast      =  1
                local step_int       =  1
                local step_int_fast  =  5
                local format         = "%.1f"

                local sSymbolsLetters      = '\"\\/;.,<>|+-_!@#$%¨&*()? \t'
                local sAbcLowerCaseLetters = 'abcdefghijklmnopqrstuvxyz'
                local sAbcUpperCaseLetters = 'ABCDEFGHIJKLMNOPQRSTUVXYZ'
                local sDiacriticLetters    = 'àèìòùÀÈÌÒÙáéíóúÁÉÍÓÚãõÃÕâêîôûÂÊÎÔÛçÇ'
                local sNumberLetters       = '0123456789'

                local tAllLetters = {{ title = 'Number',            letters = sNumberLetters },
                                     { title = 'Lowercase Alphabet',letters = sAbcLowerCaseLetters },
                                     { title = 'Uppercase Alphabet',letters = sAbcUpperCaseLetters },
                                     { title = 'Symbols',           letters = sSymbolsLetters },
                                     --{ title = 'Diacritic Letters', letters = sDiacriticLetters },
                                    }
                for i=1, #tAllLetters do
                    local tLetter = tAllLetters[i]
                    if tImGui.TreeNode('id_adjust_letters_' .. tostring(i),tLetter.title) then
                        for j=1, tLetter.letters:len() do
                            local sLetter   = tLetter.letters:sub(j,j)
                            local label     = string.format('%s##letter_%s_%d',sLetter,sLetter,j)
                            if sLetter == ' ' then
                                label       = string.format('space##letter_%s_%d',sLetter,j)
                            end
                            
                            local selected  = selectedLetter == sLetter
                            local flags     = 0
                            local size      = {x=40,y=0}
                            local result    = tImGui.Selectable(label, selected, flags, size)
                            if result then
                                selectedLetter = sLetter
                            end
                            if selected then
                                tImGui.PushItemWidth(150)
                                tImGui.Text('Change Position')
                                local iValue   = tGlobalFont:getLetterXDiff(sLetter)
                                local result, fValue = tImGui.InputFloat('X##LetterAdjust_x', iValue, step, step_fast,format,flags)
                                if result then
                                    if fValue > -1000 and fValue < 1000 then
                                        tGlobalFont:setLetterXDiff(sLetter,fValue)
                                    end
                                end
                                local iValue   = tGlobalFont:getLetterYDiff(sLetter)
                                local result, fValue = tImGui.InputFloat('Y##LetterAdjust_y', iValue, step, step_fast,format,flags)
                                if result then
                                    if fValue > -1000 and fValue < 1000 then
                                        tGlobalFont:setLetterYDiff(sLetter,fValue)
                                    end
                                end
                                tImGui.Text('Change Size')
                                local iWidth,iHeight   = tGlobalFont:getSizeLetter(sLetter)
                                local result, fValue = tImGui.InputInt('X##LetterSize_x', iWidth, step_int, step_int_fast)
                                if result then
                                    if fValue >= 0 and fValue < 1000 then
                                        iWidth = fValue
                                        tGlobalFont:setSizeLetter(sLetter,iWidth,iHeight)
                                    end
                                end
                                local result, fValue = tImGui.InputInt('Y##LetterSize_y', iHeight, step_int, step_int_fast)
                                if result then
                                    if fValue >= 0 and fValue < 1000 then
                                        iHeight = fValue
                                        tGlobalFont:setSizeLetter(sLetter,iWidth,iHeight)
                                    end
                                end
                                tImGui.PopItemWidth()
                            end
                        end
                        tImGui.TreePop()
                    end
                end
            end
            tImGui.TreePop()
        end
        
        tImGui.PopItemWidth()
    end
    tImGui.End()
end

function isBinaryFont(fileName)
	local isMbm = fileName:match("%.mbm$")
    local isFnt = fileName:match("%.fnt$")
    if isMbm or isFnt then
        local myType = meshDebug:getType(fileName)
        if myType == "font" then
            return true
        else
            local msg = "File: " ..tUtil.getShortName(fileName,true) .. "\nis not a font!\nType:" .. myType
            tUtil.showMessageWarn(msg,5)
            return false
        end
    end
	return isMbm or isFnt
end

function onTouchDown(key,x,y)
    isClickedMouseLeft = key == 1
    camera2d.mx = x
    camera2d.my = y
    
end

function onTouchMove(key,x,y)
    if bEnableMoveWorld and isClickedMouseLeft and not bMovingAnyPoint and not tImGui.IsAnyWindowHovered() then
        local px = (camera2d.mx - x) * camera2d.sx
        local py = (camera2d.my - y) * camera2d.sy
        camera2d.mx = x
        camera2d.my = y
        camera2d:setPos(camera2d.x + px,camera2d.y - py)
    end
end

function onTouchUp(key,x,y)
    isClickedMouseLeft = false
    bMovingAnyPoint = false
    camera2d.mx = x
    camera2d.my = y
end

function onKeyDown(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = true
    elseif keyControlPressed then
        if key == mbm.getKeyCode('S') then -- Ctrl+S
            if sFontSelected and tGlobalFont then
                onSaveFont()
            end
        elseif key == mbm.getKeyCode('O') then -- Ctrl+O
            loadNewFont(nil)
        elseif key == mbm.getKeyCode('P') then -- Ctrl+P
            onParseFont()
        end
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = false
    end
end

function loop(delta)
    renderMainMenu(delta)
    tex_alpha_pattern:setPos(camera2d.x,camera2d.y)
    
    if tGlobalFont then
        updateText(tGlobalFont.tText,false)
    end

    tUtil.showOverlayMessage()
end
