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

   Asset Packager lua

   This is a script based on mbm engine.

   Asset Packager meant to create and edit asset to be used in your game using lua sqlite3.
   An asset package might contain files such as texture, music, script, etc.

   More info at: https://mbm-documentation.readthedocs.io/en/latest/editors.html#asset-packager

]]--

tImGui        =     require "ImGui"
tUtil         =     require "editor_utils"
sqlite3       =     require "lsqlite3"


function onInitScene()
    
    tWindowsTitle               = {title_asset             = 'Assets', }
    tAssets                     = {}
    ImGuiWindowFlags_NoMove     = tImGui.Flags('ImGuiWindowFlags_NoMove')
    iSelectedItem               = 0
    sSelectedFile               = ''
    bShowAssets                 = false
    camera2d		            = mbm.getCamera("2d")
    isClickedMouseleft          = false
    tLineCenterX                = line:new("2dw",0,0,50)
    tLineCenterY                = line:new("2dw",0,0,50)
    sPackageName                = ''
    tSupportedImages            = { "png","jpeg","jpg","bmp","gif","psd","pic","pnm","hdr","tga","tif"}
    tAudioFormat                = {aa  = true, aac = true, aax = true, act = true, aiff= true, alac  = true, amr  = true, ape  = true, au  = true, awb  = true, dct  = true, dss  = true, dvf  = true, flac  = true, gsm  = true, iklax  = true, ivs  = true, m4a  = true, m4b  = true, m4p  = true, mmf  = true, mp3  = true, mpc  = true, msv  = true, nmf  = true, nsf  = true, ogg  = true, oga   = true, mogg  = true, opus  = true, ra  = true, rm  = true, raw  = true, rf64  = true, sln  = true, tta  = true, voc  = true, vox  = true, wav  = true, wma  = true, wv  = true, webm  = true, cda  = true}
    tAudioFormat['3gp']         = true
    tAudioFormat['8svx']        = true
    tMeshFormat                 = {msh = true, spt = true, ptl = true, tile = true, ptl = true, fnt = true}
    tCategory                   = {'all','image','lua-script','mesh','audio',index = 1}
    tUtil.sMessageOverlay       = 'Welcome to Asset Packager!'
    tLineCenterX:add({-9999999,0, 9999999,0})
    tLineCenterX:setColor(1,0,0)
    tLineCenterY:add({0,-9999999, 0,9999999})
    tLineCenterY:setColor(0,1,0)
end


function onAddAsset()
    local folderPath = mbm.openFolder('Please choose a folder','.')
    if folderPath then
        local tFolders = mbm.listFiles(folderPath,true)
        table.insert(tAssets,tFolders)
        for i=1, #tFolders do
            mbm.addPath(tFolders[i].path)
        end
        bShowAssets = true
    end
end

function onNewAsset()
    local folderPath = mbm.openFolder('Please choose a folder','.')
    if folderPath then
        sPackageName = ''
        tAssets = {}
        local tFolders = mbm.listFiles(folderPath,true)
        table.insert(tAssets,tFolders)
        for i=1, #tFolders do
            mbm.addPath(tFolders[i].path)
        end
        bShowAssets = true
    end
end

