tUtil = {}
tUtil.iCountsetInitialWindowPosition = {}

tLang = require "lang.language"

if tImGui == nil then
    mbm.messageBox(tLang.L("msg_imgui_missing"), tLang.L("msg_requires_imgui"))
    mbm.quit()
end


tUtil.supported_images = { "*.png","*.jpeg","*.jpg","*.bmp","*.gif","*.psd","*.pic","*.pnm","*.hdr","*.tga","*.tif"}
tUtil.tColorBackground = {r=37/255,g=37/255,b=37/255}
--color theme
mbm.setColor(tUtil.tColorBackground.r,tUtil.tColorBackground.g,tUtil.tColorBackground.b)

tUtil.iWindowBottomMargin = 24

tUtil.getEditorWindowHeightLimit = function(iMenuBarHeight, y, max_height)
    local _, iH = mbm.getRealSizeScreen()
    local available_height = iH - iMenuBarHeight - (y or 0) - tUtil.iWindowBottomMargin
    available_height = math.max(80, available_height)
    if max_height then
        return math.min(max_height, available_height)
    end
    return available_height
end

tUtil.iResponsiveItemMinWidth = 150
tUtil.iResponsiveItemLabelReserve = 90
tUtil.iWindowMinWidth = 120

tUtil.getResponsiveItemWidth = function(min_width, label_reserve)
    local tAvail = tImGui.GetContentRegionAvail()
    local available_width = tAvail and tAvail.x or 0
    local reserve = label_reserve
    if reserve == nil then
        reserve = tUtil.iResponsiveItemLabelReserve
    end
    return math.max(min_width or tUtil.iResponsiveItemMinWidth, available_width - reserve)
end

tUtil.getResponsiveItemSize = function(min_width, height, label_reserve)
    return {x = tUtil.getResponsiveItemWidth(min_width, label_reserve), y = height or 0}
end

tUtil.pushResponsiveItemWidth = function(min_width, label_reserve)
    tImGui.PushItemWidth(tUtil.getResponsiveItemWidth(min_width, label_reserve))
end

if not tImGui.__miniMbmResponsiveItemWidth then
    tImGui.__miniMbmResponsiveItemWidth = {
        Begin = tImGui.Begin,
        End = tImGui.End,
        stack = {}
    }
    tImGui.Begin = function(...)
        local is_opened, closed_clicked = tImGui.__miniMbmResponsiveItemWidth.Begin(...)
        if is_opened then
            tUtil.pushResponsiveItemWidth()
        end
        table.insert(tImGui.__miniMbmResponsiveItemWidth.stack, is_opened and true or false)
        return is_opened, closed_clicked
    end
    tImGui.End = function()
        local should_pop = table.remove(tImGui.__miniMbmResponsiveItemWidth.stack)
        if should_pop then
            tImGui.PopItemWidth()
        end
        return tImGui.__miniMbmResponsiveItemWidth.End()
    end
end

tUtil.setInitialWindowPositionRight = function(title,x,y,width,max_width, max_height)-- from right to left (so x must be <= 0)
    local strTitleLanguage = string.format("%s-%s", title, tLang.getLanguage())
    if tUtil.iCountsetInitialWindowPosition[strTitleLanguage] == nil then
        tUtil.iCountsetInitialWindowPosition[strTitleLanguage] = 0
    end
    local iMenuBarHeight            = tImGui.GetMainMenuBarHeight()
    local iW                        = mbm.getRealSizeScreen()
    local tPosWin                   = {x = iW - width + x,y = iMenuBarHeight + y }
    local win_height                = tUtil.getEditorWindowHeightLimit(iMenuBarHeight, y, max_height)
    local min_width = math.min(width, tUtil.iWindowMinWidth)
    tImGui.SetNextWindowSizeConstraints({x = min_width,y = math.min(win_height,width)}, {x = max_width or iW,y = win_height})
    if tUtil.iCountsetInitialWindowPosition[strTitleLanguage] <= 3 then
        tUtil.iCountsetInitialWindowPosition[strTitleLanguage] = tUtil.iCountsetInitialWindowPosition[strTitleLanguage] + 1
        tImGui.SetNextWindowSize({x = width, y = win_height},tImGui.Flags('ImGuiCond_Once'))
        tImGui.SetNextWindowPos(tPosWin , tImGui.Flags( 'ImGuiCond_Once'))
    end
end

