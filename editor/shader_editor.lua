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

   Shader Editor

   This is a script based on mbm engine.

   Shader Editor is mean to edit shader for mesh, sprites, font, or others types of renderizable from the engine.

   More info at: https://mbm-documentation.readthedocs.io/en/latest/editors.html#shader-editor

]]--

tImGui        =     require "ImGui"
tUtil         =     require "editor_utils"

if not mbm.get('USE_EDITOR_FEATURES') then
	mbm.messageBox('Missing features','Is necessary to compile using USE_EDITOR FEATURES to run this editor','ok','error',0)
	mbm.quit()
end

function onInitScene()
    camera2d	              = mbm.getCamera("2d")
    camera3d	              = mbm.getCamera("3d")
    vPosCam 	              = camera2d:getPos()
    camera3d:setFar(10000)
    vFocusCam 	              = camera3d:getFocus()
    fileNameCurrent           = ""
    fScaleMesh                = 1
    bTextureViewOpened        = false
    bShowShaderMenu           = false
    isClickedMouseleft        = false
    isClickedMouseRight       = false
    iAnimationIndex           = 1
    sLastFrameText            = ''
    sLastAnimationInfoText    = ''
    iLastFrameText            = 0
    iLastAnimationInfoText    = 0
    sLastEditorFileName       = ''
    bShowExampleShader        = false
    tShader                   = nil
    sDefaultText              = "\nSymbols:'\"\\/;.,<>|+-_!@#$%¨&*()?' '\nNumber:012345678\nabcdefghijklmnopqrstuvxz\nABCDEFGHIJKLMNOPQRSTUVXZ\nTime:"
    tWindowsTitle = {
            title_shaders_options   = 'Shader Options',
            title_image_selector    = 'Image(s) selector', 
            title_status            = 'Shader Status',
            title_example_shader    = 'Example Of Shader File',
            title_animation         = 'Add Animation' 
    }
    tAnimationOptions = {
                            sNameAnim     = 'No Name',
                            fTimeFrame    = 1,
                            iTypeAnim     = 1,
                            iFrameStart   = 1,
                            iFrameStop    = 1,
                            tAnimTypes    = {'PAUSED','GROWING','GROWING_LOOP', 'DECREASING', 'DECREASING_LOOP', 'RECURSIVE', 'RECURSIVE_LOOP'}}
    tTexturesToEditor       = {}
    bEnableMoveWindow       = false
    ImGuiWindowFlags_NoMove = tImGui.Flags('ImGuiWindowFlags_NoMove')
    tLineCenterX            = line:new("2dw",0,0,200)
    tLineCenterY            = line:new("2dw",0,0,200)
    local tLx               = {-9999999,0, 9999999,0}
    local tLy               = {0,-9999999, 0,9999999}
    tLineCenterX:add(tLx)
    tLineCenterY:add(tLy)
    tLineCenterX:setColor(1,0,0)
    tLineCenterY:setColor(0,1,0)
    tUtil.sMessageOverlay= 'Welcome to Shader Editor!!!'
    local sTextureFileName = tUtil.createAlphaPattern(1024,768,32,{r=240,g=240,b=240},{r=125,g=125,b=125})
    if sTextureFileName ~= nil then
        local iW, iH      = mbm.getSizeScreen()
        tex_alpha_patern  = texture:new('2dw')
        tex_alpha_patern:load(sTextureFileName)
        tex_alpha_patern:setSize(iW, iH)
        tex_alpha_patern.z       = 99
        tex_alpha_patern.visible = false
    else
        print('Could not create the alpha pattern!')
    end
    tLineCenterX.z = 98
    tLineCenterY.z = 97

end

function onOpenMesh()
    local fileName = mbm.openFile(fileNameCurrent,"spt","msh","fnt",'tile')
	if fileName then
        local meshD = meshDebug:new()
        if meshD:load(fileName) then
            if tMesh then
                tMesh:destroy()
                tMesh = nil
            end
            tGlobalFont = nil
            local myType = meshD:getType()
            if myType == 'sprite' then
                tMesh = sprite:new('2dw')
                tMesh.is3d = false
            elseif myType == 'mesh' then
                tMesh = mesh:new('3d')
                tMesh.is3d = true
            elseif myType == 'font' then
                local strText = " Font:" .. tUtil.getShortName(fileName) .. sDefaultText
                tGlobalFont        = font:new(fileName)
                tMesh              = tGlobalFont:add(strText,"2dw")
                tMesh.sDefaultText = strText
                tMesh.is3d = false
            elseif myType == 'tile' then
                tMesh = tile:new('2dw')
                tMesh.is3d = false
            else
                tUtil.showMessageWarn("Failed to load \n[]"..tUtil.getShortName(fileName) .. "\nUnexpected type:" .. myType)
            end

            if tGlobalFont or (tMesh and tMesh:load(fileName)) then
                fileNameCurrent = fileName
                tShader         = tMesh:getShader()
                prepare3d2d(tMesh.is3d)
                bShowShaderMenu = true
                tUtil.showMessage("File Opened Successfully!!!")
            else
                tMesh = nil
                tUtil.showMessageWarn("Failed to Load ".. myType.. "\nfile:\n["..tUtil.getShortName(fileName).."]")
            end
        end
	end
end

