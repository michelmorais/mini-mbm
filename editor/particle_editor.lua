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

   Particle Editor

   This is a script based on mbm engine.

   Particle Editor is a editor to create particle behavior.

   More info at: https://mbm-documentation.readthedocs.io/en/latest/editors.html#particle-editor

]]--

tImGui        =     require "ImGui"
tUtil         =     require "editor_utils"

if not mbm.get('USE_EDITOR_FEATURES') then
	mbm.messageBox('Missing features','Is necessary to compile using USE_EDITOR FEATURES to run this editor','ok','error',0)
	mbm.quit()
end

function onInitScene()
    camera2d	              = mbm.getCamera("2d")
    fileNameCurrent           = ""
    fScaleParticle            = 1
    bShowParticleMenu         = false
    iNumberOfParticleByStageSet = 50
    iStageParticle            = 1
    iRangeSpeedParticle       = 10000
    iRangeLifeTimeParticle    = 10
    iRangeAriseTimeParticle   = 10
    iRangeSizeParticle        = 100
    iRangeOffsetParticle      = 20
    iRangeStageTimeParticle   = 10
    sLastStageText            = ''
    sLastParticleSizeText     = ''
    iLastStageText            = 0
    iLastParticleSizeText     = 0
    sLastEditorFileName       = ''
    tShader                   = nil
    vNormal                   = vec2:new()
    vAzimuth                  = vec2:new()
    tShaderByOperator         = { index = 4,
		[1] = {name = 'particle_plus.ps', op = '+'},
		[2] = {name = 'particle_minus.ps', op = '-'},
		[3] = {name = 'particle_divide.ps', op = '/'},
        [4] = {name = 'particle_multiple.ps', op = '*'},
        sAdditionalCode = ''
        }
    addShaderParticle()

    tWindowsTitle = {
            title_particle_options = 'Particle Options',
            title_particle_status  = 'Particle Status'
    }
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
    tUtil.sMessageOverlay= 'Welcome to Particle Editor!!!'
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
     tLineCenterX.z = 98
     tLineCenterY.z = 97
     tLineCenterX.visible = false
     tLineCenterY.visible = false
end

function onNewParticle()
    if tParticle then
        tParticle:destroy()
    end
    tParticle = particle:new('2dw',0,0,0)
    if tShaderByOperator.sAdditionalCode:len() > 0 then
        tParticle:load('#FFFFFFFF',iNumberOfParticleByStageSet,tShaderByOperator[tShaderByOperator.index].op,tShaderByOperator.sAdditionalCode)
    else
        tParticle:load('#FFFFFFFF',iNumberOfParticleByStageSet,tShaderByOperator[tShaderByOperator.index].op)
    end
    bShowParticleMenu = true
    tParticle:setMinSpeed(1,500,500)
    tShader = tParticle:getShader()
end

function onOpenParticle(sFileName)
    local fileName = sFileName or mbm.openFile(sLastEditorFileName,"particle",'ptl')
	if fileName then
		local ext = tUtil.getExtension(fileName)
		if ext ~= 'particle' and ext ~= "ptl" then
			if ext == 'png' or ext == 'bmp' or ext == 'jpg' or ext == 'jpge' or ext == 'tif' then
				tUtil.showMessage("Texture found \n["..tUtil.getShortName(fileName,true) .. "]\nadded path...")
			else
				tUtil.showMessage("File found \n["..tUtil.getShortName(fileName,true) .. "]\nadded path...")
			end
			mbm.addPath(fileName)
			return
        elseif ext == "particle" then
            if tParticle then
                tParticle:destroy()
            end
            tParticle = nil
            dofile(fileName)
            if tParticle then
                bShowParticleMenu = true
                tUtil.showMessage("Particle Successfully Loaded")
                sLastEditorFileName = fileName
                mbm.setGlobal('sLastEditorFileName',sLastEditorFileName)
            else
                tUtil.showMessageWarn("Failed to Load Particle!")
            end
        elseif ext == "ptl" then
            if tParticle then
                tParticle:destroy()
            end
            tParticle = nil
			tParticle = particle:new('2dw',0,0,0)
            if tParticle:load(fileName) then
                bShowParticleMenu = true
                tShader = tParticle:getShader()
                sLastEditorFileName = fileName
                tUtil.showMessage("Particle Successfully Loaded")
                mbm.setGlobal('sLastEditorFileName',sLastEditorFileName)
            else
                tParticle = nil
                tUtil.showMessageWarn("Failed to Load Particle!")
            end
		end
	end
end