function getExtension(sFile)
    local tType = sFile:split('%.')
    if tType and #tType > 0 and tType[#tType]:len() > 0 then
        return tType[#tType]:lower()
    end
    return ''
end

function onSaveAsset()
    local filename = mbm.saveFile(sPackageName,'*.asset')
    if filename then
        local db = sqlite3.open(filename)
        if db then
            local result = db:exec([[ 
                PRAGMA foreign_keys = ON;
                DROP TABLE IF EXISTS assets;
                DROP TABLE IF EXISTS paths;
                DROP TABLE IF EXISTS dumped_folder;

                CREATE TABLE dumped_folder(path TEXT);

                CREATE TABLE paths(id INTEGER PRIMARY KEY,
                                    path TEXT);

                CREATE TABLE assets(id INTEGER PRIMARY KEY, 
                                    name TEXT,
                                    category TEXT, 
                                    content BLOB,
                                    id_path INTEGER REFERENCES paths(id) ON DELETE CASCADE); ]])

            
            if result == sqlite3.OK then
                local id = 1
                local stmt, err_id = db:prepare([[ INSERT INTO paths VALUES (?,?); ]])
                if stmt then
                    for i=1, #tAssets do
                        local tFolders = tAssets[i]
                        for j=1, #tFolders do
                            local tFiles = tFolders[j]
                            stmt:bind_values(id,tFiles.path)
                            stmt:step()
                            stmt:reset()
                            id = id + 1
                        end
                    end
                    stmt:finalize()
                else
                    local msg = db:errmsg(err_id)
                    print('line', msg)
                    tUtil.showMessageWarn('Error:\n' .. msg)
                end

                local id = 1
                local stmt, err_id = db:prepare([[ INSERT INTO assets VALUES (:id, :name, :category, readfile(:content), (SELECT id FROM paths WHERE path = :path)); ]])
                if stmt then
                    for i=1, #tAssets do
                        local tFolders = tAssets[i]
                        for j=1, #tFolders do
                            local tFiles = tFolders[j]
                            for k=1, #tFiles do
                                local sFile = tFiles[k]
                                local sFileFullPath = string.format('%s%s%s',tFiles.path,tFolders.separator,sFile)
                                local sCategory     = getCategory(sFile)
                                stmt:bind_names ({  id = id,  name = sFile, category = sCategory, content = sFileFullPath, path = tFiles.path })
                                stmt:step()
                                stmt:reset()
                                id = id + 1
                            end
                        end
                    end
                    stmt:finalize()
                    updateCategory(db)
                    tUtil.showMessage(string.format('Asset:\n%s\n\nCreated Successfully!',filename))
                    sPackageName = filename
                else
                    local msg = db:errmsg(err_id)
                    print('line', msg)
                    tUtil.showMessageWarn('Error:\n' .. msg)
                end
                db:close()
            else
                local msg = db:errmsg(result)
                print('line', msg)
                tUtil.showMessageWarn('Error:\n' .. msg)
            end
        else
            print('error', 'Could not create the database ' .. filename)
            tUtil.showMessageWarn('Could not create the database ' .. filename)
        end
    end
end

function drawFolderTree(tFiles)
    local flags  = 0
    if tImGui.TreeNodeEx(tFiles.path,flags,'##' .. tFiles.path) then
        for i=1, #tFiles do
            local sFile = tFiles[i]
            local id = string.format('##%d-%s/%s',i,tFiles.path,sFile)
            tImGui.Text(sFile)
        end
        tImGui.TreePop()
    end
end

function deleteFromDatabase(sFile)
    local db = sqlite3.open(sPackageName,sqlite3.OPEN_READWRITE)
    if db then
        local result = db:exec(string.format('DELETE FROM assets where name = %q ;',sFile))
        if result ~= sqlite3.OK then
            local msg = db:errmsg(err_id)
            print('line', msg)
            tUtil.showMessageWarn('Error:\n' .. msg)
        end
        db:close()
    else
        tUtil.showMessageWarn('Error:\nCould not open the database:\n ' .. tostring(sPackageName))
    end
end

function extractFileFromDatabase(sFile,sDestiny)
    local db = sqlite3.open(sPackageName,sqlite3.OPEN_READONLY)
    if db then
        local bRet = false
        local select_stmt, err_id = db:prepare([[ SELECT writefile(?,content) FROM assets WHERE name = ?; ]])
        if select_stmt then
            select_stmt:bind_values(sDestiny,sFile)
            if select_stmt:step() == sqlite3.ROW then
                bRet = true
                for row in select_stmt:nrows() do
                end
            else
                tUtil.showMessageWarn(string.format('Error:\nNo data found for the SQL:\n SELECT writefile("%s",content) FROM assets WHERE name = "%s";',sDestiny,sFile))
            end
            select_stmt:finalize()
            db:close()
        else
            local msg = db:errmsg(err_id)
            print('line', msg)
            tUtil.showMessageWarn('Error:\n' .. msg)
        end
        return bRet
    else
        tUtil.showMessageWarn('Error:\nCould not open the database:\n ' .. tostring(sPackageName))
        return false
    end
end

function removeFolderAnSubFolder(path)
    local bRemoved = false
    for i=1, #tAssets do
        local tFolders = tAssets[i]
        for j=1, #tFolders do
            local tFiles = tFolders[j]
            -- replace forbiden character
            local p_path=path:gsub('-', '_')
            local p_tFiles=tFiles.path:gsub('-', '_')
            local s,e = p_tFiles:find(p_path)
            if s and e then
                bRemoved = true
                table.remove(tFolders,j)
                removeFolderAnSubFolder(path)
                break
            end
        end
        if bRemoved then
            break
        end
    end
end

function showAssets()
    if bShowAssets then
        local iWs,iHs 	   = mbm.getSizeScreen()
        local width        = iWs  * 0.35
        local tPosWin      = {x = 0, y = 0}
        local size         = {x=0,y=0}
        tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_asset,tPosWin.x,tPosWin.y,width,width)
        local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_asset, true, ImGuiWindowFlags_NoMove)
        local bUniquePoupUpContext = true
        local right_mouse_pressed = false
        local bRemoved = false
        if is_opened then
            local flags  = 0
            for i=1, #tAssets do
                if bRemoved then
                    break
                end
                local tFolders = tAssets[i]
                for j=1, #tFolders do
                    local tFiles = tFolders[j]
                    if tImGui.TreeNodeEx(tFiles.path,flags,'##' .. tFiles.path) then
                        if right_mouse_pressed == false and tImGui.BeginPopupContextItem(tostring(i) .. tostring(j), 1) then
                            right_mouse_pressed = true
                            if tImGui.Selectable("Delete\n" .. tFiles.path) then
                                removeFolderAnSubFolder(tFiles.path)
                                bRemoved = true
                            end
                            tImGui.EndPopup()
                        end
                        if bRemoved then
                            tImGui.TreePop()
                            break
                        end
                        for k=1, #tFiles do
                            if bRemoved then
                                break
                            end
                            local bSelected =  (i * j * k) == iSelectedItem
                            local sFile     = tFiles[k]
                            local id        = string.format('##%d-%s/%s',i,tFiles.path,sFile)
                            local result, bSelected = tImGui.Selectable(sFile, bSelected, flags, size)
                            if result then
                                iSelectedItem = i * j * k
                                sSelectedFile = sFile
                                destroyMesh()
                            end

                            if bSelected and bUniquePoupUpContext and tImGui.BeginPopupContextItem("Options Frame List") then
                                bUniquePoupUpContext = false
                                local sOriginalFile = string.format('%s%s%s',tFiles.path,tFolders.separator,sFile)
                                if sPackageName:len() > 0 then
                                    if tImGui.Selectable("Extract to File ...") then
                                        local filename = mbm.saveFile(sOriginalFile,getExtension(sFile))
                                        if filename then
                                            if extractFileFromDatabase(sFile,filename) then
                                                tUtil.showMessage('File Successfully Extracted:\n\n' .. filename)
                                            end
                                        end
                                    end
                                else
                                    if tImGui.Selectable("Copy File to ...") then
                                        local filename = mbm.saveFile(sOriginalFile,getExtension(sFile))
                                        if filename then
                                            if tUtil.copyFile(sOriginalFile,filename) then
                                                tUtil.showMessage('File Copied Successfully:\n\n' .. filename)
                                            else
                                                tUtil.showMessageWarn('Failed to Copy File!\n ' .. sOriginalFile .. '\nto:\n' .. filename)
                                            end
                                        end
                                    end
                                end
                                if sPackageName:len() > 0 then
                                    if tImGui.Selectable("Remove From Database") then
                                        deleteFromDatabase(sFile)
                                        table.remove(tFiles,k)
                                        tImGui.EndPopup()
                                        iSelectedItem = 0
                                        sSelectedFile = ''
                                        destroyMesh()
                                        break
                                    end
                                else
                                    if tImGui.Selectable("Remove") then
                                        table.remove(tFiles,k)
                                        tImGui.EndPopup()
                                        iSelectedItem = 0
                                        sSelectedFile = ''
                                        destroyMesh()
                                        break
                                    end
                                end
                                tImGui.EndPopup()
                            end
                        end
                        tImGui.TreePop()
                    end
                end
            end
        end
        tImGui.End()
        if closed_clicked then
            bShowAssets = false
        end
    end