function showAnimationAdd()

    if tMesh then
        local width            = 200
        local tSizeBtn         = {x=width - 20,y=0}
        local x_pos, y_pos     = 0, 0
        local window_pos       = {x = 220, y = 20}
        local window_pos_pivot = {x = 0, y = 0}
        tImGui.SetNextWindowPos(window_pos, tImGui.Flags('ImGuiCond_Once'), window_pos_pivot);
        local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_animation, true, ImGuiWindowFlags_NoMove)
        if is_opened then
            tImGui.PushItemWidth(180)
            tImGui.Text('Add Animation')
            local v_min     = 1
            local v_max     = tMesh:getTotalFrame()
            local result, iValue = tImGui.SliderInt('##StartAddAnimFrame', tAnimationOptions.iFrameStart, v_min, v_max, "Start Frame %d")
            if result then
                tAnimationOptions.iFrameStart = iValue
                if tAnimationOptions.iFrameStop < iValue then
                    tAnimationOptions.iFrameStop = iValue
                end
            end

            local result, iValue = tImGui.SliderInt('##StopAddAnimFrame', tAnimationOptions.iFrameStop, v_min, v_max, "Stop Frame %d")
            if result then
                tAnimationOptions.iFrameStop  = iValue
                if tAnimationOptions.iFrameStart > iValue then
                    tAnimationOptions.iFrameStart = iValue
                end
            end

            tImGui.Text('Type Of Animation:')
            local ret, current_item, item = tImGui.Combo('##ComboAnimAdd' , tAnimationOptions.iTypeAnim, tAnimationOptions.tAnimTypes)
            if ret then
                tAnimationOptions.iTypeAnim = current_item
            end

            tImGui.Text('Time Between Frames:')
            local step       =  0.01
            local step_fast  =  0.1
            local format     = "%.3f"
            local flags      =  0
            local result, fValue = tImGui.InputFloat('##TimeFrame', tAnimationOptions.fTimeFrame, step, step_fast,format,flags)
            if result then
                if fValue >=0 and fValue <= 999999 then
                    tAnimationOptions.fTimeFrame = fValue
                end
            end

            tImGui.Text('Name:')
            local label      = '##NameAnimAdd'
            local hint       = '<Max 32 Characters>'
            local flags      = 0
            
            local modified , sNewText = tImGui.InputTextWithHint(label,tAnimationOptions.sNameAnim,hint,flags)
            if modified then
                if sNewText:len() > 32 then
                    sNewText = sNewText:sub(1,32)
                end
                tAnimationOptions.sNameAnim = sNewText
            end

            tImGui.Separator()
            tImGui.PopItemWidth()
            if tImGui.Button('Add Animation', tSizeBtn) then
                if tMesh:addAnim(tAnimationOptions.sNameAnim,tAnimationOptions.iTypeAnim-1,tAnimationOptions.iFrameStart,tAnimationOptions.iFrameStop,tAnimationOptions.fTimeFrame) then
                    tUtil.showMessage('Animation Added successfully!')
                else
                    tUtil.showMessageWarn('Error on Add Animation!')
                end
            end
        end
        tImGui.End()
        if closed_clicked then
            bShowAnimation = false
        end
    end
end

function prepare3d2d(is3d)
	if tLineCenterX then
		tLineCenterX:destroy()
		tLineCenterY:destroy()
	end
	if is3d then
		tLineCenterX = line:new("3d",0,0,0)
		tLineCenterY = line:new("3d",0,0,0)
		local tLx = {-9999999,0,0, 9999999,0,0}
		local tLy = {0,-9999999,0, 0,9999999,0}
		tLineCenterX:add(tLx)
		tLineCenterY:add(tLy)
		tLineCenterX:setColor(1,0,0)
		tLineCenterY:setColor(0,1,0)
		vPosCam 	= camera3d:getPos()
	else
		tLineCenterX = line:new("2dw",0,0,200)
		tLineCenterY = line:new("2dw",0,0,200)
		local tLx = {-9999999,0, 9999999,0}
		local tLy = {0,-9999999, 0,9999999}
		tLineCenterX:add(tLx)
		tLineCenterY:add(tLy)
		tLineCenterX:setColor(1,0,0)
		tLineCenterY:setColor(0,1,0)
		vPosCam 	= camera2d:getPos()
	end
end


function onSaveMeshBinary()
	if tMesh then
		local ext               = tUtil.getExtension(fileNameCurrent)
		local fileName          = mbm.saveFile(fileNameCurrent,ext)
		if fileName then
			local meshD = meshDebug:new()
			if meshD:load(fileNameCurrent) then
				if meshD:copyAnimationsFromMesh(tMesh) then
					if meshD:save(fileName) then
						tUtil.showMessage("file:\n" .. tUtil.getShortName(fileName) .. "\nSuccessfully Saved!")
					else
						tUtil.showMessageWarn("Failed To Save :\n" .. tUtil.getShortName(fileName))
					end
				else
					tUtil.showMessageWarn("Failed To Apply Shader:\n" .. tUtil.getShortName(fileName))
				end
			else
				tUtil.showMessageWarn("Failed To Load \n[]"..tUtil.getShortName(fileNameCurrent))
			end
		end
    else
        tUtil.showMessageWarn("There Is No Mesh To Save!")
	end
end	

function onOpenImage()
    local file_name = mbm.openMultiFile(sLastTextureOpenned,table.unpack(tUtil.supported_images))
    if file_name then
        tTexturesToEditor = tUtil.loadInfoImagesToTable(file_name,tTexturesToEditor)
        bTextureViewOpened = true
        if type(file_name) == 'string' then
            sLastTextureOpenned = file_name
        elseif type(file_name) == 'table' and #file_name > 0 then
            sLastTextureOpenned = file_name[1]
        end
    end
end