function onSaveEditionParticle(sFileName)

    local fp = io.open(sFileName,"w")
    if fp then
        local tLinesEditor = {}
        
        local sBlendState,iBlendIndex = tParticle:getBlend()
        local sOperation              = tShader:getBlendOp()
        if tex_alpha_pattern.visible then
            fp:write(string.format('tex_alpha_pattern.visible = true\n' ))
        else
            fp:write(string.format('tex_alpha_pattern.visible = false\n' ))
        end

        if tColorBackgroundGlobal then
            fp:write(string.format('tColorBackgroundGlobal = {r=%f,g=%f,b=%f}\n' ,tColorBackgroundGlobal.r,tColorBackgroundGlobal.g,tColorBackgroundGlobal.b))
            fp:write(string.format('mbm.setColor(tColorBackgroundGlobal.r,tColorBackgroundGlobal.g,tColorBackgroundGlobal.b)\n'))
        end
        
        fp:write(string.format('fScaleParticle                    = %f\n' ,fScaleParticle            ))
        fp:write(string.format('iNumberOfParticleByStageSet       = %d\n' ,iNumberOfParticleByStageSet ))
        fp:write(string.format('iRangeSpeedParticle               = %f\n' ,iRangeSpeedParticle       ))
        fp:write(string.format('iRangeLifeTimeParticle            = %f\n' ,iRangeLifeTimeParticle    ))
        fp:write(string.format('iRangeAriseTimeParticle           = %f\n' ,iRangeAriseTimeParticle   ))
        fp:write(string.format('iRangeSizeParticle                = %f\n' ,iRangeSizeParticle        ))
        fp:write(string.format('iRangeOffsetParticle              = %f\n' ,iRangeOffsetParticle      ))
        fp:write(string.format('iRangeStageTimeParticle           = %f\n' ,iRangeStageTimeParticle   ))
        fp:write(string.format('sLastStageText                    = %q\n', sLastStageText))
        fp:write(string.format('sLastParticleSizeText             = %q\n', sLastParticleSizeText))
        fp:write(string.format('sLastEditorFileName               = %q\n', sLastEditorFileName))
        fp:write(string.format('tShaderByOperator.index           = %d\n' ,tShaderByOperator.index ))
        fp:write(string.format('tShaderByOperator.sAdditionalCode = [[%s]]\n' ,tShaderByOperator.sAdditionalCode ))
        fp:write('tParticle = particle:new(\'2dw\',0,0,0)\n')
        fp:write(string.format('if tParticle:load(%q,iNumberOfParticleByStageSet,%q,tShaderByOperator.sAdditionalCode) then\n',mbm.getFullPath(tParticle:getTexture()),tShaderByOperator[tShaderByOperator.index].op))
        fp:write('    tShader = tParticle:getShader()\n')
        
        fp:write(string.format('    tParticle:setBlend(%d)   \n' ,iBlendIndex ))
        fp:write(string.format('    tShader:setBlendOp(%q) \n' ,sOperation ))

        for i=1, tParticle:getTotalStage() do

            if i > 1 then
                fp:write(string.format('    tParticle:addStage()\n' ))
            end
            fp:write(string.format('    tParticle:setStage(%d)\n',i ))
            tParticle:setStage(i)

            local x, y, z = tParticle:getMinOffset(i)
            fp:write(string.format('    tParticle:setMinOffset(%d,%f,%f,%f)\n' ,i,x,y,z))
            x, y, z = tParticle:getMaxOffset(i)
            fp:write(string.format('    tParticle:setMaxOffset(%d,%f,%f,%f)\n' ,i,x,y,z))

            x, y, z = tParticle:getMinDirection(i)
            fp:write(string.format('    tParticle:setMinDirection(%d,%f,%f,%f)\n' ,i,x,y,z))
            x, y, z = tParticle:getMaxDirection(i)
            fp:write(string.format('    tParticle:setMaxDirection(%d,%f,%f,%f)\n' ,i,x,y,z))

            local speed = tParticle:getMinSpeed(i)
            fp:write(string.format('    tParticle:setMinSpeed(%d,%f)\n' ,i,speed))
            speed = tParticle:getMaxSpeed(i)
            fp:write(string.format('    tParticle:setMaxSpeed(%d,%f)\n' ,i,speed))

            local r, g, b = tParticle:getMinColor(i)
            fp:write(string.format('    tParticle:setMinColor(%d,%f,%f,%f)\n' ,i,r, g, b))
            r, g, b = tParticle:getMaxColor(i)
            fp:write(string.format('    tParticle:setMaxColor(%d,%f,%f,%f)\n' ,i,r, g, b))

            local lifeTime = tParticle:getMinLifeTime(i)
            fp:write(string.format('    tParticle:setMinLifeTime(%d,%f)\n' ,i,lifeTime))
            lifeTime = tParticle:getMaxLifeTime(i)
            fp:write(string.format('    tParticle:setMaxLifeTime(%d,%f)\n' ,i,lifeTime))

            local size = tParticle:getMinSize(i)
            fp:write(string.format('    tParticle:setMinSize(%d,%f)\n' ,i,size))
            size = tParticle:getMaxSize(i)
            fp:write(string.format('    tParticle:setMaxSize(%d,%f)\n' ,i,size))

            local arise = tParticle:getAriseTime(i)
            fp:write(string.format('    tParticle:setAriseTime(%d,%f)\n' ,i,arise))
            
            r, g, b = tParticle:getInvertedColor(i)
            fp:write(string.format('    tParticle:setInvertedColor(%d,%s,%s,%s)\n' ,i,tostring(r), tostring(g), tostring(b)))

            if tParticle.alpha then
                fp:write(string.format('    tParticle.alpha     = true\n' ,i))
            else
                fp:write(string.format('    tParticle.alpha     = false\n' ,i))
            end

            if tParticle.revive then
                fp:write(string.format('    tParticle.revive    = true\n' ,i))
            else
                fp:write(string.format('    tParticle.revive    = false\n' ,i))
            end

            if tParticle.grow then
                fp:write(string.format('    tParticle.grow      = true\n' ,i))
            else
                fp:write(string.format('    tParticle.grow      = false\n' ,i))
            end
            if tParticle.segmented then
                fp:write(string.format('    tParticle.segmented = true\n' ,i))
            else
                fp:write(string.format('    tParticle.segmented = false\n' ,i))
            end
        end

        fp:write(string.format('    bShowParticleMenu   = true\n'))

        fp:write(string.format('else \n'))
        fp:write(string.format('    tParticle = nil\n'))
        fp:write(string.format('    print(\'error\',\'Failed to load texture %q\') \n',tParticle:getTexture()))
        fp:write(string.format('end \n'))
        
        fp:close()
        return true
    else
        print('error',string.format('Could not open the file [%s] for write',sFileName))
        tUtil.showMessageWarn(string.format('Could not open the file [%s] for write',sFileName))
        return false
    end