tUtil.setInitialWindowPositionLeft = function(title,x,y,width,max_width, max_height)-- from left to left (so x must be >= 0)
    local strTitleLanguage = string.format("%s-%s", title, tLang.getLanguage())
    if tUtil.iCountsetInitialWindowPosition[strTitleLanguage] == nil then
        tUtil.iCountsetInitialWindowPosition[strTitleLanguage] = 0
    end
    local iMenuBarHeight            = tImGui.GetMainMenuBarHeight()
    local iW                        = mbm.getRealSizeScreen()
    local tPosWin                   = {x = x,y = iMenuBarHeight + y }
    local win_height                = tUtil.getEditorWindowHeightLimit(iMenuBarHeight, y, max_height)
    local min_width = math.min(width, tUtil.iWindowMinWidth)
    tImGui.SetNextWindowSizeConstraints({x = min_width,y = math.min(win_height,width)}, {x = max_width or iW,y = win_height})
    if tUtil.iCountsetInitialWindowPosition[strTitleLanguage] <= 3 then
        tUtil.iCountsetInitialWindowPosition[strTitleLanguage] = tUtil.iCountsetInitialWindowPosition[strTitleLanguage] + 1
        tImGui.SetNextWindowPos(tPosWin , tImGui.Flags( 'ImGuiCond_Once'))
        tImGui.SetNextWindowSize({x = width, y = win_height},tImGui.Flags('ImGuiCond_Once'))
    end
end

tUtil.setInitialWindowPositionDown = function(title,xStart,YPercentage,xRight)
    local strTitleLanguage = string.format("%s-%s", title, tLang.getLanguage())
    if tUtil.iCountsetInitialWindowPosition[strTitleLanguage] == nil then
        tUtil.iCountsetInitialWindowPosition[strTitleLanguage] = 0
    end
    local iW, iH                    = mbm.getRealSizeScreen()
    local tPosWin                   = {x=xStart,y=iH - (YPercentage * iH)}
    local tSize                     = {x = iW - tPosWin.x - (xRight or 0), y = math.max(80, iH - tPosWin.y - tUtil.iWindowBottomMargin)}
    local tMinSize                  = {x = math.min(120, tSize.x), y = math.min(80, tSize.y)}
    local tMaxSize                  = {x = iW, y = tSize.y}
    tImGui.SetNextWindowSizeConstraints(tMinSize,tMaxSize)
    if tUtil.iCountsetInitialWindowPosition[strTitleLanguage] <= 3 then
        tUtil.iCountsetInitialWindowPosition[strTitleLanguage] = tUtil.iCountsetInitialWindowPosition[strTitleLanguage] + 1
        tImGui.SetNextWindowSize(tSize,tImGui.Flags('ImGuiCond_Once'))
        tImGui.SetNextWindowPos(tPosWin , tImGui.Flags( 'ImGuiCond_Once'))
    end
end