function drawStrength(title,x,y,z)
    tImGui.Text(title)
    local ImVec2        = tImGui.GetCursorScreenPos()
    local space         = 10
    local radius        = 25
    local center        = {x=ImVec2.x + radius,y=ImVec2.y + radius + space}
    local yellow_color  = {r=1,g=1,b=0.4,a=1}
    local color_red     = {r=1,g=0,b=0,a=1.0}
    local color_magent  = {r=1,g=0,b=1,a=1.0}
    local num_segments  = 30
    local thickness     = 20
    local left          = 0
    
    tImGui.AddCircle(center, radius, yellow_color, num_segments, thickness)
    vNormal:set(x,-y)
    local length = vNormal:length()
    vNormal:normalize()
    x , y = vNormal.x, -vNormal.y
    vNormal:mul(radius,radius)
    vNormal:add(center.x,center.y)
    local r_min  = {x = vNormal.x - thickness * 0.5, y = vNormal.y - thickness * 0.5}
    local r_max  = {x = vNormal.x + thickness * 0.5, y = vNormal.y + thickness * 0.5}
    local clip   = true
    if tImGui.IsMouseHoveringRect(r_min,r_max,clip) then
        if tImGui.IsMouseDown(left) then
            local mousePos = tImGui.GetMousePos()
            tImGui.AddCircleFilled(mousePos, thickness * 0.5, color_magent, 18)
            vNormal:set(mousePos.x,mousePos.y)
            vNormal:sub(center.x,center.y)
            vNormal:normalize()
            x , y = vNormal.x, -vNormal.y
        else
            tImGui.AddCircleFilled({x = vNormal.x, y = vNormal.y}, thickness * 0.5, color_magent, 18)
        end
    else
        tImGui.AddCircleFilled({x = vNormal.x, y = vNormal.y}, thickness * 0.5, color_red, 18)
    end

    vAzimuth:set(x,y)
    local azimuth = math.deg(vAzimuth:azimuth())
    tImGui.SetCursorScreenPos({x=ImVec2.x + radius * 3,y = ImVec2.y + radius * 0.7})
    tImGui.TextDisabled(string.format('%d°',azimuth))
    local tNewPos = tImGui.GetCursorScreenPos()
    tImGui.SetCursorScreenPos({x=tNewPos.x + radius * 3,y = tNewPos.y})
    tImGui.TextDisabled(string.format('X:%0.3f',x))
    local tNewPos = tImGui.GetCursorScreenPos()
    tImGui.SetCursorScreenPos({x=tNewPos.x + radius * 3,y = tNewPos.y})
    tImGui.TextDisabled(string.format('Y:%0.3f',y))
    tImGui.SetCursorScreenPos({x=ImVec2.x,y = ImVec2.y + radius * 2 + space * 2})

    local label      = '##strength' .. title
    local step       =  1.0
    local step_fast  =  10.0
    local format     = "%.2f"
    local flags      =  0
    tImGui.Text('Strength')
    tImGui.SameLine()
    tImGui.PushItemWidth(100)
    local result, fValue = tImGui.InputFloat(label, length, step, step_fast, format, flags)
    if result then
        if fValue > 1 and fValue <= 1000 then
            length = fValue
        end
    end
    tImGui.PopItemWidth()

    local label      = '##ZDir' .. title
    tImGui.Text('Z       ')
    tImGui.SameLine()
    tImGui.PushItemWidth(100)
    local result, fValue = tImGui.InputFloat(label, z, step, step_fast, format, flags)
    if result then
        if fValue >= -1000 and fValue <= 1000 then
            z = fValue
        end
    end
    tImGui.PopItemWidth()

    local tNewPos = tImGui.GetCursorScreenPos()
    tImGui.SetCursorScreenPos({x=tNewPos.x,y = tNewPos.y + space})
    return x * length , y * length, z
end

function drawSlider(value,title,v_min,v_max,power)
    tImGui.PushItemWidth(150)
    tImGui.Text(title)
    local label   = '##' ..title
    local format  = "%.3f"
    local result, fValue = tImGui.SliderFloat(label, value, v_min, v_max, format,power)
    if result then
        value = fValue
    end
    tImGui.PopItemWidth()
    return value
end

function colorRGBMinMax(ps_vs_name,tVar)
    local flags = 0
    local ret   = false
    if tImGui.TreeNodeEx(tVar.name,flags) then
        tImGui.TextDisabled('Current color')
        local flag_color = tImGui.Flags('ImGuiColorEditFlags_HDR','ImGuiColorEditFlags_NoLabel')
        local label      = 'Select the current color for ' .. tVar.name .. '##Current Color'  .. ps_vs_name .. tVar.name
        local tColor     = { r = tVar.value[1], g = tVar.value[2], b = tVar.value[3]}
        local clicked, tRgb = tImGui.ColorEdit3(label, tColor, flag_color)
        if clicked then
            tVar.value[1] = tRgb.r
            tVar.value[2] = tRgb.g
            tVar.value[3] = tRgb.b
            ret           = true
        end

        tImGui.TextDisabled('Minimum color')
        local label      = 'Select the minimum color for ' .. tVar.name ..'##Minimum Color' .. ps_vs_name .. tVar.name
        local tColor     = { r = tVar.min[1], g = tVar.min[2], b = tVar.min[3]}
        local clicked, tRgb = tImGui.ColorEdit3(label, tColor, flag_color)
        if clicked then
            tVar.min[1] = tRgb.r
            tVar.min[2] = tRgb.g
            tVar.min[3] = tRgb.b
            ret           = true
        end

        tImGui.TextDisabled('Maximum color')
        local label      = 'Select the maximum color for ' .. tVar.name .. '##Maximum Color' .. ps_vs_name .. tVar.name
        local tColor     = { r = tVar.max[1], g = tVar.max[2], b = tVar.max[3]}
        local clicked, tRgb = tImGui.ColorEdit3(label, tColor, flag_color)
        if clicked then
            tVar.max[1] = tRgb.r
            tVar.max[2] = tRgb.g
            tVar.max[3] = tRgb.b
            ret           = true
        end
        tImGui.TreePop()
    end
    return ret
end

