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

   Sprite Maker Editor

   This is a script based on mbm engine.

   Sprite Maker meant to make animations based on image(s).

   More info at: https://mbm-documentation.readthedocs.io/en/latest/editors.html#sprite-maker

]]--

tImGui        =     require "ImGui"
tUtil         =     require "editor_utils"

if not mbm.get('USE_EDITOR_FEATURES') then
	mbm.messageBox('Missing features','Is necessary to compile using USE_EDITOR FEATURES to run this editor','ok','error',0)
	mbm.quit()
end


function onInitScene()
    
    tWindowsTitle        = {title_image_selector    = 'Image(s) selector', 
                            title_add_frame         = 'Add Frame',
                            title_frame_edit        = 'Frame Edit',
                            title_frame_list        = 'Frame List',
                            title_frame_preview     = 'Frame Preview',
                            title_animation         = 'Animation',
                            title_edit_physics      = 'Edit Physics',
                            title_advanced_options  = 'Advanced Options For Binary Sprite'}

    camera2d		     = mbm.getCamera("2d")
    tLineSprite          = line:new("2dw",0,0)
    tLineCenterX         = line:new("2dw",0,0,50)
    tLineCenterY         = line:new("2dw",0,0,50)
    tLineCenterX:add({-9999999,0, 9999999,0})
    tLineCenterY:add({0,-9999999, 0,9999999})
    tLineSprite:add({0,-9999999, 0,9999999})
    tLineSprite:setColor(0,0,0)
    tLineSprite.visible = false
    tLineCenterX:setColor(1,0,0)
    tLineCenterY:setColor(0,1,0)
    bCollapseAllBut      = nil
    sLastSpriteOpenned   = ''
    sLastTextureOpenned  = ''
    sLastEditorFileName  = ''
    bTextureViewOpened   = false
    bShowAnimationEdit   = false
    bShowEditPhysics     = false
    iNumNickName         = 0
    keyControlPressed    = false
    bEnableMoveWindow    = false
    tPreviewFrameColor   = {r=1,g=1,b=1,a=1}
    ImGuiWindowFlags_NoMove = tImGui.Flags('ImGuiWindowFlags_NoMove')
    iStep_zoom           = 10
    tTexturesToEditor    = {}
    tUtil.sMessageOverlay= 'Welcome to Sprite Maker!\n\nFirst add an image from menu!'

    bShowAdvancedOptions = false

    tSaveBinaryOptions = {
                            tModeDrawList        = {'TRIANGLES','TRIANGLE_STRIP','TRIANGLE_FAN','LINES','LINE_LOOP','LINE_STRIP','POINTS'},
                            tCullFaceList        = {'FRONT', 'BACK', 'FRONT_AND_BACK'},
                            tFrontFaceList       = {'CW', 'CCW'},
                            indexFrontFace       = 1,
                            indexModeDraw        = 1,
                            indexCullFace        = 2,
                            stride               = 2,
                            indexStride          = 1} 

    tFrameAddOptions     =  {iIndexPrimitiveType = 1, 
                            iNumElements = 2,
                            bShowAddFrame = false,
                            bShowFramePreview = false,
                            iMinNumElements = 1, 
                            iMaxNumElements = 999,
                            iNumLines = 1,
                            iNumColumn = 1,
                            iMarginX = 0,
                            iMarginY = 0,
                            iSpacingx = 0,
                            iSpacingy = 0,
                            bEditVertex = false,
                            iSizeFrameWidth = 100,
                            iSizeFrameHeight = 100,
                            tFramesEnableSpriteSheet = {},
                            iFramesEnableSpriteSheetHover = 0,
                            bInvertUFrameOptions = false,
                            bInvertVFrameOptions = false,
                            tSelectedTexture = nil,
                            bStretch = false,
                            tShapeEdit = nil, --edit vertex
                            bAddAsSubset=false,
                            iIndexSelectedNode = 0}

    tFrameList           = {}

    tAnimationList       = {}

    tPhysicsOptions      = {
                            iIndexPrimitiveType = 1,
                            tScalePhysics       = {sx = 1, sy = 1},
                            tLinesPhysics       = {}
                            }

    tPhysicsOptions.reset = function(self)
        self.iIndexPrimitiveType = 1
    end

    tFrameAddOptions.reset = function(self)
        self.tSelectedTexture     = nil
        self.iIndexPrimitiveType  = 1
        self.iNumElements         = 2
        self.iMinNumElements      = 2
        self.bShowAddFrame        = false
        self.bValidFrameSelected  = false
        self.bShowFramePreview    = false
        self.bAddAsSubset         = false
        self.iIndexSelectedNode   = 0
        self.iNumLines            = 1
        self.iNumColumn           = 1
        self.iMarginX             = 0
        self.iMarginY             = 0
        self.iSpacingx            = 0
        self.iSpacingy            = 0
        self.iSizeFrameWidth      = 100
        self.iSizeFrameHeight     = 100
        self.bStretch             = false
        self.bInvertUFrameOptions  = false
        self.bInvertVFrameOptions  = false
        self.bEditVertex          = false
        self.tShapeEdit           = nil
        self.tFramesEnableSpriteSheet = {}
        self.iFramesEnableSpriteSheetHover = 0
    end

    tAnimationOptions = {   iFrameStart         = 1,
                            iFrameStop          = 1,
                            tUvZoom             = {uv0 = {x=0,y=1},uv1 = {x=1,y=0}},
                            iTypeAnim           = 3,
                            tRenders            = {},
                            tShapeAnimations    = {},
                            tScaleAnim          = {sx = 1, sy = 1},
                            iCurrentFrame       = 1,
                            bDirectionGrowing   = true,
                            iTimeAnimSimulation = 0,
                            fTimeFrame          = 0.3,
                            tDynamicAnims       = {},
                            sNameAnim           = 'No name',
                            tAnimTypes          = {'PAUSED','GROWING','GROWING_LOOP', 'DECREASING', 'DECREASING_LOOP', 'RECURSIVE', 'RECURSIVE_LOOP'}
                        }
                            

    tAnimationOptions.reset = function (self)
        self.iCurrentFrame       = self.iFrameStart
        self.bDirectionGrowing   = true
        self.iTimeAnimSimulation = 0
        self.iTypeAnim           = 3
        self.fTimeFrame          = 0.3
        self.sNameAnim           = 'No name'
        for k,v in pairs(self.tRenders) do
            v:clear()
        end
        self:disableTextureAnimations()
    end

    tAnimationOptions.disableTextureAnimations = function(self)
        for k,v in pairs(self.tShapeAnimations) do
            v.visible = false
        end
        for k,v in pairs(self.tDynamicAnims) do
            v:clear()
        end
        
    end

    tAnimationOptions.updateAnimation = function (self,delta)
        self.iTimeAnimSimulation = self.iTimeAnimSimulation + delta
        if self.iTimeAnimSimulation >= self.fTimeFrame then
            self.iTimeAnimSimulation = 0
            if self.tAnimTypes[self.iTypeAnim] == 'PAUSED' then
                self.iCurrentFrame = self.iFrameStart
            elseif self.tAnimTypes[self.iTypeAnim] == 'GROWING' then
                self.iCurrentFrame = self.iCurrentFrame + 1
                if self.iCurrentFrame > self.iFrameStop then
                    self.iCurrentFrame = self.iFrameStop
                end
            elseif self.tAnimTypes[self.iTypeAnim] == 'GROWING_LOOP' then
                self.iCurrentFrame = self.iCurrentFrame + 1
                if self.iCurrentFrame > self.iFrameStop then
                    self.iCurrentFrame = self.iFrameStart
                end
            elseif self.tAnimTypes[self.iTypeAnim] == 'DECREASING' then
                self.iCurrentFrame = self.iCurrentFrame - 1
                if self.iCurrentFrame < self.iFrameStart then
                    self.iCurrentFrame = self.iFrameStart
                end
            elseif self.tAnimTypes[self.iTypeAnim] == 'DECREASING_LOOP' then
                self.iCurrentFrame = self.iCurrentFrame - 1
                if self.iCurrentFrame < self.iFrameStart then
                    self.iCurrentFrame = self.iFrameStop
                end
            elseif self.tAnimTypes[self.iTypeAnim] == 'RECURSIVE' then
                if self.bDirectionGrowing then
                    self.iCurrentFrame = self.iCurrentFrame + 1
                    if self.iCurrentFrame > self.iFrameStop then
                        self.iCurrentFrame = self.iFrameStop - 1
                        if self.iCurrentFrame < self.iFrameStart then
                            self.iCurrentFrame = self.iFrameStart
                        end
                        self.bDirectionGrowing = false
                    end
                else
                    self.iCurrentFrame = self.iCurrentFrame - 1
                    if self.iCurrentFrame < self.iFrameStart then
                        self.iCurrentFrame = self.iFrameStart
                    end
                end
            elseif self.tAnimTypes[self.iTypeAnim] == 'RECURSIVE_LOOP' then
                if self.bDirectionGrowing then
                    self.iCurrentFrame = self.iCurrentFrame + 1
                    if self.iCurrentFrame > self.iFrameStop then
                        self.iCurrentFrame = self.iFrameStop - 1
                        if self.iCurrentFrame < self.iFrameStart then
                            self.iCurrentFrame = self.iFrameStart
                        end
                        self.bDirectionGrowing = false
                    end
                else
                    self.iCurrentFrame = self.iCurrentFrame - 1
                    if self.iCurrentFrame < self.iFrameStart then
                        self.bDirectionGrowing = true
                        self.iCurrentFrame = self.iFrameStart + 1
                        if self.iCurrentFrame > self.iFrameStop then
                            self.iCurrentFrame = self.iFrameStart
                        end
                    end
                end
            end
        end
        return self.iCurrentFrame
    end

    
    tAnimationOptions.restartAnim = function (self)
        
        if self.tAnimTypes[self.iTypeAnim] == 'PAUSED' then
            self.iCurrentFrame       = self.iFrameStart
        elseif self.tAnimTypes[self.iTypeAnim] == 'GROWING' then
            self.iCurrentFrame       = self.iFrameStart
        elseif self.tAnimTypes[self.iTypeAnim] == 'GROWING_LOOP' then
            self.iCurrentFrame       = self.iFrameStart
        elseif self.tAnimTypes[self.iTypeAnim] == 'DECREASING' then
            self.iCurrentFrame       = self.iFrameStop
        elseif self.tAnimTypes[self.iTypeAnim] == 'DECREASING_LOOP' then
            self.iCurrentFrame       = self.iFrameStop
        elseif self.tAnimTypes[self.iTypeAnim] == 'RECURSIVE' then
            self.iCurrentFrame       = self.iFrameStart
        elseif self.tAnimTypes[self.iTypeAnim] == 'RECURSIVE_LOOP' then
            self.iCurrentFrame       = self.iFrameStart
        end

        self.bDirectionGrowing   = true
        self.iTimeAnimSimulation = 0
    end
    
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

     bEnableMoveWorld = false

     tPivotShape = shape:new('2dw')
     tPivotShape:create('circle',20,20)
     tPivotShape:setColor(1,1,0.5,1)
     tPivotShape.visible = false

     tUVShape = shape:new('2dw')
     tUVShape:create('circle',10,10)
     tUVShape:setColor(1,0,1,1)
     tUVShape.visible = false

     tUvLine = line:new('2dw')
     tUvLine.visible = false
     tUvLine:setColor(1,0,1,1)
end

function hasTextureSelected(tTexturesIn)
    for i=1, #tTexturesIn do
        if tTexturesIn[i].isSelected then
            return true
        end
    end
    return false
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

function onNewSprite()
    for i=1, #tFrameList do
        local tFrame = tFrameList[i]
        tFrame.tShape:destroy()
        for j=1, #tFrame.tSubsetList do
            local tSubset = tFrame.tSubsetList[j]
            tSubset.tShape:destroy()
        end
    end
    tFrameList = {}
    tTexturesToEditor    = {}
    tAnimationList = {}
    camera2d:setPos(0,0)
    tFrameAddOptions:reset()
    tAnimationOptions:reset()
    tPivotShape.visible = false
    tex_alpha_pattern.visible = false
    bEnableMoveWorld = false
    tUVShape.visible = false
    tUvLine.visible = false
    bShowAddFrameOnceWhenSelectedTexture = nil
    tFrameAddOptions.bShowAddFrame = false
    bShowFrameList = false
    bShowFrameEdit = false
    tFrameAddOptions.bShowFramePreview = false
    bShowAnimationEdit = false
    closePhysicsWindow()
    bTextureViewOpened = false
    tLineSprite.visible = false

    tSaveBinaryOptions.indexFrontFace       = 1
    tSaveBinaryOptions.indexModeDraw        = 1
    tSaveBinaryOptions.indexCullFace        = 2
    tSaveBinaryOptions.stride               = 2
    tSaveBinaryOptions.indexStride          = 1
end

function onSaveUserData(name,value,tOut)
    if name:match('tFrameList.*tShape') then
        --print('info',name)
        table.insert(tOut,name .. ' = shape:new(\'2dw\')')
        table.insert(tOut,name .. '.bFirstRender =  true')
        local vertex            = name .. '.vertex'
        local index_read_only   = name .. '.index_read_only'
        local uv                = name .. '.uv'
        local uv_bkp            = name .. '.uv_bkp'
        local normal            = name .. '.normal'
        local index_buffer_edit = name .. '.index_buffer_edit'
        
        tUtil.save(uv,                 value.uv,                tOut, onSaveUserData)
        tUtil.save(uv_bkp,             value.uv_bkp,            tOut, onSaveUserData)
        tUtil.save(vertex,             value.vertex,            tOut, onSaveUserData)
        tUtil.save(normal,             value.normal,            tOut, onSaveUserData)
        tUtil.save(index_read_only,    value.index_read_only,   tOut, onSaveUserData)
        tUtil.save(index_buffer_edit,  value.index_buffer_edit, tOut, onSaveUserData)
        
        table.insert(tOut,string.format('%s:createDynamicIndexed(rawVertex(%s),%s,rawUv(%s),nil,%s)',name,vertex,index_read_only,uv,'getUniqueNickName()'))
        local s,e                  = name:match('^().*%]%[()')
        local tFrameListAtIndex    = name:sub(s,e)
        local sCommand             = string.format('return %stTexture"]["file_name"]', tFrameListAtIndex ) -- return tFrameList[1]["tTexture"]["file_name"]
        local sTextureNameCommand  = load(sCommand)

        if value.tVertexEdited then
            local tVertexEdited       = name .. '.tVertexEdited'
            tUtil.save(tVertexEdited, value.tVertexEdited,              tOut, onSaveUserData)
        end

        table.insert(tOut,string.format('%s:setTexture(%q)', name,sTextureNameCommand()))
        table.insert(tOut,string.format('%s:onRender(onRenderShape)',name))
    else
        print('warn', 'Userdata not expected',name)
    end
end

function onSaveEditionSprite(sFileName)

    local fp = io.open(sFileName,"w")
    if fp then
        local tLinesEditor = {}
        tUtil.save('tTexturesToEditor',tTexturesToEditor,  tLinesEditor,  onSaveUserData)
        for i =1, #tLinesEditor do
            local sLine = tLinesEditor[i]
            local s,e   = sLine:match('^()tTexturesToEditor%[%d+()%]%["id"%]') -- replace id for loadTexture in runtime
            if s and e then
                local sIdTexture           = sLine:sub(s,e)
                local sCommand             = string.format('return %s["file_name"]', sIdTexture ) -- return tTexturesToEditor[1]["file_name"]
                local sTextureNameCommand  = load(sCommand)
                tLinesEditor[i]            = string.format('%s["id"] = select(3,mbm.loadTexture(%q))',sIdTexture,sTextureNameCommand())
            end
        end

        local tLinesFrameList = {}
        tUtil.save('tFrameList',       tFrameList,         tLinesFrameList,  onSaveUserData)
        for i =1, #tLinesFrameList do
            local sLine = tLinesFrameList[i]
            local s,e   = sLine:match('^()tFrameList%[%d+%]%["tTexture"()%]%["id"%]') -- replace id for loadTexture in runtime
            if s and e then
                local sIdTexture           = sLine:sub(s,e)
                local sCommand             = string.format('return %s["file_name"]', sIdTexture ) -- return tTexturesToEditor[1]["file_name"]
                local sTextureNameCommand  = load(sCommand)
                tLinesFrameList[i]         = string.format('%s["id"] = select(3,mbm.loadTexture(%q))',sIdTexture,sTextureNameCommand())
            end
        end
        
        local tLinesAnimationList = {}
        tUtil.save('tAnimationList',       tAnimationList,         tLinesAnimationList,  onSaveUserData)

        local tLinestPreviewFrameColor = {}
        tUtil.save('tPreviewFrameColor',   tPreviewFrameColor,     tLinestPreviewFrameColor,  onSaveUserData)
        
        local sUtilEditor = [[
-- Generated Using Sprite Maker Version 2.0 + mbm engine
-- https://mbm-documentation.readthedocs.io/en/latest/editors.html#sprite-maker

local function rawVertex(vertex)
    local tVertex = {}
    for i=1, #vertex do
        table.insert(tVertex,vertex[i].x)
        table.insert(tVertex,vertex[i].y)
    end
    return tVertex
end

local function rawUv(uv)
    local tUv = {}
    for i=1, #uv do
        table.insert(tUv,uv[i].u)
        table.insert(tUv,uv[i].v)
    end
    return tUv
end

]]
        fp:write(sUtilEditor .. '\n')
        for i =1, #tLinesEditor do
            local sLine = tLinesEditor[i]
            fp:write(sLine .. '\n')
        end
        for i =1, #tLinesFrameList do
            local sLine = tLinesFrameList[i]
            fp:write(sLine .. '\n')
        end
        for i =1, #tLinesAnimationList do
            local sLine = tLinesAnimationList[i]
            fp:write(sLine .. '\n')
        end

        for i =1, #tLinestPreviewFrameColor do
            local sLine = tLinestPreviewFrameColor[i]
            fp:write(sLine .. '\n')
        end
        
        fp:close()
        return true
    else
        print('error',string.format('Could not open the file [%s] for write',sFileName))
        tUtil.showMessageWarn(string.format('Could not open the file [%s] for write',sFileName))
        return false
    end