end

function onSaveParticleEditor()
    if sLastEditorFileName:len() == 0 then
        local file_name = mbm.saveFile(sLastEditorFileName,'*.particle')
        if file_name then
            if onSaveEditionParticle(file_name) then
                sLastEditorFileName = file_name
                tUtil.showMessage('Particle Editor Saved Successfully!!')
            else
                tUtil.showMessageWarn('Failed to Save Editor of Particle!')
            end
        end
    else
        if onSaveEditionParticle(sLastEditorFileName) then
            mbm.setGlobal('sLastEditorFileName',sLastEditorFileName)
            tUtil.showMessage(string.format('Particle Editor\n %s\n Saved Successfully!!',tUtil.getShortName(sLastEditorFileName)))
        else
            tUtil.showMessageWarn('Failed to Save Editor of Particle!')
        end
    end
end

function onSaveParticleBinary()
	if tParticle then
		local sLastEditorFileName = mbm.getGlobal("fileNameSaved") or mbm.getGlobal("sLastEditorFileName") 
		local ext = "ptl"
		if sLastEditorFileName then
			sLastEditorFileName = tUtil.getShortName(sLastEditorFileName):split('%.')[1] .. ".ptl"
		else
			local last_texture_opened = tParticle:getTexture()
			if last_texture_opened then
				sLastEditorFileName = tUtil.getShortName(last_texture_opened)
				sLastEditorFileName = sLastEditorFileName:split('%.')[1] .. ".ptl"
			end
		end
		local fileName = mbm.saveFile(sLastEditorFileName,ext)
		if fileName then
			local myMesh = meshDebug:new()
			local nFrame = myMesh:addFrame()
			local indexFrame = nFrame
			local indexSubset = 1
			local totalVertex = 3
			local nSubset = myMesh:addSubSet(indexFrame)
			if not myMesh:addVertex(indexFrame,indexSubset,totalVertex) then 
                tUtil.showMessageWarn("Error on add vertex")
                return false
			end
			if not myMesh:setTexture(indexFrame,indexSubset,tParticle:getTexture()) then
				return fail("error on set texture!")
			end
			myMesh:setStride(2,indexFrame)--we don't use z here
			myMesh:enableNormal(false)  -- disable normal write in file (calculate run time)
			myMesh:enableUv(false,false)-- disable UV coordinate
			myMesh:setType('particle')  -- set it to particle
			if myMesh:copyAnimationsFromMesh(tParticle) then --copy animations created include stages
				local calcNormal,calcUv = false,false --don't wanna normal (neither recalculate it) and recalculate UV at all
				if myMesh:save(fileName,calcNormal,calcUv) then
					tUtil.showMessage("Particle Saved Successfully!")
					return true
				else
					tUtil.showMessageWarn("Failed To Save Particle!")
					return true
				end
			else
				tUtil.showMessageWarn("Failed To Copy Animations - stages, Shaders...")
				return true
			end
		end
		return false
	end