function colorRGBAMinMax(ps_vs_name,tVar)
    local flags = 0
    local ret   = false
    if tImGui.TreeNodeEx(tVar.name,flags) then
        local flag_color = tImGui.Flags('ImGuiColorEditFlags_HDR','ImGuiColorEditFlags_NoLabel')
        local label      = 'Select the current color for ' .. tVar.name .. '##Current Color' .. ps_vs_name .. tVar.name
        local tColor     = { r = tVar.value[1], g = tVar.value[2], b = tVar.value[3], a = tVar.value[4]}
        local clicked, tRgb = tImGui.ColorEdit4(label, tColor, flag_color)
        if clicked then
            tVar.value[1] = tRgb.r
            tVar.value[2] = tRgb.g
            tVar.value[3] = tRgb.b
            tVar.value[4] = tRgb.a
            ret           = true
        end

        local label      = 'Select the minimum color for ' .. tVar.name .. '##Minimum Color' .. ps_vs_name .. tVar.name
        local tColor     = { r = tVar.min[1], g = tVar.min[2], b = tVar.min[3], a = tVar.min[4]}
        local clicked, tRgb = tImGui.ColorEdit4(label, tColor, flag_color)
        if clicked then
            tVar.min[1] = tRgb.r
            tVar.min[2] = tRgb.g
            tVar.min[3] = tRgb.b
            tVar.min[4] = tRgb.a
            ret         = true
        end

        local label      = 'Select the maximum color for ' .. tVar.name .. '##Maximum Color' .. ps_vs_name .. tVar.name
        local tColor     = { r = tVar.max[1], g = tVar.max[2], b = tVar.max[3], a = tVar.max[4]}
        local clicked, tRgb = tImGui.ColorEdit4(label, tColor, flag_color)
        if clicked then
            tVar.max[1] = tRgb.r
            tVar.max[2] = tRgb.g
            tVar.max[3] = tRgb.b
            tVar.max[4] = tRgb.a
            ret         = true
        end
        tImGui.TreePop()
    end
    return ret
end

function inputFloatMinMax(ps_vs_name,tVar,index,sAlias,sTreeName)
    local flags = 0
    local ret   = false
    if tImGui.TreeNodeEx(sTreeName or tVar.name,flags) then

        local label      = '##' .. ps_vs_name .. '-' .. tVar.name .. '-' .. tostring(index)
        local step       =  (tVar.max[index] - tVar.min[index]) * 0.05
        local step_fast  =  step * 5
        local format     = "%.7f"
        local result, fValue = tImGui.InputFloat(label, tVar.value[index], step, step_fast, format, flags)
        if result then
            if fValue >= tVar.min[index] and fValue <= tVar.max[index] then
                tVar.value[index] = fValue
                ret = true
            end
        end

        step       =  0.5
        step_fast  =  1

        label      = '##' .. ps_vs_name .. '-min-' .. tVar.name
        tImGui.TextDisabled(sAlias .. ' min')
        local result, fValue = tImGui.InputFloat(label, tVar.min[index], step, step_fast, format, flags)
        if result then
            if fValue <= tVar.max[index] then
                tVar.min[index] = fValue
                ret = true
            end
        end

        label      = '##' .. ps_vs_name .. '-max-' .. tVar.name
        tImGui.TextDisabled(sAlias .. ' max')
        local result, fValue = tImGui.InputFloat(label, tVar.max[index], step, step_fast, format, flags)
        if result then
            if fValue >= tVar.min[index] then
                tVar.max[index] = fValue
                ret = true
            end
        end

        tImGui.TreePop()
    end
    return ret
end

function getSelectedTexturesFromImageSelector(tTexturesIn)
    local tSelected = {}
    for i=1, #tTexturesIn do
        if tTexturesIn[i].isSelected then
            table.insert(tSelected,tTexturesIn[i])
        end
    end
    return tSelected
end