tUtil.bEraseOnClick_showTextureAssets = false
tUtil.bModalRemoveImages_showTextureAssets = nil
tUtil.sTextRemove_showTextureAssets = ''
tUtil.showTextureAssets = function(title,tTexturesToEditor,x_pos,y_pos) -- in title, {{width,height,file_name}}, return tSelectedTexture,  closed_clicked
    local closeable    =  true
    local flags        =  tImGui.Flags('ImGuiWindowFlags_MenuBar')
    local width        = 200
    tUtil.setInitialWindowPositionRight(title,x_pos,y_pos,width,nil)
    local is_opened, closed_clicked = tImGui.Begin(title, closeable, flags)
    if is_opened then
        
        if tImGui.BeginMenuBar() then

            if tImGui.BeginMenu(tLang.L("selection")) then
                local pressed,checked = tImGui.MenuItem(tLang.L("select_all"), nil)
                if pressed then
                    for i=1, #tTexturesToEditor do
                        local tTexture      = tTexturesToEditor[i]
                        tTexture.isSelected = true
                    end
                end
                local pressed,checked = tImGui.MenuItem(tLang.L("unselect_all"), nil)
                if pressed then
                    for i=1, #tTexturesToEditor do
                        local tTexture      = tTexturesToEditor[i]
                        tTexture.isSelected = false
                    end
                end
                local pressed,checked = tImGui.MenuItem(tLang.L("invert_selection"), nil)
                if pressed then
                    for i=1, #tTexturesToEditor do
                        local tTexture      = tTexturesToEditor[i]
                        tTexture.isSelected = not tTexture.isSelected
                    end
                end
                tImGui.EndMenu();
            end
            if tImGui.BeginMenu(tLang.L("remove_menu")) then
                local pressed,checked = tImGui.MenuItem(tLang.L("remove_on_click"), nil, tUtil.bEraseOnClick_showTextureAssets)
                if pressed then
                    tUtil.bEraseOnClick_showTextureAssets = checked
                end
                tImGui.Separator()
                local pressed,checked = tImGui.MenuItem(tLang.L("remove_all"), nil)
                if pressed and #tTexturesToEditor > 0 then
                    tUtil.bModalRemoveImages_showTextureAssets = 'all'
                    tUtil.sTextRemove_showTextureAssets = 'Are you sure do you want to remove all images?'
                end
                local pressed,checked = tImGui.MenuItem(tLang.L("remove_all_selected"), nil)
                if pressed then
                    tUtil.bModalRemoveImages_showTextureAssets = 'selected'
                    tUtil.sTextRemove_showTextureAssets = 'Are you sure do you want to remove all selected images?'
                end
                local pressed,checked = tImGui.MenuItem(tLang.L("remove_all_unselected"), nil)
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

            if tImGui.ImageButton(string.format('tex_btn_%d', i), tTexture.id, size,uv0,uv1) then
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
            local str_desc = string.format(tLang.L("help_texture_info_desc_fmt"), i, #tTexturesToEditor)
            tImGui.HelpMarker(string.format(tLang.L("help_texture_info_fmt"), tTexture.file_name, tTexture.width, tTexture.height), str_desc)
        end
    end
    if tUtil.bModalRemoveImages_showTextureAssets then
        local flags       = tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize')
        local title_popup = tLang.L("remove_all_images")
        tImGui.OpenPopup(title_popup);
        local is_opened, closed_clicked = tImGui.BeginPopupModal(title_popup, false, flags)
        if is_opened then
            tImGui.Text(tUtil.sTextRemove_showTextureAssets)
            tImGui.Separator();
            if tImGui.Button(tLang.L("ok"), {x=120, y= 0}) then
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
            if tImGui.Button(tLang.L("cancel"), {x=120, y= 0}) then
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
            local texInfo = mbm.loadTexture(tFiles[i])
            if texInfo and texInfo:isValid() then
                bSuccess = true
                local base_file_name = tUtil.getBaseFileName(tFiles[i])
                table.insert(tTexturesIn,{file_name = tFiles[i],width = texInfo:getWidth(), height = texInfo:getHeight(), alpha = texInfo:hasAlpha(),id = texInfo , base_file_name = base_file_name})
            else
                print('Could not load texture:',tFiles[i])
            end
        end
    elseif type(tFiles) == 'string' then
        local texInfo = mbm.loadTexture(tFiles)
        if texInfo and texInfo:isValid() then
            local base_file_name = tUtil.getBaseFileName(tFiles)
            table.insert(tTexturesIn,{file_name = tFiles,width = texInfo:getWidth(), height = texInfo:getHeight(), alpha = texInfo:hasAlpha(),id = texInfo , base_file_name = base_file_name})
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

tUtil.hasSupportedMeshExtension = function(file_name)
    local tSupportedTypes = {'.spt', '.msh', '.fnt', '.tile', '.ptl'}
    file_name = file_name:lower()
    for i=1, #tSupportedTypes do
        local supportedType = tSupportedTypes[i]
        if file_name:match("%g%" .. supportedType .. '$') then
            return true
        end
    end
    return false
end

tUtil.getMeshFilesFromFolder = function(dirname)
    local tFiles = {}
    dirname = string.gsub(dirname, "\\", "/")
    if #dirname > 0 and dirname:sub(-1) ~= '/' then
        dirname = dirname .. '/'
    end
    local f = nil
    if mbm.is("windows") then
        f = io.popen('dir /b "' .. dirname .. '"')
    else
        f = io.popen('ls -1 "' .. dirname .. '"')
    end
    if f then
        local ret = f:read("*a")
        f:close()
        local lines = ret:split('\n')
        for i = 1, #lines do
            local file_name = lines[i]:match("^%s*(.-)%s*$")
            if file_name and file_name:len() > 0 and tUtil.hasSupportedMeshExtension(file_name) then
                table.insert(tFiles, dirname .. file_name)
            end
        end
    end
    return tFiles
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

    --[[
        Locale note (important for editor save/load files):
        - Lua editors save data as Lua source code, so numeric literals must use '.' as decimal separator.
        - Number formatting in Lua follows C locale rules (LC_NUMERIC).
        - In locales such as pt_BR/de_DE, formatted numbers may use ',' and produce invalid Lua numbers.
        - Locale-sensitive numeric formats include: %f, %e/%E, %g/%G, %a/%A.
        - Integer/string formats (%d, %i, %u, %x/%X, %o, %s, %q) are not affected.
        - Save paths should force os.setlocale('C','numeric') while writing, then restore previous locale.
        - Serializer also normalizes ',' -> '.' as a defensive fallback.
    ]]--

    local function basicSerialize(o,precision)
        if type(o) == 'number' then
            local normalize_number = function(s)
                if type(s) ~= 'string' then
                    return s
                end
                return (s:gsub(',', '.'))
            end
            if precision then
                return normalize_number(string.format("%a",o)) -- same bits as the original number
            else
                return normalize_number(tostring(o))
            end
        elseif type(o) == 'boolean' then
            return tostring(o)
        else
            return string.format("%q",o)
        end
    end

    local isRootSave = saved == nil
    saved = saved or {}
    if type(value) == 'table' and type(value[1]) == 'userdata' then
        if type(onSaveUserData) == 'function' then
            onSaveUserData(name,value,tOut,saved)
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

    if isRootSave then
        local indexOut = 1
        for i=1, #tOut do
            if tOut[i] ~= '' then
                if indexOut ~= i then
                    tOut[indexOut] = tOut[i]
                end
                indexOut = indexOut + 1
            end
        end
        for i=#tOut, indexOut, -1 do
            tOut[i] = nil
        end
    end
end

tUtil.sMessageOverlay     = 'Welcome!'
tUtil.title_overlay       = '###Overlay'
tUtil.tTimerOverlay       = timer:new(function (self) self:stop(); tUtil.sMessageOverlay = false end, 13.5)
tUtil.bRightSide          = false
tUtil.tSizeWindowOverlay  = {x=0,y=0}
                    
tUtil.showOverlayMessage = function()
    if tUtil.sMessageOverlay then
        local flags = {'ImGuiWindowFlags_NoDecoration', 'ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoSavedSettings', 'ImGuiWindowFlags_NoFocusOnAppearing', 'ImGuiWindowFlags_NoNav'}
        local window_pos = {x = 0, y = tImGui.GetMainMenuBarHeight()}
        local window_pos_pivot = {x = 0, y = 0}
        if tUtil.bRightSide then
            local iW, iH       = mbm.getRealSizeScreen()
            window_pos.x = iW
            window_pos_pivot = {x = 1, y = 0}  -- anchor window's right edge to screen right
        end
        if tUtil.bFocusMsgOnce then
            tUtil.bFocusMsgOnce = false
            tImGui.SetNextWindowFocus(tLang.L("title_overlay"))
        end
        tImGui.SetNextWindowPos(window_pos, tImGui.Flags('ImGuiCond_Always'), window_pos_pivot);
        tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=1,b=0,a=0.8})
        if tUtil.bWarnMessage then
            tImGui.SetNextWindowBgAlpha(0.5);
            tImGui.PushStyleColor('ImGuiCol_WindowBg',{r=1,g=0,b=0,a=0.5})
        else
            tImGui.SetNextWindowBgAlpha(0.75);
        end
        local is_opened, closed_clicked = tImGui.Begin(tLang.L("title_overlay"), false, tImGui.Flags(flags))
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
			tMesh.heightFont      = heightFont
			tMesh.spaceFont       = spaceFont
			tMesh.spaceHeightFont = spaceHeightFont
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
	   return
    else
        local triangles = 2
        local dynamic   = false
        local tShape    = shape:new('2dw',0,0,tObj.z - 1)
        tShape:create('rectangle',1,1,triangles,dynamic,string.format('unique_shape_nick_name_%d',tUtil.iShapeNickName))
        tUtil.iShapeNickName = tUtil.iShapeNickName + 1
        tObj.tShape = tShape
        tShape:setScale(w,h)

        -- Single code path for every type, not one branch for font and one for everything else:
        -- obj:getAABBCenter() (engine, MBM_VERSION 6.9.0) returns the object's TRUE AABB center in
        -- world space -- the object's own position for anything pivoted at its visual center
        -- (sprites/tiles/textures, the overwhelming default), and the correct alignment-corrected
        -- point for font (whose origin is top-left, not centered). This replaces the old
        -- hand-written `(x + w*0.5, y - h*0.5)` font-only correction with the same mechanism
        -- `obj:collide()` now uses internally, instead of a second, independently-written copy of
        -- the same formula.
        tObj.setPos_engine = tObj.setPos
        tObj.setPos = function (self,x,y,z)
            self:setPos_engine(x or self.x, y or self.y, z or self.z)
            local cx, cy = self:getAABBCenter(true)
            self.tShape:setPos(cx, cy, (z or self.z) - 1)
        end

        tObj.setScale_engine = tObj.setScale
        tObj.setScale = function (self,sx,sy,sz)
            self:setScale_engine(sx or self.sx, sy or self.sy, sz or self.sz)
            local w,h,d = self:getSize(true)
            self.tShape:setScale(w,h,d or 1)
            local cx, cy = self:getAABBCenter(true)
            self.tShape:setPos(cx, cy, self.z - 1)
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
    local exe = mbm.get('Exe Name')
    if mbm.is('Windows') then
        -- Windows: use exe name as-is
    elseif mbm.is('Linux') then
        -- In the shell spawned by io.popen, $PPID is the PID of the mini-mbm process.
        -- readlink /proc/$PPID/exe resolves to the absolute path of the mini-mbm binary.
        local f = io.popen('readlink /proc/$PPID/exe')
        if f then
            local resolved = f:read('*l')
            f:close()
            if resolved and resolved:len() > 0 then
                exe = resolved
            end
        end
    else
        -- macOS: lsof -p $PPID resolves the full path of the running binary;
        -- the 'txt' type entry is the executable text segment.
        local f = io.popen('lsof -p $PPID 2>/dev/null | awk \'/ txt /{print $NF; exit}\'')
        if f then
            local resolved = f:read('*l')
            f:close()
            if resolved and resolved:len() > 0 then
                exe = resolved
            else
                -- fallback: normalise underscores→hyphens
                local dir, base = exe:match('^(.*/)([^/]+)$')
                exe = dir and (dir .. base:gsub('_', '-')) or exe:gsub('_', '-')
            end
        end
    end
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
    local command
    if mbm.is('Windows') then
        -- Use "start" so the command passed to cmd.exe /c does NOT begin with a
        -- double-quote.  When the first token IS a quoted string, cmd.exe strips
        -- the leading and trailing quote from the whole command line, mangling
        -- paths.  "start" is a cmd-builtin that accepts an empty window-title
        -- ("") followed by a quoted executable path, bypassing that quirk.
        -- %q must NOT be used here: it adds Lua backslash escaping (C:\foo ->
        -- "C:\\foo") which makes Windows reject the path with ERROR_INVALID_NAME.
        command = string.format('"%s" -w %d -h %d -ew %d -eh %d --showConsole --nosplash --scene "%s" --name "%s"',
            exe, width, height, expected_width, expected_height, sFileNameScene, tUtil.getShortName(sFileNameScene))
    elseif mbm.is('Linux') then
        -- The X11 socket is now marked FD_CLOEXEC in C++ (initializeWindowx11), so the
        -- child process never inherits it. Use setsid to put mini-mbm in a new session
        -- (detached from the terminal) and & to return immediately from system().
        command = string.format('setsid %q -w %d -h %d -ew %d -eh %d --nosplash --scene %q --name %q &',
            exe, width, height, expected_width, expected_height, sFileNameScene, tUtil.getShortName(sFileNameScene))
    else
        -- macOS: setsid is not available; just background the process with &
        command = string.format('%q -w %d -h %d -ew %d -eh %d --nosplash --scene %q --name %q &',
            exe, width, height, expected_width, expected_height, sFileNameScene, tUtil.getShortName(sFileNameScene))
    end
    print('Launching:', command)
    mbm.executeInThread(command)
    tUtil.showMessage(tLang.L("command_executed"))
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