end

function isSupportedImages(sWhatType)
    sWhatType = sWhatType:lower()
    for i=1, #tSupportedImages do
        if tSupportedImages[i] == sWhatType then
            return true
        end
    end
    return false
end

function isType(sFile,sTypeMesh)
    local sType = string.format('%%.%s$',sTypeMesh:lower())
    if sFile:lower():match(sType) then
        return true
    end
    return false
end

function destroyMesh()
    if tMesh then
        tMesh.tFont = nil
        tMesh:destroy()
        tMesh = nil
    end
end

function onOpenAsset()
    local filename = mbm.openFile(sPackageName,'*.asset')
    if filename then
        local db = sqlite3.open(filename,sqlite3.OPEN_READONLY)
        if db then
            tAssets         = {}
            local tFolders  = {}
            local sPreviousPath = ''
            local select_stmt, err_id = db:prepare([[ SELECT name, path FROM assets INNER JOIN paths on paths.id = assets.id_path order by path; ]])
            if select_stmt then
                local tFolder = {}
                for row in select_stmt:nrows() do
                    if row.path ~= sPreviousPath then
                        if #tFolder > 0 then
                            table.insert(tFolders,tFolder)
                        end
                        tFolder         = {path = row.path}
                        sPreviousPath   = row.path
                    end
                    table.insert(tFolder,row.name)
                end
                if mbm.is('Windows') then
                    tFolders.separator = '\\'
                else
                    tFolders.separator = '/'
                end
                --last folder
                if #tFolder > 0 then
                    table.insert(tFolders,tFolder)
                end
                --add path
                for i=1, #tFolders do
                    mbm.addPath(tFolders[i].path)
                end
                table.insert(tAssets,tFolders)
                select_stmt:finalize()

                updateCategory(db)
                
                tUtil.showMessage(string.format('Asset:\n%s\n\nOpened Successfully!',filename))
                sPackageName = filename
                bShowAssets = true
            else
                local msg = db:errmsg(err_id)
                print('line', msg)
                tUtil.showMessageWarn('Error:\n' .. msg)
            end
            db:close()
        else
            tUtil.showMessageWarn('Could not open the database ' .. filename)
        end
    end