function showShaderOptions()
    if tMesh then
        local width = 220
        local x_pos, y_pos = 0, 0
        local max_width = 220
        local tSizeBtn   = {x=width - 20,y=0} -- size button
        local tSizeBtnAddSet   = {x=43,y=0} 
        tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_shaders_options,x_pos,y_pos,width,max_width)
        local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_shaders_options, true, ImGuiWindowFlags_NoMove)
        if is_opened then

            local sAnim, indexCurrentAnim = tMesh:getAnim()
            tImGui.PushItemWidth(150)
            tImGui.Text('Animation')
            tImGui.TextDisabled(sAnim)
            local label      = '##Number of animation'
            local step       =  1
            local step_fast  =  10
            local flags      =  0

            local result, iValue = tImGui.InputInt(label, indexCurrentAnim, step, step_fast, flags)
            if result then
                if iValue > 0 and iValue <= tMesh:getTotalAnim() then
                    tMesh:setAnim(iValue)
                    indexCurrentAnim = iValue
                end
            end

            tImGui.SameLine()
            if tImGui.Button('Add', tSizeBtnAddSet) then
                bShowAnimation = true
            end
            
            if tImGui.Button('Restart Animation', tSizeBtn) then
                tMesh:restartAnim()
                indexCurrentStage = 1
            end

            if tImGui.TreeNode("Blend") then
                local sBlendState,iBlendIndex = tMesh:getBlend()
		        local sOperation              = tShader:getBlendOp()
                local tBlend = {'DISABLE',
                                'ZERO',
                                'ONE',
                                'SRC COLOR',
                                'INV SRC COLOR',
                                'SRC ALPHA',
                                'INV SRC ALPHA',
                                'DEST ALPHA',
                                'INV DEST ALPHA',
                                'DEST COLOR',
                                'INV DEST COLOR'}

                local tBlendOperation = {"ADD",
                                        "SUBTRACT",
                                        "REVERSE_SUBTRACT",
                                        "MIN",
                                        "MAX"}

                tImGui.Text('Blend Function')
                tImGui.SameLine()
                tImGui.HelpMarker('Blend Function is the same for all animations')
                local ret, current_item, item = tImGui.Combo('##ComboBlendFunction' , iBlendIndex + 1, tBlend)
                if ret then
                    iBlendIndex = current_item - 1
                    tMesh:setBlend(iBlendIndex)
                end

                tImGui.Text('Blend Operation')
                tImGui.SameLine()
                tImGui.HelpMarker('Blend Operation is the same for all animations however it is per shader')
                local iBlendOpIndex = 1
                for i=1, #tBlendOperation do
                    if tBlendOperation[i] == sOperation then
                        iBlendOpIndex = i
                        break
                    end
                end
                local ret, current_item, item = tImGui.Combo('##ComboBlendOperation' , iBlendOpIndex, tBlendOperation)
                if ret then
                    tShader:setBlendOp(tBlendOperation[current_item])
                end

                tImGui.TreePop()
            end

            local psName,vsName        = tShader:getNames()
            local tVarPs,tVarVs        = tShader:getVars()

            if tImGui.TreeNode("Pixel Shader") then
                local tShaderList = mbm.getShaderList(false,'ps')
                table.insert(tShaderList,'\0')
                table.sort(tShaderList)
                local iIndexShader = 1
                if psName then
                    for i=1, #tShaderList do
                        if tShaderList[i] == psName then
                            iIndexShader = i
                            break
                        end
                    end
                end
                tShaderList[1]      = 'No Shader'
                
                local ret, current_item, item = tImGui.Combo('##ComboPixelShader' , iIndexShader, tShaderList)
                if ret then
                    local sType,iTypeVs = tShader:getVStype()
                    local fTimeVs       = tShader:getVStime()
                    local tTextureS2    = tShader:getTextureStage2()
                    if tShaderList[current_item] == 'No Shader' then
                        psName = nil
                    else
                        psName = tShaderList[current_item]
                    end
                    -- Fill the previous values from vertex shader since we change the pixel shader
                    if tShader:load(psName,vsName,mbm.GROWING,1.0,iTypeVs,fTimeVs) then
                        for j=1, #tVarVs do
                            local tVar = tVarVs[j]
                            tShader:setVSmin(tVar.name,  tVar.min[1],    tVar.min[2],    tVar.min[3],    tVar.min[4])
                            tShader:setVSmax(tVar.name,  tVar.max[1],    tVar.max[2],    tVar.max[3],    tVar.max[4])
                            tShader:setVS(tVar.name,     tVar.value[1],  tVar.value[2],  tVar.value[3],  tVar.value[4])
                        end
                        if tTextureS2 then
                            tMesh:setTexture(tTextureS2,true,2)
                        end
                    else
                        tUtil.showMessageWarn('Failed To Load Shader:\n' .. tostring(tShaderList[current_item]))
                    end
                end
                if psName then
                    tImGui.Text('Type')
                    local sType,iTypePs       = tShader:getPStype()
                    local ret, current_item, item = tImGui.Combo('##ComboAnimPS' , iTypePs + 1, tAnimationOptions.tAnimTypes)
                    if ret then
                        iTypePs = current_item - 1
                        tShader:setPStype(iTypePs)
                        tMesh:restartAnim()
                    end

                    tImGui.Text('Time')
                    tImGui.SameLine()
                    tImGui.TextDisabled(string.format(' default(%.3g)',tShader:getPStime(true)))
                    local iTimePs = tShader:getPStime()
                    local label   = '##TimePs'
                    local format  = "%.3f"
                    local step       =  0.1
                    local step_fast  =  1
                    local result, fValue = tImGui.InputFloat(label, iTimePs, step, step_fast, format, 0)
                    if result then
                        if fValue >= 0 and fValue <= 99999999 then
                            if not tShader:setPStime(fValue) then
                                tUtil.showMessageWarn('Error on apply time to pixel shader')
                            end
                        end
                    end
                end
                for j=1, #tVarPs do
                    local tVar = tVarPs[j]
                    if tVar.type == 'number' then
                        if inputFloatMinMax(psName,tVar,1,'Value') then
                            tShader:setPSmin(tVar.name,  tVar.min[1],    tVar.min[2],    tVar.min[3],    tVar.min[4])
                            tShader:setPSmax(tVar.name,  tVar.max[1],    tVar.max[2],    tVar.max[3],    tVar.max[4])
                            tShader:setPS(tVar.name,     tVar.value[1],  tVar.value[2],  tVar.value[3],  tVar.value[4])
                        end
                    elseif tVar.type == 'vec2' then
                        local tXY = {'X', 'Y'}
                        for k= 1, #tXY do
                            if inputFloatMinMax(psName,tVar,k,tXY[k], tVar.name .. '-' ..tXY[k]) then
                                tShader:setPSmin(tVar.name,  tVar.min[1],    tVar.min[2],    tVar.min[3],    tVar.min[4])
                                tShader:setPSmax(tVar.name,  tVar.max[1],    tVar.max[2],    tVar.max[3],    tVar.max[4])
                                tShader:setPS(tVar.name,     tVar.value[1],  tVar.value[2],  tVar.value[3],  tVar.value[4])
                            end
                        end
                    elseif tVar.type == 'vec3' then
                        local tXYZ = {'X', 'Y', 'Z'}
                        for k= 1, #tXYZ do
                            if inputFloatMinMax(psName,tVar,k,tXYZ[k],tVar.name ..'-'..tXYZ[k]) then
                                tShader:setPSmin(tVar.name,  tVar.min[1],    tVar.min[2],    tVar.min[3],    tVar.min[4])
                                tShader:setPSmax(tVar.name,  tVar.max[1],    tVar.max[2],    tVar.max[3],    tVar.max[4])
                                tShader:setPS(tVar.name,     tVar.value[1],  tVar.value[2],  tVar.value[3],  tVar.value[4])
                            end
                        end
                    elseif tVar.type == 'rgb' then
                        if colorRGBMinMax(psName,tVar) then
                            tShader:setPSmin(tVar.name,  tVar.min[1],    tVar.min[2],    tVar.min[3],    tVar.min[4])
                            tShader:setPSmax(tVar.name,  tVar.max[1],    tVar.max[2],    tVar.max[3],    tVar.max[4])
                            tShader:setPS(tVar.name,     tVar.value[1],  tVar.value[2],  tVar.value[3],  tVar.value[4])
                        end
                    elseif tVar.type == 'rgba' then
                        if colorRGBAMinMax(psName,tVar) then
                            tShader:setPSmin(tVar.name,  tVar.min[1],    tVar.min[2],    tVar.min[3],    tVar.min[4])
                            tShader:setPSmax(tVar.name,  tVar.max[1],    tVar.max[2],    tVar.max[3],    tVar.max[4])
                            tShader:setPS(tVar.name,     tVar.value[1],  tVar.value[2],  tVar.value[3],  tVar.value[4])
                        end
                    end
                end
                tImGui.TreePop()
            end

            if tImGui.TreeNode("Vertex Shader") then
                local tShaderList = mbm.getShaderList(false,'vs')
                table.insert(tShaderList,'\0')
                table.sort(tShaderList)
                local iIndexShader = 1
                if vsName then
                    for i=1, #tShaderList do
                        if tShaderList[i] == vsName then
                            iIndexShader = i
                            break
                        end
                    end
                end
                tShaderList[1]      = 'No Shader'
                
                local ret, current_item, item = tImGui.Combo('##ComboVertexShader' , iIndexShader, tShaderList)
                if ret then
                    local sType,iTypePs = tShader:getPStype()
                    local fTimePs       = tShader:getPStime()
                    local tTextureS2    = tShader:getTextureStage2()
                    if tShaderList[current_item] == 'No Shader' then
                        vsName = nil
                    else
                        vsName = tShaderList[current_item]
                    end
                    -- Fill the previous values from pixel shader since we change the vertex shader
                    if tShader:load(psName,vsName,iTypePs,fTimePs,1.0,mbm.GROWING,1) then
                        for j=1, #tVarPs do
                            local tVar = tVarPs[j]
                            tShader:setPSmin(tVar.name,  tVar.min[1],    tVar.min[2],    tVar.min[3],    tVar.min[4])
                            tShader:setPSmax(tVar.name,  tVar.max[1],    tVar.max[2],    tVar.max[3],    tVar.max[4])
                            tShader:setPS(tVar.name,     tVar.value[1],  tVar.value[2],  tVar.value[3],  tVar.value[4])
                        end
                        if tTextureS2 then
                            tMesh:setTexture(tTextureS2,true,2)
                        end
                    else
                        tUtil.showMessageWarn('Failed To Load Shader:\n' .. tostring(tShaderList[current_item]))
                    end
                end
                if vsName then
                    tImGui.Text('Type')
                    local sType,iTypeVs       = tShader:getVStype()
                    local ret, current_item, item = tImGui.Combo('##ComboAnimVS' , iTypeVs + 1, tAnimationOptions.tAnimTypes)
                    if ret then
                        iTypeVs = current_item - 1
                        tShader:setVStype(iTypeVs)
                        tMesh:restartAnim()
                    end

                    tImGui.Text('Time')
                    tImGui.SameLine()
                    tImGui.TextDisabled(string.format(' default(%.3g)',tShader:getVStime(true)))
                    local iTimeVs = tShader:getVStime()
                    local label   = '##TimeVs'
                    local format  = "%.3f"
                    local step       =  0.1
                    local step_fast  =  1
                    local result, fValue = tImGui.InputFloat(label, iTimeVs, step, step_fast, format, 0)
                    if result then
                        if fValue >= 0 and fValue <= 99999999 then
                            if not tShader:setVStime(fValue) then
                                tUtil.showMessageWarn('Error on apply time to vertex shader')
                            end
                        end
                    end
                end
                for j=1, #tVarVs do
                    local tVar = tVarVs[j]
                    if tVar.type == 'number' then
                        if inputFloatMinMax(vsName,tVar,1,'Value') then
                            tShader:setVSmin(tVar.name,  tVar.min[1],    tVar.min[2],    tVar.min[3],    tVar.min[4])
                            tShader:setVSmax(tVar.name,  tVar.max[1],    tVar.max[2],    tVar.max[3],    tVar.max[4])
                            tShader:setVS(tVar.name,     tVar.value[1],  tVar.value[2],  tVar.value[3],  tVar.value[4])
                        end
                    elseif tVar.type == 'vec2' then
                        local tXY = {'X', 'Y'}
                        for k= 1, #tXY do
                            if inputFloatMinMax(vsName,tVar,k,tXY[k], tVar.name .. '-' ..tXY[k]) then
                                tShader:setVSmin(tVar.name,  tVar.min[1],    tVar.min[2],    tVar.min[3],    tVar.min[4])
                                tShader:setVSmax(tVar.name,  tVar.max[1],    tVar.max[2],    tVar.max[3],    tVar.max[4])
                                tShader:setVS(tVar.name,     tVar.value[1],  tVar.value[2],  tVar.value[3],  tVar.value[4])
                            end
                        end
                    elseif tVar.type == 'vec3' then
                        local tXYZ = {'X', 'Y', 'Z'}
                        for k= 1, #tXYZ do
                            if inputFloatMinMax(vsName,tVar,k,tXYZ[k],tVar.name ..'-'..tXYZ[k]) then
                                tShader:setVSmin(tVar.name,  tVar.min[1],    tVar.min[2],    tVar.min[3],    tVar.min[4])
                                tShader:setVSmax(tVar.name,  tVar.max[1],    tVar.max[2],    tVar.max[3],    tVar.max[4])
                                tShader:setVS(tVar.name,     tVar.value[1],  tVar.value[2],  tVar.value[3],  tVar.value[4])
                            end
                        end
                    elseif tVar.type == 'rgb' then
                        if colorRGBMinMax(vsName,tVar) then
                            tShader:setVSmin(tVar.name,  tVar.min[1],    tVar.min[2],    tVar.min[3],    tVar.min[4])
                            tShader:setVSmax(tVar.name,  tVar.max[1],    tVar.max[2],    tVar.max[3],    tVar.max[4])
                            tShader:setVS(tVar.name,     tVar.value[1],  tVar.value[2],  tVar.value[3],  tVar.value[4])
                        end
                    elseif tVar.type == 'rgba' then
                        if colorRGBAMinMax(vsName,tVar) then
                            tShader:setVSmin(tVar.name,  tVar.min[1],    tVar.min[2],    tVar.min[3],    tVar.min[4])
                            tShader:setVSmax(tVar.name,  tVar.max[1],    tVar.max[2],    tVar.max[3],    tVar.max[4])
                            tShader:setVS(tVar.name,     tVar.value[1],  tVar.value[2],  tVar.value[3],  tVar.value[4])
                        end
                    end
                end
                tImGui.TreePop()
            end

            if tImGui.TreeNode("Texture Stage 2") then
                local tTextureS2    = tShader:getTextureStage2()
                if tTextureS2 then
                    tImGui.TextDisabled(tUtil.getShortName(tTextureS2))
                else
                    tImGui.TextDisabled('No Texture')
                end
                local tSizeBtnTex = {x = tSizeBtn.x - 50, y = tSizeBtn.y}
                if tImGui.Button('Set Texture Stage 2', tSizeBtnTex) then
                    local tSelectedTextures = getSelectedTexturesFromImageSelector(tTexturesToEditor)
                    if #tSelectedTextures == 0 then
                        bTextureViewOpened = true
                        tUtil.showMessageWarn('Please Select a Texture!')
                    elseif #tSelectedTextures > 1 then
                        bTextureViewOpened = true
                        tUtil.showMessageWarn('There Is More Then One Texture Selected!\nPlease Select Just One!')
                    else
                        tMesh:setTexture(tSelectedTextures[1].file_name,true,2)
                    end
                end
                tImGui.TreePop()
            end
            tImGui.PopItemWidth()
        end
        if closed_clicked then
            bShowShaderMenu   = false
        end
        tImGui.End()
    else
        bShowShaderMenu = false
    end