end	

function onOpenImage()
    if tParticle then
        local file_name = mbm.openFile(sLastTextureOpenned,table.unpack(tUtil.supported_images))
        if file_name then
            sLastTextureOpenned = file_name
            tParticle:setTexture(file_name)
        end
    else
        tUtil.showMessageWarn('There is no particle to apply texture!\n\nAdd a particle first!')
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

function showParticleOptions()
    if tParticle then
        local width = 220
        local x_pos, y_pos = 0, 0
        local max_width = 220
        local tSizeBtn   = {x=width - 20,y=0} -- size button
        local tSizeBtnAddSet   = {x=43,y=0} 
        tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_particle_options,x_pos,y_pos,width,max_width)
        local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_particle_options, true, ImGuiWindowFlags_NoMove)
        if is_opened then

            local indexCurrentStage, iTotalStage = tParticle:getStage()
            tImGui.PushItemWidth(150)
            tImGui.Text('Number Of Particles')
            local label      = '##Number of particles'
            local step       =  1
            local step_fast  =  10
            local flags      =  0

            local result, iValue = tImGui.InputInt(label, iNumberOfParticleByStageSet, step, step_fast, flags)
            if result then
                if iValue >=0 and iValue <= 10000 then
                    iNumberOfParticleByStageSet = iValue
                end
            end

            tImGui.SameLine()
            if tImGui.Button('Set', tSizeBtnAddSet) then
                tParticle:setTotalParticle(iStageParticle,iNumberOfParticleByStageSet)
            end

            tImGui.Text('Stage')
            local label      = '##Number of stage'
            local step_fast  =  1
            
            local result, iValue = tImGui.InputInt(label, iStageParticle, step, step_fast, flags)
            if result then
                if iValue > 0 and iValue <= tParticle:getTotalStage() then
                    iStageParticle = iValue
                end
            end

            tImGui.SameLine()
            if tImGui.Button('Add', tSizeBtnAddSet) then
                iStageParticle = tParticle:addStage()
                tParticle:setTotalParticle(iStageParticle,iNumberOfParticleByStageSet)
            end
            
            if tImGui.Button('Restart Stage(s)', tSizeBtn) then
                tParticle:restartAnim()
                indexCurrentStage = 1
            end

            tParticle:setStage(iStageParticle)
            
            if tImGui.TreeNode("Arise Time") then
                local power         = 1.0
                local arise_time = tParticle:getAriseTime(iStageParticle)
                arise_time       = drawSlider(arise_time,'Time to rise\n(when add particle)',0.1,iRangeAriseTimeParticle,power)
                tParticle:setAriseTime(iStageParticle,arise_time)

                tImGui.Text('Range Arise Time')
                local label      = '##RangeAriseTime'
                local step       =  0.001
                local step_fast  =  1.0
                local format     = "%.3f"
                local flags      =  0
                
                local result, iValue = tImGui.InputFloat(label, iRangeAriseTimeParticle, step, step_fast, format, flags)
                if result then
                    if iValue >=0.0 and iValue <= 9999999999 then
                        iRangeAriseTimeParticle = iValue
                        if iRangeAriseTimeParticle < arise_time then
                            tParticle:setAriseTime(iStageParticle,iRangeAriseTimeParticle)
                        end
                    end
                end

                tImGui.TreePop()
            end

            if tImGui.TreeNode("Blend") then
                local sBlendState,iBlendIndex = tParticle:getBlend()
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
                tImGui.HelpMarker('Blend Function is the same for all stages')
                local ret, current_item, item = tImGui.Combo('##ComboBlendFunction' , iBlendIndex + 1, tBlend)
                if ret then
                    iBlendIndex = current_item - 1
                    tParticle:setBlend(iBlendIndex)
                end

                tImGui.Text('Blend Operation')
                tImGui.SameLine()
                tImGui.HelpMarker('Blend Operation is the same for all stages however it is per shader')
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

            if tImGui.TreeNode("Color") then
                tImGui.Text('Min Color')
                local label      = 'Select the minimum color for the particle##Min Color'
                local flag_color = tImGui.Flags('ImGuiColorEditFlags_HDR','ImGuiColorEditFlags_NoLabel')
                local tColor  = {}
                tColor.r, tColor.g, tColor.b = tParticle:getMinColor(iStageParticle)
                local clicked, tRgb = tImGui.ColorEdit3(label, tColor, flag_color)
                if clicked then
                    tParticle:setMinColor(iStageParticle,tRgb.r,tRgb.g,tRgb.b)
                end

                tImGui.Text('Max Color')
                local label     = 'Select the maximum color for the particle##Max Color'
                tColor.r, tColor.g, tColor.b = tParticle:getMaxColor(iStageParticle)
                local clicked, tRgb = tImGui.ColorEdit3(label, tColor, flag_color)
                if clicked then
                    tParticle:setMaxColor(iStageParticle,tRgb.r,tRgb.g,tRgb.b)
                end
                tImGui.TreePop()
            end

            if tImGui.TreeNode("Direction") then
                local nx,ny,nz = tParticle:getMinDirection(iStageParticle)
                nx,ny, nz = drawStrength('Min Direction',nx,ny,nz)
                tParticle:setMinDirection(iStageParticle,nx,ny,nz)

                nx,ny,nz = tParticle:getMaxDirection(iStageParticle)
                nx,ny, nz = drawStrength('Max Direction',nx,ny,nz)
                tParticle:setMaxDirection(iStageParticle,nx,ny,nz)
                tImGui.TreePop()
            end

            if tImGui.TreeNode("Flags") then
                tParticle.alpha      = tImGui.Checkbox('Alpha (Enable on Shader)',tParticle.alpha)
                tParticle.grow       = tImGui.Checkbox('Min Size to Max Size',tParticle.grow)
                tParticle.segmented  = tImGui.Checkbox('Segmented (UV)',tParticle.segmented)
                tParticle.revive     = tImGui.Checkbox('Revive',tParticle.revive)
                tImGui.TreePop()
            end
            
            if tImGui.TreeNode("Invert Color") then
                local r,g,b,a  = tParticle:getInvertedColor(iStageParticle)
                r              = tImGui.Checkbox('Red color',r)
                g              = tImGui.Checkbox('Green color',g)
                b              = tImGui.Checkbox('Blue color',b)
                a              = tImGui.Checkbox('Alpha color',a)
                tParticle:setInvertedColor(iStageParticle,r,g,b,a)
                tImGui.TreePop()
            end

            if tImGui.TreeNode("Life Time") then
                local power         = 1.0
                local min_life_time = tParticle:getMinLifeTime(iStageParticle)
                min_life_time = drawSlider(min_life_time,'Min Life Time',0.1,iRangeLifeTimeParticle,power)
                tParticle:setMinLifeTime(iStageParticle,min_life_time)
                local max_life_time = tParticle:getMaxLifeTime(iStageParticle)
                max_life_time = drawSlider(max_life_time,'Max Life Time',0.1,iRangeLifeTimeParticle,power)
                tParticle:setMaxLifeTime(iStageParticle,max_life_time)

                tImGui.Text('Range Life Time')
                local label      = '##RangeLifeTime'
                local step       =  0.02
                local step_fast  =  1.0
                local format     = "%.3f"
                local flags      =  0
                
                local result, iValue = tImGui.InputFloat(label, iRangeLifeTimeParticle, step, step_fast, format, flags)
                if result then
                    if iValue >=0.0 and iValue <= 9999999999 then
                        iRangeLifeTimeParticle = iValue
                        if iRangeLifeTimeParticle < max_life_time then
                            tParticle:setMaxLifeTime(iStageParticle,iRangeLifeTimeParticle)
                        end
                    end
                end
                tImGui.TreePop()
            end

            if tImGui.TreeNode("Stage Time") then
                local power                   = 1.0
                local stageTime               = tParticle:getStageTime(iStageParticle)
                stageTime                     = drawSlider(stageTime,'Stage Time\n(each stage is like an animation)',0.1,iRangeStageTimeParticle,power)
                tParticle:setStageTime(iStageParticle,stageTime)

                tImGui.Text('Range Stage Time')
                local label      = '##RangeStageTime'
                local step       =  0.001
                local step_fast  =  1.0
                local format     = "%.3f"
                local flags      =  0
                
                local result, iValue = tImGui.InputFloat(label, iRangeStageTimeParticle, step, step_fast, format, flags)
                if result then
                    if iValue > 0.0 and iValue <= 9999999999 then
                        iRangeStageTimeParticle = iValue
                        if iRangeStageTimeParticle < stageTime then
                            tParticle:setStageTime(iStageParticle,iRangeStageTimeParticle)
                        end
                    end
                end

                tImGui.TreePop()
            end

            if tImGui.TreeNode("Offset") then
                local power             = 1.0
                local v_min             = -iRangeOffsetParticle
                local v_max             = iRangeOffsetParticle
                local x_min,y_min,z_min = tParticle:getMinOffset(iStageParticle)
                x_min = drawSlider(x_min,'X Min Offset',v_min,v_max,power)
                y_min = drawSlider(y_min,'Y Min Offset',v_min,v_max,power)
                z_min = drawSlider(z_min,'Z Min Offset',v_min,v_max,power)
                tParticle:setMinOffset(iStageParticle,x_min,y_min,z_min)

                local x_max,y_max,z_max = tParticle:getMaxOffset(iStageParticle)
                x_max = drawSlider(x_max,'X Max Offset',v_min,v_max,power)
                y_max = drawSlider(y_max,'Y Max Offset',v_min,v_max,power)
                z_max = drawSlider(z_max,'Z Max Offset',v_min,v_max,power)
                tParticle:setMaxOffset(iStageParticle,x_max,y_max,z_max)
                

                tImGui.Text('Range Offset')
                local label      = '##RangeOffset'
                local step       =  1
                local step_fast  =  5
                local format     = "%.2f"
                local flags      =  0
                
                local result, iValue = tImGui.InputFloat(label, iRangeOffsetParticle, step, step_fast, format, flags)
                if result then
                    if iValue > 1 then
                        iRangeOffsetParticle = iValue
                        if iValue < x_max then
                            x_max = iValue
                        end
                        if iValue < y_max then
                            y_max = iValue
                        end
                        if iValue < z_max then
                            z_max = iValue
                        end
                        tParticle:setMaxOffset(iStageParticle,x_max,y_max,z_max)

                        if -iValue > x_min then
                            x_min = -iValue
                        end
                        if -iValue > y_min then
                            y_min = -iValue
                        end
                        if -iValue > z_min then
                            z_min = -iValue
                        end

                        tParticle:setMinOffset(iStageParticle,x_min,y_min,z_min)
                    end
                end
                tImGui.TreePop()
            end

            if tImGui.TreeNode("Speed") then
                local power     = 3.0
                local min_speed = tParticle:getMinSpeed(iStageParticle)
                min_speed = drawSlider(min_speed,'Min Speed',0,iRangeSpeedParticle,power)
                tParticle:setMinSpeed(iStageParticle,min_speed)
                local max_speed = tParticle:getMaxSpeed(iStageParticle)
                max_speed = drawSlider(max_speed,'Max Speed',0,iRangeSpeedParticle,power)
                tParticle:setMaxSpeed(iStageParticle,max_speed)

                tImGui.Text('Range Speed')
                local label      = '##RangeSpeedparticles'
                local step       =  1
                local step_fast  =  10
                local flags      =  0
                
                local result, iValue = tImGui.InputInt(label, iRangeSpeedParticle, step, step_fast, flags)
                if result then
                    if iValue >= 0.0 and iValue <= 9999999999 then
                        iRangeSpeedParticle = iValue
                        if iRangeSpeedParticle < max_speed then
                            tParticle:setMaxSpeed(iStageParticle,iRangeSpeedParticle)
                        end
                    end
                end
                tImGui.TreePop()
            end

            if tImGui.TreeNode("Shader") then
                local indexSelected  = tImGui.RadioButton('Operator \'+\'', tShaderByOperator.index,  1)
                indexSelected        = tImGui.RadioButton('Operator \'-\'', indexSelected, 2)
                indexSelected        = tImGui.RadioButton('Operator \'/\'', indexSelected, 3)
                indexSelected        = tImGui.RadioButton('Operator \'*\'', indexSelected, 4)
                if indexSelected ~= tShaderByOperator.index then
                    tShaderByOperator.index = indexSelected
                    tShader:load(tShaderByOperator[tShaderByOperator.index].name,nil,mbm.GROWING,1.0,mbm.GROWING_LOOP,1.0)
                end

                tImGui.Text('Additional Code Shader')
                tImGui.SameLine()
                tImGui.HelpMarker('It is not saved in the binary file!')
                local modified , sNewText = tImGui.InputTextMultiline('##AdditionalCode',tShaderByOperator.sAdditionalCode,{x=-1,y=0},flags)
                if modified then
                    tShaderByOperator.sAdditionalCode = sNewText
                end
                local sAddCode = '\n // YOUR CODE HERE **********'
                if tShaderByOperator.sAdditionalCode:len() > 0 then
                    sAddCode = '\n // YOUR CODE ********** \n' ..  tShaderByOperator.sAdditionalCode .. '\n//END YOUR CODE ********** \n'
                end
                tImGui.Text('Code')
                local sCodeShader = 