-- Converts an {azimuth=, elevation=} orbit state (the same convention drawOrbitGizmo below uses
-- for the 3D camera) into a directional-light direction vector {x=,y=,z=}. The orbit angles
-- describe the unit vector pointing FROM the scene TOWARD the light source ("where is the sun" --
-- intuitive to drag, same gesture as orbiting the camera). mbm.setDirectionalLightDirection's
-- direction is the direction light TRAVELS (source -> scene), the opposite of that -- hence the
-- negation. Shared by scene_editor3d.lua and mesh_debug.lua so this math only lives once.
tUtil.dirFromOrbit = function(orbit)
    local caz, saz = math.cos(orbit.azimuth), math.sin(orbit.azimuth)
    local cel, sel = math.cos(orbit.elevation), math.sin(orbit.elevation)
    local towardLightX, towardLightY, towardLightZ = cel * saz, sel, cel * caz
    return { x = -towardLightX, y = -towardLightY, z = -towardLightZ }
end

-- Inverse of the above -- derives {azimuth=, elevation=} from a directional-light direction vector
-- (e.g. the persisted/exported directionalDir), so an orbit gizmo starts in sync with whatever
-- direction is currently set instead of snapping to some arbitrary default the first time it's
-- touched.
tUtil.orbitFromDir = function(dir)
    local towardX, towardY, towardZ = -dir.x, -dir.y, -dir.z
    local len = math.sqrt(towardX * towardX + towardY * towardY + towardZ * towardZ)
    if len < 1e-6 then
        towardX, towardY, towardZ, len = 0, 1, 0, 1 -- degenerate (zero vector) -- default to straight down
    end
    towardX, towardY, towardZ = towardX / len, towardY / len, towardZ / len
    return {
        elevation = math.asin(math.max(-1, math.min(1, towardY))),
        azimuth = math.atan(towardX, towardZ),
    }