end

function mainMenuShader()
    if tImGui.BeginMainMenuBar() then
        if tImGui.BeginMenu("File") then
            
            local pressed,checked = tImGui.MenuItem("Load Mesh", "Ctrl+O", false)
            if pressed then
                onOpenMesh()
            end
            local pressed,checked = tImGui.MenuItem("Save Mesh", "Ctrl+B", false)
            if pressed then
                onSaveMeshBinary()
            end

            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("Add Shader", nil, false)
            if pressed then
                local file_name = mbm.openFile(nil,"lua")
                if file_name then
                    if mbm.include(file_name) then
                        tUtil.showMessage('File Added Successfully!')
                    else
                        tUtil.showMessageWarn('Failed to Add File:' .. file_name)
                    end
                end
            end

            local pressed,checked = tImGui.MenuItem("Example of a Shader File", nil, false)
            if pressed then
                bShowExampleShader = true
            end

            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("Quit", "Alt+F4", false)
            if pressed then
                mbm.quit()
            end
            tImGui.EndMenu();
        end

        if tImGui.BeginMenu("Image") then
            local pressed,checked = tImGui.MenuItem("Add image(s)", "Ctrl+I", false)
            if pressed then
                onOpenImage()
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu("Options") then
            local pressed,checked = tImGui.MenuItem("Enable Alpha Pattern Background", true, tex_alpha_patern.visible)
            if pressed then
                if checked and tMesh and tMesh.is3d then
                    tUtil.showMessageWarn('Not applicable to 3D!')
                else
                    tex_alpha_patern.visible = checked
                end
            end
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

            local pressed,checked = tImGui.MenuItem("Show Shader Options", false)
            if pressed then
                if tShader then
                    bShowShaderMenu = true
                else
                    tUtil.showMessageWarn('There is No Mesh Loaded!\n\nLoad A Mesh First!')
                end
            end
            
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu("Zoom") then

            local label   = '##Scale'
            local v_min   = 0.2
            local v_max   = 10
            local format  = "Scale %.1f"
            local power   = 1.0
            local result, fValue = tImGui.SliderFloat(label, fScaleMesh, v_min, v_max, format,power)
            if result then
                fScaleMesh = fValue
            end
            tImGui.TextDisabled("Or use Scroll")
            tImGui.SameLine()
            if tImGui.SmallButton("Default") then
                fScaleMesh = 1
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu("About") then
            local pressed,checked = tImGui.MenuItem("Shader Editor", nil, false)
            if pressed then
                if mbm.is('windows') then
                    os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/editors.html#shader-editor"')
                elseif mbm.is('linux') then
                    os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/editors.html#shader-editor"')
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
    local not_anyWindowHovered =  not tImGui.IsAnyWindowHovered()
    isClickedMouseleft  = not_anyWindowHovered and  key == 1
    isClickedMouseRight = not_anyWindowHovered and  key == 2
    camera2d.mx = x
    camera2d.my = y

    if isClickedMouseRight and tMesh then
		tMesh.mx = x
		tMesh.my = y
	end
	camera2d.mx = x
	camera2d.my = y
    
