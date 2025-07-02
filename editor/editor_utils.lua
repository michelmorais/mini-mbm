tUtil = {}
tUtil.iCountsetInitialWindowPosition = {}

if tImGui == nil then
    mbm.messageBox('ImGui missing', 'This module requires tImGui = require "ImGui"')
    mbm.quit()
end


tUtil.supported_images = { "*.png","*.jpeg","*.jpg","*.bmp","*.gif","*.psd","*.pic","*.pnm","*.hdr","*.tga","*.tif"}
tUtil.tColorBackground = {r=37/255,g=37/255,b=37/255}
--color theme
mbm.setColor(tUtil.tColorBackground.r,tUtil.tColorBackground.g,tUtil.tColorBackground.b)


tUtil.setInitialWindowPositionRight = function(title,x,y,width,max_width)-- from rigth to left (so x must be <= 0)
    if tUtil.iCountsetInitialWindowPosition[title] == nil then
        tUtil.iCountsetInitialWindowPosition[title] = 0
    end
    if tUtil.iCountsetInitialWindowPosition[title] <= 3 then
        tUtil.iCountsetInitialWindowPosition[title] = tUtil.iCountsetInitialWindowPosition[title] + 1
        local iMenuBarHeight            = tImGui.GetMainMenuBarHeight()
        local iW, iH                    = mbm.getRealSizeScreen()
        local tPosWin                   = {x = iW - width + x,y = iMenuBarHeight + y }
        tImGui.SetNextWindowSizeConstraints({x = width,y = math.min(iH - iMenuBarHeight,width)}, {x = max_width or iW,y = iH - iMenuBarHeight})
        tImGui.SetNextWindowSize({x = width, y = iH - iMenuBarHeight},tImGui.Flags('ImGuiCond_Always'))
        tImGui.SetNextWindowPos(tPosWin , tImGui.Flags( 'ImGuiCond_Always'))
    end
end

tUtil.setInitialWindowPositionLeft = function(title,x,y,width,max_width)-- from left to left (so x must be >= 0)
    if tUtil.iCountsetInitialWindowPosition[title] == nil then
        tUtil.iCountsetInitialWindowPosition[title] = 0
    end
    if tUtil.iCountsetInitialWindowPosition[title] <= 3 then
        tUtil.iCountsetInitialWindowPosition[title] = tUtil.iCountsetInitialWindowPosition[title] + 1
        local iMenuBarHeight            = tImGui.GetMainMenuBarHeight()
        local iW, iH                    = mbm.getRealSizeScreen()
        local tPosWin                   = {x = x,y = iMenuBarHeight + y }
        tImGui.SetNextWindowSizeConstraints({x = width,y = math.min(iH - iMenuBarHeight,width)}, {x = max_width or iW,y = iH - iMenuBarHeight})
        tImGui.SetNextWindowPos(tPosWin , tImGui.Flags( 'ImGuiCond_Always'))
        tImGui.SetNextWindowSize({x = width, y = iH - iMenuBarHeight},tImGui.Flags('ImGuiCond_Always'))
    end
end

tUtil.setInitialWindowPositionDown = function(title,xStart,YPercentage,xRight)
    if tUtil.iCountsetInitialWindowPosition[title] == nil then
        tUtil.iCountsetInitialWindowPosition[title] = 0
    end
    if tUtil.iCountsetInitialWindowPosition[title] <= 3 then
        tUtil.iCountsetInitialWindowPosition[title] = tUtil.iCountsetInitialWindowPosition[title] + 1
        local iMenuBarHeight            = tImGui.GetMainMenuBarHeight()
        local iW, iH                    = mbm.getRealSizeScreen()
        local tPosWin                   = {x=xStart,y=iH - (YPercentage * iH)}
        local tSize                     = {x = iW - tPosWin.x - (xRight or 0), y = iH - tPosWin.y}
        local tMaxSize                  = {x = iW, y = tSize.y}
        tImGui.SetNextWindowSizeConstraints(tSize,tMaxSize)
        tImGui.SetNextWindowSize(tSize,tImGui.Flags('ImGuiCond_Always'))
        tImGui.SetNextWindowPos(tPosWin , tImGui.Flags( 'ImGuiCond_Always'))
    end
end