end

-- Blender-style navigation gizmo: draws an interactive sphere with 6 axis dots.
-- c: spherical camera state {azimuth=, elevation=, ...}, mutated in place.
-- opts (optional): size (px diameter), sensitivity (rad/px drag), elevationLimit (rad).
-- Must be called between the caller's tImGui.Begin/End. Returns true if c changed.
tUtil.drawOrbitGizmo = function(c, opts)
    opts = opts or {}
    local size           = opts.size or 110
    local sensitivity    = opts.sensitivity or 0.005
    local elevationLimit = opts.elevationLimit or (math.pi * 0.49)

    local function normalize3(x, y, z)
        local len = math.sqrt(x*x + y*y + z*z)
        if len > 0.0001 then
            return x / len, y / len, z / len
        end
        return x, y, z
    end

    local caz, saz = math.cos(c.azimuth), math.sin(c.azimuth)
    local cel, sel = math.cos(c.elevation), math.sin(c.elevation)
    local dirX, dirY, dirZ = cel * saz, sel, cel * caz -- unit vector focus -> camera
    local fwX, fwY, fwZ    = -dirX, -dirY, -dirZ        -- forward: camera -> focus

    -- Same convention as the viewport pan code (onTouchMove): right = normalize(cross(worldUp, forward))
    local rgX, rgZ = fwZ, -fwX
    local rgLen    = math.sqrt(rgX * rgX + rgZ * rgZ)
    if rgLen > 0.0001 then
        rgX, rgZ = rgX / rgLen, rgZ / rgLen
    else
        rgX, rgZ = math.cos(c.azimuth + math.pi * 0.5), math.sin(c.azimuth + math.pi * 0.5)
    end
    local rgY = 0
    local upX, upY, upZ = normalize3(fwY * rgZ - fwZ * rgY, fwZ * rgX - fwX * rgZ, fwX * rgY - fwY * rgX)

    -- Optional roll around the viewing direction. Existing camera/light orbit states do not carry
    -- roll, so the default remains zero; articulated pivot orbit states use it to expose the third
    -- rotational degree of freedom instead of silently dropping quaternion roll.
    local roll = c.roll or 0
    local rollCos, rollSin = math.cos(roll), math.sin(roll)
    local rolledRgX = rgX * rollCos + upX * rollSin
    local rolledRgY = rgY * rollCos + upY * rollSin
    local rolledRgZ = rgZ * rollCos + upZ * rollSin
    local rolledUpX = -rgX * rollSin + upX * rollCos
    local rolledUpY = -rgY * rollSin + upY * rollCos
    local rolledUpZ = -rgZ * rollSin + upZ * rollCos
    rgX, rgY, rgZ = rolledRgX, rolledRgY, rolledRgZ
    upX, upY, upZ = rolledUpX, rolledUpY, rolledUpZ

    local p0     = tImGui.GetCursorScreenPos()
    local center = {x = p0.x + size * 0.5, y = p0.y + size * 0.5}
    local radius = size * 0.5 - 12

    local tAxes = {
        {label='X', x= 1, y= 0, z= 0, color={r=0.85,g=0.27,b=0.27,a=1}, snapAz=math.pi*0.5,  snapEl=0,               positive=true},
        {label='',  x=-1, y= 0, z= 0, color={r=0.85,g=0.27,b=0.27,a=1}, snapAz=-math.pi*0.5, snapEl=0,               positive=false},
        {label='Y', x= 0, y= 1, z= 0, color={r=0.35,g=0.75,b=0.35,a=1}, snapAz=nil,          snapEl=elevationLimit,  positive=true},
        {label='',  x= 0, y=-1, z= 0, color={r=0.35,g=0.75,b=0.35,a=1}, snapAz=nil,          snapEl=-elevationLimit, positive=false},
        {label='Z', x= 0, y= 0, z= 1, color={r=0.35,g=0.55,b=0.95,a=1}, snapAz=0,            snapEl=0,               positive=true},
        {label='',  x= 0, y= 0, z=-1, color={r=0.35,g=0.55,b=0.95,a=1}, snapAz=math.pi,      snapEl=0,               positive=false},
    }

    for _, tAxis in ipairs(tAxes) do
        local sx = tAxis.x * rgX + tAxis.y * rgY + tAxis.z * rgZ
        local sy = tAxis.x * upX + tAxis.y * upY + tAxis.z * upZ
        -- depth < 0 means the axis points back toward the camera (near side); > 0 means it points
        -- away, into the screen (far side).
        tAxis.depth     = tAxis.x * fwX + tAxis.y * fwY + tAxis.z * fwZ
        tAxis.screen    = {x = center.x + sx * radius, y = center.y - sy * radius}
        tAxis.dotRadius = tAxis.depth < 0 and 7 or 5
    end
    -- Painter's algorithm: draw far side first, near side last (on top).
    table.sort(tAxes, function(a, b) return a.depth > b.depth end)

    tImGui.AddCircle(center, radius + 8, {r=1,g=1,b=1,a=0.15}, 24, 1.0)
    for _, tAxis in ipairs(tAxes) do
        tImGui.AddLine(center, tAxis.screen, {r=tAxis.color.r, g=tAxis.color.g, b=tAxis.color.b, a=0.5}, 1.0)
        if tAxis.positive then
            tImGui.AddCircleFilled(tAxis.screen, tAxis.dotRadius, tAxis.color, 12)
            local tTextSize = tImGui.CalcTextSize(tAxis.label)
            tImGui.AddText({x = tAxis.screen.x - tTextSize.x * 0.5, y = tAxis.screen.y - tTextSize.y * 0.5},
                            {r=1,g=1,b=1,a=1}, tAxis.label)
        else
            tImGui.AddCircle(tAxis.screen, tAxis.dotRadius, tAxis.color, 12, 1.5)
        end
    end

    tImGui.SetCursorScreenPos(p0)
    tImGui.InvisibleButton('##orbitGizmo', {x = size, y = size})
    local changed = false

    if tImGui.IsItemClicked(0) then
        local tMouse = tImGui.GetMousePos()
        local bestAxis, bestDist = nil, nil
        for _, tAxis in ipairs(tAxes) do
            local dx, dy = tMouse.x - tAxis.screen.x, tMouse.y - tAxis.screen.y
            local dist   = math.sqrt(dx * dx + dy * dy)
            if dist <= tAxis.dotRadius + 3 and (bestDist == nil or dist < bestDist) then
                bestAxis, bestDist = tAxis, dist
            end
        end
        if bestAxis then
            if bestAxis.snapAz then c.azimuth = bestAxis.snapAz end
            c.elevation = bestAxis.snapEl
            changed = true
        end
    end

    if tImGui.IsItemActive() then
        local tDelta = tImGui.GetMouseDragDelta(0)
        if tDelta.x ~= 0 or tDelta.y ~= 0 then
            c.azimuth   = c.azimuth - tDelta.x * sensitivity
            c.elevation = c.elevation + tDelta.y * sensitivity
            c.elevation = math.max(-elevationLimit, math.min(elevationLimit, c.elevation))
            tImGui.ResetMouseDragDelta(0)
            changed = true
        end
    end

    return changed
end

return tUtil