end

function onTouchMove(key,x,y)
    local anyWindowHovered =  tImGui.IsAnyWindowHovered()
    if isClickedMouseleft and not bMovingAnyPoint and not anyWindowHovered then
        local px = (camera2d.mx - x) * camera2d.sx
        local py = (camera2d.my - y) * camera2d.sy
        camera2d.mx = x
        camera2d.my = y
        if vPosCam.z then
			vPosCam:set(vPosCam.x + (px * 0.1),vPosCam.y - (py * 0.1))
		else
			vPosCam:set(vPosCam.x + px,vPosCam.y - py)
		end
    elseif not anyWindowHovered and isClickedMouseRight and tMesh then
        if tMesh.is3d then
            tMesh.ax = tMesh.ax - math.rad(y - tMesh.my)
            tMesh.ay = tMesh.ay - math.rad(x - tMesh.mx)
        else
            tMesh.az = tMesh.az - math.rad((x - tMesh.mx) + (y - tMesh.my))
        end
        tMesh.mx = x
        tMesh.my = y
    end
end

function onTouchUp(key,x,y)
    isClickedMouseleft = false
    isClickedMouseRight = false
    bMovingAnyPoint = false
	camera2d.mx = x
    camera2d.my = y
end

function onTouchZoom(zoom)
    local IsAnyWindowHovered = tImGui.IsAnyWindowHovered()
    if not IsAnyWindowHovered then
        fScaleMesh = fScaleMesh + zoom * 0.2
        if fScaleMesh < 0.2 then
            fScaleMesh = 0.2
        elseif fScaleMesh > 10 then
            fScaleMesh = 10
        end
    end