[[precision mediump float;
uniform vec4 color;
uniform float enableAlphaFromColor;
varying vec2 vTexCoord;
uniform sampler2D sample0;
void main()
{
    vec4 texColor;
    vec4 outColor;
    texColor = texture2D( sample0, vTexCoord );
    if(enableAlphaFromColor > 0.5)
        outColor.a = color.a;
    else
        outColor.a = texColor.a;
    outColor.rgb = color.rgb ]] .. tShaderByOperator[tShaderByOperator.index].op .. ' texColor.rgb;\n' ..
    sAddCode .. [[
    gl_FragColor = outColor;
}
]]
                tImGui.TextDisabled(sCodeShader)
                if tImGui.Button('Apply') then
                    local ext = tUtil.getExtension(sLastEditorFileName)
                    if ext == 'particle' then
                        if onSaveEditionParticle(sLastEditorFileName) then
                            onOpenParticle(sLastEditorFileName)
                        end
                    elseif ext ~= "ptl" then
                        tUtil.showMessageWarn(string.format('Not possible apply code to binary type \n%s\n\nYou must apply in your code!',sLastEditorFileName))
                    else
                        tUtil.showMessageWarn(string.format('Invalid file\n%s',sLastEditorFileName))
                    end
                end
                tImGui.SameLine()
                tImGui.HelpMarker('A wrong shader will cause crash to the program!')
                tImGui.TreePop()
            end

            if tImGui.TreeNode("Size") then
                local power         = 1.0
                local min_size = tParticle:getMinSize(iStageParticle)
                min_size = drawSlider(min_size,'Min Size',0.1,iRangeSizeParticle,power)
                tParticle:setMinSize(iStageParticle,min_size)
                local max_size = tParticle:getMaxSize(iStageParticle)
                max_size = drawSlider(max_size,'Max Size',0.1,iRangeSizeParticle,power)
                tParticle:setMaxSize(iStageParticle,max_size)

                tImGui.Text('Range Size')
                local label      = '##RangeSize'
                local step       =  5
                local step_fast  =  10
                local format     = "%.2f"
                local flags      =  0
                
                local result, iValue = tImGui.InputFloat(label, iRangeSizeParticle, step, step_fast, format, flags)
                if result then
                    if iValue >=0.1 and iValue <= 9999999999 then
                        iRangeSizeParticle = iValue
                        if iRangeSizeParticle < max_size then
                            tParticle:setMaxSize(iStageParticle,iRangeSizeParticle)
                        end
                    end
                end
                tImGui.TreePop()
            end

            tParticle:setStage(indexCurrentStage)
            tImGui.PopItemWidth()
        end
        if closed_clicked then
            bShowParticleMenu   = false
        end
        tImGui.End()
    else
        bShowParticleMenu = false
    end
