--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2020-2026 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

   Scene Editor 2D — Grid / Panel Based Layout

   This editor lives entirely in 2dw world-space; 
   the resolution rectangle is a layout guide only, not a simulated viewport.
        - Panels are defined by world-space rects, either anchored to parent (2ds) or absolute (2dw).
        - Objects are parented to panels and anchored within them, with optional clamping to stay inside.
        - Panels can be nested to create complex layouts, with visual cues for depth.
        - Objects can be freely moved or resized; anchors and clamping keep them organized within panels.
        - The editor supports resolution-independent design: change the resolution and panels/objects reflow according to their anchors.
   This is a script based on mbm engine.

   Scene Editor 2D with grid/panel system for resolution-independent 2D scene layout.

   More info at: https://mbm-documentation.readthedocs.io/en/latest/editors.html#scene-editor-2d

]]--

tImGui        = require "ImGui"
tUtil         = require "editor_utils"

-- ─────────────────────────────────────────────────────────────────────────────
-- Constants
-- ─────────────────────────────────────────────────────────────────────────────
local PANEL_DEPTH_COLORS = {
    {r=0.50, g=0.50, b=0.50, a=0.15},  -- depth 0: gray
    {r=0.30, g=0.50, b=0.80, a=0.15},  -- depth 1: blue
    {r=0.30, g=0.70, b=0.40, a=0.15},  -- depth 2: green
    {r=0.80, g=0.75, b=0.20, a=0.15},  -- depth 3: yellow
    {r=0.85, g=0.50, b=0.20, a=0.15},  -- depth 4: orange
    {r=0.70, g=0.30, b=0.70, a=0.15},  -- depth 5: purple
    {r=0.20, g=0.75, b=0.75, a=0.15},  -- depth 6: teal
}

local PANEL_BORDER_COLORS = {
    {r=0.60, g=0.60, b=0.60},  -- depth 0
    {r=0.40, g=0.60, b=1.00},  -- depth 1
    {r=0.30, g=0.85, b=0.45},  -- depth 2
    {r=1.00, g=0.90, b=0.20},  -- depth 3
    {r=1.00, g=0.60, b=0.20},  -- depth 4
    {r=0.85, g=0.40, b=0.85},  -- depth 5
    {r=0.25, g=0.90, b=0.90},  -- depth 6
}

local SELECTED_BORDER_COLOR = {r=1, g=1, b=0}

-- ─────────────────────────────────────────────────────────────────────────────
-- Resolution table (preserved from original editor)
-- ─────────────────────────────────────────────────────────────────────────────
local tResolution = {
    {x = 800 , y = 600  , comment = 'XVGA'},
    {x = 1024, y = 768  , comment = ''},
    {x = 1280, y = 720  , comment = 'Standard High Density (HD)'},
    {x = 1280, y = 736  , comment = ''},
    {x = 1280, y = 752  , comment = ''},
    {x = 1280, y = 768  , comment = 'WXGA'},
    {x = 1334, y = 750  , comment = 'Apple phones only'},
    {x = 1280, y = 800  , comment = 'WXGA'},
    {x = 1920, y = 1080 , comment = 'Standard Full HD Display'},
    {x = 2360, y = 1640 , comment = 'iPad Air / iPad Pro'},
    {x = 2560, y = 1440 , comment = 'Standard Quad HD Display'},
    {x = 3840, y = 2160 , comment = 'Standard Ultra HD Display'},
}

-- ─────────────────────────────────────────────────────────────────────────────
-- Forward declarations / globals set in onInitScene
-- ─────────────────────────────────────────────────────────────────────────────
local camera2d
local tLineCenterX, tLineCenterY, tLineScreen2d
local tex_alpha_pattern
local tWindowsArea
local ImGuiWindowFlags_NoMove
local ImGuiTreeNodeFlags_Selected
local ImGuiTreeNodeFlags_DefaultOpen

-- Editor state
local tPanels         = {}      -- tree of panels
local tFreeMeshes     = {}      -- objects not in any panel
local tSelectedPanel  = nil     -- currently selected panel ref
local tSelectedObjs   = {}      -- selected object list (for move/properties)
local tAllMesh        = {}      -- flat list of ALL objects (panels + free)
local iNextPanelId    = 1       -- auto-increment panel id counter

-- UI state
local bShowPanelBrowser   = true
local bShowPanelProps     = false
local bShowMeshList       = true
local bShowAddingMesh     = false
local bShowDetailOfMesh   = true
local bShowGridDialog     = false
local bEnableMoveWorld    = true
local tDragObj            = nil   -- object being dragged in panel browser
local tDropTarget         = nil   -- panel (or sentinel) hovered during drag
local FREE_ZONE_SENTINEL  = {}    -- unique sentinel meaning "drop to Free"
local bEnableMoveWindow   = false
local bClickedOverAnyMesh = false
local bMovingAnyMesh      = false
local isClickedMouseLeft  = false
local keyControlPressed   = false
local keyShiftPressed     = false
local sLastMeshAdd        = ''
local sLastEditorFileName = ''
local tFollowCam          = nil
local tLastMeshAdded      = nil
local cCoroutineLoadScene = nil
local v1                  = nil

-- sTextureShape* for object selection outlines
local sTextureShapeOver    = '#86FF8686'
local sTextureShapeSelected= '#8686FF48'

-- Options (editor + launch)
local tOptionsEditor = {}
local tOptionsLaunch = {}

-- Filter
local tFilter = {
    tWorld      = {'All', '2D World', '2D Screen'},
    tType       = {'All Type','Font','Gif','Mesh','Particle','Sprite','Texture','Tile'},
    tPhysicType = {'All Type','Static','Dynamic','Kinematic','Character'},
}

local tPhysicEditor = {
    tType       = {'None','Static','Dynamic','Kinematic','Character'},
    Static      = {type='static',    density=0, friction=0.3,                  scaleX=1, scaleY=1, sensor=false},
    Dynamic     = {type='dynamic',   density=1, friction=10, restitution=0.1, scaleX=1, scaleY=1, sensor=false, bullet=false},
    Kinematic   = {type='kinematic', density=1, friction=10, restitution=0.1, scaleX=1, scaleY=1, sensor=false, bullet=false},
    Character   = {type='character', density=1, friction=10, restitution=0.1, scaleX=1, scaleY=1, sensor=false, bullet=false},
}

-- Window titles (ImGui unique IDs)
local tWindowsTitle = {
    title_panel_browser    = "title_panel_browser",
    title_panel_props      = "title_panel_props",
    title_meshes           = "title_meshes",
    title_mesh_info        = "title_mesh_info",
    title_loading          = "title_loading",
    title_adding_mesh      = "title_adding_mesh",
    title_grid_dialog      = "title_grid_dialog",
    title_obj_assignment   = "title_obj_assignment",
    title_transform_quick  = "title_transform_quick",
}

-- Grid dialog state
local tGridDialog = {
    iCols = 2,
    iRows = 2,
    sColPct = "50,50",
    sRowPct = "50,50",
    bCustomCols = false,
    bCustomRows = false,
    sName = "panel",
    iWorldIndex = 1,  -- 1=2dw, 2=2ds
    tWorldOptions = {"2D World", "2D Screen"},
}

-- ─────────────────────────────────────────────────────────────────────────────
-- Panel data model functions
-- ─────────────────────────────────────────────────────────────────────────────