end

function onKeyDown(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = true
    elseif keyControlPressed then
        if key == mbm.getKeyCode('O') then -- Ctrl+O
            onOpenMesh()
        elseif key == mbm.getKeyCode('I') then -- Ctrl+I
            onOpenImage()
        elseif key == mbm.getKeyCode('B') then -- Ctrl+B
            onSaveMeshBinary()
        end
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = false
    end
end

function showExampleShader()
    tImGui.SetNextWindowSize({x = 600, y = 560 },tImGui.Flags('ImGuiCond_Appearing'))
    local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_example_shader, true, 0 )
    if is_opened then
        tImGui.TextDisabled('Shader:pie.ps')

        local sShaderExample = [[
local function addShader(tShader)
    if not mbm.existShader(tShader.name) and not mbm.addShader(tShader) then
        print("Error on add shader:",tShader.name)
    end
end

local tPie = 
{   name = 'pie.ps',
    code = ]] .. '[[' .. [[
    
    precision mediump float;
    uniform sampler2D sample0;
    varying vec2 vTexCoord;
    uniform float percent;
    uniform float angle;
    uniform float clockwise;
    
    void main( )
    {
        float x = vTexCoord.x - 0.5;
        float y = vTexCoord.y - 0.5;
        x = x * cos(angle) - y * sin(angle);
        y = x * sin(angle) + y * cos(angle);
        
        float a = atan(y,x);
        
        if ((clockwise >= 0.5 && a > percent) || (clockwise < 0.5 && a < percent))
        {
            gl_FragColor = texture2D(sample0, vTexCoord);
        }
        else
        {
            discard;
        }
    }

    ]] .. ']]' .. [[,
    var = {percent = {0}      , angle = {0}      , clockwise = {1.0}},
    min = {percent = {-3.1415}, angle = {-3.1415}, clockwise = {0.0}},
    max = {percent = {3.1415} , angle = {3.1415} , clockwise = {1.0}}
}

addShader(tPie)
]]
        local label      = '##ShaderExample'
        local size       = {x=580,y=500}
        local flags      = 0

        local modified , sNewText = tImGui.InputTextMultiline(label,sShaderExample,size,flags)
    end
    tImGui.End()
    if closed_clicked then
        bShowExampleShader = false
    end
end

function showMeshStatus(delta)
    local flags = {'ImGuiWindowFlags_NoDecoration', 'ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoSavedSettings', 'ImGuiWindowFlags_NoFocusOnAppearing', 'ImGuiWindowFlags_NoNav'}
    tImGui.SetNextWindowBgAlpha(0.75);
    local iW, iH     = mbm.getRealSizeScreen()
    local window_pos = {x = iW - 150, y = 25}
    local window_pos_pivot = {x = 0, y = 0}
    tImGui.SetNextWindowPos(window_pos, tImGui.Flags('ImGuiCond_Once'), window_pos_pivot);
    local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_status, false,tImGui.Flags(flags) )
    if is_opened then
        local sAnimName, indexAnim = tMesh:getAnim()
        local iFrame               = tMesh:getIndexFrame()
        iLastFrameText             = iLastFrameText + delta
        iLastAnimationInfoText     = iLastAnimationInfoText + delta
        local sTextAnim            = "Animation:" .. tostring(indexAnim) .. "/" .. tostring(tMesh:getTotalAnim()) .. '\n' .. sAnimName
        local psName,vsName        = tShader:getNames()
        local sTextFrame           = string.format('Current Frame:%d\nPixel Shader:\n%s\nVertex Shader:\n%s',iFrame,psName or 'none',vsName or 'none')
        if sTextFrame ~= sLastFrameText then
            sLastFrameText     = sTextFrame
            iLastFrameText = 0
        end

        if sTextAnim ~= sLastAnimationInfoText then
            sLastAnimationInfoText = sTextAnim
            iLastAnimationInfoText = 0
        end

        if iLastAnimationInfoText < 1 then
            tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=1,b=0,a=0.8})
            tImGui.Text(sLastAnimationInfoText)
            tImGui.PopStyleColor(1)
        else
            tImGui.TextDisabled(sLastAnimationInfoText)
        end

        if iLastFrameText < 1 then
            tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=1,b=0,a=0.8})
            tImGui.Text(sTextFrame)
            tImGui.PopStyleColor(1)
        else
            tImGui.TextDisabled(sTextFrame)
        end
    end
    tImGui.End()
end

function loop(delta)
    mainMenuShader()
    if bTextureViewOpened then
        local closed_clicked = tUtil.showTextureAssets(tWindowsTitle.title_image_selector,tTexturesToEditor,0,0,ImGuiWindowFlags_NoMove == 0)
        if closed_clicked then
            bTextureViewOpened = false
        end
    end
    tex_alpha_patern:setPos(camera2d.x,camera2d.y)

    if tMesh then
        vFocusCam:set(vPosCam.x,vPosCam.y,0)
        if tGlobalFont then
            tMesh.text = tMesh.sDefaultText .. os.date(" %H:%M:%S")
            local x,y = tMesh:getSize()
            tMesh.x = x * -0.5
            tMesh.y = y * 0.5
		end
        tMesh:setScale(fScaleMesh,fScaleMesh,fScaleMesh)
        if bShowShaderMenu then
            showShaderOptions()
        end
        showMeshStatus(delta)
    end

    if bShowExampleShader then
        showExampleShader()
    end

    if bShowAnimation then
        showAnimationAdd()
    end
    tUtil.showOverlayMessage()
end