end

function main_menu_particle()
    if tImGui.BeginMainMenuBar() then
        if tImGui.BeginMenu("File") then
            
            local pressed,checked = tImGui.MenuItem("New Particle", "Ctrl+N", false)
            if pressed then
                onNewParticle()
            end

            local pressed,checked = tImGui.MenuItem("Load Particle", "Ctrl+O", false)
            if pressed then
                onOpenParticle()
            end
            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("Save Editor", "Ctrl+S", false)
            if pressed then
                onSaveParticleEditor()
            end

            local pressed,checked = tImGui.MenuItem("Save Particle", "Ctrl+B", false)
            if pressed then
                onSaveParticleBinary()
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
            local pressed,checked = tImGui.MenuItem("Enable Alpha Pattern Background", true, tex_alpha_pattern.visible)
            if pressed then
                tex_alpha_pattern.visible = checked
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

            local pressed,checked = tImGui.MenuItem("Show Particle Options", false)
            if pressed then
                if tParticle then
                    bShowParticleMenu = true
                else
                    tUtil.showMessageWarn('There is no particle to show particle options!\n\nAdd a particle first!')
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
            local result, fValue = tImGui.SliderFloat(label, fScaleParticle, v_min, v_max, format,power)
            if result then
                fScaleParticle = fValue
            end
            tImGui.TextDisabled("Or use Scroll")
            tImGui.SameLine()
            if tImGui.SmallButton("Default") then
                fScaleParticle = 1
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu("About") then
            local pressed,checked = tImGui.MenuItem("Particle Editor", nil, false)
            if pressed then
                if mbm.is('windows') then
                    os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/editors.html#particle-editor"')
                elseif mbm.is('linux') then
                    os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/editors.html#particle-editor"')
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
    isClickedMouseLeft = key == 1
    camera2d.mx = x
    camera2d.my = y
    