tUtil.bEraseOnClick_showTextureAssets = false
tUtil.bModalRemoveImages_showTextureAssets = nil
tUtil.sTextRemove_showTextureAssets = ''
tUtil.showTextureAssets = function(title,tTexturesToEditor,x_pos,y_pos,bEnableMoveWindow) -- in title, {{width,height,file_name}}, return tSelectedTexture,  closed_clicked
    local closeable    =  true
    local flags
    if bEnableMoveWindow then
        flags = tImGui.Flags('ImGuiWindowFlags_MenuBar')
    else
        flags = tImGui.Flags('ImGuiWindowFlags_MenuBar', 'ImGuiWindowFlags_NoMove')
    end
    local width        = 200
    tUtil.setInitialWindowPositionRight(title,x_pos,y_pos,width,nil)
    local is_opened, closed_clicked = tImGui.Begin(title, closeable, flags)
    if is_opened then
        
        if tImGui.BeginMenuBar() then

            if tImGui.BeginMenu("Selection") then
                local pressed,checked = tImGui.MenuItem("Select all", nil)
                if pressed then
                    for i=1, #tTexturesToEditor do
                        local tTexture      = tTexturesToEditor[i]
                        tTexture.isSelected = true
                    end
                end
                local pressed,checked = tImGui.MenuItem("Unselect all", nil)
                if pressed then
                    for i=1, #tTexturesToEditor do
                        local tTexture      = tTexturesToEditor[i]
                        tTexture.isSelected = false
                    end
                end
                local pressed,checked = tImGui.MenuItem("Invert selection", nil)
                if pressed then
                    for i=1, #tTexturesToEditor do
                        local tTexture      = tTexturesToEditor[i]
                        tTexture.isSelected = not tTexture.isSelected
                    end
                end
                tImGui.EndMenu();
            end
            if tImGui.BeginMenu("Remove") then
                local pressed,checked = tImGui.MenuItem("Remove on click", nil, tUtil.bEraseOnClick_showTextureAssets)
                if pressed then
                    tUtil.bEraseOnClick_showTextureAssets = checked
                end
                tImGui.Separator()
                local pressed,checked = tImGui.MenuItem("Remove all", nil)
                if pressed and #tTexturesToEditor > 0 then
                    tUtil.bModalRemoveImages_showTextureAssets = 'all'
                    tUtil.sTextRemove_showTextureAssets = 'Are you sure do you want to remove all images?'
                end
                local pressed,checked = tImGui.MenuItem("Remove all selected", nil)
                if pressed then
                    tUtil.bModalRemoveImages_showTextureAssets = 'selected'
                    tUtil.sTextRemove_showTextureAssets = 'Are you sure do you want to remove all selected images?'
                end
                local pressed,checked = tImGui.MenuItem("Remove all unselected", nil)
                if pressed then
                    tUtil.bModalRemoveImages_showTextureAssets = 'unselected'
                    tUtil.sTextRemove_showTextureAssets = 'Are you sure do you want to remove all unselected images?'
                end
                tImGui.EndMenu();
            end
            tImGui.EndMenuBar()
        end
        local tSize         = tImGui.GetWindowSize()
        local padding       = tImGui.GetStyle('DisplayWindowPadding')
        padding.x           = padding.x * 1.2
        local sy_visible    = select(2,tImGui.IsScrollVisible())
        if sy_visible then
            padding.x = padding.x + tImGui.GetStyle('ScrollbarSize')
        end
        for i=1, #tTexturesToEditor do
            local tTexture      = tTexturesToEditor[i]
            local new_width     = math.min(tTexture.width,tSize.x) - padding.x
            local sy            = new_width / tTexture.width  * tTexture.height
            local size          = {x=new_width,y=sy}
            local uv0           = {x=0,y=0}
            local uv1           = {x=1,y=1}
            local frame_padding = 5
            local pushed_color  = 0
            if tUtil.bEraseOnClick_showTextureAssets then
                if tTexture.isSelected then
                    tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_ButtonHovered'), {r=0.5,g=0,b=0,a=1})
                    tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r=0,g=1,b=0,a=1})
                    pushed_color = 2
                else
                    tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_ButtonHovered'), {r=1,g=0,b=0,a=1})
                    pushed_color = 1
                end
            elseif tTexture.isSelected then
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_ButtonHovered'), {r=0,g=0.8,b=0,a=1})
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r=0,g=1,b=0,a=1})
                pushed_color = 2
            end

            if tImGui.ImageButton(tTexture.id, size,uv0,uv1,frame_padding) then
                if tUtil.bEraseOnClick_showTextureAssets then
                    table.remove(tTexturesToEditor,i)
                    if pushed_color > 0 then
                        tImGui.PopStyleColor(pushed_color)
                    end
                    break
                elseif tTexture.isSelected then
                    tTexture.isSelected = false
                else
                    tTexture.isSelected = true
                end
            end
            if pushed_color > 0 then
                tImGui.PopStyleColor(pushed_color)
            end
            local str_desc = string.format('info (?) texture %d/%d',i,#tTexturesToEditor)
            tImGui.HelpMarker(string.format('%s\nwidth:%d\nheight:%d',tTexture.file_name,tTexture.width,tTexture.height),str_desc)
        end
    end
    if tUtil.bModalRemoveImages_showTextureAssets then
        local flags       = tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize')
        local title_popup = 'Remove all images ?'
        tImGui.OpenPopup(title_popup);
        local is_opened, closed_clicked = tImGui.BeginPopupModal(title_popup, false, flags)
        if is_opened then
            tImGui.Text(tUtil.sTextRemove_showTextureAssets)
            tImGui.Separator();
            if tImGui.Button("OK", {x=120, y= 0}) then
                local function remove_from_table(tTexturesToEditor,value)
                    for i=1, #tTexturesToEditor do
                        if value then
                            if tTexturesToEditor[i].isSelected then
                                return i
                            end
                        else
                            if not tTexturesToEditor[i].isSelected then
                                return i
                            end
                        end
                    end
                    return 0
                end
                if tUtil.bModalRemoveImages_showTextureAssets == 'all' then
                    while #tTexturesToEditor > 0 do
                        table.remove(tTexturesToEditor,1)
                    end
                elseif tUtil.bModalRemoveImages_showTextureAssets == 'selected' then
                    local index = remove_from_table(tTexturesToEditor,true)
                    while index > 0 do
                        table.remove(tTexturesToEditor,index)
                        index = remove_from_table(tTexturesToEditor,true)
                    end
                elseif tUtil.bModalRemoveImages_showTextureAssets == 'unselected' then
                    local index = remove_from_table(tTexturesToEditor,false)
                    while index > 0 do
                        table.remove(tTexturesToEditor,index)
                        index = remove_from_table(tTexturesToEditor,false)
                    end
                else
                    print('invalid mode:',tUtil.bModalRemoveImages_showTextureAssets)
                end
                tUtil.bModalRemoveImages_showTextureAssets = nil
                tImGui.CloseCurrentPopup()
            end
            tImGui.SetItemDefaultFocus();
            tImGui.SameLine();
            if tImGui.Button("Cancel", {x=120, y= 0}) then
                tImGui.CloseCurrentPopup()
                tUtil.bModalRemoveImages_showTextureAssets = nil
            end
            tImGui.EndPopup()
        end
    end
    tImGui.End()
    return closed_clicked
end

tUtil.getBaseFileName = function(file_name)
    local sFile     = file_name:gsub('\\','/')
    local tFileName = sFile:split('/')
    if #tFileName > 0 then
        return tFileName[#tFileName]
    end
    return file_name
end

tUtil.loadInfoImagesToTable = function(tFiles,tTexturesIn)
    if type(tFiles) == 'table' then
        local bSuccess = false
        for i=1, #tFiles do
            local width,height,id,alpha = mbm.loadTexture(tFiles[i])
            if id ~= 0 then
                bSuccess = true
                local base_file_name = tUtil.getBaseFileName(tFiles[i])
                table.insert(tTexturesIn,{file_name = tFiles[i],width = width, height = height, alpha = alpha,id = id , base_file_name = base_file_name})
            else
                print('Could not load texture:',tFiles[i])
            end
        end
    elseif type(tFiles) == 'string' then
        local width,height,id,alpha = mbm.loadTexture(tFiles)
        if id ~= 0 then
            local base_file_name = tUtil.getBaseFileName(tFiles[i])
            table.insert(tTexturesIn,{file_name = tFiles,width = width, height = height, alpha = alpha,id = id , base_file_name = base_file_name})
        else
            print('Could not load texture:',tFiles)
        end
    end

    local hash = {}
    local res = {}

    for _,v in ipairs(tTexturesIn) do
        if (not hash[v.file_name]) then
            res[#res+1] = v
            hash[v.file_name] = true
        end
    end
    return res
end

tUtil.hasSupportedImageExtension = function(file_name)
    local tSupportedTypes = {'.png', '.jpeg', '.jpg', '.bmp', '.gif', '.psd', '.pic', '.pnm', '.hdr', '.tga', '.tif'}
    file_name = file_name:lower()
    for i=1, #tSupportedTypes do
        local supportedType = tSupportedTypes[i]
        if file_name:match("%g%" .. supportedType .. '$') then
            return true
        end
    end
    return false
end

tUtil.loadInfoImagesFromFolderToTable  = function(dirname,tTexturesIn)
    local f = nil
    local tFiles = {}
    dirname = string.gsub(dirname,"\\","/")
    if dirname[dirname.len] ~= '/' then
		dirname = dirname .. '/'
    end
    
	if mbm.is("windows") then
		if #dirname == 2 and string.sub( dirname, 2) == ':' then
			f = io.popen(dirname .. " & dir /b " .. dirname)
		else
			f = io.popen("dir /b \""..dirname.."\"")
		end
	else
		f = io.popen("ls -l \""..(dirname).."\"| grep -v ^d | awk '{print $9}'")
	end
    if f then
        local ret = f:read("*a")
        local line = ret:split('\n')
        for i = 1, #line do
            local file_name = line[i]
            if tUtil.hasSupportedImageExtension(file_name) then
                table.insert(tFiles,dirname .. file_name)
            elseif file_name:len() > 0 then
                print(string.format('File [%s] skipped',file_name))
            end
        end
        f.close()
        return tUtil.loadInfoImagesToTable(tFiles,tTexturesIn)
    end
    return tTexturesIn
end

tUtil.createAlphaPattern = function(width,height,block_size,color1,color2)

    local function write_pixel(tPixel, width, height, x,y, channel, r,g,b)
        index = ((y-1) * width * channel) + ((x-1) * channel)
        tPixel[index+1] = r
        tPixel[index+2] = g
        tPixel[index+3] = b
     end
     
     local tPixel = {}
     local widthTexture  = math.floor(width)
     local heightTexture = math.floor(height)
     local channel       = 3 -- no alpha channel
     
     local count_width   = 1
     local count_height  = 1
     local invert_width  = false
     local invert_height = false

     block_size  = math.ceil(block_size)
     if (block_size % widthTexture) ~= 0 then
        block_size = block_size + math.ceil(block_size % widthTexture)
     end
     
     
     for y = 1, heightTexture do
        count_height = count_height + 1
        if count_height > block_size then
            count_height = 1
            invert_height = not invert_height
        end
        for x = 1, widthTexture do
            count_width = count_width + 1
            if count_width > block_size then
                count_width = 1
                invert_width = not invert_width
            end
            local tColor 

            if invert_width then
                if invert_height then
                    tColor = color1
                else
                    tColor = color2
                end
            else
                if invert_height then
                    tColor = color2
                else
                    tColor = color1
                end
            end

            write_pixel(tPixel,widthTexture, heightTexture, x,y, channel, tColor.r,tColor.g,tColor.b)
        end
        count_width = 1
     end
     
     return mbm.createTexture(tPixel,widthTexture,heightTexture,channel)
end

tUtil.save = function(name, value, tOut, onSaveUserData, saved)

    local function basicSerialize(o,precision)
        if type(o) == 'number' then
            if precision then
                return string.format("%a",o) -- same bits as the original number
            else
                return tostring(o)
            end
        elseif type(o) == 'boolean' then
            return tostring(o)
        else
            return string.format("%q",o)
        end
    end

    saved = saved or {}
    if type(value) == 'table' and type(value[1]) == 'userdata' then
        if type(onSaveUserData) == 'function' then
            onSaveUserData(name,value,tOut)
        end
    else
        table.insert(tOut,tostring(name) .. ' = ')
        if type(value) == 'number' or type(value) == 'string' or type(value) == 'boolean' then
            local tLine = tOut[#tOut]
            tLine       = tLine .. basicSerialize(value,true)
            tOut[#tOut] = tLine
            table.insert(tOut,'')
        elseif type(value) == 'table' then
            if saved[value] then
                local tLine = tOut[#tOut]
                tLine       = tLine .. saved[value]
                tOut[#tOut] = tLine
                table.insert(tOut,'')
            else
                saved[value] = name
                local tLine = tOut[#tOut]
                tLine       = tLine .. '{}'
                tOut[#tOut] = tLine
                table.insert(tOut,'')
                for k,v in pairs(value) do
                    k = basicSerialize(k,false)
                    local fname = string.format('%s[%s]',name,k)
                    tUtil.save(fname,v,tOut,onSaveUserData,saved)
                end
            end
        else
            print('error','cannot save a ' .. type(value))
        end
    end

    local function getIndexEmptyLine(tOut)
        for i=1, #tOut do
            if tOut[i] == '' then
                return i
            end
        end
        return 0
    end

    local indexEmptyLine = getIndexEmptyLine(tOut)
    while (indexEmptyLine ~= 0) do
        table.remove(tOut,indexEmptyLine)
        indexEmptyLine = getIndexEmptyLine(tOut)
    end
end

tUtil.sMessageOverlay     = 'Welcome!'
tUtil.title_overlay       = '###Overlay'
tUtil.tTimerOverlay       = timer:new(function (self) self:stop(); tUtil.sMessageOverlay = false end, 13.5)
tUtil.bRightSide          = false
tUtil.tSizeWindowOverlay  = {x=0,y=0}
                    
tUtil.showOverlayMessage = function()
    if tUtil.sMessageOverlay then
        local flags = {'ImGuiWindowFlags_NoMove','ImGuiWindowFlags_NoDecoration', 'ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoSavedSettings', 'ImGuiWindowFlags_NoFocusOnAppearing', 'ImGuiWindowFlags_NoNav'}
        local window_pos = {x = 0, y = tImGui.GetMainMenuBarHeight()}
        if tUtil.bRightSide then
            local iW, iH       = mbm.getRealSizeScreen()
            window_pos.x = iW - tUtil.tSizeWindowOverlay.x
        end
        local window_pos_pivot = {x = 0, y = 0}
        if tUtil.bFocusMsgOnce then
            tUtil.bFocusMsgOnce = false
            tImGui.SetNextWindowFocus(tUtil.title_overlay)
        end
        tImGui.SetNextWindowPos(window_pos, tImGui.Flags('ImGuiCond_Always'), window_pos_pivot);
        tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=1,b=0,a=0.8})
        if tUtil.bWarnMessage then
            tImGui.SetNextWindowBgAlpha(0.5);
            tImGui.PushStyleColor('ImGuiCol_WindowBg',{r=1,g=0,b=0,a=0.5})
        else
            tImGui.SetNextWindowBgAlpha(0.75);
        end
        local is_opened, closed_clicked = tImGui.Begin(tUtil.title_overlay, false,tImGui.Flags(flags) )
        if is_opened then
            tImGui.Text(tUtil.sMessageOverlay)
        end
        tUtil.tSizeWindowOverlay = tImGui.GetWindowSize()
        tImGui.End()
        if tUtil.bWarnMessage then
            tImGui.PopStyleColor(2)
        else
            tImGui.PopStyleColor(1)
        end
    end
end

tUtil.showMessage = function(msg,new_time)
    if tUtil.sMessageOverlay ~= msg then
        tUtil.sMessageOverlay = msg
        tUtil.bWarnMessage = false
        tUtil.bFocusMsgOnce = true
        if new_time then
            tUtil.tTimerOverlay:set(new_time)
        end
        tUtil.tTimerOverlay:start()
    end
end

tUtil.showMessageWarn = function(msg,new_time)
    if tUtil.sMessageOverlay ~= msg then
        tUtil.sMessageOverlay = msg
        tUtil.bWarnMessage = true
        tUtil.bFocusMsgOnce = true
        if new_time then
            tUtil.tTimerOverlay:set(new_time)
        end
        tUtil.tTimerOverlay:start()
    end
end

tUtil.getShortNameBySep = function(str,sep)
    if str then
        local t = str:split(sep)
		if t and #t then
			return t[#t]
		else
			return str
		end
	end
	return nil
end

tUtil.getShortName = function(str,quote)--given a directory name (get name splited by '\' or '/')
	if str then
		local win = tUtil.getShortNameBySep(str,"\\")
		local lin = tUtil.getShortNameBySep(win,"/")
		if quote then
			return string.format('%q', lin)
		else
			return lin
		end
	end
	return nil
end

tUtil.onAddMeshToEditor = function(fileName,insertIntoCenter,_2d_3d,sText)
	if fileName == nil then return nil end
	local tInfo = meshDebug:getInfo(fileName)
	if tInfo then
		local tMesh = nil
		if tInfo.type == "mesh" then
			tMesh = mesh:new(_2d_3d)
			if not tMesh:load(fileName) then
				return nil
			end
		elseif tInfo.type == "sprite" then
			tMesh = sprite:new(_2d_3d)
			if not tMesh:load(fileName) then
				return nil
			end
		elseif tInfo.type == "tile" then
			tMesh = tile:new(_2d_3d)
			if not tMesh:load(fileName) then
				return nil
			end
		elseif tInfo.type == "particle" then
			tMesh = particle:new(_2d_3d)
			if not tMesh:load(fileName) then
				return nil
			end
		elseif tInfo.type == "font" then
			heightFont 	= heightFont or 500
			spaceFont	= spaceFont or 5
			spaceHeightFont	= spaceHeightFont or 5
			local tFont = font:new(fileName,heightFont,spaceFont,spaceHeightFont)
			if tFont == nil then
				return nil
			end
			tMesh = tFont:add(sText or "My text",_2d_3d)
			tMesh.tFont = tFont
			tMesh.sText = sText or 'My text'
		elseif tInfo.type == "texture" then
			if tInfo.ext == 'GIF' then
				tMesh = gif:new(_2d_3d)
				if not tMesh:load(fileName) then
					return nil
				end
			else
                tMesh = texture:new(_2d_3d)
				if not tMesh:load(fileName) then
					return nil
				end
			end
		else
			return nil
        end

        tMesh.type     = tInfo.type
        tUtil.setShapeToMesh(tMesh)
        if insertIntoCenter then
            local iWs,iHs 	= mbm.getSizeScreen()
			local x,y = mbm.to2dw(iWs/2,iHs/2)
			tMesh:setPos(x,y)
		end
		tMesh.mx = 0--mouse
		tMesh.my = 0--mouse
		tMesh.fileName = fileName
		return tMesh
	else
		return nil
	end
end

tUtil.iShapeNickName = 1
--Create a shape and propagate position, scale, rotation to shape
tUtil.setShapeToMesh = function(tObj)
    local w,h,d  = tObj:getAABB(true)
    if d then
	print("****************************** is3d")
    else
        local triangles = 2
        local dynamic   = false
        local tShape    = shape:new('2dw',0,0,tObj.z - 1)
        tShape:create('rectangle',1,1,triangles,dynamic,string.format('unique_shape_nick_name_%d',tUtil.iShapeNickName))
        tUtil.iShapeNickName = tUtil.iShapeNickName + 1
        tObj.tShape = tShape
        tShape:setScale(w,h)

        if tObj.type == 'font' then --little trick to put in the right place the line of font (text)
            tObj.setPos_engine = tObj.setPos
            tObj.setPos = function (self,x,y,z)
                local w,h,d = self:getSize(true)
                self.tShape:setPos((x  or self.x) + w * 0.5,(y or self.y) - h * 0.5, (z or self.z) - 1)
                self:setPos_engine(x or self.x,y or self.y, z or self.z)
            end
        else
            tObj.setPos_engine = tObj.setPos
            tObj.setPos = function (self,x,y,z)
                self.tShape:setPos(x or self.x,y or self.y, (z or self.z) - 1)
                self:setPos_engine(x or self.x,y or self.y, z or self.z)
            end
        end

        tObj.setScale_engine = tObj.setScale
        tObj.setScale = function (self,sx,sy,sz)
            self:setScale_engine(sx or self.sx, sy or self.sy, sz or self.sz)
            local w,h,d = self:getSize()
            self.tShape:setScale(w,h,d or 1)
        end

        tObj.setAngle_engine = tObj.setAngle
        tObj.setAngle = function (self,ax,ay,az)
            self:setAngle_engine(ax or self.ax,ay or self.ay, az or self.az)
            self.tShape:setAngle(ax or self.ax,ay or self.ay, az or self.az)
        end

	end
	
end

tUtil.getExtension  = function(fileName)
	local tS = fileName:split('%.')
	return tS[#tS]
end

tUtil.onNewAnyWindowsHovered = function()
    local _onNewAnyWindowsHovered = function()
        local tWindowsArea = {
            IsAnyWindowHovered = function(self)
                local function isOverWindow(tWindowArea,x,y)
                    if x < tWindowArea.xMin then
                        return false
                    elseif x > tWindowArea.xMax then
                        return false
                    elseif y < tWindowArea.yMin then
                        return false
                    elseif y > tWindowArea.yMax then
                        return false
                    else
                        return true
                    end
                end
                local tMousePos = tImGui.GetMousePos()
                for i=1, #self do
                    if isOverWindow(self[i],tMousePos.x,tMousePos.y) then
                        return true
                    end
                end
                return false
            end
        }
        tWindowsArea.addThisWindow = function(self)
            local tWindowArea = {}
            local iSize      = tImGui.GetWindowSize()
            local iPos       = tImGui.GetWindowPos()
            tWindowArea.xMin = iPos.x
            tWindowArea.yMin = iPos.y
            tWindowArea.xMax = iPos.x + iSize.x
            tWindowArea.yMax = iPos.y + iSize.y
            table.insert(self,tWindowArea)
        end
        return tWindowsArea
    end
    return _onNewAnyWindowsHovered()
end


tUtil.newInstance = function(width, height, expected_width, expected_height, sFileNameScene)
    local exe       = mbm.get('Exe Name')
    sFileNameScene  = sFileNameScene:gsub("\\","/")
    local irw,irh   = mbm.getDisplayMetrics()
    if irw > 0 and irh > 0 and (width > irw or height > irh) then
        local rate = width / height --1.77777
        if rate > 1 then 
            rate = height / width --0.5625
        end
        if width > height then
            print('resize from ',width,height)
            width  = irw
            height = irw * rate
            print('resize to ',width,height)
        elseif height >=  width then
            print('resize from ',width,height)
            height = irh
            width  = rate * irh
            print('resize to ',width,height)
        end
    end
    local command        = string.format('%s -w %d -h %d -ew %d -eh %d --showConsole --nosplash --scene %q --name %q',exe, width, height, expected_width, expected_height, sFileNameScene, tUtil.getShortName(sFileNameScene))
    mbm.executeInThread(command)
    tUtil.showMessage('Command executed.. ')
end

tUtil.deepCopyTable = function(orig)
    local sOriginType = type(orig)
    local tCopy
    if sOriginType == 'table' then
        tCopy = {}
        for orig_key, orig_value in next, orig, nil do
            tCopy[tUtil.deepCopyTable(orig_key)] = tUtil.deepCopyTable(orig_value)
        end
        setmetatable(tCopy, tUtil.deepCopyTable(getmetatable(orig)))
    else -- number, string, boolean, etc
        tCopy = orig
    end
    return tCopy
end

tUtil.copyFile = function(sSource,sDestiny)
    if mbm.is('Windows') then

    else
        local command = string.format("cp %q %q",sSource,sDestiny)
        local bResult, status, n = os.execute(command)
        return bResult
    end
end

tUtil.tStatusMessageSize = {x=0,y=0}

tUtil.showStatusMessage = function (sMessageYellow,sMessageGrayed)
    local flags = {'ImGuiWindowFlags_NoDecoration', 'ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoSavedSettings', 'ImGuiWindowFlags_NoFocusOnAppearing', 'ImGuiWindowFlags_NoNav'}
    tImGui.SetNextWindowBgAlpha(0.75);
    local iW, iH     = mbm.getRealSizeScreen()
    local window_pos = {x = iW - tUtil.tStatusMessageSize.x, y = iH - tUtil.tStatusMessageSize.y}
    local window_pos_pivot = {x = 0, y = 0}
    tImGui.SetNextWindowPos(window_pos, 0, window_pos_pivot);
    local is_opened, closed_clicked = tImGui.Begin('##StatusWindows', false,tImGui.Flags(flags) )
    if is_opened then
        if sMessageYellow and sMessageYellow:len() > 0 then
            tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=1,b=0,a=0.8})
            tImGui.Text(sMessageYellow)
            tImGui.PopStyleColor(1)
        end
        if sMessageGrayed and sMessageGrayed:len() > 0 then
            tImGui.Text(sMessageGrayed)
        end
    end
    tUtil.tStatusMessageSize      = tImGui.GetWindowSize()
    tImGui.End()
end


return tUtil