end

function onSaveSprite(fileName)
    if #tFrameList == 0 then
        tUtil.showMessageWarn('There Is No Frame To Save The Sprite!')
        return false
    end

    local function makeVertex(vertex, normal, uv, pivot, stride)
        local vertex_array = {}
        for i=1, #vertex do
            local single_vertex = { x  = vertex[i].x - pivot.x,
                                    y  = vertex[i].y - pivot.y,
                                    nx = normal[i].nx,
                                    ny = normal[i].ny,
                                    nz = normal[i].nz,
                                    u  = uv[i].u,
                                    v  = uv[i].v,
                                }
            if stride == 3 then
                single_vertex.z = vertex[i].z
            end
            table.insert(vertex_array,single_vertex)
        end
        return vertex_array
    end

    --meshDebug is used to create dynamically mesh in the engine.
    --For sprite it has to have at least one frame to be able to generate the sprite
    local stride      = tSaveBinaryOptions.stride --stride only can be 3 or 2. it means (x,y,z) or (x,y)
    local tMesh       = meshDebug:new() --new mesh debug to store the information about our sprite
    
    --First we must add the frames
    for i = 1, #tFrameList do
        local tFrame      = tFrameList[i]
        local tShape      = tFrame.tShape
        local indexFrame  = tMesh:addFrame(stride) -- Add one frame with stride 2 (x,y) or 3 (x,y,z)

        --To add vertex, first we need to add a subset
        local indexSubset = tMesh:addSubSet(indexFrame) --add one subset for the first frame
        --we are adding vertex to frame (next)
        --this vertex list has to have at least 3 vertex (one triangle) to be valid
        -- The table expected is : {{x,y,z,u,v,nx,ny,nz},{x,y,z,u,v,nx,ny,nz},{x,y,z,u,v,nx,ny,nz}, ...}
        local vertex_array = makeVertex(tShape.vertex,tShape.normal,tShape.uv,tFrame.tPivot,stride)
        if not tMesh:addVertex(indexFrame,indexSubset,vertex_array) then
            print('error', "Error on add vertex buffer",i)
            tUtil.showMessageWarn("Error on add vertex buffer")
            return false
        end

        for j=1, #tFrame.tSubsetList do
            --Add each subset
            indexSubset        = tMesh:addSubSet(indexFrame) --add one subset
            local tSubSet      = tFrame.tSubsetList[j]
            tShape             = tSubSet.tShape
            vertex_array       = makeVertex(tShape.vertex,tShape.normal,tShape.uv,tSubSet.tPivot,stride)
            if not tMesh:addVertex(indexFrame,indexSubset,vertex_array) then
                print('error', "Error on add subset to vertex buffer",i,j)
                tUtil.showMessageWarn("Error on add subset to vertex buffer")
                return false
            end
        end
    end
    
    --Then we add the index buffer and set the texture
    for indexFrame = 1, #tFrameList do
        local tFrame = tFrameList[indexFrame]
        local indexSubset = 1
        if not tMesh:addIndex(indexFrame,indexSubset,tFrame.tShape.index_buffer_edit) then 
            print('error', "Error on add index buffer",i)
            tUtil.showMessageWarn("Error on add index buffer")
            return false
        end
        local tTexture = tFrame.tTexture
        --apply the texture to frame
        if not tMesh:setTexture(indexFrame,indexSubset,tTexture.base_file_name) then
            print('error', "Error on set texture!")
            tUtil.showMessageWarn("Error on set texture!")
            return false
        end

        for j=1, #tFrame.tSubsetList do
            indexSubset = j + 1
            local tSubsetFrame = tFrame.tSubsetList[j]
            tTexture = tSubsetFrame.tTexture

            if not tMesh:addIndex(indexFrame,indexSubset,tSubsetFrame.tShape.index_buffer_edit) then 
                print('error', "Error on add index buffer",i)
                tUtil.showMessageWarn("Error on add index buffer")
                return false
            end

            --apply the texture to subset
            if not tMesh:setTexture(indexFrame,indexSubset,tTexture.base_file_name) then
                print('error', "Error on set texture to subset!",i,j)
                tUtil.showMessageWarn("Error on set texture to subset!")
                return false
            end
        end
    end

    tMesh:setType('sprite')  -- set it to sprite type
    tMesh:setModeCullFace(tSaveBinaryOptions.tCullFaceList[tSaveBinaryOptions.indexCullFace])    --{'FRONT', 'BACK', 'FRONT_AND_BACK'}
    tMesh:setModeDraw(tSaveBinaryOptions.tModeDrawList[tSaveBinaryOptions.indexModeDraw])        --{'TRIANGLES','TRIANGLE_STRIP','TRIANGLE_FAN','LINES','LINE_LOOP','LINE_STRIP','POINTS'}
    tMesh:setModeFrontFace(tSaveBinaryOptions.tFrontFaceList[tSaveBinaryOptions.indexFrontFace]) --{'CW', 'CCW'}
    
    local tAnimTypesEngine = {
                        mbm.PAUSED,
                        mbm.GROWING,
                        mbm.GROWING_LOOP,
                        mbm.DECREASING,
                        mbm.DECREASING_LOOP,
                        mbm.RECURSIVE,
                        mbm.RECURSIVE_LOOP,
                        }
    --animation
    for i=1, #tAnimationList do
        local tAnim            = tAnimationList[i]
        if isAnimationValid(tAnim) then
            tMesh:addAnim(tAnim.sNameAnim,tAnim.iFrameStart,tAnim.iFrameStop,tAnim.fTimeFrame,tAnimTypesEngine[tAnim.iTypeAnim])
        else
            print('Skipped invalid animation(' .. tostring(i) .. '):' .. tAnim.sNameAnim)
        end
    end

    --set physics
    local tPhysics = {}
    for i=1, #tPhysicsOptions.tLinesPhysics do
        table.insert(tPhysics,tPhysicsOptions.tLinesPhysics[i].tPhysic)
    end
    tMesh:setPhysics(tPhysics)

    local calcNormal,calcUv = false,false --Instruct to do not calculate normal and UV
    if tMesh:save(fileName,calcNormal,calcUv) then
        tUtil.showMessage("Sprite Saved successfully ")
        return true
    else
        print("Failed to Save Sprite!")
        tUtil.showMessageWarn("Failed to Save Sprite!")
        return false
    end
end