--- Get depth color (cycles through palette)
local function getDepthFillColor(depth)
    local idx = (depth % #PANEL_DEPTH_COLORS) + 1
    return PANEL_DEPTH_COLORS[idx]
end

local function getDepthBorderColor(depth)
    local idx = (depth % #PANEL_BORDER_COLORS) + 1
    return PANEL_BORDER_COLORS[idx]
end

--- Generate a unique panel id string
local function generatePanelId()
    local id = iNextPanelId
    iNextPanelId = iNextPanelId + 1
    return id
end

--- Get the expected resolution (accounting for invert flag)
local function getExpectedResolution()
    local entry = tResolution[tOptionsEditor.iIndexResolution]
    if tOptionsEditor.bInvertResolution then
        return entry.y, entry.x
    else
        return entry.x, entry.y
    end
end

--- Compute a panel's absolute pixel rect given parent bounds
--- For 2ds panels: anchor-based relative to parent
--- For 2dw panels: world position + size (centered at origin like the resolution rect)
local function computePanelRect(panel, parentRect)
    if panel.world == "2ds" then
        local pw  = parentRect.w
        local ph  = parentRect.h
        local pat = panel.panelAnchorType or "stretch"
        if pat == "center" then
            local w  = (panel.sizeW or 1) * pw
            local h  = (panel.sizeH or 1) * ph
            local cx = parentRect.x + (panel.cx or 0.5) * pw
            local cy = parentRect.y + (panel.cy or 0.5) * ph
            return { x = cx - w * 0.5, y = cy - h * 0.5, w = w, h = h }
        elseif pat == "width" then
            local w  = (panel.sizeW or 1) * pw
            local h  = (panel.sizeW or 1) * pw
            local cx = parentRect.x + (panel.cx or 0.5) * pw
            local cy = parentRect.y + (panel.cy or 0.5) * ph
            return { x = cx - w * 0.5, y = cy - h * 0.5, w = w, h = h }
        elseif pat == "height" then
            local w  = (panel.sizeH or 1) * ph
            local h  = (panel.sizeH or 1) * ph
            local cx = parentRect.x + (panel.cx or 0.5) * pw
            local cy = parentRect.y + (panel.cy or 0.5) * ph
            return { x = cx - w * 0.5, y = cy - h * 0.5, w = w, h = h }
        elseif pat == "width_prop" then
            -- both axes scale with parent WIDTH; clamped to fit parent (like isRestrictedToPanel)
            local w  = (panel.sizeW or 1) * pw
            local h  = (panel.sizeH or 1) * pw
            local sc = math.min(pw / w, ph / h)
            if sc < 1 then w = w * sc; h = h * sc end
            local cx = parentRect.x + (panel.cx or 0.5) * pw
            local cy = parentRect.y + (panel.cy or 0.5) * ph
            return { x = cx - w * 0.5, y = cy - h * 0.5, w = w, h = h }
        elseif pat == "height_prop" then
            -- both axes scale with parent HEIGHT; clamped to fit parent (like isRestrictedToPanel)
            local w  = (panel.sizeW or 1) * ph
            local h  = (panel.sizeH or 1) * ph
            local sc = math.min(pw / w, ph / h)
            if sc < 1 then w = w * sc; h = h * sc end
            local cx = parentRect.x + (panel.cx or 0.5) * pw
            local cy = parentRect.y + (panel.cy or 0.5) * ph
            return { x = cx - w * 0.5, y = cy - h * 0.5, w = w, h = h }
        else -- "stretch" or missing
            local a = panel.anchor
            return {
                x = parentRect.x + a.left * pw,
                y = parentRect.y + a.top  * ph,
                w = (a.right - a.left) * pw,
                h = (a.bottom - a.top) * ph,
            }
        end
    else -- 2dw
        local wp = panel.worldPos
        local ws = panel.worldSize
        return {
            x = wp.x - ws.w * 0.5,
            y = wp.y - ws.h * 0.5,
            w = ws.w,
            h = ws.h,
        }
    end
end

--- Get the root rect (scene bounds) based on expected resolution
local function getRootRect()
    local xRes, yRes = getExpectedResolution()
    return {x = -xRes * 0.5, y = -yRes * 0.5, w = xRes, h = yRes}
end

--- Create a new panel table
local function createPanel(name, world, anchor, worldPos, worldSize, depth)
    return {
        id              = generatePanelId(),
        name            = name or "panel",
        world           = world or "2dw",
        anchor          = anchor or {left=0, top=0, right=1, bottom=1},
        panelAnchorType = "stretch",
        cx              = 0.5,
        cy              = 0.5,
        sizeW           = 1.0,
        sizeH           = 1.0,
        worldPos        = worldPos  or {x=0, y=0},
        worldSize       = worldSize or {w=400, h=300},
        depth           = depth or 0,
        lineRef         = nil,
        shapeRef        = nil,
        objects         = {},
        children        = {},
    }
end

--- Destroy visual handles for a panel (recursive)
local function destroyPanelVisuals(panel)
    if panel.lineRef then
        panel.lineRef:destroy()
        panel.lineRef = nil
    end
    if panel.shapeRef then
        panel.shapeRef:destroy()
        panel.shapeRef = nil
    end
    for _, child in ipairs(panel.children) do
        destroyPanelVisuals(child)
    end
end

--- Destroy a panel and all its children, removing objects from tAllMesh
local function destroyPanel(panel)
    -- destroy child panels first
    for _, child in ipairs(panel.children) do
        destroyPanel(child)
    end
    -- destroy objects inside this panel
    for _, obj in ipairs(panel.objects) do
        obj:destroy()
        if obj.tShape then obj.tShape:destroy() end
        for j = #tAllMesh, 1, -1 do
            if tAllMesh[j] == obj then
                table.remove(tAllMesh, j)
                break
            end
        end
    end
    panel.objects = {}
    destroyPanelVisuals(panel)
end

--- Remove a panel from its parent's children list (or from tPanels root list)
local function removePanelFromParent(panel, parentList)
    parentList = parentList or tPanels
    for i, p in ipairs(parentList) do
        if p == panel then
            table.remove(parentList, i)
            return true
        end
        if removePanelFromParent(panel, p.children) then
            return true
        end
    end
    return false
end

--- Find the parent list that contains a panel
local function findParentList(panel, parentList)
    parentList = parentList or tPanels
    for _, p in ipairs(parentList) do
        if p == panel then return parentList end
        local found = findParentList(panel, p.children)
        if found then return found end
    end
    return nil
end

--- Propagate a world type to all descendants of a panel
local function propagateWorldToChildren(panel, world)
    for _, child in ipairs(panel.children) do
        child.world = world
        propagateWorldToChildren(child, world)
    end
end

--- Returns true if the panel is at the root level (has no parent panel)
local function isRootPanel(panel)
    for _, p in ipairs(tPanels) do
        if p == panel then return true end
    end
    return false
end

--- Traverse all panels calling fn(panel, parentRect, depth)
local function traversePanels(panels, parentRect, depth, fn)
    for _, panel in ipairs(panels) do
        panel.depth = depth
        local rect = computePanelRect(panel, parentRect)
        panel._rect = rect  -- cache for use in rendering/interaction
        fn(panel, rect, depth)
        traversePanels(panel.children, rect, depth + 1, fn)
    end
end

--- Rebuild visual line + shape objects for all panels
local function rebuildPanelVisuals()
    -- first destroy all existing visuals
    for _, panel in ipairs(tPanels) do
        destroyPanelVisuals(panel)
    end

    local rootRect = getRootRect()
    traversePanels(tPanels, rootRect, 0, function(panel, rect, depth)
        -- Create border line
        local ln = line:new("2dw")
        local x1, y1 = rect.x, rect.y
        local x2, y2 = rect.x + rect.w, rect.y + rect.h
        ln:add({x1,y1, x1,y2, x2,y2, x2,y1, x1,y1})
        local bc = getDepthBorderColor(depth)
        ln:setColor(bc.r, bc.g, bc.b)
        ln.z = -90 - depth  -- always in front of scene objects; deeper panels on top
        panel.lineRef = ln

        -- Create fill shape
        local sh = shape:new("2dw")
        sh:setPos((x1+x2)*0.5, (y1+y2)*0.5)
        sh:setScale(rect.w, rect.h, 1)
        local fc = getDepthFillColor(depth)
        local colorHex = string.format('#%02X%02X%02X%02X',
            math.floor(fc.a*255), math.floor(fc.r*255),
            math.floor(fc.g*255), math.floor(fc.b*255))
        sh:setTexture(colorHex)
        sh.z = -91 - depth  -- fill behind its own border but in front of scene objects
        panel.shapeRef = sh
    end)
end

--- Returns position extents for bounds clamping: le (left), re (right), be (bottom), te (top).
--- Font objects have top-left origin; all others use center origin.
local function getObjExtents(obj, ow, oh)
    if obj.type == "font" then
        return 0, ow, oh, 0
    end
    local hw, hh = ow * 0.5, oh * 0.5
    return hw, hw, hh, hh
end

--- Reflow: reposition all objects within panels according to their anchors,
--- then clamp scale and position so no object exceeds its panel bounds.
local function reflowPanelObjects()
    local rootRect = getRootRect()
    traversePanels(tPanels, rootRect, 0, function(panel, rect, depth)
        for _, obj in ipairs(panel.objects) do
            local restricted = obj.isRestrictedToPanel ~= false
            local atype      = obj.anchorType or "center"

            -- 0. Apply size-anchor: recompute scale from panel fractions
            local ow, oh = obj:getSize()
            if atype == "width" and obj.sizeAnchorW and ow > 0 and rect.w > 0 then
                -- drive both axes proportionally from width
                local scale = (rect.w * obj.sizeAnchorW) / ow
                obj.sx = obj.sx * scale
                obj.sy = obj.sy * scale
                ow, oh = obj:getSize()
                if obj.tShape then obj.tShape:setScale(ow, oh, 1) end
            elseif atype == "height" and obj.sizeAnchorH and oh > 0 and rect.h > 0 then
                -- drive both axes proportionally from height
                local scale = (rect.h * obj.sizeAnchorH) / oh
                obj.sx = obj.sx * scale
                obj.sy = obj.sy * scale
                ow, oh = obj:getSize()
                if obj.tShape then obj.tShape:setScale(ow, oh, 1) end
            elseif atype == "stretch" then
                -- independent per axis (can distort)
                if obj.sizeAnchorW and ow > 0 and rect.w > 0 then
                    local naturalW = ow / obj.sx
                    if naturalW > 0 then obj.sx = (rect.w * obj.sizeAnchorW) / naturalW end
                end
                if obj.sizeAnchorH and oh > 0 and rect.h > 0 then
                    local naturalH = oh / obj.sy
                    if naturalH > 0 then obj.sy = (rect.h * obj.sizeAnchorH) / naturalH end
                end
                ow, oh = obj:getSize()
                if obj.tShape then obj.tShape:setScale(ow, oh, 1) end
            end

            -- 1. Cap absolute scale so object fits inside the panel (only when restricted,
            --    and only for axes not governed by a size anchor)
            ow, oh = obj:getSize()
            if restricted and ow > 0 and oh > 0 and rect.w > 0 and rect.h > 0 then
                if ow > rect.w or oh > rect.h then
                    local scale = math.min(rect.w / ow, rect.h / oh)
                    obj.sx = obj.sx * scale
                    obj.sy = obj.sy * scale
                    ow, oh = obj:getSize()
                    if obj.tShape then obj.tShape:setScale(ow, oh, 1) end
                end
            end

            -- 2. Place by anchor
            local ox = rect.x + obj.anchorX * rect.w
            local oy = rect.y + obj.anchorY * rect.h

            -- 3. Clamp edges so the object stays inside the panel (only when restricted)
            -- Font objects have top-left origin; all others use center origin.
            if restricted then
                local le, re, be, te = getObjExtents(obj, ow, oh)
                ox = math.max(rect.x + le, math.min(rect.x + rect.w - re, ox))
                oy = math.max(rect.y + be, math.min(rect.y + rect.h - te, oy))
            end

            obj:setPos(ox, oy, obj.z)

            -- 4. Update anchor to reflect (possibly clamped) position
            if rect.w > 0 then obj.anchorX = (ox - rect.x) / rect.w end
            if rect.h > 0 then obj.anchorY = (oy - rect.y) / rect.h end
        end
    end)
end

--- Update panel visuals without full rebuild (just move existing line/shape)
local function updatePanelVisuals()
    local rootRect = getRootRect()
    traversePanels(tPanels, rootRect, 0, function(panel, rect, depth)
        if panel.lineRef then
            local x1, y1 = rect.x, rect.y
            local x2, y2 = rect.x + rect.w, rect.y + rect.h
            panel.lineRef:set({x1,y1, x1,y2, x2,y2, x2,y1, x1,y1}, 1)
            if panel == tSelectedPanel then
                panel.lineRef:setColor(SELECTED_BORDER_COLOR.r, SELECTED_BORDER_COLOR.g, SELECTED_BORDER_COLOR.b)
            else
                local bc = getDepthBorderColor(depth)
                panel.lineRef:setColor(bc.r, bc.g, bc.b)
            end
        end
        if panel.shapeRef then
            local cx = rect.x + rect.w * 0.5
            local cy = rect.y + rect.h * 0.5
            panel.shapeRef:setPos(cx, cy)
            panel.shapeRef:setScale(rect.w, rect.h, 1)
        end
    end)
end

-- ─────────────────────────────────────────────────────────────────────────────
-- Resolution rectangle (preserved from original)
-- ─────────────────────────────────────────────────────────────────────────────
-- The editor works in 2dw world-space throughout. scaleToScreen is intentionally
-- NOT called here: the camera zoom is the user's own, and the resolution rectangle
-- is only a planning guide. Runtime reflow is handled by the exported tScene.reflow().
local _lastResX, _lastResY = nil, nil

--- Reflow free (non-panel) objects that are 2ds + isRelative2ds when the
--- expected resolution changes. Position and scale are remapped proportionally
--- so the object keeps the same fractional placement within the resolution rect.
local function reflowFreeRelativeObjects(oldW, oldH, newW, newH)
    if oldW == 0 or oldH == 0 then return end
    local ratioX = newW / oldW
    local ratioY = newH / oldH
    for _, obj in ipairs(tFreeMeshes) do
        if obj.is2ds and obj.isRelative2ds then
            -- Scale: uniform scale using the smaller ratio to preserve shape (no deformation)
            local ratio = math.min(ratioX, ratioY)
            obj.sx = obj.sx * ratio
            obj.sy = obj.sy * ratio
            local ow, oh = obj:getSize()
            if obj.tShape then obj.tShape:setScale(ow, oh, 1) end
            -- Position: remap proportionally within the resolution rect
            local nx = obj.x * ratioX
            local ny = obj.y * ratioY
            obj:setPos(nx, ny, obj.z)
            if obj.tShape then obj.tShape.x = nx; obj.tShape.y = ny end
        end
    end
end

local function updateRectangleLine()
    local xRes, yRes = getExpectedResolution()
    local r = {-xRes/2,-yRes/2, -xRes/2,yRes/2, xRes/2,yRes/2, xRes/2,-yRes/2, -xRes/2,-yRes/2}
    tLineScreen2d:set(r, 1)
    -- reflow panel objects only when resolution actually changes (not every menu frame)
    if xRes ~= _lastResX or yRes ~= _lastResY then
        local prevX = _lastResX
        local prevY = _lastResY
        _lastResX, _lastResY = xRes, yRes
        updatePanelVisuals()
        reflowPanelObjects()
        if prevX and prevY then
            reflowFreeRelativeObjects(prevX, prevY, xRes, yRes)
        end
    end
end

-- ─────────────────────────────────────────────────────────────────────────────
-- Hit-test: find which panel a world-space point is inside (deepest first)
-- ─────────────────────────────────────────────────────────────────────────────
local function pointInRect(px, py, rect)
    return px >= rect.x and px <= rect.x + rect.w
       and py >= rect.y and py <= rect.y + rect.h
end

local function hitTestPanel(px, py, panels, parentRect)
    -- iterate in reverse so last (topmost) panel wins
    for i = #panels, 1, -1 do
        local panel = panels[i]
        local rect = computePanelRect(panel, parentRect)
        -- check children first (deeper panels)
        local childHit = hitTestPanel(px, py, panel.children, rect)
        if childHit then return childHit end
        if pointInRect(px, py, rect) then
            return panel
        end
    end
    return nil
end

-- ─────────────────────────────────────────────────────────────────────────────
-- Object selection helpers (preserved from original)
-- ─────────────────────────────────────────────────────────────────────────────
local function setSelectedObj(tObj, bValue)
    if bValue then
        if not tObj.isSelected then
            tObj.bJustSelected = true
            tObj.tShape:setTexture(sTextureShapeSelected)
        else
            tObj.bJustSelected = false
        end
        tObj.isSelected     = true
        tObj.tShape.visible = true
        local found = false
        for _, that in ipairs(tSelectedObjs) do
            if that == tObj then found = true; break end
        end
        if not found then
            table.insert(tSelectedObjs, tObj)
        end
    else
        tObj.isSelected     = false
        tObj.tShape.visible = false
        for i, that in ipairs(tSelectedObjs) do
            if that == tObj then
                table.remove(tSelectedObjs, i)
                break
            end
        end
    end
end

local function onUnSelectAll()
    for _, tObj in ipairs(tSelectedObjs) do
        tObj.isSelected     = false
        tObj.tShape.visible = false
    end
    tSelectedObjs = {}
end

local function filter(tObj)
    if tOptionsEditor.iIndexWorldMesh > 1 then
        if tOptionsEditor.iIndexWorldMesh == 2 then
            if tObj.is2ds then return false end
        elseif tOptionsEditor.iIndexWorldMesh == 3 then
            if not tObj.is2ds then return false end
        end
    end
    if tOptionsEditor.iIndexTypeMeshFilter > 1 then
        local sType = tFilter.tType[tOptionsEditor.iIndexTypeMeshFilter]:lower()
        if tObj.type ~= sType then return false end
    end
    if tOptionsEditor.iIndexTypePhysicsFilter > 1 then
        local sType = tFilter.tPhysicType[tOptionsEditor.iIndexTypePhysicsFilter]:lower()
        if not tObj.tPhysicInfo or tObj.tPhysicInfo.type ~= sType then return false end
    end
    return true
end

local function onSelectAll()
    tSelectedObjs = {}
    for _, tObj in ipairs(tAllMesh) do
        if filter(tObj) and not tObj.isBlocked then
            tObj.isSelected     = true
            tObj.tShape.visible = true
            table.insert(tSelectedObjs, tObj)
        else
            tObj.isSelected     = false
            tObj.tShape.visible = false
        end
    end
end

local function onDeleteSelected()
    for _, tObj in ipairs(tSelectedObjs) do
        for j = #tAllMesh, 1, -1 do
            if tAllMesh[j] == tObj then
                table.remove(tAllMesh, j)
                break
            end
        end
        -- also remove from panel objects list
        if tObj.panelRef then
            for j = #tObj.panelRef.objects, 1, -1 do
                if tObj.panelRef.objects[j] == tObj then
                    table.remove(tObj.panelRef.objects, j)
                    break
                end
            end
        else
            for j = #tFreeMeshes, 1, -1 do
                if tFreeMeshes[j] == tObj then
                    table.remove(tFreeMeshes, j)
                    break
                end
            end
        end
        tObj:destroy()
        if tObj.tShape then tObj.tShape:destroy() end
    end
    tSelectedObjs = {}
end

local function onInvertSelection()
    tSelectedObjs = {}
    for _, tObj in ipairs(tAllMesh) do
        if filter(tObj) and not tObj.isBlocked then
            if tObj.isSelected then
                tObj.isSelected     = false
                tObj.tShape.visible = false
            else
                tObj.isSelected     = true
                tObj.tShape.visible = true
                table.insert(tSelectedObjs, tObj)
            end
        else
            tObj.isSelected     = false
            tObj.tShape.visible = false
        end
    end
end

local function updateVisibilityByFilter()
    for _, tObj in ipairs(tAllMesh) do
        if filter(tObj) then
            tObj.visible = true
        else
            tObj.visible = false
            tObj.tShape.visible = false
        end
    end
end

-- ─────────────────────────────────────────────────────────────────────────────
-- New scene / reset
-- ─────────────────────────────────────────────────────────────────────────────
local function onNewSceneEditor()
    -- clear objects
    if tSelectedObjs then onSelectAll(); onDeleteSelected() end
    -- clear panels
    for _, panel in ipairs(tPanels) do
        destroyPanel(panel)
    end
    tPanels        = {}
    tFreeMeshes    = {}
    tSelectedPanel = nil
    tSelectedObjs  = {}
    tAllMesh       = {}
    iNextPanelId   = 1

    tOptionsEditor = {
        iIndexResolution         = 1,
        bInvertResolution        = false,
        bDrawResolution          = true,
        fSceneCamPos             = {x=0, y=0},
        tColorBackground         = {r=tUtil.tColorBackground.r, g=tUtil.tColorBackground.g, b=tUtil.tColorBackground.b},
        iIndexWorldMesh          = 1,
        iIndexTypeMeshFilter     = 1,
        iIndexTypePhysicsFilter  = 1,
        sScaleAxis               = 'y',
        sCurrentScriptExecution  = '',
        sExtraScript             = '',
    }

    tOptionsLaunch = {
        iIndexResolution  = 1,
        bInvertResolution = false,
    }

    sLastMeshAdd        = ''
    tLastMeshAdded      = nil
    tFollowCam          = nil
    sLastEditorFileName = ''
    keyControlPressed   = false
    keyShiftPressed     = false
    bShowMeshList       = true
    bShowAddingMesh     = false
    bShowPanelBrowser   = true
    bShowPanelProps     = false
    bShowDetailOfMesh   = true
    -- invalidate resolution cache so next updateRectangleLine() detects a change
    _lastResX, _lastResY = nil, nil
end

-- ─────────────────────────────────────────────────────────────────────────────
-- Camera following (preserved from original)
-- ─────────────────────────────────────────────────────────────────────────────
local function cameraFollowing()
    local iSpeedCam = 1000
    local iW, iH = mbm.getSizeScreen()
    if tManuallyMoveCam then
        camera2d:move(tManuallyMoveCam.x * iSpeedCam, tManuallyMoveCam.y * iSpeedCam)
        return
    end
    if not tFollowCam then 
        return 
    end
    local bAnyFollow = false
    local w, h = tFollowCam:getSize()
    if (w > iW or h > iH) and tFollowCam:isOnScreen() then
        tFollowCam = nil
        return
    end
    if camera2d.x + (iW*0.5) < (tFollowCam.x + w*0.5) then
        camera2d:move(iSpeedCam, 0); bAnyFollow = true
    elseif camera2d.x - (iW*0.5) > (tFollowCam.x - w*0.5) then
        camera2d:move(-iSpeedCam, 0); bAnyFollow = true
    end
    if camera2d.y + (iH*0.5) < (tFollowCam.y + h*0.5) then
        camera2d:move(0, iSpeedCam); bAnyFollow = true
    elseif camera2d.y - (iH*0.5) > (tFollowCam.y - h*0.5) then
        camera2d:move(0, -iSpeedCam); bAnyFollow = true
    end
    if not bAnyFollow and w < (iW*0.15) and h < (iH*0.15) then
        camera2d.iIteration = camera2d.iIteration + 1
        iSpeedCam = 200
        if camera2d.x + (iW*0.15) < (tFollowCam.x + w*0.5) then
            camera2d:move(iSpeedCam, 0); bAnyFollow = true
        elseif camera2d.x - (iW*0.15) > (tFollowCam.x - w*0.5) then
            camera2d:move(-iSpeedCam, 0); bAnyFollow = true
        end
        if camera2d.y + (iH*0.15) < (tFollowCam.y + h*0.5) then
            camera2d:move(0, iSpeedCam); bAnyFollow = true
        elseif camera2d.y - (iH*0.15) > (tFollowCam.y - h*0.5) then
            camera2d:move(0, -iSpeedCam); bAnyFollow = true
        end
        if camera2d.iIteration > 120 then
            camera2d.iIteration = 0
            bAnyFollow = nil
        end
    end
end

-- ─────────────────────────────────────────────────────────────────────────────
-- Loading progress overlay (preserved from original)
-- ─────────────────────────────────────────────────────────────────────────────
local function onProgress(fPercent)
    local flags = {'ImGuiWindowFlags_NoDecoration','ImGuiWindowFlags_AlwaysAutoResize',
                   'ImGuiWindowFlags_NoSavedSettings','ImGuiWindowFlags_NoFocusOnAppearing','ImGuiWindowFlags_NoNav'}
    local iW, iH = mbm.getRealSizeScreen()
    tImGui.SetNextWindowBgAlpha(0.85)
    tImGui.SetNextWindowPos({x=0,y=0}, tImGui.Flags('ImGuiCond_Always'), {x=0,y=0})
    tImGui.SetNextWindowSize({x=iW,y=iH}, tImGui.Flags('ImGuiCond_Always'))
    local is_opened = tImGui.Begin(tLang.L(tWindowsTitle.title_loading), false, tImGui.Flags(flags))
    if is_opened then
        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_FrameBg'), {r=0,g=0,b=0.5,a=0.7})
        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_PlotHistogram'), {r=0,g=1,b=0,a=1})
        tImGui.SetCursorScreenPos({x=0, y=iH*0.5})
        tImGui.ProgressBar(fPercent * 0.01, {x=-1,y=0}, '')
        tImGui.PopStyleColor(2)
        tImGui.SetCursorScreenPos({x=iW*0.48, y=(iH*0.5)+3})
        tImGui.Text(string.format('Loading %.1f', fPercent))
    end
    tImGui.End()
end

-- ─────────────────────────────────────────────────────────────────────────────
-- Pass 2 — Panel Browser, Panel Properties, Grid Dialog
-- ─────────────────────────────────────────────────────────────────────────────

--- Helper: parse a comma-separated string of numbers into a table
local function parsePercentages(str, count)
    local parts = {}
    for num in str:gmatch("[^,]+") do
        local v = tonumber(num)
        if v then table.insert(parts, v) end
    end
    -- If count doesn't match, distribute evenly
    if #parts ~= count then
        parts = {}
        local each = 100 / count
        for i = 1, count do parts[i] = each end
    end
    return parts
end

--- Helper: check if percentages sum to ~100
local function percentagesValid(parts)
    local sum = 0
    for _, v in ipairs(parts) do sum = sum + v end
    return math.abs(sum - 100) < 0.5
end

-- Forward declarations needed by renderPanelTree and showPanelBrowser
local assignObjectToPanel, buildPanelComboList

--- Recursive tree rendering for panel browser
local function renderPanelTree(panels, parentRect)
    for i, panel in ipairs(panels) do
        local rect = computePanelRect(panel, parentRect)
        local flags = ImGuiTreeNodeFlags_DefaultOpen
        if panel == tSelectedPanel then
            flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Selected
        end
        local label = string.format("%s [%s] (%d obj)##%s",
            panel.name, panel.world, #panel.objects, panel.id)

        -- Color the text by depth; highlight panel when it is the active drop target
        local bc = getDepthBorderColor(panel.depth or 0)
        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), {r=bc.r, g=bc.g, b=bc.b, a=1})
        if tDragObj ~= nil and tDropTarget == panel then
            tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Header'),      {r=0.15, g=0.65, b=0.25, a=0.55})
            tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_HeaderHovered'),{r=0.20, g=0.75, b=0.30, a=0.65})
        end

        local nodeOpen = tImGui.TreeNodeEx(label, flags)

        if tDragObj ~= nil and tDropTarget == panel then
            tImGui.PopStyleColor(2)
        end
        tImGui.PopStyleColor(1)

        -- Update drop target while dragging; IsMouseHoveringRect ignores active-item blocking
        if tDragObj ~= nil then
            local rmin = tImGui.GetItemRectMin()
            local rmax = tImGui.GetItemRectMax()
            if tImGui.IsMouseHoveringRect(rmin, rmax, false) then
                tDropTarget = panel
            end
        end

        -- Click to select this panel
        if tImGui.IsItemClicked() then
            tSelectedPanel  = panel
            bShowPanelProps = true
            updatePanelVisuals()
        end

        -- Right-click context menu
        if tImGui.BeginPopupContextItem(panel.id .. "_ctx") then
            if tImGui.MenuItem(tLang.L("add_child_panel")) then
                local child = createPanel("child", panel.world,
                    {left=0, top=0, right=1, bottom=1},
                    {x=0, y=0}, {w=200, h=150},
                    (panel.depth or 0) + 1)
                table.insert(panel.children, child)
                tSelectedPanel = child
                bShowPanelProps = true
                rebuildPanelVisuals()
                tUtil.showMessage(string.format(tLang.L("panel_created_fmt"), child.name))
            end
            if tImGui.MenuItem(tLang.L("delete_panel")) then
                if #panel.children > 0 then
                    tUtil.showMessageWarn(tLang.L("panel_has_children_warn"))
                else
                    local pList = findParentList(panel)
                    if pList then
                        if panel == tSelectedPanel then tSelectedPanel = nil end
                        local panelName = panel.name
                        destroyPanel(panel)
                        removePanelFromParent(panel, pList)
                        rebuildPanelVisuals()
                        tUtil.showMessage(string.format(tLang.L("panel_deleted_fmt"), panelName))
                    end
                end
            end
            tImGui.EndPopup()
        end

        if nodeOpen then
            -- Show child panels
            if #panel.children > 0 then
                renderPanelTree(panel.children, rect)
            end
            -- Show objects inside panel (collapsed list)
            for j, obj in ipairs(panel.objects) do
                local objLabel = string.format("%s (%d)##obj_%s_%d", obj.type or "?", j, panel.id, j)
                if tImGui.Selectable(objLabel, obj.isSelected) then
                    if not keyControlPressed then onUnSelectAll() end
                    setSelectedObj(obj, true)
                    tFollowCam = obj
                end
                -- Drag start: hold left mouse and move
                if tDragObj == nil and tImGui.IsItemActive() and tImGui.IsMouseDragging(0, 6.0) then
                    tDragObj   = obj
                    tDropTarget = nil
                end
                -- Right-click: reassign popup
                if tImGui.BeginPopupContextItem("##ctx_panelobj_" .. panel.id .. "_" .. j) then
                    tImGui.Text(tLang.L("reassign_object") .. ": " .. (obj.type or "?"))
                    tImGui.Separator()
                    tImGui.Text(tLang.L("assign_to_panel"))
                    local tPanelList   = buildPanelComboList(tPanels)
                    local comboLabels  = {tLang.L("free_object")}
                    local currentIdx   = 1
                    for k, entry in ipairs(tPanelList) do
                        table.insert(comboLabels, entry.label)
                        if obj.panelRef == entry.panel then currentIdx = k + 1 end
                    end
                    local retA, newIdx = tImGui.Combo("##reassign_panelobj_" .. panel.id .. "_" .. j, currentIdx, comboLabels)
                    if retA then
                        if newIdx == 1 then
                            assignObjectToPanel(obj, nil)
                        else
                            assignObjectToPanel(obj, tPanelList[newIdx - 1].panel)
                        end
                        tImGui.CloseCurrentPopup()
                    end
                    tImGui.EndPopup()
                end
            end
            tImGui.TreePop()
        end
    end
end

--- Panel Browser window (left side)
showPanelBrowser = function()
    if not bShowPanelBrowser then return end
    local width = 300
    local iW, iH = mbm.getSizeScreen()
    tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_panel_browser, 0, iH*0.45, width, width + 50, iH * 0.5)
    local flags = bEnableMoveWindow and 0 or ImGuiWindowFlags_NoMove
    local is_opened, closed_clicked = tImGui.Begin(tLang.L("panel_browser"), true, flags)
    if is_opened then
        -- Root-level buttons
        if tImGui.Button(tLang.L("add_root_panel"), {x=-1, y=0}) then
            local p = createPanel("root_panel", "2dw",
                {left=0, top=0, right=1, bottom=1},
                {x=0, y=0}, {w=400, h=300}, 0)
            table.insert(tPanels, p)
            tSelectedPanel  = p
            bShowPanelProps = true
            rebuildPanelVisuals()
            tUtil.showMessage(string.format(tLang.L("panel_created_fmt"), p.name))
        end

        if tImGui.Button(tLang.L("add_grid_nxm"), {x=-1, y=0}) then
            bShowGridDialog = true
        end
        if tImGui.IsItemHovered(0) then
            tImGui.BeginTooltip()
            tImGui.Text(tLang.L("grid_tooltip_short"))
            tImGui.EndTooltip()
        end

        tImGui.Separator()

        -- Tree view
        if #tPanels == 0 then
            tImGui.TextDisabled(tLang.L("no_panel_selected"))
        else
            local rootRect = getRootRect()
            renderPanelTree(tPanels, rootRect)
        end

        -- Free objects section
        if #tFreeMeshes > 0 then
            tImGui.Separator()
            -- Highlight Free header when it is the active drop target
            if tDragObj ~= nil and tDropTarget == FREE_ZONE_SENTINEL then
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Header'),      {r=0.65, g=0.45, b=0.10, a=0.55})
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_HeaderHovered'),{r=0.75, g=0.55, b=0.15, a=0.65})
            end
            local freeNodeOpen = tImGui.TreeNodeEx(string.format("%s (%d)##free_objs", tLang.L("free_object"), #tFreeMeshes), ImGuiTreeNodeFlags_DefaultOpen)
            if tDragObj ~= nil and tDropTarget == FREE_ZONE_SENTINEL then
                tImGui.PopStyleColor(2)
            end
            -- Mark Free zone as drop target while dragging
            if tDragObj ~= nil then
                local rmin = tImGui.GetItemRectMin()
                local rmax = tImGui.GetItemRectMax()
                if tImGui.IsMouseHoveringRect(rmin, rmax, false) then
                    tDropTarget = FREE_ZONE_SENTINEL
                end
            end
            if freeNodeOpen then
                for j, obj in ipairs(tFreeMeshes) do
                    local objLabel = string.format("%s (%d)##free_%d", obj.type or "?", j, j)
                    if tImGui.Selectable(objLabel, obj.isSelected) then
                        if not keyControlPressed then onUnSelectAll() end
                        setSelectedObj(obj, true)
                        tFollowCam = obj
                    end
                    -- Drag start
                    if tDragObj == nil and tImGui.IsItemActive() and tImGui.IsMouseDragging(0, 6.0) then
                        tDragObj    = obj
                        tDropTarget = nil
                    end
                    -- Right-click: reassign popup
                    if tImGui.BeginPopupContextItem("##ctx_freeobj_" .. j) then
                        tImGui.Text(tLang.L("reassign_object") .. ": " .. (obj.type or "?"))
                        tImGui.Separator()
                        tImGui.Text(tLang.L("assign_to_panel"))
                        local tPanelList   = buildPanelComboList(tPanels)
                        local comboLabels  = {tLang.L("free_object")}
                        for k, entry in ipairs(tPanelList) do
                            table.insert(comboLabels, entry.label)
                        end
                        local retA, newIdx = tImGui.Combo("##reassign_freeobj_" .. j, 1, comboLabels)
                        if retA then
                            if newIdx == 1 then
                                assignObjectToPanel(obj, nil)
                            else
                                assignObjectToPanel(obj, tPanelList[newIdx - 1].panel)
                            end
                            tImGui.CloseCurrentPopup()
                        end
                        tImGui.EndPopup()
                    end
                end
                tImGui.TreePop()
            end
        end

        -- Drag-drop completion: release mouse
        if tDragObj ~= nil and tImGui.IsMouseReleased(0) then
            if tDropTarget == FREE_ZONE_SENTINEL then
                assignObjectToPanel(tDragObj, nil)
            elseif tDropTarget ~= nil then
                assignObjectToPanel(tDragObj, tDropTarget)
            end
            tDragObj    = nil
            tDropTarget = nil
        end
        -- Clear drop target each frame if not dragging
        if tDragObj == nil then tDropTarget = nil end
    end
    tWindowsArea:addThisWindow()
    tImGui.End()
    if closed_clicked then bShowPanelBrowser = false end

    -- Drag ghost: use BeginTooltip so it is always rendered on top of all windows
    if tDragObj ~= nil then
        tImGui.BeginTooltip()
        tImGui.Text("  " .. (tDragObj.type or "?") .. "  ")
        if tDropTarget == FREE_ZONE_SENTINEL then
            tImGui.TextDisabled(tLang.L("drag_hint_free"))
        elseif tDropTarget ~= nil then
            tImGui.TextDisabled("→ " .. tDropTarget.name)
        else
            tImGui.TextDisabled(tLang.L("drag_hint_no_target"))
        end
        tImGui.EndTooltip()
    end
end

--- Panel Properties window
showPanelProperties = function()
    if not bShowPanelProps or not tSelectedPanel then return end
    local width = 300
    local iW, iH = mbm.getSizeScreen()
    tUtil.setInitialWindowPositionRight(tWindowsTitle.title_panel_props, 0, 0, width, width + 50, iH * 0.45)
    local flags = bEnableMoveWindow and 0 or ImGuiWindowFlags_NoMove
    local is_opened, closed_clicked = tImGui.Begin(tLang.L("panel_properties"), true, flags)
    if is_opened then
        local panel = tSelectedPanel
        local step = 0.01
        local step_fast = 0.05
        local format = "%.3f"

        -- Name
        tImGui.Text(tLang.L("panel_name_label"))
        local modified, newName = tImGui.InputText("##panel_name", panel.name, 0)
        if modified and newName:len() > 0 then
            panel.name = newName
        end

        -- World type
        tImGui.Text(tLang.L("panel_world"))
        if isRootPanel(panel) then
            local tWorldOpts = {"2D World", "2D Screen"}
            local worldIdx = panel.world == "2ds" and 2 or 1
            local ret, newIdx = tImGui.Combo("##panel_world_combo", worldIdx, tWorldOpts)
            if ret then
                panel.world = newIdx == 2 and "2ds" or "2dw"
                propagateWorldToChildren(panel, panel.world)
                rebuildPanelVisuals()
            end
        else
            tImGui.TextDisabled(panel.world == "2ds" and "2D Screen (inherited)" or "2D World (inherited)")
        end

        tImGui.Separator()

        if panel.world == "2ds" then
            -- Panel anchor type combo
            tImGui.Text(tLang.L("panel_anchor_type"))
            local tPanelAnchorOpts = {
                tLang.L("panel_type_stretch"),
                tLang.L("panel_type_center"),
                tLang.L("panel_type_width_prop"),
                tLang.L("panel_type_height_prop"),
                tLang.L("panel_type_width"),
                tLang.L("panel_type_height"),
            }
            local tPanelAnchorKeys = {"stretch", "center", "width_prop", "height_prop", "width", "height"}
            local curPAT = panel.panelAnchorType or "stretch"
            local curPATIdx = 1
            for i, k in ipairs(tPanelAnchorKeys) do if k == curPAT then curPATIdx = i; break end end
            local patChanged, newPATIdx = tImGui.Combo("##panel_anchor_type", curPATIdx, tPanelAnchorOpts)
            if patChanged then
                local newPAT = tPanelAnchorKeys[newPATIdx]
                local r = panel._rect
                if curPAT == "stretch" then
                    -- stretch -> any: derive cx/cy/sizeW/sizeH from current anchor fractions
                    local a = panel.anchor
                    panel.cx    = (a.left + a.right)  * 0.5
                    panel.cy    = (a.top  + a.bottom) * 0.5
                    panel.sizeW = a.right - a.left
                    panel.sizeH = a.bottom - a.top
                    -- proportional: keep aspect ratio by adjusting the secondary fraction
                    if r and r.w > 0 and r.h > 0 then
                        if newPAT == "width_prop" then
                            panel.sizeH = (r.h / r.w) * panel.sizeW
                        elseif newPAT == "height_prop" then
                            panel.sizeW = (r.w / r.h) * panel.sizeH
                        end
                    end
                elseif newPAT == "stretch" then
                    -- any -> stretch: convert back
                    local hw = (panel.sizeW or 1) * 0.5
                    local hh = (panel.sizeH or 1) * 0.5
                    panel.anchor.left   = math.max(0, (panel.cx or 0.5) - hw)
                    panel.anchor.right  = math.min(1, (panel.cx or 0.5) + hw)
                    panel.anchor.top    = math.max(0, (panel.cy or 0.5) - hh)
                    panel.anchor.bottom = math.min(1, (panel.cy or 0.5) + hh)
                elseif newPAT == "width_prop" or newPAT == "height_prop" then
                    -- non-stretch -> proportional: fix secondary fraction to preserve aspect ratio
                    if r and r.w > 0 and r.h > 0 then
                        if newPAT == "width_prop" then
                            panel.sizeH = (r.h / r.w) * (panel.sizeW or 1)
                        elseif newPAT == "height_prop" then
                            panel.sizeW = (r.w / r.h) * (panel.sizeH or 1)
                        end
                    end
                end
                panel.panelAnchorType = newPAT
                curPAT = newPAT
                rebuildPanelVisuals()
                reflowPanelObjects()
            end

            tImGui.Separator()

            if curPAT == "stretch" then
                -- Anchor editors (left/top/right/bottom)
                tImGui.Text(tLang.L("panel_anchor_left"))
                local r1, v = tImGui.InputFloat("##anchor_l", panel.anchor.left, step, step_fast, format)
                if r1 and v >= 0 and v < panel.anchor.right then
                    panel.anchor.left = v
                    rebuildPanelVisuals()
                    reflowPanelObjects()
                end

                tImGui.Text(tLang.L("panel_anchor_top"))
                r1, v = tImGui.InputFloat("##anchor_t", panel.anchor.top, step, step_fast, format)
                if r1 and v >= 0 and v < panel.anchor.bottom then
                    panel.anchor.top = v
                    rebuildPanelVisuals()
                    reflowPanelObjects()
                end

                tImGui.Text(tLang.L("panel_anchor_right"))
                r1, v = tImGui.InputFloat("##anchor_r", panel.anchor.right, step, step_fast, format)
                if r1 and v > panel.anchor.left and v <= 1 then
                    panel.anchor.right = v
                    rebuildPanelVisuals()
                    reflowPanelObjects()
                end

                tImGui.Text(tLang.L("panel_anchor_bottom"))
                r1, v = tImGui.InputFloat("##anchor_b", panel.anchor.bottom, step, step_fast, format)
                if r1 and v > panel.anchor.top and v <= 1 then
                    panel.anchor.bottom = v
                    rebuildPanelVisuals()
                    reflowPanelObjects()
                end
            else
                -- Center-point + fractional size inputs
                tImGui.Text(tLang.L("panel_center_x"))
                local r1, v = tImGui.InputFloat("##panel_cx", panel.cx or 0.5, step, step_fast, format)
                if r1 then panel.cx = math.max(0, math.min(1, v)); rebuildPanelVisuals(); reflowPanelObjects() end

                tImGui.Text(tLang.L("panel_center_y"))
                r1, v = tImGui.InputFloat("##panel_cy", panel.cy or 0.5, step, step_fast, format)
                if r1 then panel.cy = math.max(0, math.min(1, v)); rebuildPanelVisuals(); reflowPanelObjects() end

                if curPAT == "width_prop" then
                    -- sizeW and sizeH are both fractions of parent WIDTH
                    tImGui.TextDisabled(tLang.L("panel_size_of_pw"))
                    tImGui.Text(tLang.L("panel_size_w"))
                    r1, v = tImGui.InputFloat("##panel_sw", panel.sizeW or 1, step, step_fast, format)
                    if r1 then panel.sizeW = math.max(0.001, v); rebuildPanelVisuals(); reflowPanelObjects() end
                    tImGui.Text(tLang.L("panel_size_h"))
                    r1, v = tImGui.InputFloat("##panel_sh", panel.sizeH or 1, step, step_fast, format)
                    if r1 then panel.sizeH = math.max(0.001, v); rebuildPanelVisuals(); reflowPanelObjects() end
                elseif curPAT == "height_prop" then
                    -- sizeW and sizeH are both fractions of parent HEIGHT
                    tImGui.TextDisabled(tLang.L("panel_size_of_ph"))
                    tImGui.Text(tLang.L("panel_size_w"))
                    r1, v = tImGui.InputFloat("##panel_sw", panel.sizeW or 1, step, step_fast, format)
                    if r1 then panel.sizeW = math.max(0.001, v); rebuildPanelVisuals(); reflowPanelObjects() end
                    tImGui.Text(tLang.L("panel_size_h"))
                    r1, v = tImGui.InputFloat("##panel_sh", panel.sizeH or 1, step, step_fast, format)
                    if r1 then panel.sizeH = math.max(0.001, v); rebuildPanelVisuals(); reflowPanelObjects() end
                else
                    if curPAT == "center" or curPAT == "width" then
                        tImGui.Text(tLang.L("panel_size_w"))
                        r1, v = tImGui.InputFloat("##panel_sw", panel.sizeW or 1, step, step_fast, format)
                        if r1 then panel.sizeW = math.max(0.01, math.min(1, v)); rebuildPanelVisuals(); reflowPanelObjects() end
                    end
                    if curPAT == "center" or curPAT == "height" then
                        tImGui.Text(tLang.L("panel_size_h"))
                        r1, v = tImGui.InputFloat("##panel_sh", panel.sizeH or 1, step, step_fast, format)
                        if r1 then panel.sizeH = math.max(0.01, math.min(1, v)); rebuildPanelVisuals(); reflowPanelObjects() end
                    end
                end
            end
        else
            -- World position/size
            local stepW = 1.0
            local stepW_fast = 10.0
            local fmtW = "%.1f"

            tImGui.Text(tLang.L("panel_world_pos_x"))
            local r1, v = tImGui.InputFloat("##wp_x", panel.worldPos.x, stepW, stepW_fast, fmtW)
            if r1 then panel.worldPos.x = v; rebuildPanelVisuals(); reflowPanelObjects() end

            tImGui.Text(tLang.L("panel_world_pos_y"))
            r1, v = tImGui.InputFloat("##wp_y", panel.worldPos.y, stepW, stepW_fast, fmtW)
            if r1 then panel.worldPos.y = v; rebuildPanelVisuals(); reflowPanelObjects() end

            tImGui.Text(tLang.L("panel_world_size_w"))
            r1, v = tImGui.InputFloat("##ws_w", panel.worldSize.w, stepW, stepW_fast, fmtW)
            if r1 and v > 0 then panel.worldSize.w = v; rebuildPanelVisuals(); reflowPanelObjects() end

            tImGui.Text(tLang.L("panel_world_size_h"))
            r1, v = tImGui.InputFloat("##ws_h", panel.worldSize.h, stepW, stepW_fast, fmtW)
            if r1 and v > 0 then panel.worldSize.h = v; rebuildPanelVisuals(); reflowPanelObjects() end
        end

        tImGui.Separator()

        -- Info
        if panel._rect then
            local r = panel._rect
            tImGui.TextDisabled(string.format("Computed: %.0f×%.0f at (%.0f,%.0f)", r.w, r.h, r.x, r.y))
        end
        tImGui.TextDisabled(string.format("Depth: %d  |  Objects: %d  |  Children: %d",
            panel.depth or 0, #panel.objects, #panel.children))

        tImGui.Separator()

        -- Delete button
        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r=0.7, g=0.1, b=0.1, a=1})
        if tImGui.Button(tLang.L("delete_panel"), {x=-1, y=0}) then
            if #panel.children > 0 then
                tUtil.showMessageWarn(tLang.L("panel_has_children_warn"))
            else
                local pList = findParentList(panel)
                if pList then
                    local panelName = panel.name
                    tSelectedPanel = nil
                    bShowPanelProps = false
                    destroyPanel(panel)
                    removePanelFromParent(panel, pList)
                    rebuildPanelVisuals()
                    tUtil.showMessage(string.format(tLang.L("panel_deleted_fmt"), panelName))
                end
            end
        end
        tImGui.PopStyleColor(1)
    end
    tWindowsArea:addThisWindow()
    tImGui.End()
    if closed_clicked then bShowPanelProps = false end
end

--- Quick Transform window — shown when exactly one object is selected.
-- Layout: World Type | Anchor (panel only) | Scale | Rotation | Animation
showTransformQuick = function()
    if #tSelectedObjs ~= 1 then return end
    local tObj = tSelectedObjs[1]

    local iW, iH = mbm.getRealSizeScreen()
    tUtil.setInitialWindowPositionRight(tWindowsTitle.title_transform_quick, 0, iH * 0.48, 300, 350, iH * 0.5)
    local flags = bEnableMoveWindow and 0 or ImGuiWindowFlags_NoMove

    -- Detect 3D: getAABB(true) returns depth only for 3D objects (same logic as setShapeToMesh)
    local _, _, d_aabb = tObj:getAABB(true)
    local is3d = (d_aabb ~= nil and d_aabb ~= 0)

    -- Panel context (mirrors treeNodeScale locals)
    local atype      = tObj.anchorType or "center"
    local rect       = (tObj.panelRef and tObj.panelRef._rect) and tObj.panelRef._rect or nil
    local restricted = tObj.isRestrictedToPanel ~= false

    local is_opened = tImGui.Begin(tLang.L("quick_transform"), false, flags)
    if is_opened then
        tImGui.PushItemWidth(240)

        -- ── World Type ────────────────────────────────────────────────────────
        tImGui.TextDisabled(tLang.L("world_type"))
        local tWorld = {'2D World', '2D Screen'}
        local iIndexWorldMesh = tObj.is2ds and 2 or 1
        local retW, newWorldIdx = tImGui.Combo('##tq_world' .. tObj.iIndex, iIndexWorldMesh, tWorld)
        if retW then
            if newWorldIdx == 1 then
                tObj.is2ds = false
                tObj.isRelative2ds = nil
            else
                tObj.is2ds = true
                tObj.isRelative2ds = true
            end
            if filter(tObj) then
                tObj.visible = true
            else
                tObj.visible = false
                tObj.tShape.visible = false
            end
        end

        -- ── Anchor (panel-assigned objects only) ──────────────────────────────
        if tObj.panelRef and tObj.anchorX ~= nil then
            tImGui.Separator()
            tImGui.TextDisabled(string.format("Panel: %s", tObj.panelRef.name))

            -- Anchor type
            tImGui.Text(tLang.L("anchor_type"))
            local tAnchorTypes = {
                tLang.L("anchor_type_center"),
                tLang.L("anchor_type_width"),
                tLang.L("anchor_type_height"),
                tLang.L("anchor_type_stretch"),
            }
            local tAnchorKeys = {"center", "width", "height", "stretch"}
            local atypeIdx = 1
            for ki, k in ipairs(tAnchorKeys) do
                if k == atype then atypeIdx = ki; break end
            end
            local retC, newAtypeIdx = tImGui.Combo("##tq_anchorType" .. tObj.iIndex, atypeIdx, tAnchorTypes)
            if retC then
                local newType = tAnchorKeys[newAtypeIdx]
                tObj.anchorType = newType
                atype = newType
                if rect then
                    local ow, oh = tObj:getSize()
                    if (newType == "width"  or newType == "stretch") and not tObj.sizeAnchorW then
                        tObj.sizeAnchorW = math.min(1, ow / rect.w)
                    end
                    if (newType == "height" or newType == "stretch") and not tObj.sizeAnchorH then
                        tObj.sizeAnchorH = math.min(1, oh / rect.h)
                    end
                end
                if     newType == "center"  then tObj.sizeAnchorW = nil; tObj.sizeAnchorH = nil
                elseif newType == "width"   then tObj.sizeAnchorH = nil
                elseif newType == "height"  then tObj.sizeAnchorW = nil
                end
            end

            -- Anchor X
            tImGui.Text(tLang.L("anchor_x"))
            local rAx, vAx = tImGui.InputFloat("##tq_anchorX" .. tObj.iIndex, tObj.anchorX, 0.01, 0.05, "%.3f")
            if rAx then
                tObj.anchorX = math.max(0, math.min(1, vAx))
                if rect then
                    local nx = rect.x + tObj.anchorX * rect.w
                    if restricted then
                        local ow = tObj:getSize()
                        local le, re = getObjExtents(tObj, ow, 0)
                        nx = math.max(rect.x + le, math.min(rect.x + rect.w - re, nx))
                    end
                    tObj:setPos(nx, tObj.y, tObj.z)
                    tObj.tShape.x = nx
                    if rect.w > 0 then tObj.anchorX = (nx - rect.x) / rect.w end
                end
            end

            -- Anchor Y
            tImGui.Text(tLang.L("anchor_y"))
            local rAy, vAy = tImGui.InputFloat("##tq_anchorY" .. tObj.iIndex, tObj.anchorY, 0.01, 0.05, "%.3f")
            if rAy then
                tObj.anchorY = math.max(0, math.min(1, vAy))
                if rect then
                    local ny = rect.y + tObj.anchorY * rect.h
                    if restricted then
                        local _, oh = tObj:getSize()
                        local _, _, be, te = getObjExtents(tObj, 0, oh)
                        ny = math.max(rect.y + be, math.min(rect.y + rect.h - te, ny))
                    end
                    tObj:setPos(tObj.x, ny, tObj.z)
                    tObj.tShape.y = ny
                    if rect.h > 0 then tObj.anchorY = (ny - rect.y) / rect.h end
                end
            end
        end

        tImGui.Separator()

        -- ── Position ──────────────────────────────────────────────────────────
        tImGui.Text(tLang.L("position"))
        local posSpeed  = 1.0
        local posFormat = "%.3f"

        local rPX, vPX = tImGui.DragFloat(tLang.L("axis_x") .. "##tq_px", tObj.x, posSpeed, 0.0, 0.0, posFormat)
        if rPX and not tObj.isBlockedX then
            if restricted and rect then
                local ow = tObj:getSize()
                local le, re = getObjExtents(tObj, ow, 0)
                vPX = math.max(rect.x + le, math.min(rect.x + rect.w - re, vPX))
            end
            tObj.x = vPX
            tObj.tShape.x = vPX
            if rect and rect.w > 0 then tObj.anchorX = (vPX - rect.x) / rect.w end
        end

        local rPY, vPY = tImGui.DragFloat(tLang.L("axis_y") .. "##tq_py", tObj.y, posSpeed, 0.0, 0.0, posFormat)
        if rPY and not tObj.isBlockedY then
            if restricted and rect then
                local _, oh = tObj:getSize()
                local _, _, be, te = getObjExtents(tObj, 0, oh)
                vPY = math.max(rect.y + be, math.min(rect.y + rect.h - te, vPY))
            end
            tObj.y = vPY
            tObj.tShape.y = vPY
            if rect and rect.h > 0 then tObj.anchorY = (vPY - rect.y) / rect.h end
        end

        local rPZ, vPZ = tImGui.DragFloat(tLang.L("axis_z") .. "##tq_pz", tObj.z, posSpeed, 0.0, 0.0, posFormat)
        if rPZ and not tObj.isBlockedZ then
            tObj.z = vPZ
            tObj.tShape.z = vPZ - 1
        end

        tImGui.Separator()

        -- ── Scale ─────────────────────────────────────────────────────────────
        tImGui.Text(tLang.L("scale"))
        local scaleSpeed  = 0.01
        local scaleMin    = 0.01
        local scaleMax    = 0.0   -- 0 = unbounded
        local scaleFormat = "%.3f"

        -- SX — only when the anchor type doesn't drive this axis (same guard as treeNodeScale)
        if atype == "center" or atype == "height" then
            local rSX, vSX = tImGui.DragFloat(tLang.L("scale_sx") .. "##tq_sx", tObj.sx, scaleSpeed, scaleMin, scaleMax, scaleFormat)
            if rSX and vSX >= scaleMin then
                -- Cap so object width does not exceed panel width
                if rect then
                    local naturalW = tObj:getSize() / tObj.sx
                    if naturalW > 0 then vSX = math.min(vSX, rect.w / naturalW) end
                end
                tObj.sx = vSX
                local w, h, d = tObj:getSize()
                tObj.tShape.sx = w
                -- Shift X if edges overflow after scale
                if restricted and rect then
                    local le, re = getObjExtents(tObj, w, 0)
                    local cx = math.max(rect.x + le, math.min(rect.x + rect.w - re, tObj.x))
                    if cx ~= tObj.x then tObj.x = cx; tObj.tShape.x = cx end
                    if rect.w > 0 then tObj.anchorX = (tObj.x - rect.x) / rect.w end
                end
            end
        end

        -- SY — only when the anchor type doesn't drive this axis
        if atype == "center" or atype == "width" then
            local rSY, vSY = tImGui.DragFloat(tLang.L("scale_sy") .. "##tq_sy", tObj.sy, scaleSpeed, scaleMin, scaleMax, scaleFormat)
            if rSY and vSY >= scaleMin then
                -- Cap so object height does not exceed panel height
                if rect then
                    local _, naturalH = tObj:getSize()
                    naturalH = naturalH / tObj.sy
                    if naturalH > 0 then vSY = math.min(vSY, rect.h / naturalH) end
                end
                tObj.sy = vSY
                local w, h, d = tObj:getSize()
                tObj.tShape.sy = h
                -- Shift Y if edges overflow after scale
                if restricted and rect then
                    local _, _, be, te = getObjExtents(tObj, 0, h)
                    local cy = math.max(rect.y + be, math.min(rect.y + rect.h - te, tObj.y))
                    if cy ~= tObj.y then tObj.y = cy; tObj.tShape.y = cy end
                    if rect.h > 0 then tObj.anchorY = (tObj.y - rect.y) / rect.h end
                end
            end
        end

        -- SZ — 3D only, no panel clamping on depth
        if is3d then
            local rSZ, vSZ = tImGui.DragFloat(tLang.L("scale_sz") .. "##tq_sz", tObj.sz, scaleSpeed, scaleMin, scaleMax, scaleFormat)
            if rSZ and vSZ >= scaleMin then
                tObj.sz = vSZ
                local w, h, d = tObj:getSize()
                if d then tObj.tShape.sz = d end
            end
        end

        tImGui.Separator()

        -- ── Rotation ──────────────────────────────────────────────────────────
        tImGui.Text(tLang.L("angle"))
        local rotSpeed  = 1.0
        local rotMin    = -360.0
        local rotMax    =  360.0
        local rotFormat = "%.2f"

        if is3d then
            local rAX, vAX = tImGui.DragFloat(tLang.L("angle_ax") .. "##tq_ax", math.deg(tObj.ax), rotSpeed, rotMin, rotMax, rotFormat)
            if rAX then
                local rad = math.rad(vAX)
                tObj.ax        = rad
                tObj.tShape.ax = rad
            end

            local rAY, vAY = tImGui.DragFloat(tLang.L("angle_ay") .. "##tq_ay", math.deg(tObj.ay), rotSpeed, rotMin, rotMax, rotFormat)
            if rAY then
                local rad = math.rad(vAY)
                tObj.ay        = rad
                tObj.tShape.ay = rad
            end
        end

        local rAZ, vAZ = tImGui.DragFloat(tLang.L("angle_az") .. "##tq_az", math.deg(tObj.az), rotSpeed, rotMin, rotMax, rotFormat)
        if rAZ then
            local rad = math.rad(vAZ)
            tObj.az        = rad
            tObj.tShape.az = rad
        end

        -- ── Animation ─────────────────────────────────────────────────────────
        local totalAnim = tObj:getTotalAnim()
        if totalAnim > 0 then
            tImGui.Separator()
            tImGui.Text(tLang.L("animation"))
            local tAnimations = {}
            for i = 1, totalAnim do
                table.insert(tAnimations, string.format('%d:  %s', i, select(1, tObj:getAnim(i))))
            end
            local current_item = select(2, tObj:getAnim())
            local retAnim, new_item = tImGui.Combo("##tq_anim" .. tObj.iIndex, current_item, tAnimations, -1)
            if retAnim then
                tObj:setAnim(new_item)
            end
        end

        tImGui.PopItemWidth()
    end
    tWindowsArea:addThisWindow()
    tImGui.End()
end

--- Grid Creation Dialog (popup-style window)
showGridDialog = function()
    if not bShowGridDialog then return end

    local iW, iH = mbm.getRealSizeScreen()
    tImGui.SetNextWindowPos({x=iW*0.3, y=iH*0.2}, tImGui.Flags('ImGuiCond_Once'), {x=0,y=0})
    tImGui.SetNextWindowSize({x=380, y=0}, tImGui.Flags('ImGuiCond_Once'))

    local is_opened, closed_clicked = tImGui.Begin(tLang.L("title_grid_dialog"), true,
        tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoSavedSettings'))
    if is_opened then
        -- Target panel (or root)
        if tSelectedPanel then
            tImGui.Text(string.format("Target: %s", tSelectedPanel.name))
        else
            tImGui.Text("Target: Scene Root")
        end

        tImGui.Separator()

        -- Name prefix
        tImGui.Text(tLang.L("panel_name_label"))
        local mod, newName = tImGui.InputText("##grid_name", tGridDialog.sName, 0)
        if mod then tGridDialog.sName = newName end

        -- World type
        tImGui.Text(tLang.L("panel_world"))
        if tSelectedPanel then
            tImGui.TextDisabled(tSelectedPanel.world == "2ds" and "2D Screen (inherited)" or "2D World (inherited)")
        else
            local retW, idxW = tImGui.Combo("##grid_world", tGridDialog.iWorldIndex, tGridDialog.tWorldOptions)
            if retW then tGridDialog.iWorldIndex = idxW end
        end

        tImGui.Separator()

        -- Columns
        tImGui.Text(tLang.L("grid_cols"))
        local retC, newCols = tImGui.InputInt("##grid_cols", tGridDialog.iCols, 1, 1)
        if retC and newCols >= 1 and newCols <= 20 then tGridDialog.iCols = newCols end

        tGridDialog.bCustomCols = tImGui.Checkbox(tLang.L("grid_custom_pct"), tGridDialog.bCustomCols)
        if tGridDialog.bCustomCols then
            local modP, newPct = tImGui.InputText("##col_pct", tGridDialog.sColPct, 0)
            if modP then tGridDialog.sColPct = newPct end
        end

        -- Rows
        tImGui.Text(tLang.L("grid_rows"))
        local retR, newRows = tImGui.InputInt("##grid_rows", tGridDialog.iRows, 1, 1)
        if retR and newRows >= 1 and newRows <= 20 then tGridDialog.iRows = newRows end

        tGridDialog.bCustomRows = tImGui.Checkbox(tLang.L("grid_custom_row_pct"), tGridDialog.bCustomRows)
        if tGridDialog.bCustomRows then
            local modP, newPct = tImGui.InputText("##row_pct", tGridDialog.sRowPct, 0)
            if modP then tGridDialog.sRowPct = newPct end
        end

        tImGui.Separator()

        tImGui.TextWrapped(tLang.L("grid_help_full"))

        tImGui.Separator()

        -- Create button
        if tImGui.Button(tLang.L("create_grid"), {x=-1, y=0}) then
            -- Parse percentages
            local colParts
            if tGridDialog.bCustomCols then
                colParts = parsePercentages(tGridDialog.sColPct, tGridDialog.iCols)
                if not percentagesValid(colParts) then
                    tUtil.showMessageWarn(tLang.L("grid_sum_not_100"))
                    colParts = nil
                end
            else
                colParts = parsePercentages("", tGridDialog.iCols)
            end

            local rowParts
            if tGridDialog.bCustomRows then
                rowParts = parsePercentages(tGridDialog.sRowPct, tGridDialog.iRows)
                if not percentagesValid(rowParts) then
                    tUtil.showMessageWarn(tLang.L("grid_row_sum_not_100"))
                    rowParts = nil
                end
            else
                rowParts = parsePercentages("", tGridDialog.iRows)
            end

            if colParts and rowParts then
                local world = tSelectedPanel and tSelectedPanel.world
                    or (tGridDialog.iWorldIndex == 2 and "2ds" or "2dw")
                local targetList = tSelectedPanel and tSelectedPanel.children or tPanels
                local parentDepth = tSelectedPanel and (tSelectedPanel.depth or 0) or -1

                -- For 2dw grids without a parent panel, compute from resolution
                local parentRect
                if tSelectedPanel then
                    parentRect = tSelectedPanel._rect or computePanelRect(tSelectedPanel, getRootRect())
                else
                    parentRect = getRootRect()
                end

                -- Build columns cumulative fractions
                local colFrac = {0}
                for c = 1, #colParts do
                    colFrac[c+1] = colFrac[c] + colParts[c] / 100
                end
                local rowFrac = {0}
                for r = 1, #rowParts do
                    rowFrac[r+1] = rowFrac[r] + rowParts[r] / 100
                end

                -- Create panels
                for r = 1, tGridDialog.iRows do
                    for c = 1, tGridDialog.iCols do
                        local name = string.format("%s_%d_%d", tGridDialog.sName, r, c)
                        local anchor = {
                            left   = colFrac[c],
                            top    = rowFrac[r],
                            right  = colFrac[c+1],
                            bottom = rowFrac[r+1],
                        }

                        local wp, ws
                        if world == "2dw" then
                            -- Convert anchor to world coordinates
                            local x1 = parentRect.x + anchor.left * parentRect.w
                            local y1 = parentRect.y + anchor.top  * parentRect.h
                            local x2 = parentRect.x + anchor.right * parentRect.w
                            local y2 = parentRect.y + anchor.bottom * parentRect.h
                            wp = {x = (x1+x2)*0.5, y = (y1+y2)*0.5}
                            ws = {w = x2-x1, h = y2-y1}
                        else
                            wp = {x=0, y=0}
                            ws = {w=400, h=300}
                        end

                        local p = createPanel(name, world, anchor, wp, ws, parentDepth + 1)
                        table.insert(targetList, p)
                    end
                end

                rebuildPanelVisuals()
                bShowGridDialog = false
                tUtil.showMessage(string.format("%d×%d grid created.", tGridDialog.iRows, tGridDialog.iCols))
            end
        end
    end
    tWindowsArea:addThisWindow()
    tImGui.End()
    if closed_clicked then bShowGridDialog = false end
end

-- Forward declarations for Pass 3 functions (assigned below)
local onAddMesh, onDuplicated, showMeshList, showAddingMeshOptions, showDetailOfMesh
-- Forward declaration for Pass 4 stub (used inside showMeshList)
local showPropertiesForMesh = function(tObj) end

-- ─────────────────────────────────────────────────────────────────────────────
-- Pass 3 — Mesh list, object assignment, add mesh, duplicate
-- ─────────────────────────────────────────────────────────────────────────────

--- Assign an object to a panel (or free). Updates anchorX/Y from current position.
assignObjectToPanel = function(tObj, panel)
    -- Remove from previous panel / free list
    if tObj.panelRef then
        for j = #tObj.panelRef.objects, 1, -1 do
            if tObj.panelRef.objects[j] == tObj then
                table.remove(tObj.panelRef.objects, j)
                break
            end
        end
    else
        for j = #tFreeMeshes, 1, -1 do
            if tFreeMeshes[j] == tObj then
                table.remove(tFreeMeshes, j)
                break
            end
        end
    end

    if panel then
        tObj.panelRef = panel
        tObj.is2ds    = (panel.world == "2ds")
        table.insert(panel.objects, tObj)
        -- Compute anchor from current world position
        if panel._rect then
            local r = panel._rect
            if r.w > 0 and r.h > 0 then
                tObj.anchorX = (tObj.x - r.x) / r.w
                tObj.anchorY = (tObj.y - r.y) / r.h
            else
                tObj.anchorX = 0.5
                tObj.anchorY = 0.5
            end
        else
            tObj.anchorX = 0.5
            tObj.anchorY = 0.5
        end
        -- Auto-fit if object exceeds panel bounds
        if panel._rect then
            local ow, oh = tObj:getSize()
            local maxW = panel._rect.w
            local maxH = panel._rect.h
            if ow > 0 and oh > 0 and (ow > maxW or oh > maxH) then
                local scale = math.min(maxW / ow, maxH / oh)
                tObj.sx = tObj.sx * scale
                tObj.sy = tObj.sy * scale
                local w2, h2 = tObj:getSize()
                if tObj.tShape then tObj.tShape:setScale(w2, h2, 1) end
                tUtil.showMessage(tLang.L("auto_scaled_to_fit"))
            end
        end
        tUtil.showMessage(string.format(tLang.L("object_assign_panel_fmt"), panel.name))
        -- Reflow so the object is clamped inside its new panel if isRestrictedToPanel
        reflowPanelObjects()
    else
        tObj.panelRef = nil
        tObj.anchorX  = nil
        tObj.anchorY  = nil
        tObj.is2ds    = false
        table.insert(tFreeMeshes, tObj)
        tUtil.showMessage(tLang.L("object_assign_free"))
    end
end

--- Add a loaded mesh object into the editor tracking lists
local function initialSetUpForAddedMesh(tObj)
    table.insert(tAllMesh, tObj)
    tObj.index = #tAllMesh
    tObj.isSelected          = false
    tObj.isBlocked           = false
    tObj.isBlockedX          = false
    tObj.isBlockedY          = false
    tObj.isBlockedZ          = false
    tObj.isRestrictedToPanel = true   -- default: respect panel bounds

    -- Assign to selected panel, or keep as free
    if tSelectedPanel then
        assignObjectToPanel(tObj, tSelectedPanel)
        -- Position at panel center
        if tSelectedPanel._rect then
            local r = tSelectedPanel._rect
            tObj:setPos(r.x + r.w * 0.5, r.y + r.h * 0.5)
            tObj.anchorX = 0.5
            tObj.anchorY = 0.5
        end
    else
        tObj.panelRef = nil
        tObj.anchorX  = nil
        tObj.anchorY  = nil
        table.insert(tFreeMeshes, tObj)
    end

    tLastMeshAdded = tObj
    tFollowCam     = tObj
end

--- Add mesh via file picker (preserved flow from original)
onAddMesh = function()
    local fileName = mbm.openMultiFile(sLastMeshAdd,
        "tile","spt","ptl","png","msh","fnt","jpeg","jpg","bmp","gif","psd","pic","pnm","hdr","tga","tif")
    if fileName then
        if type(fileName) == 'string' then
            local tMeshTmp = tUtil.onAddMeshToEditor(fileName, true, "2dw")
            if tMeshTmp then
                initialSetUpForAddedMesh(tMeshTmp)
                bShowMeshList = true
                sLastMeshAdd  = fileName
            else
                tUtil.showMessageWarn(tLang.L("failed_to_add_mesh"))
            end
        elseif type(fileName) == 'table' then
            local width = 0
            for i = 1, #fileName do
                local tMeshTmp = tUtil.onAddMeshToEditor(fileName[i], true, "2dw")
                if tMeshTmp then
                    tMeshTmp.x = tMeshTmp.x + width
                    local w, h = tMeshTmp:getSize()
                    width = width + w
                    initialSetUpForAddedMesh(tMeshTmp)
                    sLastMeshAdd  = fileName[i]
                    bShowMeshList = true
                else
                    tUtil.showMessageWarn(tLang.L("failed_to_add_mesh_alt"))
                end
            end
        end
    end
end

--- Duplicate selected objects (preserved from original)
onDuplicated = function()
    if #tSelectedObjs > 0 then
        for i = 1, #tSelectedObjs do
            local tObj = tSelectedObjs[i]
            tLastMeshAdded = tSelectedObjs[i]
            local tMeshTmp = tUtil.onAddMeshToEditor(tObj.fileName, true, "2dw", tObj.sText)
            if tMeshTmp then
                tMeshTmp:setAnim(select(2, tObj:getAnim()))
                if tObj.tPhysicInfo then
                    tMeshTmp.tPhysicInfo = tUtil.deepCopyTable(tObj.tPhysicInfo)
                end
                tMeshTmp.is2ds = tObj.is2ds
                initialSetUpForAddedMesh(tMeshTmp)
                -- Assign to same panel as original
                if tObj.panelRef then
                    assignObjectToPanel(tMeshTmp, tObj.panelRef)
                end
                if tMeshTmp.tFont then
                    tMeshTmp:setScale(tObj.tFont.sx, tObj.tFont.sy)
                    tMeshTmp:setAngle(tObj.tFont.ax, tObj.tFont.ay, tObj.tFont.az)
                else
                    tMeshTmp:setScale(tObj.sx, tObj.sy)
                    tMeshTmp:setAngle(tObj.ax, tObj.ay, tObj.az)
                end
                sLastMeshAdd = tObj.fileName
            end
        end
        if #tSelectedObjs > 1 then
            tUtil.showMessage(string.format(tLang.L("meshes_duplicated_fmt"), #tSelectedObjs))
        else
            tUtil.showMessage(tLang.L("mesh_duplicated"))
        end
    elseif tLastMeshAdded then
        local tMeshTmp = tUtil.onAddMeshToEditor(tLastMeshAdded.fileName, true, "2dw", tLastMeshAdded.sText)
        if tMeshTmp then
            tMeshTmp:setAnim(select(2, tLastMeshAdded:getAnim()))
            if tLastMeshAdded.tPhysicInfo then
                tMeshTmp.tPhysicInfo = tUtil.deepCopyTable(tLastMeshAdded.tPhysicInfo)
            end
            tMeshTmp.is2ds = tLastMeshAdded.is2ds
            initialSetUpForAddedMesh(tMeshTmp)
            if tLastMeshAdded.panelRef then
                assignObjectToPanel(tMeshTmp, tLastMeshAdded.panelRef)
            end
            if tMeshTmp.tFont then
                tMeshTmp:setScale(tLastMeshAdded.tFont.sx, tLastMeshAdded.tFont.sy)
                tMeshTmp:setAngle(tLastMeshAdded.tFont.ax, tLastMeshAdded.tFont.ay, tLastMeshAdded.tFont.az)
            else
                tMeshTmp:setScale(tLastMeshAdded.sx, tLastMeshAdded.sy)
                tMeshTmp:setAngle(tLastMeshAdded.ax, tLastMeshAdded.ay, tLastMeshAdded.az)
            end
            tUtil.showMessage(tLang.L("mesh_duplicated"))
        end
    else
        tUtil.showMessageWarn(tLang.L("no_mesh_to_duplicate"))
    end
end

--- Build a flat list of all panels for a combo dropdown
buildPanelComboList = function(panels, out, prefix)
    out = out or {}
    prefix = prefix or ""
    for _, panel in ipairs(panels) do
        table.insert(out, {label = prefix .. panel.name, panel = panel})
        buildPanelComboList(panel.children, out, prefix .. "  ")
    end
    return out
end

--- Mesh list window (preserved + adapted for panel assignment)
showMeshList = function()
    if not bShowMeshList then return end
    local width = 300
    local iW, iH = mbm.getSizeScreen()
    tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_meshes, 0, 0, width, width + 50, iH * 0.45)
    local flags = bEnableMoveWindow and 0 or ImGuiWindowFlags_NoMove
    local is_opened, closed_clicked = tImGui.Begin(tLang.L(tWindowsTitle.title_meshes), true, flags)
    if is_opened then
        -- Filter section
        if tImGui.TreeNodeEx(string.format(tLang.L("filter"), #tAllMesh), 0, '##FilterMesh') then
            local bAnyChange = false
            tImGui.Text(tLang.L("world"))
            local ret, current_item = tImGui.Combo('##WorldMeshFilter', tOptionsEditor.iIndexWorldMesh, tFilter.tWorld)
            if ret then tOptionsEditor.iIndexWorldMesh = current_item; bAnyChange = true end

            tImGui.Text(tLang.L("mesh_type"))
            ret, current_item = tImGui.Combo('##TypeMeshFilter', tOptionsEditor.iIndexTypeMeshFilter, tFilter.tType)
            if ret then tOptionsEditor.iIndexTypeMeshFilter = current_item; bAnyChange = true end

            tImGui.Text(tLang.L("mesh_physics"))
            ret, current_item = tImGui.Combo('##TypePhysicsFilter', tOptionsEditor.iIndexTypePhysicsFilter, tFilter.tPhysicType)
            if ret then tOptionsEditor.iIndexTypePhysicsFilter = current_item; bAnyChange = true end

            if bAnyChange then updateVisibilityByFilter() end
            tImGui.TreePop()
        end

        -- Objects list
        local iFiltered = 0
        if tImGui.TreeNodeEx(string.format(tLang.L("objects") .. " (%d/%d)", iFiltered, #tAllMesh), 0, '##allMeshes') then
            iFiltered = 0
            for i, tObj in ipairs(tAllMesh) do
                if filter(tObj) then
                    iFiltered = iFiltered + 1
                    local flags_selected = tObj.isSelected and ImGuiTreeNodeFlags_Selected or 0
                    if tObj.isBlocked then
                        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), {r=1,g=0,b=0.3,a=1})
                    end

                    local panelLabel = ""
                    if tObj.panelRef then panelLabel = " [" .. tObj.panelRef.name .. "]" end
                    local node_open = tImGui.TreeNodeEx(
                        string.format("%s (%d)%s", tObj.type, i, panelLabel), flags_selected)

                    if (keyControlPressed or keyShiftPressed) and tImGui.IsItemClicked() then
                        if not tObj.isBlocked then
                            if tObj.isSelected then
                                setSelectedObj(tObj, false)
                            else
                                setSelectedObj(tObj, true)
                                tFollowCam = tObj
                            end
                        else
                            tUtil.showMessageWarn(tLang.L("object_blocked"))
                        end
                    end

                    if tObj.isBlocked then tImGui.PopStyleColor(1) end

                    if node_open then
                        -- Panel assignment combo
                        tImGui.Text(tLang.L("assign_to_panel"))
                        local tPanelList = buildPanelComboList(tPanels)
                        local comboLabels = {tLang.L("free_object")}
                        local currentIdx = 1
                        for j, entry in ipairs(tPanelList) do
                            table.insert(comboLabels, entry.label)
                            if tObj.panelRef == entry.panel then currentIdx = j + 1 end
                        end

                        local retA, newIdxA = tImGui.Combo("##assign_" .. i, currentIdx, comboLabels)
                        if retA then
                            if newIdxA == 1 then
                                assignObjectToPanel(tObj, nil)
                            else
                                assignObjectToPanel(tObj, tPanelList[newIdxA - 1].panel)
                            end
                        end

                        -- Anchor display for panel-assigned objects
                        if tObj.panelRef and tObj.anchorX then
                            tImGui.PushItemWidth(150)

                            -- Anchor type selector
                            tImGui.Text(tLang.L("anchor_type"))
                            local tAnchorTypes = {
                                tLang.L("anchor_type_center"),
                                tLang.L("anchor_type_width"),
                                tLang.L("anchor_type_height"),
                                tLang.L("anchor_type_stretch"),
                            }
                            local tAnchorKeys = {"center", "width", "height", "stretch"}
                            local atype = tObj.anchorType or "center"
                            local atypeIdx = 1
                            for ki, k in ipairs(tAnchorKeys) do
                                if k == atype then atypeIdx = ki; break end
                            end
                            local rect = tObj.panelRef._rect
                            local retC, newIdx = tImGui.Combo("##anchorType_ml_" .. i, atypeIdx, tAnchorTypes)
                            if retC then
                                local newType = tAnchorKeys[newIdx]
                                tObj.anchorType = newType
                                atype = newType
                                if rect then
                                    local ow, oh = tObj:getSize()
                                    if (newType == "width" or newType == "stretch") and not tObj.sizeAnchorW then
                                        tObj.sizeAnchorW = math.min(1, ow / rect.w)
                                    end
                                    if (newType == "height" or newType == "stretch") and not tObj.sizeAnchorH then
                                        tObj.sizeAnchorH = math.min(1, oh / rect.h)
                                    end
                                end
                                if newType == "center" then
                                    tObj.sizeAnchorW = nil; tObj.sizeAnchorH = nil
                                elseif newType == "width" then
                                    tObj.sizeAnchorH = nil
                                elseif newType == "height" then
                                    tObj.sizeAnchorW = nil
                                end
                            end
                            tImGui.Separator()

                            tImGui.Text(tLang.L("anchor_x"))
                            local rAx, vAx = tImGui.InputFloat("##anchorX_" .. i, tObj.anchorX, 0.01, 0.05, "%.3f")
                            if rAx then
                                tObj.anchorX = math.max(0, math.min(1, vAx))
                                if tObj.panelRef._rect then
                                    local r  = tObj.panelRef._rect
                                    local nx = r.x + tObj.anchorX * r.w
                                    if tObj.isRestrictedToPanel ~= false then
                                        local ow = tObj:getSize()
                                        local le, re = getObjExtents(tObj, ow, 0)
                                        nx = math.max(r.x + le, math.min(r.x + r.w - re, nx))
                                    end
                                    tObj:setPos(nx, tObj.y, tObj.z)
                                    tObj.tShape.x = nx
                                    if r.w > 0 then tObj.anchorX = (nx - r.x) / r.w end
                                end
                            end
                            tImGui.Text(tLang.L("anchor_y"))
                            local rAy, vAy = tImGui.InputFloat("##anchorY_" .. i, tObj.anchorY, 0.01, 0.05, "%.3f")
                            if rAy then
                                tObj.anchorY = math.max(0, math.min(1, vAy))
                                if tObj.panelRef._rect then
                                    local r  = tObj.panelRef._rect
                                    local ny = r.y + tObj.anchorY * r.h
                                    if tObj.isRestrictedToPanel ~= false then
                                        local _, oh = tObj:getSize()
                                        local _, _, be, te = getObjExtents(tObj, 0, oh)
                                        ny = math.max(r.y + be, math.min(r.y + r.h - te, ny))
                                    end
                                    tObj:setPos(tObj.x, ny, tObj.z)
                                    tObj.tShape.y = ny
                                    if r.h > 0 then tObj.anchorY = (ny - r.y) / r.h end
                                end
                            end
                            tImGui.PopItemWidth()
                        end

                        -- Object properties (Pass 4 will fill this)
                        showPropertiesForMesh(tObj)
                        tImGui.TreePop()
                    end
                else
                    setSelectedObj(tObj, false)
                end
            end
            tImGui.TreePop()
        end
    end
    tWindowsArea:addThisWindow()
    tImGui.End()
    if closed_clicked then bShowMeshList = false end
end

--- Options when adding mesh window (preserved from original)
showAddingMeshOptions = function()
    if not bShowAddingMesh then return end
    local width = 300
    tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_adding_mesh, 0, 0, width, width + 50)
    local is_opened, closed_clicked = tImGui.Begin(
        tLang.L(tWindowsTitle.title_adding_mesh), true, ImGuiWindowFlags_NoMove)
    if is_opened then
        -- Target panel info
        if tSelectedPanel then
            tImGui.TextDisabled(string.format("Target panel: %s", tSelectedPanel.name))
        else
            tImGui.TextDisabled("Objects will be added as free (no panel)")
        end
        tImGui.Separator()

        local title_duplicated = tLang.L("duplicate_last_mesh")
        if #tSelectedObjs > 0 then
            title_duplicated = tLang.L("duplicate_all_mesh_selected")
        end
        if tImGui.Button(title_duplicated, {x=200, y=0}) then
            onDuplicated()
        end
    end
    tWindowsArea:addThisWindow()
    tImGui.End()
    if closed_clicked then bShowAddingMesh = false end
end

--- Detail overlay for single selected mesh (preserved from original)
showDetailOfMesh = function()
    if not bShowDetailOfMesh or #tSelectedObjs ~= 1 then return end
    local tObj = tSelectedObjs[1]
    local flags = {'ImGuiWindowFlags_AlwaysAutoResize','ImGuiWindowFlags_NoSavedSettings','ImGuiWindowFlags_NoFocusOnAppearing'}
    tImGui.SetNextWindowBgAlpha(0.75)
    tImGui.SetNextWindowPos({x=300, y=25}, tImGui.Flags('ImGuiCond_Once'), {x=0,y=0})
    local is_opened, closed_clicked = tImGui.Begin(
        tLang.L(tWindowsTitle.title_mesh_info), true, tImGui.Flags(flags))
    if is_opened then
        local sWorld = tObj.is2ds and '2d Screen' or '2d World'
        tImGui.Text(string.format('Type:%s (%s)(%d)', tObj.type, sWorld, tObj.iIndex or 0))
        tImGui.Text(string.format('File:%s', tUtil.getShortName(tObj.fileName, false)))
        tImGui.Text(string.format('Position: X:%g Y:%g Z:%g', tObj.x, tObj.y, tObj.z))
        tImGui.Text(string.format('Scale   : X:%g Y:%g Z:%g', tObj.sx, tObj.sy, tObj.sz))
        tImGui.Text(string.format('Angle   : X:%g Y:%g Z:%g', tObj.ax, tObj.ay, tObj.az))

        if tObj.panelRef then
            tImGui.Text(string.format('Panel: %s', tObj.panelRef.name))
            if tObj.anchorX then
                tImGui.Text(string.format('Anchor: %.3f, %.3f', tObj.anchorX, tObj.anchorY))
            end
        else
            tImGui.Text('Panel: (free)')
        end

        if tObj.sText then
            tImGui.Text(tLang.L("text_label_2"))
            tImGui.Text(tObj.sText)
        end
        if tObj.tPhysicInfo then
            local tp = tObj.tPhysicInfo
            tImGui.Text(tLang.L("physics_colon"))
            if tp.type       then tImGui.Text(string.format('Type (%s)', tp.type)) end
            if tp.density    then tImGui.Text(string.format('Density (%g)', tp.density)) end
            if tp.friction   then tImGui.Text(string.format('Friction (%g)', tp.friction)) end
            if tp.restitution then tImGui.Text(string.format('Restitution (%g)', tp.restitution)) end
            if tp.scaleX     then tImGui.Text(string.format('Scale X:%g Y:%g', tp.scaleX, tp.scaleY)) end
            if type(tp.sensor) == 'boolean' then tImGui.Text(string.format('Sensor <%s>', tostring(tp.sensor))) end
            if type(tp.bullet) == 'boolean' then tImGui.Text(string.format('Bullet <%s>', tostring(tp.bullet))) end
        end
    end
    if closed_clicked then bShowDetailOfMesh = false end
    tWindowsArea:addThisWindow()
    tImGui.End()
end

-- ─────────────────────────────────────────────────────────────────────────────
-- Pass 4 — Object property tree nodes
-- ─────────────────────────────────────────────────────────────────────────────

local function drawBlockButton(isBlocked, axis)
    tImGui.SameLine()
    local label
    if isBlocked then
        label = tLang.L("blocked") .. '##' .. axis
    else
        label = tLang.L("free") .. '##' .. axis
    end
    if tImGui.SmallButton(label) then
        return not isBlocked
    end
    return isBlocked
end

local function treeNodePosition(tObj)
    local flags = 0
    if tImGui.TreeNodeEx(tLang.L("position"), flags, string.format("Position-%d", tObj.iIndex)) then
        local step      = 1.0
        local step_fast = 10.0
        local format    = "%.3f"
        local inputFlags = 0
        tImGui.PushItemWidth(150)

        local result, fValue = tImGui.InputFloat(tLang.L("axis_x") .. '##Mesh(s)', tObj.x, step, step_fast, format, inputFlags)
        tObj.isBlockedX = drawBlockButton(tObj.isBlockedX, 'X')
        if result and not tObj.isBlockedX then
            -- Clamp X to panel bounds
            if tObj.isRestrictedToPanel ~= false and tObj.panelRef and tObj.panelRef._rect then
                local r  = tObj.panelRef._rect
                local ow = tObj:getSize()
                local le, re = getObjExtents(tObj, ow, 0)
                fValue = math.max(r.x + le, math.min(r.x + r.w - re, fValue))
            end
            tObj.x = fValue
            tObj.tShape.x = fValue
            if tObj.panelRef and tObj.panelRef._rect then
                local r = tObj.panelRef._rect
                if r.w > 0 then tObj.anchorX = (fValue - r.x) / r.w end
            end
        end

        result, fValue = tImGui.InputFloat(tLang.L("axis_y") .. '##Mesh(s)', tObj.y, step, step_fast, format, inputFlags)
        tObj.isBlockedY = drawBlockButton(tObj.isBlockedY, 'Y')
        if result and not tObj.isBlockedY then
            -- Clamp Y to panel bounds
            if tObj.isRestrictedToPanel ~= false and tObj.panelRef and tObj.panelRef._rect then
                local r  = tObj.panelRef._rect
                local _, oh = tObj:getSize()
                local _, _, be, te = getObjExtents(tObj, 0, oh)
                fValue = math.max(r.y + be, math.min(r.y + r.h - te, fValue))
            end
            tObj.y = fValue
            tObj.tShape.y = fValue
            if tObj.panelRef and tObj.panelRef._rect then
                local r = tObj.panelRef._rect
                if r.h > 0 then tObj.anchorY = (fValue - r.y) / r.h end
            end
        end

        result, fValue = tImGui.InputFloat(tLang.L("axis_z") .. '##Mesh(s)', tObj.z, step, step_fast, format, inputFlags)
        tObj.isBlockedZ = drawBlockButton(tObj.isBlockedZ, 'Z')
        if result and not tObj.isBlockedZ then
            tObj.z = fValue
            tObj.tShape.z = fValue - 1
        end
        tImGui.PopItemWidth()

        tImGui.TreePop()
    end
end

local function treeNodeScale(tObj)
    local flags = 0
    if tImGui.TreeNodeEx(tLang.L("scale"), flags, string.format("Scale-%d", tObj.iIndex)) then
        local step      = 0.02
        local step_fast = 0.1
        local format    = "%.3f"
        local inputFlags = 0
        tImGui.PushItemWidth(150)

        local atype      = tObj.anchorType or "center"
        local hasPanelR  = tObj.panelRef and tObj.panelRef._rect
        local rect       = hasPanelR and tObj.panelRef._rect or nil

        -- SX input — hidden when width or stretch drives this axis
        local showSX = (atype == "center" or atype == "height")
        if showSX then
            local result, fValue = tImGui.InputFloat(tLang.L("scale_sx") .. '##Mesh(s)', tObj.sx, step, step_fast, format, inputFlags)
            if result then
                if fValue > 0 then
                    -- Cap SX so object width does not exceed panel width
                    if rect then
                        local naturalW = tObj:getSize() / tObj.sx
                        if naturalW > 0 then fValue = math.min(fValue, rect.w / naturalW) end
                    end
                    tObj.sx = fValue
                    local w, h, d = tObj:getSize()
                    tObj.tShape.sx = w
                    -- Shift X if edges now go out of panel bounds
                    if tObj.isRestrictedToPanel ~= false and rect then
                        local le, re = getObjExtents(tObj, w, 0)
                        local cx = math.max(rect.x + le, math.min(rect.x + rect.w - re, tObj.x))
                        if cx ~= tObj.x then tObj.x = cx; tObj.tShape.x = cx end
                        if rect.w > 0 then tObj.anchorX = (tObj.x - rect.x) / rect.w end
                    end
                end
            end
        else
            -- Show the width-fraction input instead
            local label = (atype == "stretch") and (tLang.L("panel_size_w") .. " (W)##swW_" .. tObj.iIndex)
                                                or (tLang.L("panel_size_w") .. "##swW_" .. tObj.iIndex)
            local cur = tObj.sizeAnchorW or 1.0
            local rW, vW = tImGui.InputFloat(label, cur, 0.01, 0.05, "%.3f", 0)
            if rW then
                local clamped = math.max(0.01, math.min(1.0, vW))
                tObj.sizeAnchorW = clamped
                if rect then
                    local ow, oh = tObj:getSize()
                    local naturalW = ow / tObj.sx
                    if naturalW > 0 then
                        local newSx = (rect.w * clamped) / naturalW
                        if atype == "width" then
                            -- proportional: drive both axes
                            local ratio = newSx / tObj.sx
                            tObj.sx = newSx
                            tObj.sy = tObj.sy * ratio
                        else
                            tObj.sx = newSx
                        end
                        local w, h = tObj:getSize()
                        tObj.tShape:setScale(w, h, 1)
                        if tObj.isRestrictedToPanel ~= false then
                            local le, re = getObjExtents(tObj, w, 0)
                            local cx = math.max(rect.x + le, math.min(rect.x + rect.w - re, tObj.x))
                            if cx ~= tObj.x then tObj.x = cx; tObj.tShape.x = cx end
                            if rect.w > 0 then tObj.anchorX = (tObj.x - rect.x) / rect.w end
                        end
                    end
                end
            end
        end

        -- SY input — hidden when height or stretch drives this axis
        local showSY = (atype == "center" or atype == "width")
        if showSY then
            local result, fValue = tImGui.InputFloat(tLang.L("scale_sy") .. '##Mesh(s)', tObj.sy, step, step_fast, format, inputFlags)
            if result then
                if fValue > 0 then
                    -- Cap SY so object height does not exceed panel height
                    if rect then
                        local _, naturalH = tObj:getSize()
                        naturalH = naturalH / tObj.sy
                        if naturalH > 0 then fValue = math.min(fValue, rect.h / naturalH) end
                    end
                    tObj.sy = fValue
                    local w, h, d = tObj:getSize()
                    tObj.tShape.sy = h
                    -- Shift Y if edges now go out of panel bounds
                    if tObj.isRestrictedToPanel ~= false and rect then
                        local _, _, be, te = getObjExtents(tObj, 0, h)
                        local cy = math.max(rect.y + be, math.min(rect.y + rect.h - te, tObj.y))
                        if cy ~= tObj.y then tObj.y = cy; tObj.tShape.y = cy end
                        if rect.h > 0 then tObj.anchorY = (tObj.y - rect.y) / rect.h end
                    end
                end
            end
        else
            -- Show the height-fraction input instead (skip for "width" — SY is computed there)
            if atype == "stretch" or atype == "height" then
                local label = (atype == "stretch") and (tLang.L("panel_size_h") .. " (H)##swH_" .. tObj.iIndex)
                                                    or (tLang.L("panel_size_h") .. "##swH_" .. tObj.iIndex)
                local cur = tObj.sizeAnchorH or 1.0
                local rH, vH = tImGui.InputFloat(label, cur, 0.01, 0.05, "%.3f", 0)
                if rH then
                    local clamped = math.max(0.01, math.min(1.0, vH))
                    tObj.sizeAnchorH = clamped
                    if rect then
                        local ow, oh = tObj:getSize()
                        local naturalH = oh / tObj.sy
                        if naturalH > 0 then
                            local newSy = (rect.h * clamped) / naturalH
                            if atype == "height" then
                                -- proportional: drive both axes
                                local ratio = newSy / tObj.sy
                                tObj.sy = newSy
                                tObj.sx = tObj.sx * ratio
                            else
                                tObj.sy = newSy
                            end
                            local w, h = tObj:getSize()
                            tObj.tShape:setScale(w, h, 1)
                            if tObj.isRestrictedToPanel ~= false then
                                local _, _, be, te = getObjExtents(tObj, 0, h)
                                local cy = math.max(rect.y + be, math.min(rect.y + rect.h - te, tObj.y))
                                if cy ~= tObj.y then tObj.y = cy; tObj.tShape.y = cy end
                                if rect.h > 0 then tObj.anchorY = (tObj.y - rect.y) / rect.h end
                            end
                        end
                    end
                end
            end
        end

        local result, fValue = tImGui.InputFloat(tLang.L("scale_sz") .. '##Mesh(s)', tObj.sz, step, step_fast, format, inputFlags)
        if result then
            if fValue > 0 then
                tObj.sz = fValue
                local w, h, d = tObj:getSize()
                if d then
                    tObj.tShape.sz = d
                end
            end
        end
        tImGui.PopItemWidth()

        tImGui.TreePop()
    end
end

local function treeNodeAngle(tObj)
    local flags = 0
    if tImGui.TreeNodeEx(tLang.L("angle"), flags, string.format("Angle-%d", tObj.iIndex)) then
        local step      = 1.0
        local step_fast = 5.0
        local format    = "%.2f"
        local inputFlags = 0
        tImGui.PushItemWidth(150)

        local result, fValue = tImGui.InputFloat(tLang.L("angle_ax") .. '##Mesh(s)', math.deg(tObj.ax), step, step_fast, format, inputFlags)
        if result then
            if fValue >= -360 and fValue <= 360 then
                local radian   = math.rad(fValue)
                tObj.ax        = radian
                tObj.tShape.ax = radian
            end
        end

        result, fValue = tImGui.InputFloat(tLang.L("angle_ay") .. '##Mesh(s)', math.deg(tObj.ay), step, step_fast, format, inputFlags)
        if result then
            if fValue >= -360 and fValue <= 360 then
                local radian   = math.rad(fValue)
                tObj.ay        = radian
                tObj.tShape.ay = radian
            end
        end

        result, fValue = tImGui.InputFloat(tLang.L("angle_az") .. '##Mesh(s)', math.deg(tObj.az), step, step_fast, format, inputFlags)
        if result then
            if fValue >= -360 and fValue <= 360 then
                local radian   = math.rad(fValue)
                tObj.az        = radian
                tObj.tShape.az = radian
            end
        end
        tImGui.PopItemWidth()

        tImGui.TreePop()
    end
end

local function treeNodeText(tObj)
    local sText = tObj.sText
    if sText then
        local flags = 0
        if tImGui.TreeNodeEx(tLang.L("text_label"), flags, string.format("Text-%d", tObj.iIndex)) then
            tImGui.Text(tLang.L("text_label"))
            local label = string.format("##Font-Text-%d", tObj.iIndex)
            local size  = {x = -1, y = 100}

            local modified, sNewText = tImGui.InputTextMultiline(label, sText, size, 0)
            if modified then
                tObj.sText = sNewText
                tObj.text  = sNewText
                local w, h, d = tObj:getSize(sNewText)
                tObj.tShape:setScale(w, h, d or 1)
            end
            tImGui.TreePop()
        end
    end
end

local function treeNodeAnimation(tObj)
    local flags = 0
    if tImGui.TreeNodeEx(tLang.L("animation"), flags, string.format("Animation-%d", tObj.iIndex)) then
        local label       = '##Animation' .. tostring(tObj.iIndex)
        local tAnimations = {}
        for i = 1, tObj:getTotalAnim() do
            table.insert(tAnimations, string.format('%d:  %s', i, select(1, tObj:getAnim(i))))
        end

        local current_item    = select(2, tObj:getAnim())
        local height_in_items = -1
        local ret, new_item, item_as_string = tImGui.Combo(label, current_item, tAnimations, height_in_items)
        if ret then
            tObj:setAnim(new_item)
        end
        tImGui.TreePop()
    end
end

local function treeNodePhysics(tObj)
    local flags = 0
    if tImGui.TreeNodeEx(tLang.L("physics"), flags, string.format("Physics-%d", tObj.iIndex)) then
        local height_in_items = -1
        tImGui.Text(tLang.L("type"))

        local tPhysicInfo = tObj.tPhysicInfo or {type = 'None'}

        local iType = 1
        for i = 1, #tPhysicEditor.tType do
            if tPhysicEditor.tType[i]:lower() == tPhysicInfo.type then
                iType = i
                break
            end
        end

        local ret, current_item, item_as_string = tImGui.Combo(
            string.format("##TypePhysics-%d", tObj.iIndex), iType, tPhysicEditor.tType, height_in_items)
        if ret then
            if item_as_string == 'None' then
                tObj.tPhysicInfo = nil
            else
                tObj.tPhysicInfo = tUtil.deepCopyTable(tPhysicEditor[item_as_string])
            end
        end

        if tPhysicInfo.density then
            tImGui.Text(tLang.L("density"))
            tImGui.SameLine()
            tImGui.TextDisabled(tLang.L("unit_kg_m2"))
            local result, fValue = tImGui.InputFloat(
                string.format('##density-%d', tObj.iIndex), tPhysicInfo.density, 1.0, 2.0, "%.3f", 0)
            if result and fValue >= 0 then
                tPhysicInfo.density = fValue
            end
        end

        if tPhysicInfo.friction then
            tImGui.Text(tLang.L("friction"))
            tImGui.SameLine()
            tImGui.TextDisabled(tLang.L("unit_coefficient"))
            local result, fValue = tImGui.InputFloat(
                string.format('##Friction-%d', tObj.iIndex), tPhysicInfo.friction, 0.002, 0.02, "%.7f", 0)
            if result and fValue >= 0 and fValue <= 1 then
                tPhysicInfo.friction = fValue
            end
        end

        if tPhysicInfo.restitution then
            tImGui.Text(tLang.L("restitution"))
            tImGui.SameLine()
            tImGui.TextDisabled(tLang.L("unit_elasticity"))
            local result, fValue = tImGui.InputFloat(
                string.format('##Restitution-%d', tObj.iIndex), tPhysicInfo.restitution, 0.002, 0.02, "%.7f", 0)
            if result and fValue >= 0 and fValue <= 1 then
                tPhysicInfo.restitution = fValue
            end
        end

        if tPhysicInfo.scaleX then
            tImGui.Text(tLang.L("scale_x"))
            tImGui.SameLine()
            tImGui.TextDisabled(tLang.L("unit_scale_x"))
            local result, fValue = tImGui.InputFloat(
                string.format('##Scale X-%d', tObj.iIndex), tPhysicInfo.scaleX, 0.02, 0.2, "%.2f", 0)
            if result and fValue > 0 and fValue <= 1000 then
                tPhysicInfo.scaleX = fValue
            end
        end

        if tPhysicInfo.scaleY then
            tImGui.Text(tLang.L("scale_y"))
            tImGui.SameLine()
            tImGui.TextDisabled(tLang.L("unit_scale_y"))
            local result, fValue = tImGui.InputFloat(
                string.format('##Scale Y-%d', tObj.iIndex), tPhysicInfo.scaleY, 0.02, 0.2, "%.2f", 0)
            if result and fValue > 0 and fValue <= 1000 then
                tPhysicInfo.scaleY = fValue
            end
        end

        if type(tPhysicInfo.sensor) == 'boolean' then
            tPhysicInfo.sensor = tImGui.Checkbox(
                string.format(tLang.L("sensor") .. '##%d', tObj.iIndex), tPhysicInfo.sensor)
        end

        if type(tPhysicInfo.bullet) == 'boolean' then
            tPhysicInfo.bullet = tImGui.Checkbox(
                string.format(tLang.L("bullet") .. '##%d', tObj.iIndex), tPhysicInfo.bullet)
        end

        tImGui.TreePop()
    end
end

--- Full property editor for an object (called from showMeshList tree nodes)
showPropertiesForMesh = function(tObj)
    local isBlocked = tObj.isBlocked or false
    if not isBlocked then
        tImGui.TextDisabled(tLang.L("world_type"))
        local tWorld = {'2D World', '2D Screen'}
        local iIndexWorldMesh = tObj.is2ds and 2 or 1
        local ret, current_item = tImGui.Combo('##WorldObj' .. tObj.iIndex, iIndexWorldMesh, tWorld)
        if ret then
            if current_item == 1 then
                tObj.is2ds = false
                tObj.isRelative2ds = nil
            else
                tObj.is2ds = true
                tObj.isRelative2ds = true
            end
            if filter(tObj) then
                tObj.visible = true
            else
                tObj.visible = false
                tObj.tShape.visible = false
            end
        end

        if tObj.is2ds and not tObj.panelRef then
            tObj.isRelative2ds = tImGui.Checkbox(tLang.L("relative_2d_screen"), tObj.isRelative2ds)
            tImGui.SameLine()
            tImGui.HelpMarker(tLang.L("help_flag_2d_screen"))
        elseif tObj.is2ds and tObj.panelRef then
            tImGui.TextDisabled(tLang.L("relative_2d_screen") .. ": N/A (panel reflow)")
        end

        local bSelected = tImGui.Checkbox(tLang.L("selected") .. '##' .. tObj.iIndex, tObj.isSelected)
        if bSelected ~= tObj.isSelected then
            setSelectedObj(tObj, bSelected)
            if bSelected then
                tFollowCam = tObj
            end
        end

        -- Restrict-to-panel toggle (only meaningful when object has a panel)
        if tObj.panelRef then
            local bRestrict = tObj.isRestrictedToPanel ~= false
            local bNew = tImGui.Checkbox(tLang.L("restrict_to_panel") .. '##rp' .. tObj.iIndex, bRestrict)
            if bNew ~= bRestrict then
                tObj.isRestrictedToPanel = bNew
            end
        end
    end

    if isBlocked then
        local idx   = tImGui.Flags('ImGuiCol_Text')
        local color = {r = 1, g = 0, b = 0.3, a = 1}
        tImGui.PushStyleColor(idx, color)
    end

    local bBlocked = tImGui.Checkbox(tLang.L("blocked") .. '##' .. tObj.iIndex, isBlocked)
    if bBlocked ~= isBlocked then
        tObj.isBlocked = bBlocked
        if bBlocked then
            setSelectedObj(tObj, false)
        end
    end
    if isBlocked then
        tImGui.PopStyleColor(1)
    end

    if not bBlocked then
        -- Panel / anchor info
        if tObj.panelRef then
            tImGui.Separator()
            tImGui.TextDisabled(string.format("Panel: %s", tObj.panelRef.name))
            if tObj.anchorX ~= nil and tObj.anchorY ~= nil then
                tImGui.TextDisabled(string.format("Anchor  X=%.3f  Y=%.3f", tObj.anchorX, tObj.anchorY))
            end
            tImGui.Separator()
        end
        treeNodePosition(tObj)
        treeNodeScale(tObj)
        treeNodeAngle(tObj)
        treeNodeText(tObj)
        treeNodeAnimation(tObj)
        treeNodePhysics(tObj)
    end
end

-- ─────────────────────────────────────────────────────────────────────────────
-- Pass 5 — Save / Load / Export
-- ─────────────────────────────────────────────────────────────────────────────

local function onSaveUserData(name, value, tOut)
    -- placeholder for custom userdata serialisation (none needed currently)
end

local function getHeader(fileName)
    local sHeader = '' .. "--[[\n" .. [[
    Scene 2d - this file is meant to be used in the engine mbm
    More info at: https://mbm-documentation.readthedocs.io/en/latest/
	Scene Editor: https://mbm-documentation.readthedocs.io/en/latest/editors.html#scene-editor-2d

	how to:
	* in your scene do:

		tScene = require "SCENE_NAME"

		tScene:load()

	* To retrieve mesh(s) use get or getAll:

		local tMesh      = tScene:get('mesh_name.msh')
		local tLife      = tScene:get('life.spt')
		local tAllCircle = tScene:getAll('mario.png')

	* To load new mesh(s)(same information as used in the editor, position, scale, rotation, etc) use:

        local tNewLife   = tScene:addMesh('mike.spt')

    * Index (from the table tAllMeshInfo) might be used instead of fileName to retrieve a specific mesh:

        local tMesh      = tScene:get(2)
        local tNewLife   = tScene:addMesh(3)

]] .. "]]\n\n"
    sHeader = sHeader:gsub('SCENE_NAME', tUtil.getShortName(fileName, false))
    sHeader = sHeader:gsub('%.lua', '')
    return sHeader
end

-- ── Serialisation helpers ────────────────────────────────────────────────────

local function getIs2ds4Save(tObj)
    if tObj.is2ds then return ',is2ds=true' end
    return ''
end

local function getBlockFlag4Save(tObj)
    if tObj.isBlocked then return ',isBlocked=true' end
    return ''
end

local function getText4Font4Save(sText)
    if sText then return ',sText=[[' .. sText .. ']]' end
    return ''
end

local function getFontParams4Save(tObj)
    if tObj.type == "font" then
        local s = ''
        if tObj.heightFont      then s = s .. string.format(',heightFont=%g',      tObj.heightFont)      end
        if tObj.spaceFont       then s = s .. string.format(',spaceFont=%g',       tObj.spaceFont)       end
        if tObj.spaceHeightFont then s = s .. string.format(',spaceHeightFont=%g', tObj.spaceHeightFont) end
        return s
    end
    return ''
end

local function getIndependentCalCam4Save(tObj)
    if tObj.is2ds and tObj.isRelative2ds then return ',isRelative2ds=true' end
    return ''
end

local function getPhysicInfo4Save(tPhysicInfo)
    if tPhysicInfo then
        local sRet = {}
        if tPhysicInfo.type        then table.insert(sRet, string.format('type=%q', tPhysicInfo.type)) end
        if tPhysicInfo.density     then table.insert(sRet, string.format('density=%g', tPhysicInfo.density)) end
        if tPhysicInfo.friction    then table.insert(sRet, string.format('friction=%g', tPhysicInfo.friction)) end
        if tPhysicInfo.restitution then table.insert(sRet, string.format('restitution=%g', tPhysicInfo.restitution)) end
        if tPhysicInfo.scaleX      then table.insert(sRet, string.format('scaleX=%g', tPhysicInfo.scaleX)) end
        if tPhysicInfo.scaleY      then table.insert(sRet, string.format('scaleY=%g', tPhysicInfo.scaleY)) end
        if type(tPhysicInfo.sensor) == 'boolean' then table.insert(sRet, string.format('sensor=%s', tostring(tPhysicInfo.sensor))) end
        if type(tPhysicInfo.bullet) == 'boolean' then table.insert(sRet, string.format('bullet=%s', tostring(tPhysicInfo.bullet))) end
        return ',tPhysicInfo={' .. table.concat(sRet, ',') .. '}'
    end
    return ''
end

--- Serialize anchor & panel info for each object
local function getPanelInfo4Save(tObj)
    if tObj.panelRef then
        local s = string.format(',panelId=%d', tObj.panelRef.id)
        if tObj.anchorX then s = s .. string.format(',anchorX=%g', tObj.anchorX) end
        if tObj.anchorY then s = s .. string.format(',anchorY=%g', tObj.anchorY) end
        -- only write when false (true is the default, no need to bloat the file)
        if tObj.isRestrictedToPanel == false then
            s = s .. ',isRestrictedToPanel=false'
        end
        -- anchor type (omit when "center" — that is the default)
        if tObj.anchorType and tObj.anchorType ~= "center" then
            s = s .. string.format(',anchorType=%q', tObj.anchorType)
        end
        if tObj.sizeAnchorW then s = s .. string.format(',sizeAnchorW=%g', tObj.sizeAnchorW) end
        if tObj.sizeAnchorH then s = s .. string.format(',sizeAnchorH=%g', tObj.sizeAnchorH) end
        return s
    end
    return ''
end

--- Serialize panel tree recursively
local function serializePanelTree(fp, panels, indent)
    indent = indent or '    '
    for _, panel in ipairs(panels) do
        fp:write(string.format('%s{id=%d,name=%q,world=%q', indent, panel.id, panel.name, panel.world))
        if panel.anchor then
            local a = panel.anchor
            fp:write(string.format(',anchor={left=%s,top=%s,right=%s,bottom=%s}',
                a.left, a.top, a.right, a.bottom))
        end
        if panel.panelAnchorType and panel.panelAnchorType ~= "stretch" then
            fp:write(string.format(',panelAnchorType=%q', panel.panelAnchorType))
            fp:write(string.format(',cx=%g,cy=%g', panel.cx or 0.5, panel.cy or 0.5))
            local pat_ = panel.panelAnchorType
            if pat_ == "center" or pat_ == "width" or pat_ == "width_prop" or pat_ == "height_prop" then
                fp:write(string.format(',sizeW=%g', panel.sizeW or 1))
            end
            if pat_ == "center" or pat_ == "height" or pat_ == "width_prop" or pat_ == "height_prop" then
                fp:write(string.format(',sizeH=%g', panel.sizeH or 1))
            end
        end
        if panel.splitDir then
            fp:write(string.format(',splitDir=%q', panel.splitDir))
        end
        if panel.pctList then
            fp:write(',pctList={' .. table.concat(panel.pctList, ',') .. '}')
        end
        if #panel.children > 0 then
            fp:write(',children={\n')
            serializePanelTree(fp, panel.children, indent .. '    ')
            fp:write(indent .. '}')
        else
            fp:write(',children={}')
        end
        fp:write('},\n')
    end
end

local function getPhysicsFunction()
    return [[

tScene.addPhysics = function (self,tObj,tPhysicInfo)

    if self.tPhysics == nil then
        local gravity_x           = 0
        local gravity_y           = -90.8
        local scale_box_2d        = 10
        local velocityIterations  = 10
        local positionIterations  = 3
        local multiplyStep        = 1
        self.tPhysics = box2d:new(gravity_x,gravity_y,scale_box_2d,velocityIterations,positionIterations,multiplyStep)
    end

    if tPhysicInfo.type == 'static' then
        self.tPhysics:addStaticBody(tObj,tPhysicInfo.density,tPhysicInfo.friction,tPhysicInfo.scaleX,tPhysicInfo.scaleY,tPhysicInfo.sensor)
    elseif tPhysicInfo.type == 'dynamic' then
        self.tPhysics:addDynamicBody(tObj,tPhysicInfo.density,tPhysicInfo.friction,tPhysicInfo.restitution,tPhysicInfo.scaleX,tPhysicInfo.scaleY,tPhysicInfo.sensor,tPhysicInfo.bullet)
    elseif tPhysicInfo.type == 'kinematic' then
        self.tPhysics:addKinematicBody(tObj,tPhysicInfo.density,tPhysicInfo.friction,tPhysicInfo.restitution,tPhysicInfo.scaleX,tPhysicInfo.scaleY,tPhysicInfo.sensor,tPhysicInfo.bullet)
    elseif tPhysicInfo.type == 'character' then
        self.tPhysics:addDynamicBody(tObj,tPhysicInfo.density,tPhysicInfo.friction,tPhysicInfo.restitution,tPhysicInfo.scaleX,tPhysicInfo.scaleY,tPhysicInfo.sensor,tPhysicInfo.bullet)
        self.tPhysics:setFixedRotation(tObj,true)
        self.tPhysics:setSleepingAllowed(tObj,false)
    else
        print('error','Not found type of physic:' .. tostring(tPhysicInfo.type))
    end
end

]]
end

local function getSceneLoaderCode(xCam, yCam, sScaleAxis)
    local sScene = [[
local function _computePanelRect(panel, parentRect)
    local pw, ph = parentRect.w, parentRect.h
    local pat = panel.panelAnchorType or "stretch"
    if pat == "center" then
        local w  = (panel.sizeW or 1) * pw
        local h  = (panel.sizeH or 1) * ph
        local cx = parentRect.x + (panel.cx or 0.5) * pw
        local cy = parentRect.y + (panel.cy or 0.5) * ph
        return {x=cx-w*0.5, y=cy-h*0.5, w=w, h=h}
    elseif pat == "width" then
        local s  = (panel.sizeW or 1) * pw
        local cx = parentRect.x + (panel.cx or 0.5) * pw
        local cy = parentRect.y + (panel.cy or 0.5) * ph
        return {x=cx-s*0.5, y=cy-s*0.5, w=s, h=s}
    elseif pat == "height" then
        local s  = (panel.sizeH or 1) * ph
        local cx = parentRect.x + (panel.cx or 0.5) * pw
        local cy = parentRect.y + (panel.cy or 0.5) * ph
        return {x=cx-s*0.5, y=cy-s*0.5, w=s, h=s}
    elseif pat == "width_prop" then
        local w  = (panel.sizeW or 1) * pw
        local h  = (panel.sizeH or 1) * pw
        local sc = math.min(pw/w, ph/h); if sc < 1 then w=w*sc; h=h*sc end
        local cx = parentRect.x + (panel.cx or 0.5) * pw
        local cy = parentRect.y + (panel.cy or 0.5) * ph
        return {x=cx-w*0.5, y=cy-h*0.5, w=w, h=h}
    elseif pat == "height_prop" then
        local w  = (panel.sizeW or 1) * ph
        local h  = (panel.sizeH or 1) * ph
        local sc = math.min(pw/w, ph/h); if sc < 1 then w=w*sc; h=h*sc end
        local cx = parentRect.x + (panel.cx or 0.5) * pw
        local cy = parentRect.y + (panel.cy or 0.5) * ph
        return {x=cx-w*0.5, y=cy-h*0.5, w=w, h=h}
    else
        local a = panel.anchor
        return {x=parentRect.x+a.left*pw, y=parentRect.y+a.top*ph,
                w=(a.right-a.left)*pw, h=(a.bottom-a.top)*ph}
    end
end

local function _traversePanels(panels, refParent, curParent, refRects, curRects)
    for _, panel in ipairs(panels) do
        local rr = _computePanelRect(panel, refParent)
        local cr = _computePanelRect(panel, curParent)
        refRects[panel.id] = rr
        curRects[panel.id] = cr
        if panel.children and #panel.children > 0 then
            _traversePanels(panel.children, rr, cr, refRects, curRects)
        end
    end
end

tScene.updateCamera = (
    function (self)
        local camera2d = mbm.getCamera('2d')
        camera2d:setPos(POS_CAM_X,POS_CAM_Y)
        camera2d:scaleToScreen(self.iExpectedWidth,self.iExpectedHeight,"AXIS_SCALE_CAM")
        self.iSizeScreenWidth, self.iSizeScreenHeight  = mbm.getSizeScreen()
        self.scale_cam_x = camera2d.sx
        self.scale_cam_y = camera2d.sy
    end
    )

tScene.load = (
    function(self,onProgress)
        local bEnableCoroutine = type(onProgress) == 'function'
        self:updateCamera()
        local iYieldForEach      = 60
        local iByYield,stepYield = 0,0
        if bEnableCoroutine then
            if #self.tAllMeshInfo > iYieldForEach then
                iByYield = math.ceil(#self.tAllMeshInfo / iYieldForEach)
            end
        end
        for i=1, #self.tAllMeshInfo do
            local tInfo    = self.tAllMeshInfo[i]
            local tMeshTmp = self:_addMesh(tInfo)
            if bEnableCoroutine then
                stepYield = stepYield + 1
                if stepYield >= iByYield then
                    stepYield = 0
                    if onProgress and type(onProgress) == 'function' then
                        onProgress( i / #self.tAllMeshInfo * 100.0)
                    end
                    coroutine.yield(#self.tMeshesLoaded,#self.tAllMeshInfo)
                end
            end
        end
        if self.tPanelTree then
            self:reflow(self.iSizeScreenWidth, self.iSizeScreenHeight)
        end
        if self.tPhysics then
            self.tPhysics:start()
        end
    end
    )

tScene._addMesh = (
    function(self,tInfo)
        local tMeshTmp = nil
        local sWorld = '2dw'
        if tInfo.is2ds then
            sWorld = '2ds'
        end
        local fileName = tInfo.fileName
        if tInfo.type == 'mesh' then
            tMeshTmp = mesh:new(sWorld)
            if not tMeshTmp:load(fileName) then
                print("failed to load mesh:["..fileName.."]")
                return nil
            end
        elseif tInfo.type == 'sprite' then
            tMeshTmp = sprite:new(sWorld)
            if not tMeshTmp:load(fileName) then
                print("failed to load sprite:["..fileName.."]")
                return nil
            end
        elseif tInfo.type == 'particle' then
            tMeshTmp = particle:new(sWorld)
            if not tMeshTmp:load(fileName) then
                print("failed to load particle:["..fileName.."]")
                return nil
            end
        elseif tInfo.type == 'tile' then
            tMeshTmp = tile:new(sWorld)
            if not tMeshTmp:load(fileName) then
                print("failed to load tile:["..fileName.."]")
                return nil
            end
        elseif tInfo.type == 'texture' then
            tMeshTmp = texture:new(sWorld)
            if not tMeshTmp:load(fileName) then
                print("failed to load texture:["..fileName.."]")
                return nil
            end
        elseif tInfo.type == 'gif' then
            tMeshTmp = gif:new(sWorld)
            if not tMeshTmp:load(fileName) then
                print("failed to load gif:["..fileName.."]")
                return nil
            end
        elseif tInfo.type == 'font' then
            local tMyFonts = self.tMyFonts
            if tMyFonts and tMyFonts[fileName] then
                local tFont = tMyFonts[fileName]
                tMeshTmp = tFont:add(tInfo.sText or "My text",sWorld)
                tMeshTmp.tFont = tFont
            else
                local heightFont      = tInfo.heightFont or 50
                local spaceFont       = tInfo.spaceFont  or 5
                local spaceHeightFont = tInfo.spaceHeightFont or 5
                local tFont           = font:new(fileName,heightFont,spaceFont,spaceHeightFont)
                if not tFont then
                    print("failed to load font:["..fileName.."]")
                    return nil
                end
                tMeshTmp = tFont:add(tInfo.sText or "My text",sWorld)
                tMeshTmp.tFont = tFont
                if self.tMyFonts == nil then
                    self.tMyFonts = {}
                end
                self.tMyFonts[fileName] = tFont
            end
        else
            print("Only mesh 2d:["..fileName.."] \nMesh type:"..(tInfo.type or "nil"))
            return nil
        end
        tMeshTmp:setScale(tInfo.sx,tInfo.sy,tInfo.sz)
        tMeshTmp:setAngle(tInfo.ax,tInfo.ay,tInfo.az)

        if tInfo.is2ds then
            local ew_half  = self.iExpectedWidth  * 0.5
            local eh_half  = self.iExpectedHeight * 0.5
            if tInfo.isRelative2ds then
                local x
                local y
                local w,h          = tMeshTmp:getSize()
                local w_half_mesh  = w * 0.5
                local h_half_mesh  = h * 0.5
                if tInfo.x == 0 then
                    x = ew_half
                elseif tInfo.x > 0 then
                    local xDiff = ew_half - (tInfo.x + w_half_mesh)
                    x = self.iSizeScreenWidth - xDiff - w_half_mesh
                else
                    local xDiff = (ew_half + tInfo.x - w_half_mesh)
                    x = (xDiff + w_half_mesh)
                end

                if tInfo.y == 0 then
                    y = eh_half
                elseif tInfo.y > 0 then
                    local yDiff = eh_half - (tInfo.y + h_half_mesh)
                    y = yDiff + h_half_mesh
                else
                    local yDiff = eh_half + (tInfo.y - h_half_mesh)
                    y = self.iSizeScreenHeight - (yDiff + h_half_mesh)
                end

                tMeshTmp:setPos(x,y,tInfo.z)
            else
                local x    =  ((tInfo.x + ew_half) / self.iExpectedWidth  * self.iSizeScreenWidth )
                local y    =  self.iSizeScreenHeight - (((tInfo.y + eh_half) / self.iExpectedHeight * self.iSizeScreenHeight))
                tMeshTmp:setPos(x,y,tInfo.z)
            end
        else
            tMeshTmp:setPos(tInfo.x,tInfo.y,tInfo.z)
        end

        if tInfo.tPhysicInfo then
            self:addPhysics(tMeshTmp,tInfo.tPhysicInfo)
        end
        if tInfo.iAnim and tInfo.iAnim > 1 then
            tMeshTmp:setAnim(tInfo.iAnim)
        end
        table.insert(self.tMeshesLoaded,tMeshTmp)
        return tMeshTmp
    end
    )

tScene.addMesh = (
    function(self,fileNameOrIndex)
        if type(fileNameOrIndex) == 'number' then
            if fileNameOrIndex >= 1 and fileNameOrIndex <= #self.tAllMeshInfo then
                local tInfo = self.tAllMeshInfo[fileNameOrIndex]
                return self:_addMesh(tInfo)
            end
        else
            local indexByName = self.tAllMeshInfo[fileNameOrIndex]
            if indexByName then
                local tInfo = self.tAllMeshInfo[indexByName]
                return self:_addMesh(tInfo)
            end
            for i=1, #self.tAllMeshInfo do
                local tInfo = self.tAllMeshInfo[i]
                if tInfo.fileName == fileNameOrIndex then
                    self.tAllMeshInfo[fileNameOrIndex] = i
                    return self:_addMesh(tInfo)
                end
            end
            return nil
        end
    end
    )

tScene.add = tScene.addMesh

tScene.getAll = (
    function(self,fileName)
        local tAll = {}
        for i=1, #self.tAllMeshInfo do
            local tInfo = self.tAllMeshInfo[i]
            if tInfo.fileName == fileName or select(1,tInfo.fileName:find(fileName)) then
                local tMesh = self.tMeshesLoaded[i]
                table.insert(tAll,tMesh)
            end
        end
        return tAll
    end
    )

tScene.get = (
    function(self,fileNameOrIndex)
        if type(fileNameOrIndex) == 'number' then
            if fileNameOrIndex >= 1 and fileNameOrIndex <= #self.tMeshesLoaded then
                return self.tMeshesLoaded[fileNameOrIndex]
            end
        else
            local tMesh = self.tMeshesLoadedDictionary[fileNameOrIndex]
            if tMesh then
                return tMesh
            end
            for i=1, #self.tAllMeshInfo do
                local tInfo = self.tAllMeshInfo[i]
                if tInfo.fileName == fileNameOrIndex or select(1,tInfo.fileName:find(fileNameOrIndex)) then
                    local tMesh = self.tMeshesLoaded[i]
                    self.tMeshesLoadedDictionary[fileNameOrIndex] = tMesh
                    return tMesh
                end
            end
        end
        return nil
    end
    )
tScene.reflow = (
    function(self, iW, iH)
        if not self.tPanelTree or #self.tPanelTree == 0 then return end
        local iEW = self.iExpectedWidth
        local iEH = self.iExpectedHeight
        local refRoot = {x=-iEW*0.5, y=-iEH*0.5, w=iEW, h=iEH}
        local curRoot = {x=-iW*0.5,  y=-iH*0.5,  w=iW,  h=iH}
        local refRects, curRects = {}, {}
        _traversePanels(self.tPanelTree, refRoot, curRoot, refRects, curRects)
        for i, tMesh in ipairs(self.tMeshesLoaded) do
            local tInfo = self.tAllMeshInfo[i]
            if tInfo and tInfo.panelId then
                local rr = refRects[tInfo.panelId]
                local cr = curRects[tInfo.panelId]
                if rr and cr then
                    local atype = tInfo.anchorType or "center"
                    if atype == "width" and tInfo.sizeAnchorW and rr.w > 0 then
                        local sc = cr.w / rr.w
                        tMesh:setScale(tInfo.sx*sc, tInfo.sy*sc, tInfo.sz)
                    elseif atype == "height" and tInfo.sizeAnchorH and rr.h > 0 then
                        local sc = cr.h / rr.h
                        tMesh:setScale(tInfo.sx*sc, tInfo.sy*sc, tInfo.sz)
                    elseif atype == "stretch" then
                        local nsx = tInfo.sizeAnchorW and rr.w > 0 and tInfo.sx*(cr.w/rr.w) or tInfo.sx
                        local nsy = tInfo.sizeAnchorH and rr.h > 0 and tInfo.sy*(cr.h/rr.h) or tInfo.sy
                        tMesh:setScale(nsx, nsy, tInfo.sz)
                    end
                    local ex = cr.x + (tInfo.anchorX or 0.5)*cr.w
                    local ey = cr.y + (tInfo.anchorY or 0.5)*cr.h
                    if tInfo.isRestrictedToPanel ~= false then
                        local ow, oh = tMesh:getSize()
                        local isFont = tInfo.type == "font"
                        local le = isFont and 0  or ow*0.5
                        local re = isFont and ow or ow*0.5
                        local be = isFont and oh or oh*0.5
                        local te = isFont and 0  or oh*0.5
                        ex = math.max(cr.x + le, math.min(cr.x + cr.w - re, ex))
                        ey = math.max(cr.y + be, math.min(cr.y + cr.h - te, ey))
                    end
                    if tInfo.is2ds then
                        tMesh:setPos(ex + iW*0.5, iH*0.5 - ey, tInfo.z)
                    else
                        tMesh:setPos(ex, ey, tInfo.z)
                    end
                end
            end
        end
    end
    )

tScene.onResizeWindow = (
    function(self)
        self:updateCamera()
        local iW, iH = mbm.getSizeScreen()
        self:reflow(iW, iH)
    end
    )

    EXTRA_SCRIPT
return tScene
]]

    sScene = sScene:gsub('POS_CAM_X', tostring(xCam))
    sScene = sScene:gsub('POS_CAM_Y', tostring(yCam))
    sScene = sScene:gsub('AXIS_SCALE_CAM', sScaleAxis)
    if tOptionsEditor.sExtraScript and tOptionsEditor.sExtraScript:len() > 0 then
        sScene = sScene:gsub('EXTRA_SCRIPT', string.format('\nif not mbm.include(%q) then\n    print("error on execute script",%q)\nend\n', tOptionsEditor.sExtraScript, tOptionsEditor.sExtraScript))
    else
        sScene = sScene:gsub('EXTRA_SCRIPT', '\n')
    end
    return sScene
end

-- ── Save editor state (.scene_edit.lua) ──────────────────────────────────────

--- Saves the full scene: mesh info + panel tree + editor/launch options
local function onSaveScene(sFileName)
    local oldLocaleNumeric = os.setlocale(nil, 'numeric')
    os.setlocale('C', 'numeric')
    local fp = io.open(sFileName, "w")
    if fp then
        fp:write(getHeader(sFileName))

        local bHasPhysics = false
        for _, tObj in ipairs(tAllMesh) do
            if type(tObj.tPhysicInfo) == 'table' then
                bHasPhysics = true
                break
            end
        end
        if bHasPhysics then
            fp:write('require "box2d"\n\n')
        end

        fp:write('local tScene = {}\n')

        if bHasPhysics then
            fp:write(getPhysicsFunction())
        end

        -- Options editor
        local tOptionsOut = {}
        fp:write('tScene.getOptionsEditor = function()\n    local')
        tUtil.save('tOptionsEditor', tOptionsEditor, tOptionsOut, onSaveUserData)
        for _, sLine in ipairs(tOptionsOut) do
            fp:write('    ' .. sLine .. '\n')
        end
        fp:write('    return tOptionsEditor\nend\n\n')

        -- Options launch
        tOptionsOut = {}
        fp:write('tScene.getOptionsLaunch = function()\n    local')
        tUtil.save('tOptionsLaunch', tOptionsLaunch, tOptionsOut, onSaveUserData)
        for _, sLine in ipairs(tOptionsOut) do
            fp:write('    ' .. sLine .. '\n')
        end
        fp:write('    return tOptionsLaunch\nend\n\n')

        -- Panel tree (new for grid-based editor)
        fp:write('tScene.tPanelTree = {\n')
        serializePanelTree(fp, tPanels)
        fp:write('}\n\n')

        -- Paths
        local tPaths = mbm.getAllPaths()
        for _, sPath in ipairs(tPaths) do
            fp:write(string.format('mbm.addPath(%q)\n', sPath))
        end

        -- Resolution
        local xRes, yRes
        if tOptionsEditor.bInvertResolution then
            xRes = tResolution[tOptionsEditor.iIndexResolution].y
            yRes = tResolution[tOptionsEditor.iIndexResolution].x
        else
            xRes = tResolution[tOptionsEditor.iIndexResolution].x
            yRes = tResolution[tOptionsEditor.iIndexResolution].y
        end
        fp:write(string.format('\ntScene.iExpectedWidth   = %d', xRes))
        fp:write(string.format('\ntScene.iExpectedHeight  = %d\n', yRes))

        -- Mesh info table
        fp:write('\ntScene.tAllMeshInfo = {')
        for i, tObj in ipairs(tAllMesh) do
            fp:write(string.format(
                '\n[%d]={fileName=%s,x=%g,y=%g,z=%g,sx=%g,sy=%g,sz=%g,ax=%g,ay=%g,az=%g,type=%q,iAnim=%d%s%s%s%s%s%s%s},',
                i, tUtil.getShortName(tObj.fileName, true),
                tObj.x, tObj.y, tObj.z,
                tObj.sx, tObj.sy, tObj.sz,
                tObj.ax, tObj.ay, tObj.az,
                tObj.type,
                select(2, tObj:getAnim()),
                getIs2ds4Save(tObj),
                getPhysicInfo4Save(tObj.tPhysicInfo),
                getBlockFlag4Save(tObj),
                getText4Font4Save(tObj.sText),
                getFontParams4Save(tObj),
                getIndependentCalCam4Save(tObj),
                getPanelInfo4Save(tObj)))
        end
        fp:write('}\n\n')

        fp:write('tScene.tMeshesLoaded = {}\n')
        fp:write('tScene.tMeshesLoadedDictionary = {}\n\n')
        fp:write(getSceneLoaderCode(tOptionsEditor.fSceneCamPos.x, tOptionsEditor.fSceneCamPos.y, tOptionsEditor.sScaleAxis))
        fp:close()
        os.setlocale(oldLocaleNumeric, 'numeric')
        return true
    else
        os.setlocale(oldLocaleNumeric, 'numeric')
        print('error', string.format('Could not open the file [%s] for write', sFileName))
        tUtil.showMessageWarn(string.format(tLang.L("could_not_open_for_write_fmt"), sFileName))
        return false
    end
end

local function onSaveSceneEditor()
    if sLastEditorFileName:len() == 0 then
        local fileName = mbm.saveFile(sLastEditorFileName, '*.gui.lua')
        if fileName then
            -- ensure the file always carries the .gui.lua extension
            if not fileName:match('%.gui%.lua$') then
                fileName = fileName:gsub('%.lua$', '') .. '.gui.lua'
            end
            if onSaveScene(fileName) then
                sLastEditorFileName = fileName
                tUtil.showMessage(string.format(tLang.L("scene_saved_ok_fmt"), sLastEditorFileName))
            else
                tUtil.showMessageWarn(tLang.L("failed_to_save_scene"))
            end
        end
    else
        if onSaveScene(sLastEditorFileName) then
            tUtil.showMessage(string.format(tLang.L("scene_saved_ok_fmt"), sLastEditorFileName))
        else
            tUtil.showMessageWarn(tLang.L("failed_to_save_scene"))
        end
    end
end

-- ── Load editor state ────────────────────────────────────────────────────────

--- Rebuild panel tree from saved data
local function rebuildPanelsFromData(tSavedPanels, parent)
    local result = {}
    for _, pd in ipairs(tSavedPanels) do
        local panel = createPanel(pd.name, pd.world, pd.anchor)
        panel.id       = pd.id
        panel.splitDir = pd.splitDir
        panel.pctList  = pd.pctList
        if pd.panelAnchorType then panel.panelAnchorType = pd.panelAnchorType end
        if pd.cx    ~= nil then panel.cx    = pd.cx    end
        if pd.cy    ~= nil then panel.cy    = pd.cy    end
        if pd.sizeW ~= nil then panel.sizeW = pd.sizeW end
        if pd.sizeH ~= nil then panel.sizeH = pd.sizeH end
        if pd.id >= iNextPanelId then
            iNextPanelId = pd.id + 1
        end
        if pd.children and #pd.children > 0 then
            panel.children = rebuildPanelsFromData(pd.children, panel)
        end
        table.insert(result, panel)
    end
    return result
end

local function onLoadScene()
    local fileName = mbm.openFile(sLastEditorFileName, "*.gui.lua")
    if fileName then
        onNewSceneEditor()
        local tScene = dofile(fileName)
        if tScene and type(tScene.getOptionsEditor) == 'function' then
            tOptionsEditor = tScene:getOptionsEditor()
            mbm.setColor(tOptionsEditor.tColorBackground.r, tOptionsEditor.tColorBackground.g, tOptionsEditor.tColorBackground.b)
            updateRectangleLine()
        else
            tUtil.showMessageWarn(tLang.L("not_found_get_options_editor"))
        end
        if tScene and type(tScene.getOptionsLaunch) == 'function' then
            tOptionsLaunch = tScene:getOptionsLaunch()
        else
            tUtil.showMessageWarn(tLang.L("not_found_get_options_launch"))
        end

        -- Rebuild panel tree if present
        if tScene and type(tScene.tPanelTree) == 'table' then
            tPanels = rebuildPanelsFromData(tScene.tPanelTree, nil)
        end

        if tScene and type(tScene.tAllMeshInfo) == 'table' then
            cCoroutineLoadScene = coroutine.create(function()
                local iYieldForEach            = 60
                local bOldOption               = tOptionsEditor.bCenterOfScreen
                tOptionsEditor.bCenterOfScreen = true
                local iByYield, stepYield      = 0, 0
                if #tScene.tAllMeshInfo > iYieldForEach then
                    iByYield = math.ceil(#tScene.tAllMeshInfo / iYieldForEach)
                end
                for i = 1, #tScene.tAllMeshInfo do
                    local tInfo = tScene.tAllMeshInfo[i]
                    local tMeshTmp = tUtil.onAddMeshToEditor(tInfo.fileName, false, '2dw', tInfo.sText)
                    if tMeshTmp then
                        if tInfo.iAnim and tInfo.iAnim > 1 then
                            tMeshTmp:setAnim(tInfo.iAnim)
                        end
                        tMeshTmp.tPhysicInfo = tInfo.tPhysicInfo
                        initialSetUpForAddedMesh(tMeshTmp)
                        tMeshTmp:setScale(tInfo.sx, tInfo.sy, tInfo.sz)
                        tMeshTmp:setAngle(tInfo.ax, tInfo.ay, tInfo.az)
                        tMeshTmp:setPos(tInfo.x, tInfo.y, tInfo.z)
                        tMeshTmp.is2ds         = tInfo.is2ds
                        tMeshTmp.isRelative2ds = tInfo.isRelative2ds
                        tMeshTmp.isBlocked     = tInfo.isBlocked

                        -- Restore panel assignment by id
                        if tInfo.panelId then
                            local targetPanel = nil
                            local rootRect = getRootRect()
                            traversePanels(tPanels, rootRect, 0, function(p)
                                if p.id == tInfo.panelId then targetPanel = p end
                            end)
                            if targetPanel then
                                assignObjectToPanel(tMeshTmp, targetPanel)
                                if tInfo.anchorX then tMeshTmp.anchorX = tInfo.anchorX end
                                if tInfo.anchorY then tMeshTmp.anchorY = tInfo.anchorY end
                                -- false is the only non-default value saved
                                tMeshTmp.isRestrictedToPanel = (tInfo.isRestrictedToPanel ~= false)
                                -- anchor type (nil / missing → "center" by default)
                                tMeshTmp.anchorType  = tInfo.anchorType or "center"
                                tMeshTmp.sizeAnchorW = tInfo.sizeAnchorW
                                tMeshTmp.sizeAnchorH = tInfo.sizeAnchorH
                            end
                        end
                    end
                    stepYield = stepYield + 1
                    if iByYield > 0 and stepYield >= iByYield then
                        stepYield = 0
                        coroutine.yield(i, #tScene.tAllMeshInfo)
                    end
                end
                tOptionsEditor.bCenterOfScreen = bOldOption
                sLastEditorFileName = fileName
                updateVisibilityByFilter()
                -- Rebuild visuals for restored panels then reflow objects to their anchor positions
                rebuildPanelVisuals()
                reflowPanelObjects()
            end)
        else
            tUtil.showMessageWarn(tLang.L("not_found_scene_table"))
        end
    end
end

-- ── Export clean game scene (.scene.lua) ─────────────────────────────────────

local function onExportGameScene()
    -- strip .gui from the editor filename so the export dialog defaults to e.g. "test.lua"
    local exportDefault = sLastEditorFileName:gsub('%.gui%.lua$', '.lua')
    local fileName = mbm.saveFile(exportDefault, '*.lua')  -- exported game scene stays *.lua
    if not fileName then return end

    local oldLocaleNumeric = os.setlocale(nil, 'numeric')
    os.setlocale('C', 'numeric')
    local fp = io.open(fileName, "w")
    if not fp then
        os.setlocale(oldLocaleNumeric, 'numeric')
        tUtil.showMessageWarn(string.format(tLang.L("could_not_open_for_write_fmt"), fileName))
        return
    end

    fp:write(getHeader(fileName))

    local bHasPhysics = false
    for _, tObj in ipairs(tAllMesh) do
        if type(tObj.tPhysicInfo) == 'table' then
            bHasPhysics = true
            break
        end
    end
    if bHasPhysics then
        fp:write('require "box2d"\n\n')
    end

    fp:write('local tScene = {}\n')
    if bHasPhysics then
        fp:write(getPhysicsFunction())
    end

    -- Paths
    local tPaths = mbm.getAllPaths()
    for _, sPath in ipairs(tPaths) do
        fp:write(string.format('mbm.addPath(%q)\n', sPath))
    end

    -- Resolution
    local xRes, yRes
    if tOptionsEditor.bInvertResolution then
        xRes = tResolution[tOptionsEditor.iIndexResolution].y
        yRes = tResolution[tOptionsEditor.iIndexResolution].x
    else
        xRes = tResolution[tOptionsEditor.iIndexResolution].x
        yRes = tResolution[tOptionsEditor.iIndexResolution].y
    end
    fp:write(string.format('\ntScene.iExpectedWidth   = %d', xRes))
    fp:write(string.format('\ntScene.iExpectedHeight  = %d\n', yRes))

    -- Reflow objects to their up-to-date panel positions before writing
    reflowPanelObjects()

    -- Mesh info: includes panel anchor data so runtime reflow can work
    fp:write('\ntScene.tAllMeshInfo = {')
    for i, tObj in ipairs(tAllMesh) do
        fp:write(string.format(
            '\n[%d]={fileName=%s,x=%g,y=%g,z=%g,sx=%g,sy=%g,sz=%g,ax=%g,ay=%g,az=%g,type=%q,iAnim=%d%s%s%s%s%s%s},',
            i, tUtil.getShortName(tObj.fileName, true),
            tObj.x, tObj.y, tObj.z,
            tObj.sx, tObj.sy, tObj.sz,
            tObj.ax, tObj.ay, tObj.az,
            tObj.type,
            select(2, tObj:getAnim()),
            getIs2ds4Save(tObj),
            getPhysicInfo4Save(tObj.tPhysicInfo),
            getText4Font4Save(tObj.sText),
            getFontParams4Save(tObj),
            getIndependentCalCam4Save(tObj),
            getPanelInfo4Save(tObj)))
    end
    fp:write('}\n\n')

    -- Panel tree: needed by tScene:reflow() at runtime
    if #tPanels > 0 then
        fp:write('tScene.tPanelTree = {\n')
        serializePanelTree(fp, tPanels, '    ')
        fp:write('}\n\n')
    end

    fp:write('tScene.tMeshesLoaded = {}\n')
    fp:write('tScene.tMeshesLoadedDictionary = {}\n\n')
    fp:write(getSceneLoaderCode(tOptionsEditor.fSceneCamPos.x, tOptionsEditor.fSceneCamPos.y, tOptionsEditor.sScaleAxis))
    fp:close()
    os.setlocale(oldLocaleNumeric, 'numeric')
    tUtil.showMessage(string.format(tLang.L("scene_exported_ok_fmt"), fileName))
end

-- ─────────────────────────────────────────────────────────────────────────────
-- Pass 6 — Menu bar, play, script generation
-- ─────────────────────────────────────────────────────────────────────────────

local function onSetScript()
    local fileName = mbm.openFile(tOptionsEditor.sExtraScript, "*.lua")
    if fileName then
        dofile(fileName)
        tUtil.showMessage(tLang.L("script_executed"))
        tOptionsEditor.sExtraScript = fileName
    end
end

local function onNewScene()
    onSelectAll()
    onDeleteSelected()
    camera2d.x          = 0
    camera2d.y          = 0
    bShowMeshList       = true
    tLastMeshAdded      = nil
    tFollowCam          = nil
    sLastEditorFileName = ''
    bShowAddingMesh     = false
    bShowDetailOfMesh   = true
end

local function onPlay()
    local width, height
    if tOptionsLaunch.bInvertResolution then
        width  = tResolution[tOptionsLaunch.iIndexResolution].y
        height = tResolution[tOptionsLaunch.iIndexResolution].x
    else
        width  = tResolution[tOptionsLaunch.iIndexResolution].x
        height = tResolution[tOptionsLaunch.iIndexResolution].y
    end

    local expected_width, expected_height
    if tOptionsEditor.bInvertResolution then
        expected_width  = tResolution[tOptionsEditor.iIndexResolution].y
        expected_height = tResolution[tOptionsEditor.iIndexResolution].x
    else
        expected_width  = tResolution[tOptionsEditor.iIndexResolution].x
        expected_height = tResolution[tOptionsEditor.iIndexResolution].y
    end

    if tOptionsEditor.sCurrentScriptExecution and tOptionsEditor.sCurrentScriptExecution:len() > 0 then
        tUtil.newInstance(width, height, expected_width, expected_height, tOptionsEditor.sCurrentScriptExecution)
    else
        tUtil.newInstance(width, height, expected_width, expected_height, sLastEditorFileName)
    end
end

local function createBasicScriptForScene(sFullSceneName)
    local tDefaultScene = [[
tScene = require "YOUR_SCENE"

local tLogicScene = {}

tLogicScene.onProgress = function(self,percent)
    print(string.format('Loading your scene %.1f',percent))
end

tLogicScene.onInitScene = function(self)
    camera2d    = mbm.getCamera("2d")
    camera2d.mx = 0
    camera2d.my = 0
    mbm.setColor(1,1,1)
    tScene:load(onProgress)
    bEnableMoveCamera  = true
    isClickedMouseLeft = false

    --tScene:get() -- get any mesh to do something
end

tLogicScene.onTouchDown = function(self,key,x,y)
    isClickedMouseLeft = key == 0
    camera2d.mx = x
    camera2d.my = y
end

tLogicScene.onTouchMove = function(self,key,x,y)
    if isClickedMouseLeft and bEnableMoveCamera then
        local px = (camera2d.mx - x) * camera2d.sx
        local py = (camera2d.my - y) * camera2d.sy
        camera2d.mx = x
        camera2d.my = y
        camera2d:setPos(camera2d.x + px,camera2d.y - py)
    end
end

tLogicScene.onTouchUp = function(self,key,x,y)
    isClickedMouseLeft = false
    camera2d.mx = x
    camera2d.my = y
end

tLogicScene.loop = function(self,delta)
    -- your logic here
end

return tLogicScene
]]

    local sProjectName = sFullSceneName:gsub("\\", '/')
    local tProjectName = sProjectName:split('/')
    sProjectName       = tProjectName[#tProjectName]:gsub('%..*$', '')
    tDefaultScene      = tDefaultScene:gsub('YOUR_SCENE', string.format('%s', sProjectName))
    local sTemp        = tProjectName[1] or ''
    local sSeparator   = '/'
    if mbm.is('windows') then
        sSeparator = '\\'
    end
    for i = 2, (#tProjectName - 1) do
        sTemp = sTemp .. sSeparator .. tProjectName[i]
    end

    local sFileToSave = string.format('%s%s%s-logic.lua', sTemp, sSeparator, sProjectName)
    sFileToSave = mbm.saveFile(sFileToSave, 'lua')
    if sFileToSave then
        local fp = io.open(sFileToSave, "w")
        if fp then
            fp:write(tDefaultScene)
            fp:close()
            tOptionsEditor.sCurrentScriptExecution = sFileToSave
            tUtil.showMessage(tLang.L("file_created_ok"))
        else
            print('error', string.format('Could not open the file [%s] for write', sFileToSave))
            tUtil.showMessageWarn(string.format(tLang.L("could_not_open_for_write_fmt"), sFileToSave))
        end
    end
end

local function main_menu_scene_editor_2d()
    if tImGui.BeginMainMenuBar() then

        -- ── File ─────────────────────────────────────────────────────────
        if tImGui.BeginMenu(tLang.L("menu_file")) then
            local pressed
            pressed = tImGui.MenuItem(tLang.L("new_scene"), "Ctrl+N", false)
            if pressed then onNewScene() end

            pressed = tImGui.MenuItem(tLang.L("load_scene"), "Ctrl+O", false)
            if pressed then onLoadScene() end

            tImGui.Separator()
            pressed = tImGui.MenuItem(tLang.L("set_extra_script"), 'Extension', false)
            if pressed then onSetScript() end
            tImGui.SameLine()
            tImGui.HelpMarker(tLang.L("help_script_extra") .. tostring(tOptionsEditor.sExtraScript))

            tImGui.Separator()
            pressed = tImGui.MenuItem(tLang.L("save_scene"), "Ctrl+S", false)
            if pressed then onSaveSceneEditor() end
            if sLastEditorFileName and sLastEditorFileName:len() > 0 then
                tImGui.SameLine()
                tImGui.HelpMarker(sLastEditorFileName)
            end

            pressed = tImGui.MenuItem(tLang.L("save_scene_as"), nil, false)
            if pressed then
                local sEditorFileName = sLastEditorFileName
                sLastEditorFileName = ''
                onSaveSceneEditor()
                if sLastEditorFileName == '' then
                    sLastEditorFileName = sEditorFileName
                end
            end

            tImGui.Separator()
            pressed = tImGui.MenuItem(tLang.L("export_scene"), nil, false)
            if pressed then onExportGameScene() end

            tImGui.Separator()
            pressed = tImGui.MenuItem(tLang.L("menu_quit"), "Alt+F4", false)
            if pressed then mbm.quit() end

            tWindowsArea:addThisWindow()
            tImGui.EndMenu()
        end

        -- ── Mesh ─────────────────────────────────────────────────────────
        if tImGui.BeginMenu(tLang.L("menu_mesh")) then
            local pressed, checked

            pressed = tImGui.MenuItem(tLang.L("add_mesh"), 'Ctrl+M', false)
            if pressed then onAddMesh() end

            tImGui.Separator()
            local title_dup = tLang.L("duplicate_last_mesh")
            if #tSelectedObjs > 0 then
                title_dup = tLang.L("duplicate_all_mesh_selected")
            end
            pressed = tImGui.MenuItem(title_dup, 'Ctrl+D', false)
            if pressed then onDuplicated() end

            pressed = tImGui.MenuItem(tLang.L("select_all_mesh"), 'Ctrl+A', false)
            if pressed then onSelectAll() end

            pressed = tImGui.MenuItem(tLang.L("invert_selected_mesh"), 'Ctrl+I', false)
            if pressed then onInvertSelection() end

            pressed = tImGui.MenuItem(tLang.L("unselect_all_mesh"), 'Ctrl+U', false)
            if pressed then onUnSelectAll() end

            pressed = tImGui.MenuItem(tLang.L("delete_selected_mesh"), 'Ctrl+Delete', false)
            if pressed then onDeleteSelected() end

            tImGui.Separator()
            pressed = tImGui.MenuItem(tLang.L("view_mesh_list"), 'Ctrl+L', false)
            if pressed then bShowMeshList = true end

            tImGui.Separator()
            pressed = tImGui.MenuItem(tLang.L("view_options_adding_mesh"), nil, false)
            if pressed then bShowAddingMesh = true end

            tImGui.Separator()
            pressed, checked = tImGui.MenuItem(tLang.L("view_detail_selected_mesh"), true, bShowDetailOfMesh)
            if pressed then bShowDetailOfMesh = checked end

            tWindowsArea:addThisWindow()
            tImGui.EndMenu()
        end

        -- ── Grid (new) ──────────────────────────────────────────────────
        if tImGui.BeginMenu(tLang.L("menu_grid")) then
            local pressed, checked

            pressed, checked = tImGui.MenuItem(tLang.L("panel_browser"), true, bShowPanelBrowser)
            if pressed then bShowPanelBrowser = checked end

            pressed, checked = tImGui.MenuItem(tLang.L("panel_properties"), true, bShowPanelProps)
            if pressed then bShowPanelProps = checked end

            tImGui.Separator()
            pressed = tImGui.MenuItem(tLang.L("add_root_panel"), nil, false)
            if pressed then
                local newPanel = createPanel("panel_" .. iNextPanelId, tGridDialog.tWorldOptions[tGridDialog.iWorldIndex] == "2D Screen" and "2ds" or "2dw", nil)
                table.insert(tPanels, newPanel)
                tSelectedPanel = newPanel
                bShowPanelProps = true
                rebuildPanelVisuals()
            end

            pressed = tImGui.MenuItem(tLang.L("create_grid"), nil, false)
            if pressed then bShowGridDialog = true end

            tWindowsArea:addThisWindow()
            tImGui.EndMenu()
        end

        -- ── World ────────────────────────────────────────────────────────
        if tImGui.BeginMenu(tLang.L("menu_world")) then
            tImGui.Text(tLang.L("resolution_expected"))
            tOptionsEditor.bInvertResolution = tImGui.Checkbox(tLang.L("invert_width_height"), tOptionsEditor.bInvertResolution)
            local tResolutionString = {}
            for i = 1, #tResolution do
                if tOptionsEditor.bInvertResolution then
                    table.insert(tResolutionString, string.format('%d x %d %s', tResolution[i].y, tResolution[i].x, tResolution[i].comment))
                else
                    table.insert(tResolutionString, string.format('%d x %d %s', tResolution[i].x, tResolution[i].y, tResolution[i].comment))
                end
            end
            local ret, current_item = tImGui.Combo('##ComboResolution', tOptionsEditor.iIndexResolution, tResolutionString)
            if ret then
                tOptionsEditor.iIndexResolution = current_item
            end

            tImGui.Text(tLang.L("axis_camera_scale"))
            local indexAxis
            if tOptionsEditor.sScaleAxis == 'x' then indexAxis = 1
            elseif tOptionsEditor.sScaleAxis == 'y' then indexAxis = 2
            else indexAxis = 3
            end

            local index_activated = tImGui.RadioButton(tLang.L("axis_x"), indexAxis, 1)
            tImGui.SameLine()
            index_activated = tImGui.RadioButton('Y', index_activated, 2)
            tImGui.SameLine()
            index_activated = tImGui.RadioButton(tLang.L("stretched"), index_activated, 3)
            tImGui.SameLine()
            tImGui.HelpMarker(tLang.L("help_default_scale"))

            if index_activated == 1 then tOptionsEditor.sScaleAxis = 'x'
            elseif index_activated == 2 then tOptionsEditor.sScaleAxis = 'y'
            else tOptionsEditor.sScaleAxis = 'xy'
            end

            tOptionsEditor.bDrawResolution = tImGui.Checkbox(tLang.L("draw_resolution_rect"), tOptionsEditor.bDrawResolution)
            updateRectangleLine()

            tImGui.Separator()
            tImGui.Text(tLang.L("camera_position"))

            local step      = 1.0
            local step_fast = 10.0
            local format    = "%.2f"

            local result, fValue = tImGui.InputFloat(tLang.L("axis_x") .. '##XCamera', camera2d.x, step, step_fast, format, 0)
            if result then camera2d.x = fValue end

            result, fValue = tImGui.InputFloat(tLang.L("axis_y") .. '##YCamera', camera2d.y, step, step_fast, format, 0)
            if result then camera2d.y = fValue end

            if tImGui.Button(tLang.L("set_initial_camera_pos"), {x = -1, y = 0}) then
                tOptionsEditor.fSceneCamPos.x = camera2d.x
                tOptionsEditor.fSceneCamPos.y = camera2d.y
            end

            tImGui.Text(tLang.L("initial_scene_position"))
            tImGui.TextDisabled(string.format('X:%.2f', tOptionsEditor.fSceneCamPos.x))
            tImGui.TextDisabled(string.format('Y:%.2f', tOptionsEditor.fSceneCamPos.y))

            tWindowsArea:addThisWindow()
            tImGui.EndMenu()
        end

        -- ── Options ──────────────────────────────────────────────────────
        if tImGui.BeginMenu(tLang.L("menu_options")) then
            local pressed, checked

            pressed, checked = tImGui.MenuItem(tLang.L("move_windows"), true, bEnableMoveWindow)
            if pressed then
                bEnableMoveWindow = checked
                if bEnableMoveWindow then
                    ImGuiWindowFlags_NoMove = 0
                else
                    ImGuiWindowFlags_NoMove = tImGui.Flags('ImGuiWindowFlags_NoMove')
                end
            end

            pressed, checked = tImGui.MenuItem(tLang.L("show_alpha_pattern"), true, tex_alpha_pattern.visible)
            if pressed then tex_alpha_pattern.visible = checked end

            tLang.renderLanguageSubmenu()

            tImGui.Separator()
            if tImGui.BeginMenu(tLang.L("background_color")) then
                local sz       = tImGui.GetTextLineHeight()
                local rounding = 0
                local flags    = 0

                local colors = {
                    {'default',  tUtil.tColorBackground},
                    {'white',    {r = 1, g = 1, b = 1, a = 1}},
                    {'black',    {r = 0, g = 0, b = 0, a = 1}},
                    {'red',      {r = 1, g = 0, b = 0, a = 1}},
                    {'green',    {r = 0, g = 1, b = 0, a = 1}},
                    {'blue',     {r = 0, g = 0, b = 1, a = 1}},
                    {'cyan',     {r = 0, g = 1, b = 1, a = 1}},
                    {'yellow',   {r = 1, g = 1, b = 0, a = 1}},
                    {'magenta',  {r = 1, g = 0, b = 1, a = 1}},
                }

                for i = 1, #colors do
                    local winPos = tImGui.GetCursorScreenPos()
                    local p_max  = {x = winPos.x + sz, y = winPos.y + sz}
                    local name   = tLang.L(colors[i][1])
                    local color  = colors[i][2]
                    tImGui.AddRectFilled(winPos, p_max, color, rounding, flags)
                    tImGui.Dummy({x = sz, y = sz})
                    tImGui.SameLine()
                    pressed = tImGui.MenuItem(name)
                    if pressed then
                        mbm.setColor(color.r, color.g, color.b)
                        tOptionsEditor.tColorBackground = color
                    end
                end
                tImGui.EndMenu()
            end
            tWindowsArea:addThisWindow()
            tImGui.EndMenu()
        end

        -- ── Run ──────────────────────────────────────────────────────────
        if tImGui.BeginMenu(tLang.L("menu_run")) then
            tImGui.Text(tLang.L("resolution"))
            tOptionsLaunch.bInvertResolution = tImGui.Checkbox(tLang.L("invert_width_height"), tOptionsLaunch.bInvertResolution)
            local tResolutionString = {}
            for i = 1, #tResolution do
                if tOptionsLaunch.bInvertResolution then
                    table.insert(tResolutionString, string.format('%d x %d %s', tResolution[i].y, tResolution[i].x, tResolution[i].comment))
                else
                    table.insert(tResolutionString, string.format('%d x %d %s', tResolution[i].x, tResolution[i].y, tResolution[i].comment))
                end
            end
            local ret, current_item = tImGui.Combo('##ComboResolutionLaunch', tOptionsLaunch.iIndexResolution, tResolutionString)
            if ret then
                tOptionsLaunch.iIndexResolution = current_item
            end

            if tImGui.Button(tLang.L("play"), {x = 200, y = 0}) then
                onPlay()
            end
            tImGui.SameLine()
            tImGui.TextDisabled('F5')

            tImGui.Text(tLang.L("execute_script"))
            tImGui.SameLine()
            tImGui.HelpMarker(tLang.L("help_execute_script_test"))
            if tImGui.Button('...', {x = 30, y = 0}) then
                local fileName = mbm.openFile(tOptionsEditor.sCurrentScriptExecution, "*.lua")
                if fileName then
                    tOptionsEditor.sCurrentScriptExecution = fileName
                end
            end

            if tOptionsEditor.sCurrentScriptExecution:len() == 0 then
                tImGui.SameLine()
                if tImGui.Button(tLang.L("create_it_for_me"), {x = 160, y = 0}) then
                    if tOptionsEditor.sCurrentScriptExecution and tOptionsEditor.sCurrentScriptExecution:len() > 0 then
                        createBasicScriptForScene(tOptionsEditor.sCurrentScriptExecution)
                    elseif sLastEditorFileName and sLastEditorFileName:len() > 0 then
                        createBasicScriptForScene(sLastEditorFileName)
                    else
                        tUtil.showMessageWarn(tLang.L("no_scene_loaded_for_script"))
                    end
                end
            end

            if tOptionsEditor.sCurrentScriptExecution and tOptionsEditor.sCurrentScriptExecution:len() > 0 then
                tImGui.TextDisabled(tUtil.getShortName(tOptionsEditor.sCurrentScriptExecution))
                tImGui.SameLine()
                tImGui.HelpMarker(tOptionsEditor.sCurrentScriptExecution)
                tImGui.SameLine()
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), {r = 1, g = 0, b = 0.3, a = 1})
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r = 0, g = 0, b = 0.3, a = 0})
                if tImGui.Button('X', {x = 40, y = 0}) then
                    tOptionsEditor.sCurrentScriptExecution = ''
                end
                tImGui.PopStyleColor(2)
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(tLang.L("clear_script"))
                    tImGui.EndTooltip()
                end
            end
            tImGui.EndMenu()
            tWindowsArea:addThisWindow()
        end

        -- ── About ────────────────────────────────────────────────────────
        if tImGui.BeginMenu(tLang.L("menu_about")) then
            local pressed
            pressed = tImGui.MenuItem(tLang.L("scene_editor_2d"), nil, false)
            if pressed then
                if mbm.is('windows') then
                    os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/editors.html#scene-editor-2d"')
                elseif mbm.is('linux') then
                    os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/editors.html#scene-editor-2d"')
                elseif mbm.is('macos') then
                    os.execute('open "https://mbm-documentation.readthedocs.io/en/latest/editors.html#scene-editor-2d"')
                end
            end
            pressed = tImGui.MenuItem(tLang.L("mbm_engine"), nil, false)
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
                tImGui.TextDisabled(string.format('%s\nIMGUI: %s', mbm.get('version'), tImGui.GetVersion()))
                tImGui.EndMenu()
            end

            tImGui.EndMenu()
            tWindowsArea:addThisWindow()
        end

        tImGui.EndMainMenuBar()
    end
end

-- ─────────────────────────────────────────────────────────────────────────────
-- onInitScene
-- ─────────────────────────────────────────────────────────────────────────────
function onInitScene()
    camera2d = mbm.getCamera("2d")
    camera2d.iIteration = 0

    -- Origin lines
    tLineCenterX = line:new("2dw", 0, 0, 50)
    tLineCenterY = line:new("2dw", 0, 0, 50)
    tLineCenterX:add({-9999999,0, 9999999,0})
    tLineCenterY:add({0,-9999999, 0,9999999})
    tLineCenterX:setColor(1, 0, 0)
    tLineCenterY:setColor(0, 1, 0)

    -- Resolution boundary rectangle
    tLineScreen2d = line:new("2dw")
    local xRes, yRes = 800, 600
    tLineScreen2d:add({-xRes/2,-yRes/2, -xRes/2,yRes/2, xRes/2,yRes/2, xRes/2,-yRes/2, -xRes/2,-yRes/2})
    tLineScreen2d:setColor(0.7, 0.7, 0.7)

    -- Alpha pattern background
    local sTextureFileName = tUtil.createAlphaPattern(1024, 768, 32,
        {r=240,g=240,b=240}, {r=125,g=125,b=125})
    if sTextureFileName then
        local iW, iH = mbm.getSizeScreen()
        tex_alpha_pattern = texture:new('2dw')
        tex_alpha_pattern:load(sTextureFileName)
        tex_alpha_pattern:setSize(iW, iH)
        tex_alpha_pattern.z       = 99
        tex_alpha_pattern.visible = false
    end

    v1 = vec2:new()

    ImGuiWindowFlags_NoMove        = tImGui.Flags('ImGuiWindowFlags_NoMove')
    ImGuiTreeNodeFlags_Selected    = tImGui.Flags('ImGuiTreeNodeFlags_Selected')
    ImGuiTreeNodeFlags_DefaultOpen = tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')

    tWindowsArea = tUtil.onNewAnyWindowsHovered()

    tUtil.sMessageOverlay = 'Welcome to Scene Editor 2D!!!'

    onNewSceneEditor()
end

-- ─────────────────────────────────────────────────────────────────────────────
-- Input handlers
-- ─────────────────────────────────────────────────────────────────────────────
function onTouchDown(key, x, y)
    if cCoroutineLoadScene then return end
    local anyWindowHovered = tWindowsArea:IsAnyWindowHovered(x, y)
    isClickedMouseLeft     = key == 0 and not anyWindowHovered
    camera2d.mx = x
    camera2d.my = y
    bEnableMoveWorld    = true
    bClickedOverAnyMesh = false
    bMovingAnyMesh      = false
    tFollowCam          = nil

    if isClickedMouseLeft then
        local clickedX, clickedY = mbm.to2dw(x, y)

        -- Check if clicked on a panel (for panel selection)
        local rootRect = getRootRect()
        local hitPanel = hitTestPanel(clickedX, clickedY, tPanels, rootRect)
        if hitPanel then
            tSelectedPanel  = hitPanel
            bShowPanelProps = true
            updatePanelVisuals()
        end

        -- Check if clicked on an object
        for _, tObj in ipairs(tAllMesh) do
            if not tObj.isBlocked and tObj.visible and tObj:isOver(x, y) then
                if keyControlPressed then
                    setSelectedObj(tObj, true)
                else
                    onUnSelectAll()
                    setSelectedObj(tObj, true)
                end
                bEnableMoveWorld    = false
                bClickedOverAnyMesh = true
            end
        end

        -- Save origin positions for dragging
        for _, tObj in ipairs(tSelectedObjs) do
            tObj.originx  = tObj.x
            tObj.originy  = tObj.y
            tObj.clickedX = clickedX
            tObj.clickedY = clickedY
        end
    end
end

function onTouchMove(key, x, y)
    if cCoroutineLoadScene then return end
    if bEnableMoveWorld then
        if isClickedMouseLeft then
            local px = (camera2d.mx - x) * camera2d.sx
            local py = (camera2d.my - y) * camera2d.sy
            camera2d.mx = x
            camera2d.my = y
            camera2d:setPos(camera2d.x + px, camera2d.y - py)
        end
    elseif key == 0 then
        local mx, my = mbm.to2dw(x, y)
        if bClickedOverAnyMesh then
            bMovingAnyMesh = true
            for _, tObj in ipairs(tSelectedObjs) do
                v1:set(tObj.originx, tObj.originy)
                v1:sub(tObj.clickedX, tObj.clickedY)
                v1:add(mx, my)
                if tObj.isBlockedX then v1.x = tObj.x end
                if tObj.isBlockedY then v1.y = tObj.y end
                -- Clamp to panel bounds so the object stays inside its assigned panel
                -- Font objects have top-left origin; all others use center origin.
                if tObj.isRestrictedToPanel ~= false and tObj.panelRef and tObj.panelRef._rect then
                    local r = tObj.panelRef._rect
                    local ow, oh = tObj:getSize()
                    local le, re, be, te = getObjExtents(tObj, ow, oh)
                    v1.x = math.max(r.x + le, math.min(r.x + r.w - re, v1.x))
                    v1.y = math.max(r.y + be, math.min(r.y + r.h - te, v1.y))
                end
                tObj:setPos(v1.x, v1.y)
            end
        elseif not tWindowsArea:IsAnyWindowHovered(x, y) then
            for _, tObj in ipairs(tAllMesh) do
                if not tObj.isBlocked and tObj.visible and tObj:isOver(x, y) then
                    tObj.tShape:setTexture(sTextureShapeOver)
                    tObj.tShape.visible = true
                elseif tObj.isSelected then
                    tObj.tShape:setTexture(sTextureShapeSelected)
                    tObj.tShape.visible = true
                else
                    tObj.tShape.visible = false
                end
            end
        end
    end
end

function onTouchUp(key, x, y)
    if cCoroutineLoadScene then return end
    if key == 0 and isClickedMouseLeft and not bEnableMoveWorld then
        if not bMovingAnyMesh and bClickedOverAnyMesh then
            for _, tObj in ipairs(tSelectedObjs) do
                if not tObj.bJustSelected and tObj:isOver(x, y) then
                    setSelectedObj(tObj, false)
                    break
                end
            end
        end
        -- After drag: update anchor for panel-assigned objects
        if bMovingAnyMesh then
            for _, tObj in ipairs(tSelectedObjs) do
                if tObj.panelRef and tObj.panelRef._rect then
                    local r = tObj.panelRef._rect
                    tObj.anchorX = (tObj.x - r.x) / r.w
                    tObj.anchorY = (tObj.y - r.y) / r.h
                    if tObj.isRestrictedToPanel ~= false then
                        tUtil.showMessage(string.format(
                            "[%s] anchor X=%.3f Y=%.3f",
                            tObj.panelRef.name, tObj.anchorX, tObj.anchorY))
                    end
                end
            end
        end
    end
    isClickedMouseLeft  = false
    bEnableMoveWorld    = false
    bClickedOverAnyMesh = false
    camera2d.mx = x
    camera2d.my = y
end

function onTouchZoom(zoom)
    if cCoroutineLoadScene then return end
end

function onKeyDown(key)
    if cCoroutineLoadScene then return end
    if key == mbm.getKeyCode('control') then
        keyControlPressed = true
    elseif key == mbm.getKeyCode('shift') then
        keyShiftPressed = true
    elseif keyControlPressed then
        if key == mbm.getKeyCode('S') then
            onSaveSceneEditor()
        elseif key == mbm.getKeyCode('O') then
            onLoadScene()
        elseif key == mbm.getKeyCode('N') then
            onNewSceneEditor()
            updateRectangleLine()
        elseif key == mbm.getKeyCode('I') then
            onInvertSelection()
        elseif key == mbm.getKeyCode('A') then
            onSelectAll()
        elseif key == mbm.getKeyCode('M') then
            onAddMesh()
        elseif key == mbm.getKeyCode('D') or key == mbm.getKeyCode('V') then
            onDuplicated()
        elseif key == mbm.getKeyCode('U') then
            onUnSelectAll()
        elseif key == mbm.getKeyCode('delete') then
            onDeleteSelected()
        elseif key == mbm.getKeyCode('L') then
            bShowMeshList = true
        end
    elseif key == mbm.getKeyCode('F5') then
        onPlay()
    elseif key == mbm.getKeyCode('esc') or mbm.getKeyName(key) == 'ESCAPE' then
        onUnSelectAll()
        tSelectedPanel = nil
        updatePanelVisuals()
    elseif key == mbm.getKeyCode('up') or key == mbm.getKeyCode('W') then
        tManuallyMoveCam = {x = 0, y = 1}
    elseif key == mbm.getKeyCode('down') or key == mbm.getKeyCode('S') then
        tManuallyMoveCam = {x = 0, y = -1}
    elseif key == mbm.getKeyCode('left') or key == mbm.getKeyCode('A') then
        tManuallyMoveCam = {x = -1, y = 0}
    elseif key == mbm.getKeyCode('right') or key == mbm.getKeyCode('D') then
        tManuallyMoveCam = {x = 1, y = 0}
    end
end

function onKeyUp(key)
    if cCoroutineLoadScene then return end
    if key == mbm.getKeyCode('control') then
        keyControlPressed = false
    elseif key == mbm.getKeyCode('shift') then
        keyShiftPressed = false
    elseif key == mbm.getKeyCode('up') or key == mbm.getKeyCode('W') or
           key == mbm.getKeyCode('down') or key == mbm.getKeyCode('S') or
           key == mbm.getKeyCode('left') or key == mbm.getKeyCode('A') or
           key == mbm.getKeyCode('right') or key == mbm.getKeyCode('D') then
        tManuallyMoveCam = nil
    end
end

-- ─────────────────────────────────────────────────────────────────────────────
-- onLoop — main frame dispatcher
-- ─────────────────────────────────────────────────────────────────────────────
function onLoop(delta)
    if cCoroutineLoadScene then
        local sStatus = coroutine.status(cCoroutineLoadScene)
        if sStatus == 'suspended' or sStatus == 'normal' then
            local bRet, iCurrent, iTotal = coroutine.resume(cCoroutineLoadScene)
            if not bRet then
                tUtil.showMessageWarn(tostring(iCurrent))
            else
                if iCurrent and iTotal then
                    iTotal = iTotal + 1
                    onProgress(iCurrent / iTotal * 100)
                end
            end
        elseif sStatus == 'dead' then
            cCoroutineLoadScene = nil
            local tSplash = mbm.getSplash()
            if tSplash then tSplash.visible = false end
            mbm.setFakeFps(120, 60)
            onProgress(100)
        end
    else
        tWindowsArea = tUtil.onNewAnyWindowsHovered()

        -- Update index for all objects
        for i, tObj in ipairs(tAllMesh) do
            tObj.iIndex = i
        end

        -- Menu bar
        main_menu_scene_editor_2d()

        -- Resolution rect visibility
        tLineScreen2d.visible = tOptionsEditor.bDrawResolution

        -- Alpha pattern centered on camera
        if tex_alpha_pattern then
            tex_alpha_pattern:setPos(camera2d.x, camera2d.y)
        end

        -- Panel visuals update (selection highlight)
        updatePanelVisuals()

        -- ImGui panels
        showPanelBrowser()
        showPanelProperties()
        showTransformQuick()
        showGridDialog()
        showMeshList()
        showAddingMeshOptions()
        showDetailOfMesh()

        -- Overlay messages
        tUtil.showOverlayMessage()

        -- Camera follow
        cameraFollowing()
    end
end