end

function updateCategory(db)
    local select_stmt, err_id = db:prepare([[ SELECT DISTINCT category FROM assets; ]])
    if select_stmt:step() == sqlite3.ROW then
        for row in select_stmt:nrows() do
            local bExist = false
            for i=1, #tCategory do
                if tCategory[i] == row.category then
                    bExist = true
                    break
                end
            end
            if bExist == false then
                table.insert(tCategory,row.category)
            end
        end
    end

    table.sort(tCategory)
    if tCategory[1] ~= 'all' then
        local sOld = tCategory[1]
        tCategory[1] = 'all'
        for i=2, #tCategory do
            if tCategory[i] == 'all' then
                tCategory[i] = sOld
            end
        end
    end
end

function getCategory(sFile)
    local tType = sFile:split('%.')
    if tType and #tType > 0 and tType[#tType]:len() > 0 then
        local sType = tType[#tType]:lower()
        if isSupportedImages(sType) then
            return 'image'
        elseif sType == 'lua' then
            return 'lua-script'
        elseif tAudioFormat[sType] then
            return 'audio'
        elseif tMeshFormat[sType] then
            return 'mesh'
        else
            return sType
        end
    else
        return ''
    end
end

function showCurrentSelection()
    if tMesh == nil and sSelectedFile and sSelectedFile:len() > 0 then
        local tType = sSelectedFile:split('%.')
        if tType and #tType > 0 and tType[#tType]:len() > 0 then
            local sType = tType[#tType]:lower()
            if isSupportedImages(sType) then
                tMesh = texture:new('2dw')
                tMesh:load(sSelectedFile)
            elseif isType(sSelectedFile,'msh') then
                tMesh = mesh:new('2dw')
                tMesh:load(sSelectedFile)
            elseif isType(sSelectedFile,'spt') then
                tMesh = sprite:new('2dw')
                tMesh:load(sSelectedFile)
            elseif isType(sSelectedFile,'tile') then
                tMesh = tile:new('2dw')
                tMesh:load(sSelectedFile)
            elseif isType(sSelectedFile,'ptl') then
                tMesh = particle:new('2dw')
                tMesh:load(sSelectedFile)
                tMesh:add(100)
                tMesh.revive = true
            elseif isType(sSelectedFile,'fnt') then
                local tFont = font:new(sSelectedFile)
                tMesh       = tFont:add('2dw','Hello\nWorld!')
                tMesh.tFont = tFont
            end
        end
    end
end

function onTouchDown(key,x,y)
    if not tImGui.IsAnyWindowHovered() then
        isClickedMouseleft = key == 1
        camera2d.mx = x
        camera2d.my = y
    end
end

function onTouchMove(key,x,y)
    if isClickedMouseleft and not tImGui.IsAnyWindowHovered() then
        local px = (camera2d.mx - x) * camera2d.sx
        local py = (camera2d.my - y) * camera2d.sy
        camera2d.mx = x
        camera2d.my = y
        camera2d:setPos(camera2d.x + px,camera2d.y - py)
    end
end

function onTouchUp(key,x,y)
    isClickedMouseleft = false
    camera2d.mx = x
    camera2d.my = y
end

function onTouchZoom(zoom)
    if tMesh then
        local s = zoom * (0.2)
        tMesh.sx = tMesh.sx + s
        if tMesh.sx < 0.2 then
            tMesh.sx = 0.2
        end
        tMesh.sy = tMesh.sx
        tMesh.sz = tMesh.sy
    end
end

function onKeyDown(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = true
    elseif keyControlPressed then
        if key == mbm.getKeyCode('A') then -- Ctrl+A
            onAddAsset()
        elseif key == mbm.getKeyCode('S') then -- Ctrl+S
            onSaveAsset()
        elseif key == mbm.getKeyCode('O') then -- Ctrl+O
            onOpenAsset()
        elseif key == mbm.getKeyCode('N') then -- Ctrl+N
            onNewAsset()
        end
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = false
    end
end

function main_menu_asset()
    if tImGui.BeginMainMenuBar() then
        if tImGui.BeginMenu("File") then
            
            local pressed,checked = tImGui.MenuItem("New Asset (From Folder)", "Ctrl+N", false)
            if pressed then
                onNewAsset()
            end

            local pressed,checked = tImGui.MenuItem("Add Asset (From Folder)", "Ctrl+A", false)
            if pressed then
                onAddAsset()
            end

            local pressed,checked = tImGui.MenuItem("Load Asset (From Database)", "Ctrl+O", false)
            if pressed then
                onOpenAsset()
            end
            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("Save Asset (To Database)", "Ctrl+S", false)
            if pressed then
                onSaveAsset()
            end

            if tImGui.BeginMenu("Extract Asset (To Folder)") then
                
                tImGui.Text('Category')
                local label            = '##Category'
                local height_in_items  =  -1

                local ret, current_item, item_as_string = tImGui.Combo(label, tCategory.index, tCategory, height_in_items)
                if ret then
                    tCategory.index = current_item
                end
                local size   =  {x=-1,y=0}
                if tImGui.Button('Extract', size) then
                    if sPackageName:len() > 0 then
                        local db = sqlite3.open(sPackageName,sqlite3.OPEN_READWRITE)
                        if db then
                            local folderPath = mbm.openFolder('Please choose a folder','.')
                            if folderPath then
                                local sSeparator
                                if mbm.is('Windows') then
                                    sSeparator = '\\'
                                else
                                    sSeparator = '/'
                                end
                                local result = db:exec(string.format('SELECT ADD_ASSET_FOLDER("%s%s%s");',folderPath,sSeparator,'asset'))
                                if result ~= sqlite3.OK then
                                    print(result,db:errmsg())
                                    tUtil.showMessageWarn('Error:\n' .. db:errmsg())
                                else
                                    if tCategory[tCategory.index] == 'all' then
                                        local result = db:exec('SELECT SAVE_ASSET(name,content) FROM assets;')
                                        if result == sqlite3.OK then
                                            tUtil.showMessage('Command Executed Successfully!')
                                        else
                                            tUtil.showMessageWarn('Error:\n' .. db:errmsg())
                                            print(result,db:errmsg())
                                        end
                                    else
                                        local result = db:exec(string.format('SELECT SAVE_ASSET(name,content) FROM assets where category = %q;',tCategory[tCategory.index]))
                                        if result == sqlite3.OK then
                                            tUtil.showMessage('Command Executed Successfully!')
                                        else
                                            tUtil.showMessageWarn('Error:\n' .. db:errmsg())
                                            print(result,db:errmsg())
                                        end
                                    end
                                end
                            end
                            db:close()
                        else
                            print('error', 'Could not open the database ' .. sPackageName)
                            tUtil.showMessageWarn('Could not open the database ' .. sPackageName)
                        end
                    else
                        tUtil.showMessageWarn('There is no database loaded!')
                    end
                end

                tImGui.EndMenu()
            end

            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("Quit", "Alt+F4", false)
            if pressed then
                mbm.quit()
            end

            tImGui.EndMenu();
        end

        if tImGui.BeginMenu("About") then
            local pressed,checked = tImGui.MenuItem("Asset Packager", nil, false)
            if pressed then
                if mbm.is('windows') then
                    os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/editors.html#asset-packager"')
                elseif mbm.is('linux') then
                    os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/editors.html#asset-packager"')
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
                tImGui.TextDisabled(string.format('%s\nSQLITE: %s \nIMGUI: %s', mbm.get('version'),sqlite3.version(),tImGui.GetVersion()))
                tImGui.EndMenu()
            end
            
            tImGui.EndMenu()
        end

        tImGui.EndMainMenuBar()
    end
end

function loop(delta)
    main_menu_asset()
    showAssets()
    showCurrentSelection()
    tUtil.showOverlayMessage()
end