function main_menu_sprite()
    if (tImGui.BeginMainMenuBar()) then
        if tImGui.BeginMenu("File") then
            
            local pressed,checked = tImGui.MenuItem("New Sprite", "Ctrl+N", false)
            if pressed then
                onNewSpriteEditor()
            end

            local pressed,checked = tImGui.MenuItem("Load Sprite", "Ctrl+O", false)
            if pressed then
                onOpenSprite()
            end
            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("Save Editor", "Ctrl+S", false)
            if pressed then
                onSaveSpriteEditor()
            end

            local pressed,checked = tImGui.MenuItem("Save Editor As", nil, false)
            if pressed then
                sLastEditorFileName = ''
                onSaveSpriteEditor()
            end

            local pressed,checked = tImGui.MenuItem("Save Sprite", "Ctrl+B", false)
            if pressed then
                onSaveSpriteBinary()
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
            local pressed,checked = tImGui.MenuItem("Add images from folder", nil, false)
            if pressed then
                tPivotShape.visible = false
                local file_name = mbm.openFolder(sLastTextureOpenned)
                if file_name then
                    tTexturesToEditor = tUtil.loadInfoImagesFromFolderToTable(file_name,tTexturesToEditor)
                    if type(file_name) == 'string' then
                        sLastTextureOpenned = file_name
                    elseif type(file_name) == 'table' and #file_name > 0 then
                        sLastTextureOpenned = file_name[1]
                    end
                    bTextureViewOpened = true
                    unCollapse(tWindowsTitle.title_image_selector)
                end
            end
            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("View Image List", nil, false)
            if pressed then
                bTextureViewOpened = checked
                tPivotShape.visible = false
                unCollapse(tWindowsTitle.title_image_selector)
            end
            tImGui.EndMenu()
        end
        if tImGui.BeginMenu("Frame") then
            local pressed,checked = tImGui.MenuItem("Add frame", 'Ctrl+F', false)
            if pressed then
                onShowAddFrame()
            end
            tImGui.Separator()
            local pressed,checked = tImGui.MenuItem("View Frame List", nil, false)
            if pressed then
                closePhysicsWindow()
                bShowFrameList = true
                unCollapse(tWindowsTitle.title_frame_list)
            end

            local pressed,checked = tImGui.MenuItem("View Frame Edit", nil, false)
            if pressed then
                closeAnimationWindow()
                bShowFrameEdit     = true
                if tFrameList.indexSelectedFrameNode and 
                   tFrameList.indexSelectedFrameNode <= #tFrameList and
                   tFrameList.indexSelectedFrameNode > 0 then
                    closePhysicsWindow()
                    setAllFrameAsNotVisible()
                    collapseAllBut(tWindowsTitle.title_frame_edit)
                end
            end
            tImGui.Separator()
            if tImGui.BeginMenu("Frame Preview Background Color") then
                local sz        = tImGui.GetTextLineHeight()
                
                local rounding  =  0
                local flags     =  0

                local colors    = { {'White',      {r=1,g=1,b=1,a=1}},
                                    {'Black',      {r=0,g=0,b=0,a=1}},
                                    {'Red',        {r=1,g=0,b=0,a=1}},
                                    {'Green',      {r=0,g=1,b=0,a=1}},
                                    {'Blue',       {r=0,g=0,b=1,a=1}},
                                    {'Cyan',       {r=0,g=1,b=1,a=1}},
                                    {'Yellow',     {r=1,g=1,b=0,a=1}},
                                    {'Magenta',    {r=1,g=0,b=1,a=1}},
                                    {'Transparent',{r=0,g=0,b=0,a=0}},
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
                        tPreviewFrameColor = color
                    end
                end
                tImGui.EndMenu()
            end

            tImGui.EndMenu()
        end

        if tImGui.BeginMenu("Animation") then
            local pressed,checked = tImGui.MenuItem("Edit Animations", 'Ctrl+A', false)
            if pressed then
                onEditAnimations()
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu("Physics") then
            local pressed,checked = tImGui.MenuItem("Edit Physics", 'Ctrl+P', false)
            if pressed then
                onEditPhysics()
            end
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
            local pressed,checked = tImGui.MenuItem("Advanced Options For Binary Sprite", nil, false)
            if pressed then
                bShowAdvancedOptions = true
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu("About") then
            local pressed,checked = tImGui.MenuItem("Sprite Maker", nil, false)
            if pressed then
                if mbm.is('windows') then
                    os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/editors.html#sprite-maker"')
                elseif mbm.is('linux') then
                    os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/editors.html#sprite-maker"')
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

function getRangeUV(uv)
    local uMax = -999999999
    local vMax = -999999999
    local uMin =  999999999
    local vMin =  999999999
    for i=1, #uv do
        uMax = math.max(uMax,uv[i].u)
        vMax = math.max(vMax,uv[i].v)
        uMin = math.min(uMin,uv[i].u)
        vMin = math.min(vMin,uv[i].v)
    end
    return uMin, vMin, uMax, vMax
end

function onRenderShape(self,vertex,normal,uv,index_read_only)
    if self.mark_to_remove == true then
        self:destroy()
        --print(string.format('removed shape %s',self ))
        return nil
    end

    if self.bFirstRender then
        self.visible      = false --stop render
        self.bFirstRender = false
    end

    if self.tUvSpriteSheet then --uv calculated in the editor
        local xMin          = self.tUvSpriteSheet.xMin
        local xMax          = self.tUvSpriteSheet.xMax
        local yMin          = self.tUvSpriteSheet.yMin
        local yMax          = self.tUvSpriteSheet.yMax
        local textureWidth  = self.tUvSpriteSheet.textureWidth
        local textureHeight = self.tUvSpriteSheet.textureHeight
        local xRange        = xMax - xMin
        local yRange        = yMax - yMin
        self.tUvSpriteSheet = nil -- just once
        
        local uv_bkp = {}
        --print('New shape',xRange,yRange)
        for i=1, #uv do
            local u     = ((uv[i].u * xRange) + xMin) / textureWidth
            local v     = ((uv[i].v * yRange) + yMin) / textureHeight
            --print(uv[i].u,uv[i].v,u,v)
            uv[i].u     = u
            uv[i].v     = v
            uv_bkp[i]   = {u = u, v = v}
        end
        self.uv = uv
        self.uv_bkp = uv_bkp
    end

    if self.bInvertUFirst then
        self.bInvertUFirst = false
        local uMin, vMin, uMax, vMax = getRangeUV(uv)
        for i=1, #uv do
            if uv[i].u < uMax then
                uv[i].u = uMax - uv[i].u + uMin
            else
                uv[i].u = uMin
            end
        end
        self.uv = uv
    end

    if self.bInvertVFirst then
        self.bInvertVFirst = false
        local uMin, vMin, uMax, vMax = getRangeUV(uv)
        for i=1, #uv do
            if uv[i].v < vMax then
                uv[i].v = vMax - uv[i].v + vMin
            else
                uv[i].v = vMin
            end
        end
        self.uv = uv
    end
    
    if self.vertex == nil then
        self.vertex = vertex
    end
    if self.uv == nil then
        self.uv = uv
        local uv_bkp = {}
        for i=1, #uv do
            uv_bkp[i] = {}
            uv_bkp[i].u = uv[i].u
            uv_bkp[i].v = uv[i].v
        end
        self.uv_bkp = uv_bkp
    end
    if self.normal == nil then
        self.normal = normal
    end
    self.index_read_only = index_read_only
    if self.index_buffer_edit == nil then
        self.index_buffer_edit = {}
        for i=1, #index_read_only do
            self.index_buffer_edit[i] = index_read_only[i]
        end
    end
    return self.vertex, self.normal, self.uv
end

function setShapeToRender(tShape,tFrame)
    if tShape.visible == false then
        tShape.visible           = true
        tLineSprite.visible      = true
        tex_alpha_pattern.visible = true
        bEnableMoveWorld         = true
        tPivotShape.tPivot       = tFrame.tPivot
        tLineSprite.z            = tShape.z - 1
        tUvLine.z                = tShape.z - 1.5
        tUVShape.z               = tShape.z - 2
        tShape:onRender(onRenderShape)
    end
end

function setAllFrameAsNotVisible()
    for i=1, #tFrameList do
        local tFrame = tFrameList[i]
        tFrame.tShape.visible = false
        for j=1, #tFrame.tSubsetList do
            local tSubset = tFrame.tSubsetList[j]
            tSubset.tShape.visible = false
        end
    end
end

function getSelectedFrame()
    if tFrameList.indexSelectedFrameNode and 
      tFrameList.indexSelectedFrameNode <= #tFrameList and
      tFrameList.indexSelectedFrameNode > 0 then
        local tFrame = tFrameList[tFrameList.indexSelectedFrameNode]
        if tFrameList.iIndexSelectedSubsetNode and
            tFrameList.iIndexSelectedSubsetNode <= #tFrame.tSubsetList and
            tFrameList.iIndexSelectedSubsetNode > 0 then
                tFrame = tFrame.tSubsetList[tFrameList.iIndexSelectedSubsetNode]
        end
        return tFrame
    end
    return nil
end

function resizeShape(tFrame,width,height)
    local tShape           = tFrame.tShape
    local uv               = tShape.uv
    local uv_bkp           = tShape.uv_bkp
    tShape.vertex          = nil
    tShape.normal          = nil
    tShape.uv              = nil
    tShape.mark_to_remove  = true
    if tShape.tVertexEdited then
        tShape,width,height = newPrimitiveShape(tFrame.tTexture,tShape.tVertexEdited,width,height)
    else
        tShape              = newShape(tFrame.type,tFrame.tTexture,width,height,tFrame.iNumElements)
    end
    tFrame.width           = width
    tFrame.height          = height
    tFrame.tShape          = tShape
    tShape.uv              = uv -- copy old uv
    tShape.uv_bkp          = uv_bkp
    setShapeToRender(tShape,tFrame)
    return tShape
end

function showFrameEdit()
    local tShape = nil
    local tFrame = getSelectedFrame()
    if tFrame then

        tShape      = tFrame.tShape
        if tPivotShape.visible then
            tShape:setPos(0,0) --reset pos
            setShapeToRender(tShape,tFrame)
            for i=1, #tFrame.tSubsetList do
                local tSubset = tFrame.tSubsetList[i]
                tSubset.tShape.visible = true
                tSubset.tShape:onRender(nil)
                tSubset.tShape:setPos(-tSubset.tPivot.x + tFrame.tPivot.x,-tSubset.tPivot.y + tFrame.tPivot.y)
            end
        else
            tShape:setPos(-tFrame.tPivot.x,-tFrame.tPivot.y) -- position shape considering pivot
            setShapeToRender(tShape,tFrame)
            for i=1, #tFrame.tSubsetList do
                local tSubset = tFrame.tSubsetList[i]
                tSubset.tShape.visible = true
                tSubset.tShape:onRender(nil)
                tSubset.tShape:setPos(-tSubset.tPivot.x,-tSubset.tPivot.y)
            end
        end


        tLineSprite:drawBounding(tShape,false)
        local width        = 200
        local tSizeBtn     = {x=width - 20,y=0}
        local x_pos, y_pos = 200, 0
        tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_frame_edit,x_pos,y_pos,width)
        shouldICollapse(tWindowsTitle.title_frame_edit)
        local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_frame_edit, true, ImGuiWindowFlags_NoMove)
        if is_opened then
            local step       =  1.0
            local step_fast  =  100.0
            local format     = "%.3f"
            local flags      =  0

            if tFrameList.iIndexSelectedSubsetNode > 0 then
                tImGui.Text(string.format('Subset:%d ',tFrameList.iIndexSelectedSubsetNode))
                tImGui.Text(string.format('Frame:%d/%d',tFrameList.indexSelectedFrameNode,#tFrameList))
            else
                tImGui.Text(string.format('Frame:%d/%d',tFrameList.indexSelectedFrameNode,#tFrameList))
            end
            tImGui.Text(string.format('Elements:%d',tFrame.iNumElements))
            tImGui.Text(string.format('Subsets:%d',#tFrame.tSubsetList))
            tImGui.Text(string.format('Type:%s',tFrame.type))

            local result, fValue = tImGui.InputFloat('Width', tFrame.width , step, step_fast, format, flags)
            if result then
                if fValue > 0 then
                    tShape = resizeShape(tFrame,fValue,tFrame.height)
                end
            end
            local result, fValue = tImGui.InputFloat('Height', tFrame.height , step, step_fast, format, flags)
            if result then
                if fValue > 0 then
                    tShape = resizeShape(tFrame,tFrame.width,fValue)
                end
                tPivotShape.visible      = false
            end

            bEnableResizeOnScroll = tImGui.Checkbox('Resize using scroll',bEnableResizeOnScroll)
            if bEnableResizeOnScroll then
                
                local label    = 'speed'
                local v_speed  =  1.0
                local v_min    =  1
                local v_max    =  100
                local format   =  "%d resize"

                local dragged, iValue = tImGui.DragInt(label,iStep_zoom, v_speed, v_min, v_max, format)
                if dragged then
                    iStep_zoom = iValue
                end
            end
            if tImGui.Button('Reset Size', tSizeBtn) then
                tShape = resizeShape(tFrame,tFrame.tTexture.width,tFrame.tTexture.height)
            end

            tImGui.Separator()
            tImGui.Text('Pivot')
            tPivotShape.visible  = tImGui.Checkbox('Edit Pivot',tPivotShape.visible)
            local result, fValue = tImGui.InputFloat('X', tFrame.tPivot.x,step, step_fast, format, flags)
            if result then
                tFrame.tPivot.x = fValue
                tPivotShape.visible = true
            end
            local result, fValue = tImGui.InputFloat('Y', tFrame.tPivot.y,step, step_fast, format, flags)
            if result then
                tFrame.tPivot.y = fValue
                tPivotShape.visible = true
            end
            if tImGui.Button('Reset Pivot', tSizeBtn) then
                tFrame.tPivot.x = 0
                tFrame.tPivot.y = 0
            end

            tImGui.Separator()
            tPivotShape:setPos(tFrame.tPivot.x,tFrame.tPivot.y)
            tPivotShape.z = tShape.z - 2

            tImGui.Text('UV map')
            if tImGui.Button('Invert U', tSizeBtn) then
                if tShape.uv then
                    local uv = tShape.uv
                    local uMin, vMin, uMax, vMax = getRangeUV(uv)
                    for i=1, #uv do
                        if uv[i].u < uMax then
                            uv[i].u = uMax - uv[i].u + uMin
                        else
                            uv[i].u = uMin
                        end
                    end
                end
            end

            if tImGui.Button('Invert V', tSizeBtn) then
                if tShape.uv then
                    local uv = tShape.uv
                    local uMin, vMin, uMax, vMax = getRangeUV(uv)
                    for i=1, #uv do
                        if uv[i].v < vMax then
                            uv[i].v = vMax - uv[i].v + vMin
                        else
                            uv[i].v = vMin
                        end
                    end
                end
            end
            if tImGui.Button('Invert UV', tSizeBtn) then
                if tShape.uv then
                    local uv = tShape.uv
                    local uMin, vMin, uMax, vMax = getRangeUV(uv)
                    for i=1, #uv do
                        if uv[i].u < uMax then
                            uv[i].u = uMax - uv[i].u + uMin
                        else
                            uv[i].u = uMin
                        end
                        if uv[i].v < vMax then
                            uv[i].v = vMax - uv[i].v + vMin
                        else
                            uv[i].v = vMin
                        end
                    end
                end
            end

            if tImGui.Button('Reset UV', tSizeBtn) then
                if tShape.uv then
                    local uv     = tShape.uv
                    local uv_bkp = tShape.uv_bkp
                    for i=1, #uv do
                        uv[i].u = tShape.uv_bkp[i].u
                        uv[i].v = tShape.uv_bkp[i].v
                    end
                end
            end

            tImGui.Separator()
            local flag_node      = 0
            tUVShape.visible = false
            tUvLine.visible = false
            if tImGui.TreeNodeEx('Individual UVs',flag_node,'##Uvs') then
                local step       =  0.01
                local step_fast  =  0.1
                local format     = "%.3f"

                local function showUvs(tFrame,tShape,uv,index)
                    local tLines = {}
                    for i=1, #tShape.index_read_only, 3 do
                        local tTRiangle      = {}
                        local index_buffer_1 = tShape.index_read_only[i  ] -- one based
                        local index_buffer_2 = tShape.index_read_only[i+1] -- one based
                        local index_buffer_3 = tShape.index_read_only[i+2] -- one based
                        local px_1,   py_1   = tShape.vertex[index_buffer_1].x, tShape.vertex[index_buffer_1].y
                        local px_2,   py_2   = tShape.vertex[index_buffer_2].x, tShape.vertex[index_buffer_2].y
                        local px_3,   py_3   = tShape.vertex[index_buffer_3].x, tShape.vertex[index_buffer_3].y
                        
                        table.insert(tTRiangle,px_1)
                        table.insert(tTRiangle,py_1)
                        table.insert(tTRiangle,px_2)
                        table.insert(tTRiangle,py_2)
                        table.insert(tTRiangle,px_3)
                        table.insert(tTRiangle,py_3)
                        table.insert(tTRiangle,px_1)
                        table.insert(tTRiangle,py_1)
                        
                        table.insert(tLines,tTRiangle)
                    end
                    
                    if tUvLine:size() > #tLines then
                        local z = tUvLine.z
                        tUvLine:destroy()
                        tUvLine    = line:new('2dw')
                        tUvLine.z  = z
                    end

                    for i=1, #tLines do
                        if i > tUvLine:size() then
                            tUvLine:add(tLines[i])
                        else
                            tUvLine:set(tLines[i],i)
                        end
                    end
                    tUvLine:setColor(1,0,1,1)
                    local x,   y   = tShape.vertex[index].x, tShape.vertex[index].y
                    if tPivotShape.visible then
                        tUVShape:setPos(x,y)
                        tUvLine:setPos(0,0)
                    else
                        tUVShape:setPos(x - tFrame.tPivot.x ,y - tFrame.tPivot.y)
                        tUvLine:setPos(-tFrame.tPivot.x,-tFrame.tPivot.y)
                    end
                    tUVShape.visible = true
                    tUvLine.visible = true
                end

                if tShape.uv then
                    local uv = tShape.uv
                    for i=1, #uv do
                        local result, fValue = tImGui.InputFloat(string.format('U #%d',i), uv[i].u,step, step_fast, format, flags)
                        if result then
                            uv[i].u = fValue
                        end
                        if tShape.vertex and tImGui.IsItemHovered(0) then
                            showUvs(tFrame,tShape,uv,i)
                        end
                        local result, fValue = tImGui.InputFloat(string.format('V #%d',i), uv[i].v,step, step_fast, format, flags)
                        if result then
                            uv[i].v = fValue
                        end
                        if tShape.vertex and tImGui.IsItemHovered(0) then
                            showUvs(tFrame,tShape,uv,i)
                        end
                    end
                end
                tImGui.TreePop()
            end

            tImGui.Separator()
            tImGui.Text('Index Buffer Options')
            tImGui.SameLine()
            tImGui.HelpMarker('Advanced options for index buffer\n\nChange the index buffer here will not reflect in the preview.\n\nOnly when the sprite is saved as  binary that the new indexes will be replaced.')
            if tImGui.TreeNodeEx('Advanced (Size)',flag_node,'##AdvancedIndexBufferTree') then
                tImGui.Text('Resize')
                tPivotShape.visible      = false
                if clicked then
                    if iValue >= 3 and iValue <= 99999999 then
                        while #tShape.index_buffer_edit < iValue  do
                            table.insert(tShape.index_buffer_edit,tShape.index_buffer_edit[#tShape.index_buffer_edit])
                        end
                        while #tShape.index_buffer_edit > iValue  do
                            table.remove(tShape.index_buffer_edit,#tShape.index_buffer_edit)
                        end
                    end
                end
                
                if tImGui.Button('Reset Index Buffer', {x=0,y=0}) then
                    tShape.index_buffer_edit = {}
                    for i=1, #tShape.index_read_only do
                        tShape.index_buffer_edit[i] = tShape.index_read_only[i]
                    end 
                end
                tImGui.TreePop()
            end

            if tImGui.TreeNodeEx('Advanced (Edit Index)',flag_node,'##IndexBufferTree') then
                for i=1, #tShape.index_buffer_edit do
                    local sId = string.format('%d##indexBuffer_%d',i,i)
                    local clicked, iValue = tImGui.InputInt(sId, tShape.index_buffer_edit[i], 1, 1)
                    if clicked then
                        if iValue > 0 and iValue <= #tShape.vertex then
                            tShape.index_buffer_edit[i] = iValue
                        end
                    end
                end
                tImGui.TreePop()
            end
        end
        if closed_clicked then
            closeFrameEdit()
            unCollapseAll()
        end
        tImGui.End()
    else
        bShowFrameEdit = false
        tUtil.showMessageWarn('There is no frame selected!')
    end
end

function showFrameList()
    
    local width = 200
    local x_pos, y_pos = 0, 0
    local max_width = 200
    local tSizeBtn   = {x=width - 20,y=0} -- size button
    tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_frame_list,x_pos,y_pos,width,max_width)

    tAnimationOptions:disableTextureAnimations()
    bShowAnimationEdit = false
    
    local function onSelectedOtherFrame()
        bShowFrameEdit     = true
        
        if tFrameList.indexSelectedFrameNode and 
            tFrameList.indexSelectedFrameNode <= #tFrameList and
            tFrameList.indexSelectedFrameNode > 0 then
                collapseAllBut({tWindowsTitle.title_frame_edit,tWindowsTitle.title_frame_list})
                setAllFrameAsNotVisible()--force refresh calling setShapeToRender (not visible will reset the shape state)
                closeAnimationWindow()
                closePhysicsWindow()
        else
            bShowFrameEdit = false
        end
    end
    shouldICollapse(tWindowsTitle.title_frame_list)
    local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_frame_list, true, ImGuiWindowFlags_NoMove)
    if is_opened then
        if tFrameList.indexSelectedFrameNode ~= 0 and #tFrameList > 0 then
            if tImGui.IsWindowFocused(0) then
                if tImGui.IsKeyDown('ImGuiKey_DownArrow') then
                    if tFrameList.iIndexSelectedSubsetNode > 0 then --are we in the subset tree?
                        tFrameList.iIndexSelectedSubsetNode = tFrameList.iIndexSelectedSubsetNode + 1
                        local tFrame       = tFrameList[tFrameList.indexSelectedFrameNode]
                        if tFrameList.iIndexSelectedSubsetNode <= #tFrame.tSubsetList then
                            onSelectedOtherFrame()
                        else
                            tFrameList.iIndexSelectedSubsetNode = 0
                            if tFrameList.indexSelectedFrameNode < #tFrameList then
                                tFrameList.indexSelectedFrameNode = tFrameList.indexSelectedFrameNode + 1
                            end
                            onSelectedOtherFrame()
                        end
                    else
                        if tFrameList.indexSelectedFrameNode < #tFrameList then
                            tFrameList.indexSelectedFrameNode = tFrameList.indexSelectedFrameNode + 1
                            tFrameList.iIndexSelectedSubsetNode = 0
                            onSelectedOtherFrame()
                        end
                    end
                elseif tImGui.IsKeyDown('ImGuiKey_UpArrow') then
                    if tFrameList.iIndexSelectedSubsetNode > 0 then --are we in the subset tree?
                        tFrameList.iIndexSelectedSubsetNode = tFrameList.iIndexSelectedSubsetNode - 1
                        if tFrameList.iIndexSelectedSubsetNode > 0 then
                            onSelectedOtherFrame()
                        else
                            if tFrameList.indexSelectedFrameNode > 1 then
                                tFrameList.indexSelectedFrameNode = tFrameList.indexSelectedFrameNode - 1
                            end
                            onSelectedOtherFrame()
                        end
                    else
                        if tFrameList.indexSelectedFrameNode > 1 then
                            tFrameList.indexSelectedFrameNode = tFrameList.indexSelectedFrameNode - 1
                            tFrameList.iIndexSelectedSubsetNode = 0
                            onSelectedOtherFrame()
                        end
                    end
                end
            end
        end
        local label_frame    = string.format('Frames (%d)',#tFrameList)
        tImGui.Text(label_frame)
        local bUniquePopUpContext = true
        for i=1, #tFrameList do
            local tFrame       = tFrameList[i]
            local flag_node    = 0
            local id_nodeframe = string.format('##frame_%d',i)
            local frame_name   = string.format('frame %d',i)
            if tFrameList.indexSelectedFrameNode == i then
                flag_node  = tImGui.Flags('ImGuiTreeNodeFlags_Selected')
            end
            if tFrameList.indexSelectedFrameNodeToExpand == i then
                tFrameList.indexSelectedFrameNodeToExpand = nil
                tImGui.SetNextItemOpen(true)
            end
            local bIsOpen = tImGui.TreeNodeEx(frame_name,flag_node,id_nodeframe)
            if tImGui.IsItemClicked() then
                tFrameList.indexSelectedFrameNode = i
                tFrameList.iIndexSelectedSubsetNode = 0
                onSelectedOtherFrame()
            end
            if bUniquePopUpContext and tImGui.BeginPopupContextItem("Options Frame List") then
                bUniquePopUpContext = false
                if tImGui.Selectable("Duplicate frame") then
                    print('Duplicating not implemented')
                    
                end
                if tImGui.Selectable("Remove Frame") then
                    tFrame.tShape.mark_to_remove = true
                    table.remove(tFrameList,i)
                    tFrameList.iIndexSelectedSubsetNode = 0
                    if i > #tFrameList then
                        tFrameList.indexSelectedFrameNode = #tFrameList
                    end
                    if #tFrameList == 0 then
                        tFrameList.indexSelectedFrameNode = 0
                    end
                    onSelectedOtherFrame()
                    tImGui.EndPopup()
                    if bIsOpen then
                        tImGui.TreePop()
                    end
                    break
                end
                tImGui.EndPopup()
            end
            if bIsOpen then
                
                for j=1, #tFrame.tSubsetList do
                    local flags_subset = 0
                    local tSubset      = tFrame.tSubsetList[j]
                    local id_node      = string.format('##subset_%d_frame %d',j,i)
                    local subset_name  = string.format('subset %d',j)
                    if tFrameList.iIndexSelectedSubsetNode == j and tFrameList.indexSelectedFrameNode == i then
                        flags_subset  = tImGui.Flags('ImGuiTreeNodeFlags_Selected')
                    end
                    if tImGui.TreeNodeEx(subset_name,flags_subset,id_node) then
                        if tImGui.IsItemClicked() then
                            tFrameList.indexSelectedFrameNode = i
                            tFrameList.iIndexSelectedSubsetNode = j
                            onSelectedOtherFrame()
                        end
                        tImGui.TreePop()
                    else
                        if tImGui.IsItemClicked() then
                            tFrameList.indexSelectedFrameNode = i
                            tFrameList.iIndexSelectedSubsetNode = j
                            onSelectedOtherFrame()
                        end
                    end
                end
                tImGui.TreePop()
            end
        end

        if closed_clicked then
            bShowFrameList   = false
        end
    end
    tImGui.End()
end

function showFramePreview()
    
    local iW, iH       = mbm.getRealSizeScreen()
    local width        = iW - 400
    local x_pos, y_pos = 0, 0
    local max_width    = iW
    local tSizeBtn     = {x=width - 20,y=0} -- size button
    tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_frame_preview,x_pos,y_pos,width,max_width)
    tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_WindowBg'), tPreviewFrameColor)
    tImGui.PushStyleVar('ImGuiStyleVar_WindowMinSize',{x = 200, y= 200})

    shouldICollapse(tWindowsTitle.title_frame_preview)
    local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_frame_preview, true,tImGui.Flags('ImGuiWindowFlags_NoMove'))
    if is_opened and tFrameAddOptions.tSelectedTexture then
        local padding        = tImGui.GetStyle('DisplayWindowPadding')
        local sy_visible     = select(2,tImGui.IsScrollVisible())
        if sy_visible then
            padding.x = padding.x + tImGui.GetStyle('ScrollbarSize')
        end
        local tSizeWindow    = tImGui.GetWindowSize()
        local tTexture       = tFrameAddOptions.tSelectedTexture
        local new_width      = math.min(tTexture.width,tSizeWindow.x - padding.x)
        local sy             = new_width / tTexture.width  * tTexture.height
        local size           = {x=new_width,y=sy}
        local uv0            = {x=0,y=0}
        local uv1            = {x=1,y=1}
        local bg_col         = {r=1,g=1,b=1,a=1}
        local line_color     = {r=0,g=0,b=0,a=1}
        local tCursorPos     = tImGui.GetCursorPos()
        local color_rect     = {r=0,g=0,b=0,a=1.0}
        local thickness      = 1.5
        tImGui.Image(tTexture.id,size,uv0,uv1,bg_col,line_color)
        local winPos         = tImGui.GetWindowPos()
        local originImg      = {x= winPos.x + tCursorPos.x , y = winPos.y + tCursorPos.y - tImGui.GetScrollY()}
        
        if tFrameAddOptions.bEditVertex then
            if tFrameAddOptions.tShapeEdit then

                local color_frame = {r=0, g = 0, b=1, a=1}

                for i=1, #tFrameAddOptions.tShapeEdit.index, 3 do
                    local index_1 = tFrameAddOptions.tShapeEdit.index[i]   --one based (meshDebug)
                    local index_2 = tFrameAddOptions.tShapeEdit.index[i+1] --one based (meshDebug)
                    local index_3 = tFrameAddOptions.tShapeEdit.index[i+2] --one based (meshDebug)
                    local v1      = tFrameAddOptions.tShapeEdit.vertex[index_1]  
                    local v2      = tFrameAddOptions.tShapeEdit.vertex[index_2]
                    local v3      = tFrameAddOptions.tShapeEdit.vertex[index_3]

                    --scale to screen
                    local p1      = {x = v1.x / tTexture.width * size.x + originImg.x, y = v1.y / tTexture.height * size.y + originImg.y}
                    local p2      = {x = v2.x / tTexture.width * size.x + originImg.x, y = v2.y / tTexture.height * size.y + originImg.y}
                    local p3      = {x = v3.x / tTexture.width * size.x + originImg.x, y = v3.y / tTexture.height * size.y + originImg.y}

                    tImGui.AddLine(p1, p2, color_frame, thickness)
                    tImGui.AddLine(p2, p3, color_frame, thickness)
                    tImGui.AddLine(p3, p1, color_frame, thickness)
                end
                local radius        = 5
                local num_segments  = 12
                for i=1, #tFrameAddOptions.tShapeEdit.vertex do
                    local vertex        = tFrameAddOptions.tShapeEdit.vertex[i]  
                    local point         = { x = vertex.x / tTexture.width * size.x + originImg.x, y = vertex.y / tTexture.height * size.y + originImg.y}
                    local point_1       = { x = point.x - 10  , y = point.y - 10}
                    local point_2       = { x = point.x + 10  , y = point.y + 10}
                    if tImGui.IsMouseHoveringRect(point_1,point_2) or tFrameAddOptions.tShapeEdit.index_over_vertex_input == i then
                        local color_circle  = {r=1,g=0,b=0,a=1}
                        tImGui.AddCircleFilled(point, radius, color_circle, num_segments)
                        if tFrameAddOptions.tShapeEdit.index_over_vertex_input == 0 and tImGui.IsMouseClicked(0) then
                            tFrameAddOptions.tShapeEdit.index_mouse_clicked = i
                        end
                    else
                        local color_circle  = {r=0,g=0,b=1,a=1}
                        tImGui.AddCircleFilled(point, radius, color_circle, num_segments)
                    end
                end
                if tImGui.IsMouseReleased(0) then
                    tFrameAddOptions.tShapeEdit.index_mouse_clicked = nil
                end
                if tFrameAddOptions.tShapeEdit.index_mouse_clicked and tFrameAddOptions.tShapeEdit.index_mouse_clicked <= #tFrameAddOptions.tShapeEdit.vertex then
                    local vertex        = tFrameAddOptions.tShapeEdit.vertex[tFrameAddOptions.tShapeEdit.index_mouse_clicked]
                    local point         = tImGui.GetMousePos()
                    local calculated    = { x = point.x - originImg.x, y = point.y - originImg.y}
                    local new_vertex    = { x = calculated.x * tTexture.width / size.x, y = calculated.y * tTexture.height / size.y}
                    if new_vertex.x > tFrameAddOptions.tShapeEdit.max_size_x then
                        new_vertex.x = tFrameAddOptions.tShapeEdit.max_size_x
                    elseif new_vertex.x < 0 then
                        new_vertex.x = 0
                    end
                    if new_vertex.y > tFrameAddOptions.tShapeEdit.max_size_y then
                        new_vertex.y = tFrameAddOptions.tShapeEdit.max_size_y
                    elseif new_vertex.y < 0 then
                        new_vertex.y = 0
                    end
                    tFrameAddOptions.tShapeEdit.vertex[tFrameAddOptions.tShapeEdit.index_mouse_clicked] = new_vertex
                    local color_circle  = {r=1,g=1,b=0,a=1}
                    tImGui.AddCircleFilled(point, radius, color_circle, num_segments)
                end
            end
        elseif tFrameAddOptions.iIndexPrimitiveType == 1 then --rectangle
            
            local tRects     = calcRectForSpriteSheet(tTexture)
            if tRects then
            
                local function draw_X(a1,a2,a3,a4,color_x)
                    local thickness_x = 2
                    tImGui.AddLine(a1, a4, color_x, thickness_x)
                    tImGui.AddLine(a2, a3, color_x, thickness_x)
                end

                for i=1, #tRects do
                    local tRect = tRects[i]
                    local tMin = {x = tRect.tMin.x / tTexture.width * size.x + originImg.x, y = tRect.tMin.y / tTexture.height * size.y + originImg.y}
                    local tMax = {x = tRect.tMax.x / tTexture.width * size.x + originImg.x, y = tRect.tMax.y / tTexture.height * size.y + originImg.y}
                    local a1   = {x = tMin.x, y = tMin.y} -- left up
                    local a2   = {x = tMax.x, y = tMin.y} -- right up
                    local a3   = {x = tMin.x, y = tMax.y} -- left down
                    local a4   = {x = tMax.x, y = tMax.y} -- right down

                    tImGui.AddLine(a1, a2, color_rect, thickness)
                    tImGui.AddLine(a1, a3, color_rect, thickness)
                    tImGui.AddLine(a3, a4, color_rect, thickness)
                    tImGui.AddLine(a2, a4, color_rect, thickness)

                    if tFrameAddOptions.tFramesEnableSpriteSheet[i] == false then
                        local color_x    = {r=1,g=0,b=0,a=0.8}
                        draw_X(a1,a2,a3,a4,color_x)
                    end
                    if tImGui.IsWindowFocused(0) and tImGui.IsMouseHoveringRect(a1,a4) then
                        local color_x    = {r=1,g=0,b=0,a=0.8}
                        if tFrameAddOptions.tFramesEnableSpriteSheet[i] then
                            color_x      = {r=1,g=1,b=0,a=0.8}
                        end
                        draw_X(a1,a2,a3,a4,color_x)
                        if tImGui.IsItemClicked() then
                            tFrameAddOptions.tFramesEnableSpriteSheet[i] = not tFrameAddOptions.tFramesEnableSpriteSheet[i]
                        end
                    end
                    if tFrameAddOptions.iFramesEnableSpriteSheetHover > 0 and tFrameAddOptions.iFramesEnableSpriteSheetHover <= #tRects then
                        local tRectHover = tRects[tFrameAddOptions.iFramesEnableSpriteSheetHover]
                        local tMin = {x = tRectHover.tMin.x / tTexture.width * size.x + originImg.x, y = tRectHover.tMin.y / tTexture.height * size.y + originImg.y}
                        local tMax = {x = tRectHover.tMax.x / tTexture.width * size.x + originImg.x, y = tRectHover.tMax.y / tTexture.height * size.y + originImg.y}
                        local a1   = {x = tMin.x, y = tMin.y} -- left up
                        local a2   = {x = tMax.x, y = tMin.y} -- right up
                        local a3   = {x = tMin.x, y = tMax.y} -- left down
                        local a4   = {x = tMax.x, y = tMax.y} -- right down

                        local color_x    = {r=1,g=1,b=0,a=0.8}
                        draw_X(a1,a2,a3,a4,color_x)
                    end
                end
            end
        elseif tFrameAddOptions.iIndexPrimitiveType == 2 then --circle
            local center        = {x=winPos.x + (size.x * 0.5) + tCursorPos.x,y=winPos.y + (size.y * 0.5) + tCursorPos.y}
            local radius        = math.min(size.x,size.y) * 0.5
            tImGui.AddCircle(center, radius, color_rect, tFrameAddOptions.iNumElements, thickness)
        elseif tFrameAddOptions.iIndexPrimitiveType == 3 then --triangle
            local origin     = {x = winPos.x + tCursorPos.x, y = winPos.y + tCursorPos.y}
            local p1         = {x= origin.x          , y = origin.y }
            local p3         = {x= origin.x          , y = origin.y + size.y}
            local p4         = {x= origin.x + size.x , y = origin.y + size.y}
            local p5         = {x= origin.x + (size.x * 0.5) , y = origin.y }
            tImGui.AddLine(p3, p4, color_rect, thickness) --down
            tImGui.AddLine(p3, p5, color_rect, thickness) --left
            tImGui.AddLine(p4, p5, color_rect, thickness) --right
        end
    end
    tImGui.End()
    tImGui.PopStyleColor(1)
    tImGui.PopStyleVar(1)
    if closed_clicked then
        tFrameAddOptions.bShowFramePreview   = false
    end
end

function calcRectForSpriteSheet(tTexture)
    -- Calc rects for sprite sheet
    local size       = {x = tTexture.width , y = tTexture.height}
    local origin     = {x = tFrameAddOptions.iMarginX, y = tFrameAddOptions.iMarginY}
    local p1         = {x= origin.x                                       , y = origin.y }
    local p2         = {x= origin.x + size.x - tFrameAddOptions.iMarginX  , y = origin.y }
    local p3         = {x= origin.x                                       , y = origin.y + size.y - tFrameAddOptions.iMarginY}
    local p4         = {x= origin.x + size.x - tFrameAddOptions.iMarginX  , y = origin.y + size.y - tFrameAddOptions.iMarginY}

    local tColumns = {}
    local tLines   = {}

    if tFrameAddOptions.iNumColumn > 1 then
        local width_column = 1.0 / tFrameAddOptions.iNumColumn * size.x
        local a1 = {x = p1.x, y = p1.y}
        local a3 = {x = p3.x, y = p3.y}
        while a1.x  < p2.x do
            a1.x = a1.x + width_column -- + tFrameAddOptions.iSpacingx
            a3.x = a3.x + width_column -- + tFrameAddOptions.iSpacingx
            table.insert(tColumns,math.min(a1.x, p4.x))
        end
    end

    if #tColumns == 0 then
        table.insert(tColumns,p4.x)
    end

    if tFrameAddOptions.iNumLines > 1 then
        local height_lines = 1.0 / tFrameAddOptions.iNumLines * size.y
        while p1.y  < p4.y do
            p1.y = p1.y + height_lines -- + tFrameAddOptions.iSpacingy
            p2.y = p2.y + height_lines -- + tFrameAddOptions.iSpacingy
            table.insert(tLines,math.min(p4.y, p1.y))
        end
    end

    if #tLines == 0 then
        table.insert(tLines,p4.y)
    end

    local tRects = {}
    local x = origin.x
    local y = origin.y
    for i =1, #tColumns do
        
        for j =1, #tLines do
            local tRect = {}
            tRect.tMin = {x = x           , y = y         }
            tRect.tMax = {x = tColumns[i] , y = tLines[j] }
            --tImGui.Text(string.format('Min (%.2f %.2f) Max (%.2f %.2f)',tRect.tMin.x,tRect.tMin.y, tRect.tMax.x,tRect.tMax.y ))
            table.insert(tRects,tRect)
            y = tRect.tMax.y
        end
        x = tColumns[i]
        y = origin.y
    end

    for i=1, #tRects do
        local tRect = tRects[i]
        tRect.tMin.x = tRect.tMin.x + tFrameAddOptions.iSpacingx
        tRect.tMax.x = tRect.tMax.x - tFrameAddOptions.iSpacingx
        tRect.tMin.y = tRect.tMin.y + tFrameAddOptions.iSpacingy
        tRect.tMax.y = tRect.tMax.y - tFrameAddOptions.iSpacingy
    end

    local function indexInvalidRects(tRects)
        for i=1, #tRects do --sanity check
            local tRect = tRects[i]
            if tRect.tMin.x >= tRect.tMax.x then
                return i
            end
            if tRect.tMin.y >= tRect.tMax.y then
                return i
            end
        end
        return 0
    end

    local index_invalid = indexInvalidRects(tRects)
    while (index_invalid > 0) do
        table.remove(tRects,index_invalid)
        index_invalid = indexInvalidRects(tRects)
    end

    if #tRects == 0 then
        return nil
    end

    return tRects

end

function showFrameAdd()
    local width = 200
    local x_pos, y_pos = -200, 0
    local max_width = 200
    tUtil.setInitialWindowPositionRight(tWindowsTitle.title_add_frame,x_pos,y_pos,width,max_width)

    shouldICollapse(tWindowsTitle.title_add_frame)
    local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_add_frame, true, ImGuiWindowFlags_NoMove)
    if is_opened then
        tImGui.Text('Primitive type')
        local indexPrimitive = tImGui.RadioButton('Rectangle', tFrameAddOptions.iIndexPrimitiveType, 1)
        indexPrimitive       = tImGui.RadioButton('Circle',    indexPrimitive, 2)
        indexPrimitive       = tImGui.RadioButton('Triangle',  indexPrimitive, 3)
        local tSizeBtn       = {x=width - 20,y=0} -- size button
        
        local color             = {r=1,g=1,b=0.4,a=1}
        local thickness         =  5.0
        local winPos            = tImGui.GetCursorScreenPos()
        if indexPrimitive == 1 then
            local p_min             = {x = winPos.x + 75,  y = winPos.y + 15}
            local p_max             = {x = winPos.x + 125, y = winPos.y + 65}
            local rounding          =  2.0
            local rounding_corners  =  tImGui.Flags('ImDrawCornerFlags_All')
            tImGui.AddRect(p_min, p_max, color, rounding, rounding_corners, thickness)
            if tFrameAddOptions.iIndexPrimitiveType ~= indexPrimitive then
                tFrameAddOptions.iNumElements    = 2
                tFrameAddOptions.iMinNumElements = 2
                tFrameAddOptions.tShapeEdit      = nil
            end
        elseif indexPrimitive == 2 then
            local center        = {x=winPos.x + 100,y=winPos.y + 25 + 7.5}
            local radius        = 25
            if tFrameAddOptions.iIndexPrimitiveType ~= indexPrimitive then
                tFrameAddOptions.iNumElements    = 8
                tFrameAddOptions.iMinNumElements = 4
                tFrameAddOptions.tShapeEdit      = nil
            end
            tImGui.AddCircle(center, radius, color, math.max(tFrameAddOptions.iNumElements,4), thickness)
        elseif indexPrimitive == 3 then
            local p1     = {x=0 +  winPos.x + 75,y=50 + winPos.y + 15}
            local p2     = {x=25 + winPos.x + 75,y=0  + winPos.y + 15}
            local p3     = {x=50 + winPos.x + 75,y=50 + winPos.y + 15}
            tImGui.AddTriangle(p1, p2, p3, color, thickness + 5)
            if tFrameAddOptions.iIndexPrimitiveType ~= indexPrimitive then
                tFrameAddOptions.iNumElements    = 1
                tFrameAddOptions.iMinNumElements = 1
                tFrameAddOptions.tShapeEdit      = nil
            end
        end

        tFrameAddOptions.iIndexPrimitiveType = indexPrimitive

        local step       =  1
        local step_fast  =  10
        if tFrameAddOptions.iNumElements < tFrameAddOptions.iMinNumElements then
            tFrameAddOptions.iNumElements = tFrameAddOptions.iMinNumElements
        end
        if tFrameAddOptions.iNumElements > tFrameAddOptions.iMaxNumElements then
            tFrameAddOptions.iNumElements = tFrameAddOptions.iMaxNumElements
        end
        
        winPos.y = winPos.y + 100
        tImGui.SetCursorScreenPos(winPos)
        tImGui.Text('Number of Elements')
        local result, iValue = tImGui.InputInt('##NumberElements', tFrameAddOptions.iNumElements, step, step_fast)
        if result then
            if indexPrimitive == 1 then -- only even for rectangles
                if (iValue % 2) == 1 then
                    if iValue < tFrameAddOptions.iNumElements then
                        iValue = iValue - 1
                        if iValue < 2 then
                            iValue = 2
                        end
                    else
                        iValue = iValue + 1
                    end
                end
            end
            tFrameAddOptions.iNumElements = iValue
            tFrameAddOptions.tShapeEdit   = nil
        end

        tFrameAddOptions.bEditVertex = tImGui.Checkbox('Edit Primitive',tFrameAddOptions.bEditVertex)

        if tFrameAddOptions.bEditVertex then
            if tFrameAddOptions.tSelectedTexture then
                local bValidConf = false
                if indexPrimitive == 1 then
                    local tRects = calcRectForSpriteSheet(tFrameAddOptions.tSelectedTexture)
                    if tRects == nil then
                        tUtil.showMessageWarn('Invalid Primitive!',2.5)
                        tFrameAddOptions.bEditVertex = false
                    elseif #tRects == 0 then
                        tUtil.showMessageWarn('Zero primitive selected!',2.5)
                        tFrameAddOptions.bEditVertex = false
                    elseif #tRects > 1 then
                        tUtil.showMessageWarn('More then 1 primitive selected!\nEdit primitive disabled',2.5)
                        tFrameAddOptions.bEditVertex = false
                    else
                        bValidConf = true
                    end
                else
                    bValidConf = true
                end
                if bValidConf then
                    if tFrameAddOptions.tShapeEdit == nil then
                        local sPrimitiveType
                        local tTexture = tFrameAddOptions.tSelectedTexture
                        if tFrameAddOptions.iIndexPrimitiveType  == 1 then
                            sPrimitiveType = 'rectangle'
                        elseif tFrameAddOptions.iIndexPrimitiveType  == 2 then
                            sPrimitiveType = 'circle'
                        else
                            sPrimitiveType = 'triangle'
                        end
                        tFrameAddOptions.tShapeEdit = newShape(sPrimitiveType,tTexture,tTexture.width,tTexture.height,tFrameAddOptions.iNumElements)
                        tFrameAddOptions.tShapeEdit.visible = false -- just in case
                        local tMesh = meshDebug:new()
                        if tMesh:load(tFrameAddOptions.tShapeEdit) then
                            local index_frame  = 1
                            local index_subset = 1
                            local index_vertex = 1
                            local total_vertex = tMesh:getTotalVertex(index_frame,index_subset)
                            tFrameAddOptions.tShapeEdit.index  = tMesh:getIndex(index_frame,index_subset)
                            tFrameAddOptions.tShapeEdit.vertex = tMesh:getVertex(index_frame,index_subset,index_vertex,total_vertex)

                            --put it in the 2d coordinate screen
                            local size_x,size_y = tFrameAddOptions.tShapeEdit:getSize()
                            half_size_x , half_size_y = size_x / 2, size_y / 2
                            for i=1, #tFrameAddOptions.tShapeEdit.vertex do
                                local vertex  = tFrameAddOptions.tShapeEdit.vertex[i]  
                                vertex.x      = vertex.x + half_size_x
                                vertex.y      = vertex.y + half_size_y
                            end
                            tFrameAddOptions.tShapeEdit.max_size_x = tTexture.width
                            tFrameAddOptions.tShapeEdit.max_size_y = tTexture.height
                            tFrameAddOptions.tShapeEdit.index_over_vertex_input = 0
                            --print('Created shape total_vertex:',total_vertex,#tFrameAddOptions.tShapeEdit.vertex)
                            tMesh = nil
                        else
                            tUtil.showMessageWarn('Could not get shape from memory',2.5)
                            tFrameAddOptions.tShapeEdit = nil
                            tFrameAddOptions.bEditVertex = false
                        end
                    end
                    local step       =  1.0
                    local step_fast  =  100.0
                    local format     = "%.3f"
                    local flags      =  0
                    tFrameAddOptions.tShapeEdit.index_over_vertex_input = 0
                    if tImGui.TreeNode('##vertex_for_frame', 'Vertex Edit') then
                        for i=1, #tFrameAddOptions.tShapeEdit.vertex do
                            local vertex  = tFrameAddOptions.tShapeEdit.vertex[i] 
                            local label_x    = string.format('X%d',i)
                            local label_y    = string.format('Y%d',i)
                            
                            local result, fValue = tImGui.InputFloat(label_x, vertex.x, step, step_fast, format, flags)
                            if result then
                                if fValue > tFrameAddOptions.tShapeEdit.max_size_x then
                                    fValue = tFrameAddOptions.tShapeEdit.max_size_x
                                elseif fValue < 0 then
                                    fValue = 0
                                end
                                vertex.x = fValue
                            end
                            if tImGui.IsItemHovered(0) then
                                tFrameAddOptions.tShapeEdit.index_over_vertex_input = i
                            end
                            local result, fValue = tImGui.InputFloat(label_y, vertex.y, step, step_fast, format, flags)
                            if result then
                                if fValue > tFrameAddOptions.tShapeEdit.max_size_y then
                                    fValue = tFrameAddOptions.tShapeEdit.max_size_y
                                elseif fValue < 0 then
                                    fValue = 0
                                end
                                vertex.y = fValue
                            end
                            if tImGui.IsItemHovered(0) then
                                tFrameAddOptions.tShapeEdit.index_over_vertex_input = i
                            end
                        end
                        tImGui.TreePop()
                    end
                    if tFrameAddOptions.bAddAsSubset then
                        if tImGui.Button('Add Subset Edited', tSizeBtn) then
                            tFrameList.indexSelectedFrameNodeToExpand = tFrameList.indexSelectedFrameNode
                            local tFrame   = tFrameList[tFrameList.indexSelectedFrameNode]
                            local tTexture = tFrameAddOptions.tSelectedTexture
                            local tSubset  = newPrimitiveFrameFromFrameAddOptions(tTexture,tTexture.width,tTexture.height)
                            table.insert(tFrame.tSubsetList,tSubset)
                            bShowFrameList = true
                            unCollapse(tWindowsTitle.title_frame_list)
                            tUtil.showMessage('Edited Subset Added!',3)
                        end
                    else
                        if tImGui.Button('Add Frame Edited', tSizeBtn) then
                            local tTexture = tFrameAddOptions.tSelectedTexture
                            local tFrame   = newPrimitiveFrameFromFrameAddOptions(tTexture,tTexture.width,tTexture.height)
                            bShowFrameList = true
                            table.insert(tFrameList,tFrame)
                            unCollapse(tWindowsTitle.title_frame_list)
                            tUtil.showMessage('Edited Frame Added!',3)
                        end
                    end
                end
            else
                tUtil.showMessageWarn('There is no texture selected!\nEdit primitive disabled',2.5)
                tFrameAddOptions.bEditVertex = false
            end
        end

        tImGui.Separator()

        local tSelectedTextures = getSelectedTexturesFromImageSelector(tTexturesToEditor)
        local label_textures    = string.format('Textures (%d)',#tSelectedTextures)
        tFrameAddOptions.tSelectedTexture = nil
        if tImGui.TreeNode('##textures_for_frame', label_textures) then
            local frame_padding = 5
            for i=1, #tSelectedTextures do
                local flag_node      = 0
                local id_node        = string.format('##tex_%d',i)
                local tTexture       = tSelectedTextures[i]
                local new_width      = 50
                local sy             = new_width / tTexture.width  * tTexture.height
                local size           = {x=new_width,y=sy}
                local uv0            = {x=0,y=0}
                local uv1            = {x=1,y=1}
                local base_file_name = tTexture.base_file_name
                if tFrameAddOptions.iIndexSelectedNode == i then
                    tFrameAddOptions.tSelectedTexture = tTexture
                    flag_node  = tImGui.Flags('ImGuiTreeNodeFlags_Selected')
                end
                if tImGui.TreeNodeEx(base_file_name,flag_node,id_node) then
                    local pushed_color     = 0
                    if tImGui.IsItemClicked() then
                        tFrameAddOptions.iIndexSelectedNode = i
                    end
                    local winPos = tImGui.GetCursorScreenPos()
                    tImGui.AddRectFilled(winPos,{x = winPos.x + size.x, y = winPos.y + size.y},{r=1,g=1,b=1,a=1})
                    if tFrameAddOptions.iIndexSelectedNode == i then
                        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_ButtonHovered'), {r=0,g=0.8,b=0,a=1})
                        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r=0,g=1,b=0,a=1})
                        pushed_color = 2
                    end
                    if tImGui.ImageButton(tTexture.id, size,uv0,uv1,frame_padding) then
                        tFrameAddOptions.iIndexSelectedNode       = i
                        tFrameAddOptions.bValidFrameSelected      = true
                        tFrameAddOptions.bShowFramePreview        = true
                        tFrameAddOptions.tSelectedTexture         = tTexture
                        tFrameAddOptions.tFramesEnableSpriteSheet = {}
                        unCollapse(tWindowsTitle.title_frame_preview)
                    end
                    if pushed_color > 0 then
                        tImGui.PopStyleColor(pushed_color)
                    end
                    tImGui.TreePop()
                end
            end
            tImGui.TreePop()
        end

        tImGui.Separator()

        tImGui.Text('Subset Options')
        tFrameAddOptions.bAddAsSubset = tImGui.Checkbox('Add as Subset',tFrameAddOptions.bAddAsSubset,tImGui.Flags('ImGuiSelectableFlags_Disabled') )
        tImGui.Separator()
        tImGui.Text('Single Frame Options')
        if tFrameAddOptions.bAddAsSubset then
            if tFrameList.indexSelectedFrameNode and tFrameList.indexSelectedFrameNode <= #tFrameList and tFrameList.indexSelectedFrameNode > 0 then
                if tImGui.Button('Add Subset', tSizeBtn) then
                    if tFrameAddOptions.tSelectedTexture then
                        tFrameList.indexSelectedFrameNodeToExpand = tFrameList.indexSelectedFrameNode
                        local tFrame =  tFrameList[tFrameList.indexSelectedFrameNode]
                        local tSubset = newFrameFromFrameAddOptions(tFrameAddOptions.tSelectedTexture)
                        table.insert(tFrame.tSubsetList,tSubset)
                        bShowFrameList = true
                        unCollapse(tWindowsTitle.title_frame_list)
                        tUtil.showMessage('Subset Added!',3)
                    else
                        tUtil.showMessageWarn('There is no texture selected on tree node!',2.5)
                    end
                end
            else
                tFrameAddOptions.bAddAsSubset = false
                tUtil.showMessageWarn('There is no frame selected on frame list!',2.5)
            end
        else
            if tImGui.Button('Add Selected', tSizeBtn) then
                if tFrameAddOptions.tSelectedTexture then
                    local tFrame = newFrameFromFrameAddOptions(tFrameAddOptions.tSelectedTexture)
                    table.insert(tFrameList,tFrame)
                    tFrameList.indexSelectedFrameNodeToExpand = #tFrameList
                    bShowFrameList = true
                    unCollapse(tWindowsTitle.title_frame_list)
                    tUtil.showMessage('Frame Added!',3)
                else
                    tUtil.showMessageWarn('There is no texture selected on tree node!',2.5)
                end
            end
            if tImGui.Button('Add all', tSizeBtn) then
                if hasTextureSelected(tTexturesToEditor) then
                    local tTextures = getSelectedTexturesFromImageSelector(tTexturesToEditor)
                    for i=1, #tTextures do
                        local tFrame = newFrameFromFrameAddOptions(tTextures[i])
                        table.insert(tFrameList,tFrame)
                    end
                    bShowFrameList = true
                    unCollapse(tWindowsTitle.title_frame_list)
                    tUtil.showMessage(string.format('Added %d Frame(s)!',#tTextures),3)
                else
                    tUtil.showMessageWarn('There is no texture selected!',2.5)
                end
            end
        end

        tImGui.Separator()
        tImGui.Text('UV Map Options')
        tFrameAddOptions.bInvertUFrameOptions = tImGui.Checkbox('Invert U',tFrameAddOptions.bInvertUFrameOptions)
        tFrameAddOptions.bInvertVFrameOptions = tImGui.Checkbox('Invert V',tFrameAddOptions.bInvertVFrameOptions)
        
        tImGui.Separator()

        if tFrameAddOptions.tSelectedTexture and tFrameAddOptions.iIndexPrimitiveType == 1 then --rectangles
            local step       =  0.5
            local step_fast  =  10
            local format     = "%.3f"
            local flags      =  0

            tImGui.Text('Sprite Sheet Mode')

            local result, iValue = tImGui.InputFloat('Column', tFrameAddOptions.iNumColumn, step, step_fast,format, flags)
            if result and iValue >= 1 and iValue < tFrameAddOptions.tSelectedTexture.width then
                tFrameAddOptions.iNumColumn = iValue
            end
            local result, iValue = tImGui.InputFloat('Lines', tFrameAddOptions.iNumLines, step, step_fast,format, flags)
            if result and iValue >= 1 and iValue < tFrameAddOptions.tSelectedTexture.height then
                tFrameAddOptions.iNumLines = iValue
            end

            local result, iValue = tImGui.InputFloat('Marg. X', tFrameAddOptions.iMarginX, step, step_fast,format, flags)
            if result and iValue >= 0 and iValue < tFrameAddOptions.tSelectedTexture.width then
                tFrameAddOptions.iMarginX = iValue
            end
            local result, iValue = tImGui.InputFloat('Marg. Y', tFrameAddOptions.iMarginY, step, step_fast,format, flags)
            if result and iValue >= 0 and iValue < tFrameAddOptions.tSelectedTexture.height then
                tFrameAddOptions.iMarginY = iValue
            end

            local result, iValue = tImGui.InputFloat('Space X', tFrameAddOptions.iSpacingx, step, step_fast,format, flags)
            if result and iValue >= 0 and iValue < tFrameAddOptions.tSelectedTexture.width then
                tFrameAddOptions.iSpacingx = iValue
            end
            local result, iValue = tImGui.InputFloat('Space Y', tFrameAddOptions.iSpacingy, step, step_fast,format, flags)
            if result and iValue >= 0 and iValue < tFrameAddOptions.tSelectedTexture.height then
                tFrameAddOptions.iSpacingy = iValue
            end
            tImGui.Text('Expected Size of Frame')
            tImGui.SameLine()
            tImGui.HelpMarker('If the rectangle is not square it will be calculated the width/height according to do not stretch!')
            local step       =  1
            local step_fast  =  10
            local format     = "%.3f"
            local result, iValue = tImGui.InputFloat('Width', tFrameAddOptions.iSizeFrameWidth, step, step_fast,format, flags)
            if result and iValue > 0 and iValue < tFrameAddOptions.tSelectedTexture.width then
                tFrameAddOptions.iSizeFrameWidth = iValue
            end
            local result, iValue = tImGui.InputFloat('Height', tFrameAddOptions.iSizeFrameHeight, step, step_fast,format, flags)
            if result and iValue > 0 and iValue < tFrameAddOptions.tSelectedTexture.height then
                tFrameAddOptions.iSizeFrameHeight = iValue
            end

            tFrameAddOptions.bStretch = tImGui.Checkbox('Stretch anyway',tFrameAddOptions.bStretch)

            local tRects = calcRectForSpriteSheet(tFrameAddOptions.tSelectedTexture)
            if tRects == nil then
                tUtil.showMessageWarn('Invalid Rectangles!',2.5)
            else

                if #tFrameAddOptions.tFramesEnableSpriteSheet ~= #tRects then
                    while #tFrameAddOptions.tFramesEnableSpriteSheet < #tRects do
                        table.insert(tFrameAddOptions.tFramesEnableSpriteSheet,true)
                    end
                    while #tFrameAddOptions.tFramesEnableSpriteSheet > #tRects do
                        tFrameAddOptions.tFramesEnableSpriteSheet[#tFrameAddOptions.tFramesEnableSpriteSheet] = nil
                    end
                end

                local iTotalSelected = 0
                for i=1, #tFrameAddOptions.tFramesEnableSpriteSheet do
                    if tFrameAddOptions.tFramesEnableSpriteSheet[i] then
                        iTotalSelected = iTotalSelected + 1
                    end
                end

                local sFrameSubset = 'Frames'
                if tFrameAddOptions.bAddAsSubset then
                    sFrameSubset   = 'Subsets'
                end
                local sSelectedFrames  = string.format('Add (%d/%d) %s',iTotalSelected,#tFrameAddOptions.tFramesEnableSpriteSheet,sFrameSubset)
                
                if tFrameAddOptions.bAddAsSubset then
                    if tImGui.Button(sSelectedFrames, tSizeBtn) then
                        if tFrameAddOptions.tSelectedTexture then
                            local bHasFrame = false
                            local tTexture  = tFrameAddOptions.tSelectedTexture
                            local tFrame    = tFrameList[tFrameList.indexSelectedFrameNode]
                            tFrameList.indexSelectedFrameNodeToExpand = tFrameList.indexSelectedFrameNode
                            for i=1, #tRects do
                                if tFrameAddOptions.tFramesEnableSpriteSheet[i] then
                                    local tSubset = newRectFrameFromFrameAddOptions(tTexture,tRects[i].tMin,tRects[i].tMax)
                                    table.insert(tFrame.tSubsetList,tSubset)
                                    bHasFrame = true
                                end
                            end
                            if bHasFrame then
                                bShowFrameList = true
                                unCollapse(tWindowsTitle.title_frame_list)
                                tUtil.showMessage(string.format('Added %d Subset(s)!',#tRects),3)
                            else
                                tUtil.showMessageWarn('There is no frame enabled!',2.5)
                            end
                        end
                    end
                elseif tImGui.Button(sSelectedFrames, tSizeBtn) then
                    if tFrameAddOptions.tSelectedTexture then
                        local bHasFrame = false
                        local tTexture = tFrameAddOptions.tSelectedTexture
                        for i=1, #tRects do
                            if tFrameAddOptions.tFramesEnableSpriteSheet[i] then
                                local tFrame = newRectFrameFromFrameAddOptions(tTexture,tRects[i].tMin,tRects[i].tMax)
                                table.insert(tFrameList,tFrame)
                                bHasFrame = true
                            end
                        end
                        if bHasFrame then
                            bShowFrameList = true
                            unCollapse(tWindowsTitle.title_frame_list)
                            tUtil.showMessage(string.format('Added %d Frame(s)!',#tRects),3)
                        else
                            tUtil.showMessageWarn('There is no frame enabled!',2.5)
                        end
                    else
                        tUtil.showMessageWarn('There is no texture selected on tree node!',2.5)
                    end
                end

                if tImGui.TreeNode('##select_frames_from_sprite_sheet', string.format('%s Options', sFrameSubset)) then
                    tFrameAddOptions.iFramesEnableSpriteSheetHover = 0
                    if tImGui.Button('Include All',tSizeBtn) then
                        for i=1, #tFrameAddOptions.tFramesEnableSpriteSheet do
                            tFrameAddOptions.tFramesEnableSpriteSheet[i] = true
                        end
                        tFrameAddOptions.bShowFramePreview        = true
                        unCollapse(tWindowsTitle.title_frame_preview)
                    end
                    if tImGui.Button('Exclude All',tSizeBtn) then
                        for i=1, #tFrameAddOptions.tFramesEnableSpriteSheet do
                            tFrameAddOptions.tFramesEnableSpriteSheet[i] = false
                        end
                        tFrameAddOptions.bShowFramePreview        = true
                        unCollapse(tWindowsTitle.title_frame_preview)
                    end
                    if tImGui.Button('Invert Selection',tSizeBtn) then
                        for i=1, #tFrameAddOptions.tFramesEnableSpriteSheet do
                            tFrameAddOptions.tFramesEnableSpriteSheet[i] = not tFrameAddOptions.tFramesEnableSpriteSheet[i]
                        end
                        tFrameAddOptions.bShowFramePreview        = true
                        unCollapse(tWindowsTitle.title_frame_preview)
                    end
                    
                    for i=1, #tFrameAddOptions.tFramesEnableSpriteSheet do
                        local str = string.format('%s #%d',sFrameSubset,i)
                        tFrameAddOptions.tFramesEnableSpriteSheet[i] = tImGui.Checkbox(str,tFrameAddOptions.tFramesEnableSpriteSheet[i])
                        if tImGui.IsItemHovered(0) then
                            tFrameAddOptions.iFramesEnableSpriteSheetHover = i
                        end
                    end
                    tImGui.TreePop()
                end
            end
        end
    end
    tImGui.End()
    if closed_clicked then
        tFrameAddOptions:reset()
    end
end

function getShapeViewForAnim(tFrame)
    local sTexHash = string.format('%dx%d', math.floor(tFrame.width),math.floor(tFrame.height))
    if tAnimationOptions.tShapeAnimations[sTexHash] == nil then
        local x,y      = tFrame.width / 0.5, tFrame.height / 0.5
        local tVertex  = {-x,-y,  -x,y,  x,-y,       x,-y, -x,y, x,y}
        local tUv      = {0,0, 0,1, 1,0, 1,0, 0,1, 1,1} --invert V
        local tNormal  = nil
        local nickName = getUniqueNickName()
        local tShape   = shape:new('2dw')
        tShape:create(tVertex,tUv,tNormal,nickName)
        tAnimationOptions.tShapeAnimations[sTexHash] = tShape
    end
    return tAnimationOptions.tShapeAnimations[sTexHash]
end

function makeHashStringForAnimImage(tFrame, iNumImage)
    local sTexHash = string.format('%d:%dx%d',iNumImage, math.floor(tFrame.width),math.floor(tFrame.height))
    return sTexHash
end

function getTextureIdForAnimImage(tFrame, iNumImage)
    local sTexHash = makeHashStringForAnimImage(tFrame, iNumImage)
    local tRender  = tAnimationOptions.tDynamicAnims[sTexHash]
    --it will not render the frame twice, thats why we return the same previous frame
    if iNumImage == 2 and tAnimationOptions.iFrameStart == tAnimationOptions.iFrameStop then
        sTexHash = makeHashStringForAnimImage(tFrame, 1)
        tRender  = tAnimationOptions.tDynamicAnims[sTexHash]
        if tRender == nil then
            return getTextureIdForAnimImage(tFrame, 1)
        end
        return tRender.texture_id, tRender.nick_name
    elseif iNumImage == 3 and tAnimationOptions.iFrameStart == tAnimationOptions.iCurrentFrame then
        sTexHash = makeHashStringForAnimImage(tFrame, 1)
        tRender  = tAnimationOptions.tDynamicAnims[sTexHash]
        if tRender == nil then
            return getTextureIdForAnimImage(tFrame, 1)
        end
        return tRender.texture_id, tRender.nick_name
    elseif iNumImage == 3 and tAnimationOptions.iFrameStop == tAnimationOptions.iCurrentFrame then
        sTexHash = makeHashStringForAnimImage(tFrame, 2)
        tRender  = tAnimationOptions.tDynamicAnims[sTexHash]
        if tRender == nil then
            return getTextureIdForAnimImage(tFrame, 2)
        end
        return tRender.texture_id, tRender.nick_name
    end

    if tRender == nil then
        tRender = render2texture:new('2dw')
        local bSuccess,nick_name, id = tRender:create(math.floor(tFrame.width),math.floor(tFrame.height),true,sTexHash)
        if bSuccess and id > 0 then
            tRender.texture_id = id
            tRender.nick_name = nick_name
        else
            print('error','Could not create dynamic texture!',sTexHash)
            tUtil.showMessageWarn("Could not create dynamic texture!\n\n" .. sTexHash)
        end
        tRender:enableFrame(false)
        tRender:setColor(0,0,0,0)
        tAnimationOptions.tDynamicAnims[sTexHash] = tRender
    end

    tRender:clear()
    tRender.visible       = true
    tFrame.tShape.visible = true
    tFrame.tShape:setPos(-tFrame.tPivot.x,-tFrame.tPivot.y)
    tRender:add(tFrame.tShape)
    for i=1, #tFrame.tSubsetList do
        local tSubset = tFrame.tSubsetList[i]
        tSubset.tShape.visible = true
        tSubset.tShape:setPos(-tSubset.tPivot.x,-tSubset.tPivot.y)
        tRender:add(tSubset.tShape)
    end
    return tRender.texture_id, tRender.nick_name
end

function applyZoomFrameAnimation()
    local tUvZoom = tAnimationOptions.tUvZoom
    if tImGui.IsItemHovered(0) then
        local zoom = tImGui.GetZoom()
        if zoom ~= 0 then
            local inc = (zoom * 0.02)
            tUvZoom.uv0.x = tUvZoom.uv0.x + inc
            tUvZoom.uv0.y = tUvZoom.uv0.y - inc

            if tUvZoom.uv0.x < 0.0 then
                tUvZoom.uv0.x = 0.0
            elseif tUvZoom.uv0.x > 0.48 then
                tUvZoom.uv0.x = 0.48
            end

            if tUvZoom.uv0.y < 0.52 then
                tUvZoom.uv0.y = 0.52
            elseif tUvZoom.uv0.y > 1 then
                tUvZoom.uv0.y = 1
            end

            tUvZoom.uv1.x = 1 - tUvZoom.uv0.x
            tUvZoom.uv1.y = 1 - tUvZoom.uv0.y
        end
    end
end

function addDynamicTextureToImGuiImage(tFrame,winSize,padding,iNumImage)
    local iW, iH        = mbm.getRealSizeScreen()
    local bg_col        = {r=1,g=1,b=1,a=1}
    local line_color    = {r=0,g=0,b=0,a=1}
    if tFrame == nil then trace() end
    local new_width     = math.min(tFrame.width, winSize.x - padding.x)
    local sy            = new_width / tFrame.width  * tFrame.height
    local size          = {x = math.min(new_width,iW), y = math.min(sy,iH) }
    local texture_id, _ = getTextureIdForAnimImage(tFrame, iNumImage)
    tImGui.Image(texture_id,size,tAnimationOptions.tUvZoom.uv0,tAnimationOptions.tUvZoom.uv1,bg_col,line_color)
    applyZoomFrameAnimation()
    tImGui.HelpMarker('Use scroll to zoom it!')
    
end

function showAnimationAdd(delta)

    tAnimationOptions:disableTextureAnimations()
    setAllFrameAsNotVisible()
    tLineSprite.visible      = false
    if #tFrameList > 0 then
        local width        = 230
        local tSizeBtn     = {x=width - 30,y=0}
        local x_pos, y_pos = 0, 0
        local indexFrame   = tAnimationOptions:updateAnimation(delta)
        tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_animation,x_pos,y_pos,width)
        shouldICollapse(tWindowsTitle.title_animation)
        tImGui.PushStyleVar('ImGuiStyleVar_WindowMinSize',{x = width, y= width})
        local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_animation, true, ImGuiWindowFlags_NoMove)
        if tAnimationOptions.iFrameStart <= 0 then
            tAnimationOptions.iFrameStart = 1
        end
        if tAnimationOptions.iFrameStop <= 0 then
            tAnimationOptions.iFrameStop = 1
        end
        if tAnimationOptions.iFrameStart > #tFrameList then
            tAnimationOptions.iFrameStart = #tFrameList
        end
        if tAnimationOptions.iFrameStop > #tFrameList then
            tAnimationOptions.iFrameStop = #tFrameList
        end
        if is_opened then
            tImGui.Text('Add Animation')
            local v_min     = 1
            local v_max     = #tFrameList
            local winSize   = tImGui.GetWindowSize()
            local animWidth = winSize.x -30
            
            
            local tFrameStart    = tFrameList[tAnimationOptions.iFrameStart]
            local tFrameStop     = tFrameList[tAnimationOptions.iFrameStop]
            local padding        = tImGui.GetStyle('DisplayWindowPadding')
            local sy_visible     = select(2,tImGui.IsScrollVisible())
            if sy_visible then
                padding.x = padding.x + tImGui.GetStyle('ScrollbarSize')
            end
            tImGui.PushItemWidth(animWidth)
            local result, iValue = tImGui.SliderInt('##Start', tAnimationOptions.iFrameStart, v_min, v_max, "Start Frame %d")
            tImGui.PopItemWidth()
            if result then
                tAnimationOptions.iFrameStart = iValue
                if tAnimationOptions.iFrameStop < iValue then
                    tAnimationOptions.iFrameStop = iValue
                end
            end
            
            addDynamicTextureToImGuiImage(tFrameStart,winSize,padding,1)

            tImGui.PushItemWidth(animWidth)
            local result, iValue = tImGui.SliderInt('##Stop', tAnimationOptions.iFrameStop, v_min, v_max, "Stop Frame %d")
            tImGui.PopItemWidth()
            if result then
                tAnimationOptions.iFrameStop  = iValue
                if tAnimationOptions.iFrameStart > iValue then
                    tAnimationOptions.iFrameStart = iValue
                end
            end
            addDynamicTextureToImGuiImage(tFrameStop,winSize,padding,2)
            tImGui.PushItemWidth(200)
            tImGui.Separator()
            tImGui.Text('Current Frame:' .. tostring(indexFrame))
            tImGui.Text(string.format('Current Time: %.3f', tAnimationOptions.iTimeAnimSimulation))
            
            tImGui.Separator()
            tImGui.Text('Type Of Animation:')
            local ret, current_item, item = tImGui.Combo('##ComboAnim' , tAnimationOptions.iTypeAnim, tAnimationOptions.tAnimTypes)
            if ret then
                tAnimationOptions.iTypeAnim = current_item
            end
            
            if tImGui.Button('Restart Animation', tSizeBtn) then
                tAnimationOptions:restartAnim()
            end

            tImGui.Separator()
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
            
            local tFrame             = tFrameList[indexFrame]
            local id, nick_name      = getTextureIdForAnimImage(tFrame, 3)
            local alpha              = true
            local stage              = 1
            local tShapeAnimations   = getShapeViewForAnim(tFrame)
            tShapeAnimations.visible = true
            tShapeAnimations:setTexture(nick_name,alpha,stage)
            
            tShapeAnimations:setScale(tAnimationOptions.tScaleAnim.sx,tAnimationOptions.tScaleAnim.sy)

            tImGui.Separator()
            tImGui.Text('Scale of Preview:')
            local result, fValue = tImGui.InputFloat('##ScaleAnimPreview', tAnimationOptions.tScaleAnim.sx, step, step_fast,format,flags)
            if result then
                if fValue >=0 and fValue <= 10 then
                    tAnimationOptions.tScaleAnim.sx = fValue
                    tAnimationOptions.tScaleAnim.sy = fValue
                end
            end

            tImGui.Separator()
            tImGui.Text('Name:')
            local label      = '##NameAnim'
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

            if tImGui.Button('Add Animation', tSizeBtn) then
                local tAnim = {}
                tAnim.sNameAnim    = tAnimationOptions.sNameAnim
                tAnim.fTimeFrame   = tAnimationOptions.fTimeFrame
                tAnim.iTypeAnim    = tAnimationOptions.iTypeAnim
                tAnim.iFrameStart  = tAnimationOptions.iFrameStart
                tAnim.iFrameStop   = tAnimationOptions.iFrameStop
                table.insert(tAnimationList,tAnim)
                tUtil.showMessage('Animation added successfully!')
            end

            tImGui.Separator()

            tImGui.PopItemWidth()

            if tImGui.TreeNode('##Animations', 'Animations') then
                local flag_node      = 0
                local tSizeBtn     = {x=0,y=0}
                for i=1, #tAnimationList do
                    local tAnim    = tAnimationList[i]
                    local id_node  = string.format('##anim %s %d',tAnim.sNameAnim,i)
                    
                    if tImGui.TreeNodeEx(tAnim.sNameAnim,flag_node,id_node) then
                        if tImGui.BeginPopupContextItem("Options Frame List") then
                            if tImGui.Selectable("Delete Animation") then
                                table.remove(tAnimationList,i)
                                tImGui.EndPopup()
                                tImGui.TreePop()
                                break
                            end
                            tImGui.EndPopup()
                        end
                        tImGui.Text(string.format('Start :%d',tAnim.iFrameStart))
                        tImGui.Text(string.format('Stop  :%d',tAnim.iFrameStop))
                        tImGui.Text(string.format('Type  :%s',tAnimationOptions.tAnimTypes[tAnim.iTypeAnim]))
                        tImGui.Text(string.format('Time  :%.3f',tAnim.fTimeFrame))

                        if isAnimationValid(tAnim) == false then
                            local color         = {r=1,g=0,b=0,a=1}
                            local text          = 'Invalid!'
                            tImGui.TextColored(color, text)
                            tImGui.SameLine()
                            tImGui.HelpMarker('This animation is invalid and will not be added to the final sprite!')
                            if tImGui.Button('Delete', tSizeBtn) then
                                table.remove(tAnimationList,i)
                                tImGui.TreePop()
                                break
                            end
                        end
                        if tImGui.Button('Select this animation', tSizeBtn) then
                            tAnimationOptions.sNameAnim   = tAnim.sNameAnim
                            tAnimationOptions.fTimeFrame  = tAnim.fTimeFrame
                            tAnimationOptions.iTypeAnim   = tAnim.iTypeAnim
                            tAnimationOptions.iFrameStart = tAnim.iFrameStart
                            tAnimationOptions.iFrameStop  = tAnim.iFrameStop
                            tAnimationOptions:restartAnim()
                        end
                        if tImGui.Button('Apply current options', tSizeBtn) then
                            tAnim.sNameAnim   = tAnimationOptions.sNameAnim
                            tAnim.fTimeFrame  = tAnimationOptions.fTimeFrame
                            tAnim.iTypeAnim   = tAnimationOptions.iTypeAnim
                            tAnim.iFrameStart = tAnimationOptions.iFrameStart
                            tAnim.iFrameStop  = tAnimationOptions.iFrameStop
                            tAnimationOptions:restartAnim()
                        end
                        tImGui.TreePop()
                    end
                end
                tImGui.TreePop()
            end
            
        end
        tImGui.End()
        tImGui.PopStyleVar(1)

        if closed_clicked then
            tAnimationOptions:reset()
            closeAnimationWindow()
            setAllFrameAsNotVisible()
            tex_alpha_pattern.visible = false
        end
    else
        closeAnimationWindow()
        tUtil.showMessageWarn('There is no frame to make animation!\n\nAdd Frame first!')
    end
end

function showAdvancedOptions()
    
    local width        = 300
    local height       = 230
    local iW, iH       = mbm.getRealSizeScreen()
    local tPosWin      = {x = iW / 2 - width / 2, y = iH / 2 - height / 2}
    tImGui.SetNextWindowSize({x = width, y = height},tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowPos(tPosWin , tImGui.Flags('ImGuiCond_Once'))

    shouldICollapse(tWindowsTitle.title_advanced_options)
    tImGui.PushStyleVar('ImGuiStyleVar_WindowMinSize',{x = width, y= height})
    local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_advanced_options, true, ImGuiWindowFlags_NoMove)
    if is_opened then

        tImGui.PushItemWidth(270)
        tImGui.Text('Mode draw')
        local ret, current_item, item = tImGui.Combo('##ModeDraw', tSaveBinaryOptions.indexModeDraw, tSaveBinaryOptions.tModeDrawList)
        if ret then
            tSaveBinaryOptions.indexModeDraw = current_item
        end
        
        tImGui.Text('Cull Face')
        local ret, current_item, item = tImGui.Combo('##CullFace', tSaveBinaryOptions.indexCullFace, tSaveBinaryOptions.tCullFaceList)
        if ret then
            tSaveBinaryOptions.indexCullFace = current_item
        end

        tImGui.Text('Front Face')
        local ret, current_item, item = tImGui.Combo('##FrontFace', tSaveBinaryOptions.indexFrontFace, tSaveBinaryOptions.tFrontFaceList)
        if ret then
            tSaveBinaryOptions.indexFrontFace = current_item
        end

        tImGui.Text('Stride')
        local ret, current_item, item = tImGui.Combo('##Stride', tSaveBinaryOptions.indexStride, {'2 (x,y)', '3 (x,y,z)'})
        if ret then
            if current_item == 1 then
                tSaveBinaryOptions.stride = 2
            elseif current_item == 2 then
                tSaveBinaryOptions.stride = 3
            else
                print('error','Unexpected stride index')
            end
            tSaveBinaryOptions.indexStride = current_item
        end

        tImGui.Separator()

        if tImGui.Button('Reset to Default Options', {x=270,y=0}) then
            tSaveBinaryOptions.indexFrontFace       = 1
            tSaveBinaryOptions.indexModeDraw        = 1
            tSaveBinaryOptions.indexCullFace        = 2
            tSaveBinaryOptions.stride               = 2
            tSaveBinaryOptions.indexStride          = 1
        end

        tImGui.PopItemWidth()
    end
    tImGui.End()
    tImGui.PopStyleVar(1)

    if closed_clicked then
        bShowAdvancedOptions = false
    end
end



function updatePhysics(tLinesPhysics,tPhysic)
    
    local function setupPhysics(tLinesPhysics,tLines,tPhysic)
        tLinesPhysics:set(tLines,1)
        tLinesPhysics.tPhysic = tPhysic
    end
    
    if tPhysic.type == 'cube' then
        local x       = tPhysic.x
        local y       = tPhysic.y
        local half_x  = tPhysic.width  * 0.5
        local half_y  = tPhysic.height * 0.5

        local tLines = {-half_x, - half_y,
                        -half_x,   half_y,
                         half_x,   half_y,
                         half_x, - half_y,
                        -half_x, - half_y}
        tLinesPhysics:setPos(x,y)
        setupPhysics(tLinesPhysics,tLines,tPhysic)
    elseif tPhysic.type == 'sphere' then
        local x       = tPhysic.x
        local y       = tPhysic.y
        local ray     = tPhysic.ray
        local tLines  = {}
        for j=0, 360 do
            local radian = math.rad(j)
            table.insert(tLines,math.sin(radian) * ray)
            table.insert(tLines,math.cos(radian) * ray)
        end
        tLinesPhysics:setPos(x,y)
        setupPhysics(tLinesPhysics,tLines,tPhysic)
    elseif tPhysic.type == 'triangle' then
        local x       = tPhysic.x
        local y       = tPhysic.y
        local tLines  = {}
        table.insert(tLines,tPhysic.a.x)
        table.insert(tLines,tPhysic.a.y)
        table.insert(tLines,tPhysic.b.x)
        table.insert(tLines,tPhysic.b.y)
        table.insert(tLines,tPhysic.c.x)
        table.insert(tLines,tPhysic.c.y)
        table.insert(tLines,tPhysic.a.x)
        table.insert(tLines,tPhysic.a.y)
        tLinesPhysics:setPos(x,y)
        setupPhysics(tLinesPhysics,tLines,tPhysic)
    elseif tPhysic.type == 'complex' then
        local x       = tPhysic.x
        local y       = tPhysic.y
        local tLines  = {}
        
        table.insert(tLines,tPhysic.a.x)
        table.insert(tLines,tPhysic.a.y)
        table.insert(tLines,tPhysic.b.x)
        table.insert(tLines,tPhysic.b.y)
        table.insert(tLines,tPhysic.c.x)
        table.insert(tLines,tPhysic.c.y)
        table.insert(tLines,tPhysic.d.x)
        table.insert(tLines,tPhysic.d.y)
        table.insert(tLines,tPhysic.a.x)
        table.insert(tLines,tPhysic.a.y)
        table.insert(tLines,tPhysic.e.x)
        table.insert(tLines,tPhysic.e.y)
        table.insert(tLines,tPhysic.f.x)
        table.insert(tLines,tPhysic.f.y)
        table.insert(tLines,tPhysic.b.x)
        table.insert(tLines,tPhysic.b.y)
        table.insert(tLines,tPhysic.f.x)
        table.insert(tLines,tPhysic.f.y)
        table.insert(tLines,tPhysic.g.x)
        table.insert(tLines,tPhysic.g.y)
        table.insert(tLines,tPhysic.h.x)
        table.insert(tLines,tPhysic.h.y)
        table.insert(tLines,tPhysic.d.x)
        table.insert(tLines,tPhysic.d.y)
        table.insert(tLines,tPhysic.c.x)
        table.insert(tLines,tPhysic.c.y)
        table.insert(tLines,tPhysic.g.x)
        table.insert(tLines,tPhysic.g.y)
        table.insert(tLines,tPhysic.h.x)
        table.insert(tLines,tPhysic.h.y)
        table.insert(tLines,tPhysic.e.x)
        table.insert(tLines,tPhysic.e.y)
        tLinesPhysics:setPos(x,y)
        setupPhysics(tLinesPhysics,tLines,tPhysic)
    else
        print('error','line', 'Type unexpected of physics',tPhysic.type)
    end
    
end

function addPhysics(tShape)
    tShape:setScale(0,0)

    local function newPhysicsLine(tLines,tPhysic)
        local tLinesPhysics   = line:new('2dw')
        tLinesPhysics.visible = false
        tLinesPhysics:add(tLines)
        tLinesPhysics:setColor(0,0,1)
        tLinesPhysics.tPhysic = tPhysic
        table.insert(tPhysicsOptions.tLinesPhysics,tLinesPhysics)
        return tLinesPhysics
    end
    if tShape.tVertexEdited then
        
        local vertex = tShape.vertex
        local index  = tShape.index_read_only
        for i=1, #index, 3 do
            local tLines  = {}
            local tPhysic = {type = 'triangle'}
            local index_buffer_1 = index[i  ] -- one based
            local index_buffer_2 = index[i+1] -- one based
            local index_buffer_3 = index[i+2] -- one based
            local px_1,   py_1   = vertex[index_buffer_1].x, vertex[index_buffer_1].y
            local px_2,   py_2   = vertex[index_buffer_2].x, vertex[index_buffer_2].y
            local px_3,   py_3   = vertex[index_buffer_3].x, vertex[index_buffer_3].y
            table.insert(tLines,px_1)
            table.insert(tLines,py_1)
            table.insert(tLines,px_2)
            table.insert(tLines,py_2)
            table.insert(tLines,px_3)
            table.insert(tLines,py_3)
            table.insert(tLines,px_1)
            table.insert(tLines,py_1)

            tPhysic.a = {x = px_1, y = py_1}
            tPhysic.b = {x = px_2, y = py_2}
            tPhysic.c = {x = px_3, y = py_3}
            tPhysic.x = 0
            tPhysic.y = 0
            newPhysicsLine(tLines,tPhysic)
        end
    else
        local tInfoPhysics  = tShape:getPhysics()
        for i=1, #tInfoPhysics do
            if tInfoPhysics[i].type == 'cube' then
                local tPhysic = tInfoPhysics[i]
                local x       = tInfoPhysics[i].x
                local y       = tInfoPhysics[i].y
                local half_x  = tInfoPhysics[i].width  * 0.5
                local half_y  = tInfoPhysics[i].height * 0.5

                local tLines = {- half_x, - half_y,
                                - half_x,   half_y,
                                  half_x,   half_y,
                                  half_x, - half_y,
                                - half_x, - half_y}
                local tLinesPhysics = newPhysicsLine(tLines,tPhysic)
                tLinesPhysics:setPos(x,y)
            elseif tInfoPhysics[i].type == 'sphere' then
                local tPhysic = tInfoPhysics[i]
                local x       = tInfoPhysics[i].x
                local y       = tInfoPhysics[i].y
                local ray     = tInfoPhysics[i].ray
                local tLines  = {}
                for j=0, 360 do
                    local radian = math.rad(j)
                    table.insert(tLines,math.sin(radian) * ray)
                    table.insert(tLines,math.cos(radian) * ray)
                end
                local tLinesPhysics = newPhysicsLine(tLines,tPhysic)
                tLinesPhysics:setPos(x,y)
            elseif tInfoPhysics[i].type == 'triangle' then
                local tLines  = {}
                local tPhysic = tInfoPhysics[i]
                table.insert(tLines,tInfoPhysics[i].a.x)
                table.insert(tLines,tInfoPhysics[i].a.y)
                table.insert(tLines,tInfoPhysics[i].b.x)
                table.insert(tLines,tInfoPhysics[i].b.y)
                table.insert(tLines,tInfoPhysics[i].c.x)
                table.insert(tLines,tInfoPhysics[i].c.y)
                table.insert(tLines,tInfoPhysics[i].a.x)
                table.insert(tLines,tInfoPhysics[i].a.y)
                tPhysic.x = 0
                tPhysic.y = 0
                newPhysicsLine(tLines,tPhysic)
            elseif tInfoPhysics[i].type == 'complex' then
                local tLines = {}
                local tPhysic = tInfoPhysics[i]
                
                table.insert(tLines,tInfoPhysics[i].a.x)
                table.insert(tLines,tInfoPhysics[i].a.y)
                table.insert(tLines,tInfoPhysics[i].b.x)
                table.insert(tLines,tInfoPhysics[i].b.y)
                table.insert(tLines,tInfoPhysics[i].c.x)
                table.insert(tLines,tInfoPhysics[i].c.y)
                table.insert(tLines,tInfoPhysics[i].d.x)
                table.insert(tLines,tInfoPhysics[i].d.y)
                table.insert(tLines,tInfoPhysics[i].a.x)
                table.insert(tLines,tInfoPhysics[i].a.y)
                table.insert(tLines,tInfoPhysics[i].e.x)
                table.insert(tLines,tInfoPhysics[i].e.y)
                table.insert(tLines,tInfoPhysics[i].f.x)
                table.insert(tLines,tInfoPhysics[i].f.y)
                table.insert(tLines,tInfoPhysics[i].b.x)
                table.insert(tLines,tInfoPhysics[i].b.y)
                table.insert(tLines,tInfoPhysics[i].f.x)
                table.insert(tLines,tInfoPhysics[i].f.y)
                table.insert(tLines,tInfoPhysics[i].g.x)
                table.insert(tLines,tInfoPhysics[i].g.y)
                table.insert(tLines,tInfoPhysics[i].h.x)
                table.insert(tLines,tInfoPhysics[i].h.y)
                table.insert(tLines,tInfoPhysics[i].d.x)
                table.insert(tLines,tInfoPhysics[i].d.y)
                table.insert(tLines,tInfoPhysics[i].c.x)
                table.insert(tLines,tInfoPhysics[i].c.y)
                table.insert(tLines,tInfoPhysics[i].g.x)
                table.insert(tLines,tInfoPhysics[i].g.y)
                table.insert(tLines,tInfoPhysics[i].h.x)
                table.insert(tLines,tInfoPhysics[i].h.y)
                table.insert(tLines,tInfoPhysics[i].e.x)
                table.insert(tLines,tInfoPhysics[i].e.y)
                tPhysic.x = 0
                tPhysic.y = 0
                newPhysicsLine(tLines,tPhysic)
            end
        end
    end
end

function showEditPhysics()
    bShowEditPhysics = false
    if #tFrameList > 0 then
        tUtil.showMessageWarn('Please use physic_editor instead!')
        local width = 250
        local x_pos, y_pos = 0, 0
        local max_width = 250
        local tSizeBtn   = {x=width - 20,y=0} -- size button
        setAllFrameAsNotVisible()
        bShowAnimationEdit = false
        bShowFrameEdit     = false
        tLineSprite.visible      = false
        bShowFrameList           = false
        tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_edit_physics,x_pos,y_pos,width,max_width)
        shouldICollapse(tWindowsTitle.title_edit_physics)
        local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_edit_physics, true, ImGuiWindowFlags_NoMove)
        local tFrame = tFrameList[1]
        tFrame.tShape.visible = true
        tFrame.tShape:setScale(tPhysicsOptions.tScalePhysics.sx,tPhysicsOptions.tScalePhysics.sy)
        bEnableMoveWorld = true

        -- TODO physics is not been loaded from file (fix me)
        for i=1, #tPhysicsOptions.tLinesPhysics do
            tPhysicsOptions.tLinesPhysics[i].visible = true
            tPhysicsOptions.tLinesPhysics[i]:setScale(tPhysicsOptions.tScalePhysics.sx,tPhysicsOptions.tScalePhysics.sy)
            tPhysicsOptions.tLinesPhysics[i].z = tFrame.tShape.z - 1
        end

        if is_opened then
            
            tImGui.Text('Primitive type')
            local indexPrimitive = tImGui.RadioButton('Rectangle', tPhysicsOptions.iIndexPrimitiveType, 1)
            indexPrimitive       = tImGui.RadioButton('Circle',    indexPrimitive, 2)
            indexPrimitive       = tImGui.RadioButton('Triangle',  indexPrimitive, 3)
            local tSizeBtn       = {x=width - 20,y=0} -- size button
            
            local color             = {r=1,g=1,b=0.4,a=1}
            local thickness         =  5.0
            local winPos            = tImGui.GetCursorScreenPos()
            if indexPrimitive == 1 then
                local p_min             = {x = winPos.x + 75,  y = winPos.y + 15}
                local p_max             = {x = winPos.x + 125, y = winPos.y + 65}
                local rounding          =  2.0
                local rounding_corners  =  tImGui.Flags('ImDrawCornerFlags_All')
                tImGui.AddRect(p_min, p_max, color, rounding, rounding_corners, thickness)
            elseif indexPrimitive == 2 then
                local center        = {x=winPos.x + 100,y=winPos.y + 25 + 7.5}
                local radius        = 25
                tImGui.AddCircle(center, radius, color, 18, thickness)
            elseif indexPrimitive == 3 then
                local p1     = {x=0 +  winPos.x + 75,y=50 + winPos.y + 15}
                local p2     = {x=25 + winPos.x + 75,y=0  + winPos.y + 15}
                local p3     = {x=50 + winPos.x + 75,y=50 + winPos.y + 15}
                tImGui.AddTriangle(p1, p2, p3, color, thickness + 5)
            end

            tPhysicsOptions.iIndexPrimitiveType = indexPrimitive

            local step       =  1
            local step_fast  =  10
            
            winPos.y = winPos.y + 100
            tImGui.SetCursorScreenPos(winPos)
            
            if tImGui.Button('Add Physic', tSizeBtn) then
                local tFakeShape = {tVertexEdited = false}
                tFakeShape.setScale = function(self,x,y) end

                local tInfoPhysics      = {}
                local tInfoPhysicsInner = {}
                table.insert(tInfoPhysics,tInfoPhysicsInner)
                tFakeShape.tInfoPhysics = tInfoPhysics
                tFakeShape.getPhysics = function (self)
                    return self.tInfoPhysics
                end
                tInfoPhysicsInner.x      = 0
                tInfoPhysicsInner.y      = 0
                tInfoPhysicsInner.z      = 0
                local width, height      = tFrame.tShape:getSize()
                if indexPrimitive == 1 then --cube
                    tInfoPhysicsInner.type = 'cube'
                    tInfoPhysicsInner.width  = width
                    tInfoPhysicsInner.height = height
                elseif indexPrimitive == 2 then --sphere
                    tInfoPhysicsInner.type = 'sphere'
                    tInfoPhysicsInner.ray  = width * 0.5
                elseif indexPrimitive == 3 then --triangle
                    tInfoPhysicsInner.type = 'triangle'
                    tInfoPhysicsInner.a = {x = width * -0.25, y = height * -0.25}
                    tInfoPhysicsInner.b = {x = 0, y = height * 0.25}
                    tInfoPhysicsInner.c = {x = width * 0.25, y = height * -0.25}
                elseif indexPrimitive == 4 then --complex
                    tInfoPhysicsInner.type = 'complex'
                    tInfoPhysicsInner.a={x=-50,y=-50}
                    tInfoPhysicsInner.b={x=0,y=0}
                    tInfoPhysicsInner.c={x=40,y=-40}
                    tInfoPhysicsInner.d={x=50,y=50}
                    tInfoPhysicsInner.e={x=0,y=0}
                    tInfoPhysicsInner.f={x=-50,y=50}
                    tInfoPhysicsInner.g={x=-250,y=250}
                end
                addPhysics(tFakeShape)
            end

            tImGui.Separator()
            
            local step       =  1.0
            local step_fast  =  10.0
            local format     = "%.3f"
            local flags      =  0
            tImGui.PushItemWidth(150)
            if tImGui.TreeNode('##physics_tree', 'Physics') then
                for i=1, #tPhysicsOptions.tLinesPhysics do
                    local tLinesPhysics = tPhysicsOptions.tLinesPhysics[i]
                    local tPhysic       = tLinesPhysics.tPhysic
                    local id_node       = '##physics_' .. tostring(i) .. '_' .. tPhysic.type
                    local flag_node     = 0
                    local name          = tPhysic.type
                    if name == 'cube' then
                        name = 'rectangle'
                    elseif name == 'sphere' then
                        name = 'circle'
                    end
                    tLinesPhysics:setColor(0,0,1)
                    if tImGui.TreeNodeEx(name,flag_node,id_node) then
                        if tPhysic.type == 'cube' then
                            local label    = string.format('X##cube_%d_x',i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.x, step, step_fast, format, flags)
                            if result then
                                tPhysic.x = fValue
                                updatePhysics(tLinesPhysics,tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                            local label    = string.format('Y##cube_%d_y',i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.y, step, step_fast, format, flags)
                            if result then
                                tPhysic.y = fValue
                                updatePhysics(tLinesPhysics,tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                            local label    = string.format('Width##cube_%d_width',i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.width, step, step_fast, format, flags)
                            if result then
                                if fValue > 0 then
                                    tPhysic.width = fValue
                                    updatePhysics(tLinesPhysics,tPhysic)
                                end
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                            local label    = string.format('Height##cube_%d_height',i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.height, step, step_fast, format, flags)
                            if result then
                                if fValue > 0 then
                                    tPhysic.height = fValue
                                    updatePhysics(tLinesPhysics,tPhysic)
                                end
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                        elseif tPhysic.type == 'sphere' then
                            local label    = string.format('X##sphere_%d_x',i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.x, step, step_fast, format, flags)
                            if result then
                                tPhysic.x = fValue
                                updatePhysics(tLinesPhysics,tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                            local label    = string.format('Y##sphere_%d_y',i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.y, step, step_fast, format, flags)
                            if result then
                                tPhysic.y = fValue
                                updatePhysics(tLinesPhysics,tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                            local label    = string.format('Ray##sphere_%d_ray',i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.ray, step, step_fast, format, flags)
                            if result then
                                if fValue > 0 then
                                    tPhysic.ray = fValue
                                    updatePhysics(tLinesPhysics,tPhysic)
                                end
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                        elseif tPhysic.type == 'triangle' then
                            local label    = string.format('X##triangle_%d_x',i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.x, step, step_fast, format, flags)
                            if result then
                                tPhysic.x = fValue
                                updatePhysics(tLinesPhysics,tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                            local label    = string.format('Y##triangle_%d_y',i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.y, step, step_fast, format, flags)
                            if result then
                                tPhysic.y = fValue
                                updatePhysics(tLinesPhysics,tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                            local label    = string.format('#%dX##triangle_%d_x',1,i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.a.x, step, step_fast, format, flags)
                            if result then
                                tPhysic.a.x = fValue
                                updatePhysics(tLinesPhysics,tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                            local label    = string.format('#%dY##triangle_%d_y',1,i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.a.y, step, step_fast, format, flags)
                            if result then
                                tPhysic.a.y = fValue
                                updatePhysics(tLinesPhysics,tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                            local label    = string.format('#%dX##triangle_%d_x',2,i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.b.x, step, step_fast, format, flags)
                            if result then
                                tPhysic.b.x = fValue
                                updatePhysics(tLinesPhysics,tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                            local label    = string.format('#%dY##triangle_%d_y',2,i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.b.y, step, step_fast, format, flags)
                            if result then
                                tPhysic.b.y = fValue
                                updatePhysics(tLinesPhysics,tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                            local label    = string.format('#%dX##triangle_%d_x',3,i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.c.x, step, step_fast, format, flags)
                            if result then
                                tPhysic.c.x = fValue
                                updatePhysics(tLinesPhysics,tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end

                            local label    = string.format('#%dY##triangle_%d_y',3,i)
                            local result, fValue = tImGui.InputFloat(label, tPhysic.c.y, step, step_fast, format, flags)
                            if result then
                                tPhysic.c.y = fValue
                                updatePhysics(tLinesPhysics,tPhysic)
                            end
                            if tImGui.IsItemHovered(0) then
                                tLinesPhysics:setColor(1,0,1)
                            end
                        elseif tPhysic.type == 'complex' then
                            tImGui.Text('Not implemented')
                        end
                        if tImGui.Button('Delete Physic', {x=0,y=0}) then
                            local tLinesPhysics = tPhysicsOptions.tLinesPhysics[i]
                            tLinesPhysics:destroy()
                            table.remove(tPhysicsOptions.tLinesPhysics,i)
                            tImGui.TreePop()
                            break
                        end
                        tImGui.TreePop()
                    end
                end
                tImGui.TreePop()
            end
            tImGui.PopItemWidth()
        end
        if closed_clicked then
            closePhysicsWindow()
        end
        tImGui.End()
    else
        tUtil.showMessageWarn('There is no frame to edit physics!')
        closePhysicsWindow()
    end
end

function closePhysicsWindow()
    bShowEditPhysics   = false
    if #tFrameList > 0 then
        local tFrame = tFrameList[1]
        tFrame.tShape:setScale(1,1)
        tFrame.tShape.visible = false
    end
    for i=1, #tPhysicsOptions.tLinesPhysics do
        tPhysicsOptions.tLinesPhysics[i].visible = false
    end
    tPivotShape.visible = false
    tex_alpha_pattern.visible = false
    bEnableMoveWindow = false
    camera2d:setPos(0,0)
    bEnableMoveWorld = false
    setAllFrameAsNotVisible()
end

function closeAnimationWindow()
    bShowAnimationEdit = false
    tPivotShape.visible = false
    tAnimationOptions:reset()
end

function closeFrameEdit()
    setAllFrameAsNotVisible()
    bShowFrameEdit           = false
    tLineSprite.visible      = false
    tex_alpha_pattern.visible = false
    bEnableMoveWorld         = false
    tPivotShape.visible      = false
    tUVShape.visible         = false
    tUvLine.visible          = false
end

function isAnimationValid(tAnim)
    if tAnim.iFrameStart <= 0 then
        return false
    end
    if tAnim.iFrameStart > #tFrameList then
        return false
    end
    if tAnim.iFrameStop <= 0 then
        return false
    end
    if tAnim.iFrameStop > #tFrameList then
        return false
    end
    return true
end

function collapseAllBut(title)
    bCollapseAllBut = title
    tTimerCollapse = timer:new(function(self) 
        self:destroy()
        bCollapseAllBut = nil
    end,
    0.3)
end

function unCollapseAll()
    bUnCollapse = true
    tTimerUnCollapse = timer:new(function(self) 
        self:destroy()
        bUnCollapse = nil
    end,
    0.3)
end

function unCollapse(title)
    bUnCollapseMe = title
    tTimerUnCollapseMe = timer:new(function(self) 
        self:destroy()
        bUnCollapseMe = nil
    end,
    0.3)
end

function shouldICollapse(title)
    if type(bCollapseAllBut) == 'table' then
        local found = false
        for i=1, #bCollapseAllBut do
            local sTilte = bCollapseAllBut[i]
            if sTilte == title then
                found = true
            end
        end
        if found == false then
            tImGui.SetNextWindowCollapsed(true)
        elseif bUnCollapse then
            tImGui.SetNextWindowCollapsed(false)
        elseif bUnCollapseMe and found  then
            tImGui.SetNextWindowCollapsed(false)
        end
    elseif bCollapseAllBut and bCollapseAllBut ~= title then
        tImGui.SetNextWindowCollapsed(true)
    elseif bUnCollapse then
        tImGui.SetNextWindowCollapsed(false)
    elseif bUnCollapseMe == title  then
        tImGui.SetNextWindowCollapsed(false)
    end
end

function getUniqueNickName()
    iNumNickName            = iNumNickName + 1
    local nickName          = string.format('Unique_frame_%d',iNumNickName)
    return nickName
end

function newPrimitiveShape(tTexture,tVertexEdited,width,height)
    local tShape            = shape:new('2dw')
    local tUv               = {}
    local tNormal           = nil
    local tIndex            = {}
    local tVertex           = {}
    local tVertexBkp        = {}
    local nickName          = getUniqueNickName()
    local old_width         = width
    local old_height        = height
    if tVertexEdited == nil then
        
        for i=1, #tFrameAddOptions.tShapeEdit.vertex do
            local vertex  = tFrameAddOptions.tShapeEdit.vertex[i]
            table.insert(tVertex,vertex.x)
            table.insert(tVertex,vertex.y)
            table.insert(tUv, vertex.x / tTexture.width)  -- find u
            table.insert(tUv, vertex.y / tTexture.height) -- find v
        end

        --invert y position since comes from screend 2d
        for i=2, #tVertex, 2 do
            local y  = -tVertex[i]
            tVertex[i] = y
        end
        --find range
        local fMinX  =  9999999999999
        local fMinY  =  9999999999999
        local fMaxX  = -9999999999999
        local fMaxY  = -9999999999999
        for i=1, #tVertex, 2 do
            local x  = tVertex[i]
            local y  = tVertex[i+1]
            fMinX = math.min(fMinX,x)
            fMinY = math.min(fMinY,y)
            fMaxX = math.max(fMaxX,x)
            fMaxY = math.max(fMaxY,y)
        end
        local xRange = fMaxX - fMinX
        local yRange = fMaxY - fMinY
        local half_size_x, half_size_y = xRange / 2 , yRange / 2

        --put in the center
        for i=1, #tVertex, 2 do
            local x      = tVertex[i]
            local y      = tVertex[i+1]
            tVertex[i  ] = x - fMinX - half_size_x --put x in the center
            tVertex[i+1] = y - fMinY - half_size_y --put y in the center
        end

        --invert index since we inverted y
        for i=1, #tFrameAddOptions.tShapeEdit.index, 3 do
            table.insert(tIndex,tFrameAddOptions.tShapeEdit.index[i + 2])
            table.insert(tIndex,tFrameAddOptions.tShapeEdit.index[i + 1])
            table.insert(tIndex,tFrameAddOptions.tShapeEdit.index[i    ])
        end

        width      = xRange
        height     = yRange
        old_width  = xRange
        old_height = yRange
        tVertexBkp = tVertex
    else
        old_width     = tVertexEdited.old_width
        old_height    = tVertexEdited.old_height
        tIndex        = tVertexEdited.index
        local sx      = width  / tVertexEdited.old_width
        local sy      = height / tVertexEdited.old_height
        local vertex  = tVertexEdited.vertex
        for i=1, #vertex, 2 do
            local x      = vertex[i]   * sx
            local y      = vertex[i+1] * sy
            tVertex[i]   = x
            tVertex[i+1] = y
        end
        tVertexBkp = vertex
    end
    tShape:createDynamicIndexed(tVertex,tIndex,tUv,tNormal,nickName)
    tShape:setTexture(tTexture.file_name)
    tShape.tVertexEdited     = { vertex = tVertexBkp, index = tIndex, old_width = old_width, old_height = old_height}
    tShape.visible           = true
    tShape.bFirstRender      = true
    tShape.bInvertUFirst     = tFrameAddOptions.bInvertUFrameOptions
    tShape.bInvertVFirst     = tFrameAddOptions.bInvertVFrameOptions

    tShape:onRender(onRenderShape) -- first render
    return tShape, width, height
end

function newRectShape(tTexture,width,height,iNumElements,tMin,tMax)
    local tShape            = shape:new('2dw')
    local dynamic_buffer    = true
    local nickName          = getUniqueNickName()
    tShape:create('rectangle',width,height,iNumElements,dynamic_buffer,nickName)
    tShape:setTexture(tTexture.file_name)
    tShape.visible          = true
    tShape.bFirstRender     = true
    tShape.tUvSpriteSheet   = { xMin            = tMin.x,
                                yMin            = tMin.y,
                                xMax            = tMax.x,
                                yMax            = tMax.y,
                                textureWidth    = tTexture.width,
                                textureHeight   = tTexture.height}

    tShape.bInvertUFirst   = tFrameAddOptions.bInvertUFrameOptions
    tShape.bInvertVFirst   = tFrameAddOptions.bInvertVFrameOptions
    tShape:onRender(onRenderShape) -- first render
    return tShape
end

function newShape(type,tTexture,width,height,iNumElements)
    local tShape            = shape:new('2dw')
    local dynamic_buffer    = true
    tShape.bFirstRender     = true
    tShape.visible          = true
    local nickName          = getUniqueNickName()
    tShape:create(type,width,height,iNumElements,dynamic_buffer,nickName)
    tShape:setTexture(tTexture.file_name)
    tShape.bInvertUFirst   = tFrameAddOptions.bInvertUFrameOptions
    tShape.bInvertVFirst   = tFrameAddOptions.bInvertVFrameOptions
    tShape:onRender(onRenderShape) -- first render
    return tShape
end

function newFrameFromFrameAddOptions(tTexture)
    local tFrame            = {}
    if tFrameAddOptions.iIndexPrimitiveType  == 1 then
        tFrame.type = 'rectangle'
    elseif tFrameAddOptions.iIndexPrimitiveType  == 2 then
        tFrame.type = 'circle'
    else
        tFrame.type = 'triangle'
    end
    tFrame.iNumElements = tFrameAddOptions.iNumElements
    tFrame.tTexture     = tTexture
    tFrame.width        = tTexture.width
    tFrame.height       = tTexture.height
    tFrame.tSubsetList  = {}
    tFrame.tPivot       = {x = 0, y = 0}
    tFrame.tShape       = newShape(tFrame.type,tTexture,tTexture.width,tTexture.height,tFrame.iNumElements)
    return tFrame
end

function newRectFrameFromFrameAddOptions(tTexture,tMin,tMax)
    local tFrame        = {}
    tFrame.type         = 'rectangle'
    tFrame.iNumElements = tFrameAddOptions.iNumElements
    tFrame.tTexture     = tTexture
    if tFrameAddOptions.bStretch then
        tFrame.width        = tFrameAddOptions.iSizeFrameWidth
        tFrame.height       = tFrameAddOptions.iSizeFrameHeight
    else
        local width         = tFrameAddOptions.iSizeFrameWidth
        local height        = tFrameAddOptions.iSizeFrameHeight
        local xRange        = tMax.x - tMin.x
        local yRange        = tMax.y - tMin.y
        if width == xRange and height == yRange then
            tFrame.width        = tFrameAddOptions.iSizeFrameWidth
            tFrame.height       = tFrameAddOptions.iSizeFrameHeight
        elseif width > height then
            local new_width     = width
            local sy            = new_width / xRange  * yRange
            tFrame.width        = new_width
            tFrame.height       = sy
        else
            local new_height    = height
            local sx            = new_height / yRange  * xRange
            tFrame.height       = new_height
            tFrame.width        = sx
        end
    end
    tFrame.tSubsetList           = {}
    tFrame.tPivot                = {x = 0, y = 0}
    tFrame.tShape                = newRectShape(tTexture,tFrame.width,tFrame.height,tFrame.iNumElements,tMin,tMax)
    return tFrame
end

function newPrimitiveFrameFromFrameAddOptions(tTexture,width,height)
    local tFrame                              = {}
    local tVertexEdited                       = nil --from editor
    local old_width, old_height               = width,height
    tFrame.type                               = 'triangle'
    tFrame.iNumElements                       = tFrameAddOptions.iNumElements
    tFrame.tTexture                           = tTexture
    tFrame.tSubsetList                        = {}
    tFrame.tPivot                             = {x = 0, y = 0}
    tFrame.tShape,tFrame.width,tFrame.height  = newPrimitiveShape(tTexture,tVertexEdited,width,height)
    return tFrame
end

function onOpenImage()
    tPivotShape.visible = false
    local file_name = mbm.openMultiFile(sLastTextureOpenned,table.unpack(tUtil.supported_images))
    if file_name then
        tTexturesToEditor = tUtil.loadInfoImagesToTable(file_name,tTexturesToEditor)
        bTextureViewOpened = true
        if type(file_name) == 'string' then
            sLastTextureOpenned = file_name
        elseif type(file_name) == 'table' and #file_name > 0 then
            sLastTextureOpenned = file_name[1]
        end
        unCollapse(tWindowsTitle.title_image_selector)
    end
end

function onEditAnimations()
    closePhysicsWindow()
    closeFrameEdit()
    bShowAnimationEdit = true
    bShowFrameEdit     = false
    bEnableMoveWorld   = true
    bShowFrameList     = false
    tex_alpha_pattern.visible = true
    tPivotShape.visible = false
    collapseAllBut(tWindowsTitle.title_animation)
end

function onEditPhysics()
    closeAnimationWindow()
    closeFrameEdit()
    bShowEditPhysics = true
    tex_alpha_pattern.visible = true
    collapseAllBut(tWindowsTitle.title_edit_physics)
end

function onShowAddFrame()
    tFrameAddOptions.bShowAddFrame = true
    tPivotShape.visible = false
    unCollapse(tWindowsTitle.title_add_frame)
end

function onNewSpriteEditor()
    onNewSprite()
    tUtil.showMessage('New Sprite!')
    sLastEditorFileName = ''
end

function onOpenSprite()
    local file_name = mbm.openFile(sLastEditorFileName,'*.sprite')
    if file_name then
        onNewSprite()
        dofile(file_name)
        sLastEditorFileName = file_name
        bShowAddFrameOnceWhenSelectedTexture = true
        tUtil.showMessage('Sprite Editor Loaded!')
    end
end

function onSaveSpriteEditor()
    if sLastEditorFileName:len() == 0 then
        local file_name = mbm.saveFile(sLastEditorFileName,'*.sprite')
        if file_name then
            if onSaveEditionSprite(file_name) then
                sLastEditorFileName = file_name
                tUtil.showMessage('Sprite Editor Saved Successfully!!')
            else
                tUtil.showMessageWarn('Failed to Save Editor of Sprite!')
            end
        end
    else
        if onSaveEditionSprite(sLastEditorFileName) then
            tUtil.showMessage('Sprite Editor Saved Successfully!!')
        else
            tUtil.showMessageWarn('Failed to Save Editor of Sprite!')
        end
    end
end

function onSaveSpriteBinary()
    local file_name = mbm.saveFile(sLastSpriteOpenned,'*.spt')
    if file_name then
        if onSaveSprite(file_name) then
            sLastSpriteOpenned = file_name
            tUtil.showMessage('Sprite Saved Successfully!!')
        end
    end
end

function onTouchDown(key,x,y)
    isClickedMouseLeft = key == 1
    camera2d.mx = x
    camera2d.my = y
    if tPivotShape.visible and tPivotShape:isOver(x,y) then
        tPivotShape.bFollowMouse = true
        bMovingAnyPoint = true
    end
end

function onTouchMove(key,x,y)
    if bEnableMoveWorld and isClickedMouseLeft and not bMovingAnyPoint and not tImGui.IsAnyWindowHovered() then
        local px = (camera2d.mx - x) * camera2d.sx
        local py = (camera2d.my - y) * camera2d.sy
        camera2d.mx = x
        camera2d.my = y
        camera2d:setPos(camera2d.x + px,camera2d.y - py)
    end
    if tPivotShape.visible then
        if tPivotShape.bFollowMouse then
            local x,y = mbm.to2dw(x,y)
            tPivotShape:setPos(x,y)
            tPivotShape.tPivot.x = x
            tPivotShape.tPivot.y = y
        end
        if tPivotShape:isOver(x,y) then
            tPivotShape:setColor(1,1,0,1)
        else
            tPivotShape:setColor(1,0,0,1)
        end
    end
end

function onTouchUp(key,x,y)
    isClickedMouseLeft = false
    tPivotShape.bFollowMouse = false
    bMovingAnyPoint = false
    camera2d.mx = x
    camera2d.my = y
end

function onTouchZoom(zoom)
    local IsAnyWindowHovered = tImGui.IsAnyWindowHovered()
    if bEnableResizeOnScroll and not IsAnyWindowHovered then
        local tFrame = getSelectedFrame()
        if tFrame and tFrame.tShape.visible then
            local width  = tFrame.width +  ( zoom * iStep_zoom)
            local height = tFrame.height + ( zoom * iStep_zoom)
            if width > 0 and height > 0 then
                resizeShape(tFrame,width,height)
            end
        end
    end
    if IsAnyWindowHovered == false and bShowAnimationEdit then
        tAnimationOptions.tScaleAnim.sx = tAnimationOptions.tScaleAnim.sx + (zoom * 0.1)
        if tAnimationOptions.tScaleAnim.sx < 0.1 then
            tAnimationOptions.tScaleAnim.sx = 0.1
        elseif tAnimationOptions.tScaleAnim.sx > 10 then
            tAnimationOptions.tScaleAnim.sx = 10
        end
        tAnimationOptions.tScaleAnim.sy = tAnimationOptions.tScaleAnim.sx
    end
    if IsAnyWindowHovered == false and bShowEditPhysics then
        tPhysicsOptions.tScalePhysics.sx = tPhysicsOptions.tScalePhysics.sx + (zoom * 0.1)
        if tPhysicsOptions.tScalePhysics.sx < 0.1 then
            tPhysicsOptions.tScalePhysics.sx = 0.1
        elseif tPhysicsOptions.tScalePhysics.sx > 10 then
            tPhysicsOptions.tScalePhysics.sx = 10
        end
        tPhysicsOptions.tScalePhysics.sy = tPhysicsOptions.tScalePhysics.sx
    end
end

function onKeyDown(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = true
    elseif keyControlPressed then
        if key == mbm.getKeyCode('S') then -- Ctrl+S
            onSaveSpriteEditor()
        elseif key == mbm.getKeyCode('O') then -- Ctrl+O
            onOpenSprite()
        elseif key == mbm.getKeyCode('N') then -- Ctrl+N
            onNewSpriteEditor()
        elseif key == mbm.getKeyCode('I') then -- Ctrl+I
            onOpenImage()
        elseif key == mbm.getKeyCode('A') then -- Ctrl+A
            onEditAnimations()
        elseif key == mbm.getKeyCode('F') then -- Ctrl+F
            onShowAddFrame()
        elseif key == mbm.getKeyCode('B') then -- Ctrl+B
            onSaveSpriteBinary()
        elseif key == mbm.getKeyCode('P') then -- Ctrl+P
            onEditPhysics()
        end
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = false
    end
end

function loop(delta)
    main_menu_sprite()
    if bTextureViewOpened then
        shouldICollapse(tWindowsTitle.title_image_selector)
        local closed_clicked = tUtil.showTextureAssets(tWindowsTitle.title_image_selector,tTexturesToEditor,0,0,ImGuiWindowFlags_NoMove == 0)
        if closed_clicked then
            bTextureViewOpened = false
        end
    end
    if bShowAddFrameOnceWhenSelectedTexture == nil and hasTextureSelected(tTexturesToEditor) then
        tFrameAddOptions.bShowAddFrame = true
        bShowAddFrameOnceWhenSelectedTexture = true
        unCollapse(tWindowsTitle.title_add_frame)
    end
    if tFrameAddOptions.bShowAddFrame then
        showFrameAdd()
    end
    
    if bShowFrameList then
        showFrameList()
    end
    if bShowFrameEdit then
        showFrameEdit()
    end
    if tFrameAddOptions.bShowFramePreview then
        showFramePreview()
    end
    if bShowAnimationEdit then
        showAnimationAdd(delta)
    end
    if bShowAdvancedOptions then
        showAdvancedOptions()
    end
    if bShowEditPhysics then
        showEditPhysics()
    end
    tUtil.showOverlayMessage()
    if #tFrameList > 0 and #tPhysicsOptions.tLinesPhysics == 0 and tFrameList[1].tShape.bFirstRender == false then
        addPhysics(tFrameList[1].tShape)
        for i=1, #tFrameList[1].tSubsetList do
            addPhysics(tFrameList[1].tSubsetList[i].tShape)
        end
    end
    tex_alpha_pattern:setPos(camera2d.x,camera2d.y)
end