end

function onTouchMove(key,x,y)
    if isClickedMouseLeft and not bMovingAnyPoint and not tImGui.IsAnyWindowHovered() then
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

function onTouchZoom(zoom)
    local IsAnyWindowHovered = tImGui.IsAnyWindowHovered()
    if not IsAnyWindowHovered then
        fScaleParticle = fScaleParticle + zoom * 0.2
        if fScaleParticle < 0.2 then
            fScaleParticle = 0.2
        elseif fScaleParticle > 10 then
            fScaleParticle = 10
        end
    end
end

function onKeyDown(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = true
    elseif keyControlPressed then
        if key == mbm.getKeyCode('S') then -- Ctrl+S
            onSaveParticleEditor()
        elseif key == mbm.getKeyCode('O') then -- Ctrl+O
            onOpenParticle()
        elseif key == mbm.getKeyCode('N') then -- Ctrl+N
            onNewParticle()
        elseif key == mbm.getKeyCode('I') then -- Ctrl+I
            onOpenImage()
        elseif key == mbm.getKeyCode('B') then -- Ctrl+B
            onSaveParticleBinary()
        end
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = false
    end
end

function addShaderParticle()
    for i =1, 4 do
        local tShaderParticle = 
        {	
            name = tShaderByOperator[i].name,
            code = [[
            precision mediump float;
            uniform vec4 color;
            uniform float enableAlphaFromColor;
            varying vec2 vTexCoord;
            uniform sampler2D sample0;
            void main()
            {
                vec4 texColor;
                vec4 outColor;
                texColor = texture2D( sample0, vTexCoord );
                if(enableAlphaFromColor > 0.5)
                    outColor.a = color.a;
                else
                    outColor.a = texColor.a;
                outColor.rgb = color.rgb ]] .. tShaderByOperator[i].op .. [[ texColor.rgb;
                gl_FragColor = outColor;
            }
            ]],
            var = {color = {0.5,0.5,0.5,0.5}, enableAlphaFromColor = {1}},
            min = {color = {0.0,0.0,0.0,0.0}, enableAlphaFromColor = {0}},
            max = {color = {1.0,1.0,1.0,1.0}, enableAlphaFromColor = {1}}
        }
        if not mbm.addShader(tShaderParticle) then
            print("Error on add shader:",tShaderParticle.name)
        end
    end
end

function showParticleStatus(delta)
    local flags = {'ImGuiWindowFlags_NoDecoration', 'ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoSavedSettings', 'ImGuiWindowFlags_NoFocusOnAppearing', 'ImGuiWindowFlags_NoNav'}
    tImGui.SetNextWindowBgAlpha(0.75);
    local iW, iH   = mbm.getRealSizeScreen()
    window_pos     = {x = iW - 150, y = 25}
    local window_pos_pivot = {x = 0, y = 0}
    tImGui.SetNextWindowPos(window_pos, tImGui.Flags('ImGuiCond_Once'), window_pos_pivot);
    local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_particle_status, false,tImGui.Flags(flags) )
    if is_opened then
        local indexCurrentStage, iTotalStage = tParticle:getStage()
        iLastStageText            = iLastStageText + delta
        iLastParticleSizeText     = iLastParticleSizeText + delta
        local sTextStage = string.format('Current Stage:%d/%d',indexCurrentStage,iTotalStage)
        local sTextAlive = "Alive:" .. tostring(tParticle:getTotalAlive()) .. "/" .. tostring(tParticle:getTotalParticle(iStageParticle))
        if sTextStage ~= sLastStageText then
            sLastStageText     = sTextStage
            iLastStageText = 0
        end

        if sTextAlive ~= sLastParticleSizeText then
            sLastParticleSizeText = sTextAlive
            iLastParticleSizeText = 0
        end

        if iLastStageText < 1 then
            tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=1,b=0,a=0.8})
            tImGui.Text(sTextStage)
            tImGui.PopStyleColor(1)
        else
            tImGui.TextDisabled(sTextStage)
        end
        if iLastParticleSizeText < 1 then
            tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=1,b=0,a=0.8})
            tImGui.Text(sLastParticleSizeText)
            tImGui.PopStyleColor(1)
        else
            tImGui.TextDisabled(sLastParticleSizeText)
        end
    end
    tImGui.End()
end

function loop(delta)
    main_menu_particle()
    tex_alpha_pattern:setPos(camera2d.x,camera2d.y)

    if tParticle then
        tParticle:setScale(fScaleParticle,fScaleParticle)
        if bShowParticleMenu then
            showParticleOptions()
        end
        showParticleStatus(delta)
    end
    tUtil.showOverlayMessage()
end
