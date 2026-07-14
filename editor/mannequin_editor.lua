tImGui   = require "ImGui"
tUtil    = require "editor_utils"
tBlender = require "blender_cli_wrapper"

------------------------------------------------------------------------------------------------------------------
-- State
------------------------------------------------------------------------------------------------------------------

-- 'mark' (2D image + drag-to-adjust template) | 'preview' (3D primitive skeleton/camera) |
-- 'mesh' (continuous exported-mesh geometry, untextured) | 'textured' (same mesh, front photo applied)
sActiveTab = 'mark'

camera3d = nil
camera2d = nil
cam3d = { azimuth = 0.3, elevation = 0.35, distance = 900, fx = 0, fy = 0, fz = 0 }
tCam3dMove = { forward = 0, right = 0 }
mouseLastX, mouseLastY = 0, 0
isOrbiting = false
tDragKey = nil
bCameraAutoFramed = false  -- true once the user manually orbits/zooms -- stops auto-framing after that

-- Reference images live in 2D world space ("2dw"), NOT 3D -- a flat 2D render object always
-- faces the viewer and has no camera-angle-dependent picking math, unlike a 3D quad (a 3D quad
-- went invisible from some camera angles and its ray-vs-plane click math had a sign inversion at
-- others, both fixed by this switch -- see docs/mannequin-editor-plan.md's bugfix log). The 3D
-- viewport (cam3d) is now exclusively for orbiting/inspecting the generated mannequin preview,
-- not for marking. originX/originY = world position of the image's bottom-left corner; scale =
-- display scale (shrinks huge photos to a manageable on-screen size -- marker data itself always
-- stays in true image-pixel units, scale is purely a display convenience, undone in worldToImagePixels).
tImages = {
    front = { path = nil, tex = nil, w = 0, h = 0, scale = 1, originX = 0, originY = 0 },
    side  = { path = nil, tex = nil, w = 0, h = 0, scale = 1, originX = 0, originY = 0, facing = 1 },
}
sLastImagePath = ''
tMarkerDotHandles = {}   -- [key] = { front = handle|nil, side = handle|nil }
tSkeletonLineHandles = {} -- one 2D line per bone segment, front-view projection only
MAX_DISPLAY_UNITS = 800  -- longer axis of a loaded image is shrunk to fit within this many world units

tMarkerDefs = {
    {key='chin',      label='Queixo',        defaultRadius=25, defaultDepthRatio=0.9},
    {key='lShoulder', label='Ombro E',       defaultRadius=22, defaultDepthRatio=0.8},
    {key='rShoulder', label='Ombro D',       defaultRadius=22, defaultDepthRatio=0.8},
    {key='lElbow',    label='Cotovelo E',    defaultRadius=16, defaultDepthRatio=1.0},
    {key='rElbow',    label='Cotovelo D',    defaultRadius=16, defaultDepthRatio=1.0},
    {key='lWrist',    label='Pulso E',       defaultRadius=12, defaultDepthRatio=1.0},
    {key='rWrist',    label='Pulso D',       defaultRadius=12, defaultDepthRatio=1.0},
    {key='groin',     label='Virilha',       defaultRadius=35, defaultDepthRatio=0.7},
    {key='lKnee',     label='Joelho E',      defaultRadius=24, defaultDepthRatio=1.0},
    {key='rKnee',     label='Joelho D',      defaultRadius=24, defaultDepthRatio=1.0},
    {key='lHandTip',  label='Ponta Mao E',   defaultRadius=10, defaultDepthRatio=1.0},
    {key='rHandTip',  label='Ponta Mao D',   defaultRadius=10, defaultDepthRatio=1.0},
    {key='lFootTip',  label='Ponta Pe E',    defaultRadius=14, defaultDepthRatio=1.0},
    {key='rFootTip',  label='Ponta Pe D',    defaultRadius=14, defaultDepthRatio=1.0},
}

-- Default T-pose template, as (xFrac, yFrac) of whatever front image gets loaded -- derived
-- directly from a real marked photo (706x686, person filling ~80% of frame height, centered),
-- so a brand-new image starts with a full plausible skeleton to DRAG into alignment instead of
-- an empty one to click-place point by point. Not meant to be anatomically precise -- just a
-- reasonable starting guess for a person centered and mostly filling a portrait-oriented photo.
DEFAULT_TEMPLATE = {
    chin      = {0.4966, 0.8222}, groin     = {0.5036, 0.4344},
    lShoulder = {0.3775, 0.7712}, rShoulder = {0.6325, 0.7712},
    lElbow    = {0.2472, 0.7551}, rElbow    = {0.7529, 0.7464},
    lWrist    = {0.1651, 0.7566}, rWrist    = {0.8478, 0.7478},
    lHandTip  = {0.0432, 0.7449}, rHandTip  = {0.9426, 0.7551},
    lKnee     = {0.4257, 0.2872}, rKnee     = {0.5787, 0.2842},
    lFootTip  = {0.3521, 0.0175}, rFootTip  = {0.6467, 0.0189},
}

-- child -> parent. Doubles as the bone list: iterating this table's pairs is iterating every
-- bone segment. spine1/shoulderCenter/lAnkle/rAnkle are synthetic joints (never clicked directly,
-- see resolveJoints) so the 14 marked points alone produce a full bipedal hierarchy.
PARENT_OF = {
    spine1='groin', shoulderCenter='spine1', chin='shoulderCenter',
    lShoulder='shoulderCenter', rShoulder='shoulderCenter',
    lElbow='lShoulder', rElbow='rShoulder',
    lWrist='lElbow', rWrist='rElbow',
    lHandTip='lWrist', rHandTip='rWrist',
    lKnee='groin', rKnee='groin',
    lAnkle='lKnee', rAnkle='rKnee',
    lFootTip='lAnkle', rFootTip='rAnkle',
}

tMarkers = {}       -- [key] = {front={px,py}, side={px,py}, radius=, depthRatio=}
sArmedMarker = nil

tJoints = {}         -- resolved (marked + synthetic) world positions, rebuilt by resolveJoints()
iMannequinRebuildGen = 0
tMannequinHandles = { spheres = {}, bones = {} }
tMeshPreviewHandle = nil  -- one continuous shape built from buildExportMesh() -- shown untextured
                          -- on 'mesh', with the front photo applied as texture on 'textured'
iMeshPreviewRebuildGen = 0
MESH_PREVIEW_COLOR = {0.85, 0.85, 0.9, 1.0}

sStatusMessage = ''

-- Marco 3: headless Blender build (JSON -> real armature+mesh -> FBX), same coroutine/polling
-- shape as mesh_debug.lua's blenderImportCoroutine, but much simpler (one file in, one file out,
-- no batch/streaming/animation-bake options).
tBlenderBuildState = {
    bOpen = false,
    bOpenPopup = false,
    bBuilding = false,
    co = nil,
    sStatus = '',
    bStatusOk = true,
    iTimeoutSecs = 120,
    bAbortRequested = false,
    sCancelFile = '',
    sOutputFbx = '',
    bDebugSteps = false,
}
sLastFbxExportPath = 'character.fbx'

------------------------------------------------------------------------------------------------------------------
-- Orbit camera (adapted from scene_editor3d.lua's cam3dGetPos/applyCam3d/updateCam3dKeyboardMovement --
-- those are local/non-exported there, same reason this file re-implements them, per that file's own comment).
-- This camera is used ONLY to inspect the 3D mannequin preview -- marking happens in 2D (see below).
------------------------------------------------------------------------------------------------------------------

function cam3dGetPos(c)
    local x = c.fx + c.distance * math.cos(c.elevation) * math.sin(c.azimuth)
    local y = c.fy + c.distance * math.sin(c.elevation)
    local z = c.fz + c.distance * math.cos(c.elevation) * math.cos(c.azimuth)
    return x, y, z
end

function applyCam3d(c)
    local x, y, z = cam3dGetPos(c)
    camera3d:setPos(x, y, z)
    camera3d:setFocus(c.fx, c.fy, c.fz)
end

function updateCam3dKeyboardMovement(delta)
    if tCam3dMove.forward == 0 and tCam3dMove.right == 0 then return end
    if tImGui.GetWantCaptureKeyboard() then return end
    local fw = camera3d:getNormal('F')
    local rg = camera3d:getNormal('R')
    local fwLen = math.sqrt(fw.x * fw.x + fw.z * fw.z)
    local rgLen = math.sqrt(rg.x * rg.x + rg.z * rg.z)
    if fwLen < 1e-6 or rgLen < 1e-6 then return end
    local fwx, fwz = fw.x / fwLen, fw.z / fwLen
    local rgx, rgz = rg.x / rgLen, rg.z / rgLen
    local speed = cam3d.distance * 0.8 * delta
    cam3d.fx = cam3d.fx + (fwx * tCam3dMove.forward + rgx * tCam3dMove.right) * speed
    cam3d.fz = cam3d.fz + (fwz * tCam3dMove.forward + rgz * tCam3dMove.right) * speed
end

------------------------------------------------------------------------------------------------------------------
-- Primitive geometry (raw-vertex build -- no named sphere/cylinder primitive exists in SHAPE_MESH's
-- Lua binding, same situation documented in scene_editor3d.lua's unitSphereVerts, which this copies)
------------------------------------------------------------------------------------------------------------------

local function unitSphereVerts(latSegments, lonSegments)
    latSegments = latSegments or 8
    lonSegments = lonSegments or 12
    local function toXYZ(theta, phi)
        local s = math.sin(theta)
        return s * math.cos(phi), math.cos(theta), s * math.sin(phi)
    end
    local verts = {}
    local function push(x, y, z) table.insert(verts, x); table.insert(verts, y); table.insert(verts, z) end
    for i = 0, latSegments - 1 do
        local theta1 = (i / latSegments) * math.pi
        local theta2 = ((i + 1) / latSegments) * math.pi
        for j = 0, lonSegments - 1 do
            local phi1 = (j / lonSegments) * math.pi * 2
            local phi2 = ((j + 1) / lonSegments) * math.pi * 2
            local x1, y1, z1 = toXYZ(theta1, phi1)
            local x2, y2, z2 = toXYZ(theta1, phi2)
            local x3, y3, z3 = toXYZ(theta2, phi1)
            local x4, y4, z4 = toXYZ(theta2, phi2)
            push(x1, y1, z1); push(x3, y3, z3); push(x4, y4, z4)
            push(x1, y1, z1); push(x4, y4, z4); push(x2, y2, z2)
        end
    end
    return verts
end

-- Built along local +Y from y=0 (radiusBottom) to y=height (radiusTop). No caps -- joint spheres
-- already cover the seams between adjoining bones.
local function unitCylinderVerts(radiusTop, radiusBottom, height, radialSegments)
    radialSegments = radialSegments or 10
    local verts = {}
    local function push(x, y, z) table.insert(verts, x); table.insert(verts, y); table.insert(verts, z) end
    for i = 0, radialSegments - 1 do
        local a1 = (i / radialSegments) * math.pi * 2
        local a2 = ((i + 1) / radialSegments) * math.pi * 2
        local x1b, z1b = math.cos(a1) * radiusBottom, math.sin(a1) * radiusBottom
        local x2b, z2b = math.cos(a2) * radiusBottom, math.sin(a2) * radiusBottom
        local x1t, z1t = math.cos(a1) * radiusTop,    math.sin(a1) * radiusTop
        local x2t, z2t = math.cos(a2) * radiusTop,    math.sin(a2) * radiusTop
        -- Winding reversed from the first version: the cross-product of the original order
        -- ((bottom-a1, bottom-a2, top-a2)) works out to point radially INWARD, not outward
        -- (checked by hand: cross((bottom-a2)-(bottom-a1), (top-a2)-(bottom-a1)) faces -X at
        -- angle~0, where the true outward normal is +X) -- confirmed visually too, textured
        -- cylinders were rendering their inside surface. Swapping the last two vertices of each
        -- triangle flips every normal to point outward.
        push(x1b, 0, z1b); push(x2t, height, z2t); push(x2b, 0, z2b)
        push(x1b, 0, z1b); push(x1t, height, z1t); push(x2t, height, z2t)
    end
    return verts
end

local function clamp01(v) return math.max(0, math.min(1, v)) end

------------------------------------------------------------------------------------------------------------------
-- Image display + 2D pixel<->world conversion (defined early -- both the marker-dot renderer and
-- the click-picking functions below depend on these).
------------------------------------------------------------------------------------------------------------------

-- Shrinks (never enlarges) so a photo's longer edge fits within MAX_DISPLAY_UNITS -- marker data
-- always stays in true image-pixel units regardless of this, this only affects what's comfortable
-- to click on screen.
local function computeDisplayScale(w, h)
    return math.min(1, MAX_DISPLAY_UNITS / math.max(w, h))
end

-- True image-pixel (px,py, both in [0,w]x[0,h], py=0 at the bottom like world space) -> 2D world
-- position, given the image's own scale/origin.
local function imagePixelsToWorld(which, px, py)
    local img = tImages[which]
    return img.originX + px * img.scale, img.originY + py * img.scale
end

-- Inverse of the above, clamped to the image bounds; returns nil if (wx,wy) falls outside them.
local function worldToImagePixels(which, wx, wy)
    local img = tImages[which]
    if not img.tex then return nil end
    local px = (wx - img.originX) / img.scale
    local py = (wy - img.originY) / img.scale
    if px < 0 or px > img.w or py < 0 or py > img.h then return nil end
    return px, py
end

------------------------------------------------------------------------------------------------------------------
-- Marker/joint data helpers
------------------------------------------------------------------------------------------------------------------

local function findMarkerDef(key)
    for _, d in ipairs(tMarkerDefs) do
        if d.key == key then return d end
    end
    return nil
end

-- Average X (in the side image's own pixel/world space) of Chin and Groin as marked on the side
-- image -- both are centerline points, so in a true profile shot they sit at ~the same depth.
-- Any other side-marked point's Z is this centerline's offset, signed by tImages.side.facing.
local function sideCenterlineX()
    local c1 = tMarkers.chin and tMarkers.chin.side
    local c2 = tMarkers.groin and tMarkers.groin.side
    if c1 and c2 then return (c1.px + c2.px) / 2 end
    if c1 then return c1.px end
    if c2 then return c2.px end
    return nil
end

-- Returns world x,y,z for a marked key, or nil if it has no front data yet. Z defaults to 0
-- (flat, on the front image's own plane) unless a side-image mark exists for this key AND a
-- side centerline could be established.
local function markerWorldPos(key)
    local m = tMarkers[key]
    if not m or not m.front then return nil end
    local z = 0
    if m.side then
        local scx = sideCenterlineX()
        if scx then z = (m.side.px - scx) * tImages.side.facing end
    end
    return m.front.px, m.front.py, z
end

-- Rebuilds tJoints from tMarkers: the 14 marked joints directly, then the 4 synthetic joints
-- (spine1, shoulderCenter, lAnkle, rAnkle) interpolated from their dependencies, in dependency
-- order (shoulderCenter before spine1, which needs it).
local function resolveJoints()
    tJoints = {}

    for _, def in ipairs(tMarkerDefs) do
        local x, y, z = markerWorldPos(def.key)
        if x then
            local m = tMarkers[def.key]
            tJoints[def.key] = { name = def.key, x = x, y = y, z = z, radius = m.radius, depthRatio = m.depthRatio }
        end
    end

    if tJoints.lShoulder and tJoints.rShoulder then
        local a, b = tJoints.lShoulder, tJoints.rShoulder
        tJoints.shoulderCenter = { name = 'shoulderCenter',
            x = (a.x + b.x) / 2, y = (a.y + b.y) / 2, z = (a.z + b.z) / 2,
            radius = (a.radius + b.radius) / 2, depthRatio = (a.depthRatio + b.depthRatio) / 2 }
    end
    if tJoints.groin and tJoints.shoulderCenter then
        local a, b = tJoints.groin, tJoints.shoulderCenter
        tJoints.spine1 = { name = 'spine1',
            x = (a.x + b.x) / 2, y = (a.y + b.y) / 2, z = (a.z + b.z) / 2,
            radius = (a.radius + b.radius) / 2, depthRatio = (a.depthRatio + b.depthRatio) / 2 }
    end
    for _, triple in ipairs({ {'lAnkle', 'lKnee', 'lFootTip'}, {'rAnkle', 'rKnee', 'rFootTip'} }) do
        local ankleKey, kneeKey, footKey = triple[1], triple[2], triple[3]
        if tJoints[kneeKey] and tJoints[footKey] then
            local a, b, t = tJoints[kneeKey], tJoints[footKey], 0.85
            tJoints[ankleKey] = { name = ankleKey,
                x = a.x + (b.x - a.x) * t, y = a.y + (b.y - a.y) * t, z = a.z + (b.z - a.z) * t,
                radius = (a.radius + b.radius) / 2 * 0.8, depthRatio = (a.depthRatio + b.depthRatio) / 2 }
        end
    end

    for name, parent in pairs(PARENT_OF) do
        if tJoints[name] then tJoints[name].parent = parent end
    end
end

------------------------------------------------------------------------------------------------------------------
-- 2D marker dots (visual feedback on the reference image(s) -- these, not the 3D spheres, are
-- what the user actually clicks/drags)
------------------------------------------------------------------------------------------------------------------

local MARKER_DOT_COLOR = {1, 0, 1, 0.9}
local SKELETON_LINE_COLOR = {1, 0, 1, 0.6}

-- 2D skeleton lines (front-view projection only -- side-image depth doesn't have its own 2D
-- panel to draw on) between resolved joints, so the marking tab shows the actual topology instead
-- of disconnected dots (matches the reference-marker UIs this tool is modeled after, e.g. Mixamo's
-- own point picker). Rebuilt alongside the dots every time markers change.
local function rebuildSkeletonLines()
    for _, ln in ipairs(tSkeletonLineHandles) do ln:destroy() end
    tSkeletonLineHandles = {}
    if not tImages.front.tex then return end

    for childKey, parentKey in pairs(PARENT_OF) do
        local a, b = tJoints[parentKey], tJoints[childKey]
        if a and b then
            local ax, ay = imagePixelsToWorld('front', a.x, a.y)
            local bx, by = imagePixelsToWorld('front', b.x, b.y)
            -- line:add()'s points are LOCAL to the object's own position (same convention as
            -- every other render type here) -- creating at (ax,ay) and adding (ax,ay,bx,by) was
            -- double-applying that offset, which is why the skeleton rendered ~2x too large and
            -- shifted. Create at the origin and add the already-absolute coordinates directly.
            local ln = line:new('2dw', 0, 0)
            ln:add({ ax, ay, bx, by })
            ln:setColor(SKELETON_LINE_COLOR[1], SKELETON_LINE_COLOR[2], SKELETON_LINE_COLOR[3], SKELETON_LINE_COLOR[4])
            ln.visible = (sActiveTab == 'mark')
            table.insert(tSkeletonLineHandles, ln)
        end
    end
end

local function rebuildMarkerDots()
    for _, pair in pairs(tMarkerDotHandles) do
        if pair.front then pair.front:destroy() end
        if pair.side then pair.side:destroy() end
    end
    tMarkerDotHandles = {}
    rebuildSkeletonLines()

    for _, def in ipairs(tMarkerDefs) do
        local m = tMarkers[def.key]
        if m then
            local entry = {}
            for _, which in ipairs({ 'front', 'side' }) do
                local p = m[which]
                if p then
                    local wx, wy = imagePixelsToWorld(which, p.px, p.py)
                    local h = shape:new('2dw', wx, wy)
                    -- Fixed base size (20x20), scaled per-instance -- same "shared unit geometry"
                    -- convention as the 3D joint spheres, avoids the create()-by-nickname cache
                    -- pitfall since the raw geometry itself never changes between markers.
                    h:create('circle', 20, 20, 14, nil, 'mannequin_marker_dot_unit')
                    local dotScale = math.max(0.3, (m.radius * tImages[which].scale) / 10)
                    h:setScale(dotScale, dotScale, 1)
                    h:setColor(MARKER_DOT_COLOR[1], MARKER_DOT_COLOR[2], MARKER_DOT_COLOR[3], MARKER_DOT_COLOR[4])
                    h.visible = (sActiveTab == 'mark')
                    entry[which] = h
                end
            end
            tMarkerDotHandles[def.key] = entry
        end
    end
end

------------------------------------------------------------------------------------------------------------------
-- Live 3D preview (semi-transparent spheres at joints + cylinders between them)
------------------------------------------------------------------------------------------------------------------

local function destroyMannequinHandles()
    for _, h in ipairs(tMannequinHandles.spheres) do h:destroy() end
    for _, h in ipairs(tMannequinHandles.bones) do h:destroy() end
    tMannequinHandles.spheres = {}
    tMannequinHandles.bones = {}
end

-- Same magenta/alpha convention as scene_editor3d.lua's tSceneMarkerColor -- "this is an editor
-- marker, not real geometry".
local MARKER_COLOR = {1, 0, 1, 0.55}

-- Frames cam3d around the current joints' bounding box. Only runs until the user first manually
-- orbits/zooms the 3D view (bCameraAutoFramed), so it doesn't fight a camera position they've
-- already set up themselves.
local function autoFrameCamera()
    if bCameraAutoFramed then return end
    local minX, maxX, minY, maxY = nil, nil, nil, nil
    for _, j in pairs(tJoints) do
        minX = (not minX or j.x < minX) and j.x or minX
        maxX = (not maxX or j.x > maxX) and j.x or maxX
        minY = (not minY or j.y < minY) and j.y or minY
        maxY = (not maxY or j.y > maxY) and j.y or maxY
    end
    if not minX then return end
    cam3d.fx = (minX + maxX) / 2
    cam3d.fy = (minY + maxY) / 2
    cam3d.distance = math.max(maxX - minX, maxY - minY, 50) * 2.2
end

function rebuildMannequinPreview()
    resolveJoints()
    destroyMannequinHandles()
    rebuildMarkerDots()
    rebuildMeshPreview()
    autoFrameCamera()

    for _, j in pairs(tJoints) do
        local h = shape:new('3d', j.x, j.y, j.z)
        -- Fixed shared nickname: unit-radius geometry never changes, only :setScale per instance --
        -- safe against the create()-by-nickname cache (unlike the bone cylinders below).
        h:create(unitSphereVerts(), nil, 'mannequin_marker_sphere_unit')
        h:setScale(j.radius, j.radius, j.radius)
        h:setColor(MARKER_COLOR[1], MARKER_COLOR[2], MARKER_COLOR[3], MARKER_COLOR[4])
        h.visible = (sActiveTab == 'preview')
        table.insert(tMannequinHandles.spheres, h)
    end

    for childKey, parentKey in pairs(PARENT_OF) do
        local a, b = tJoints[parentKey], tJoints[childKey]
        if a and b then
            local dx, dy = b.x - a.x, b.y - a.y
            local height = math.sqrt(dx * dx + dy * dy + (b.z - a.z) * (b.z - a.z))
            if height > 0.01 then
                local h = shape:new('3d', a.x, a.y, a.z)
                iMannequinRebuildGen = iMannequinRebuildGen + 1
                -- Vertex data genuinely differs per bone/rebuild (radii, height) -- MUST use a
                -- unique nickname each time or shape:create()'s name-keyed cache silently serves
                -- stale geometry from a previous rebuild (documented pitfall, see docs/lua-api.md
                -- and scene_editor3d.lua's own triangle-marker comment).
                local nick = 'mannequin_bone_' .. childKey .. '_' .. iMannequinRebuildGen
                h:create(unitCylinderVerts(b.radius, a.radius, height, 10), nil, nick)
                -- theta = atan2(-dx, dy) with setAngle(0,0,theta) empirically verified (see
                -- docs/mannequin-editor-plan.md risk log) to point local +Y at world (dx,dy,0).
                -- This is the flat (no side-image, dz=0) case only -- when a side image gives a
                -- non-zero dz, this still orients correctly in the XY plane but does not tilt for
                -- depth (untested 3-axis case, deliberately left as v1 known limitation).
                local theta = math.atan(-dx, dy)
                h:setAngle(0, 0, theta)
                h:setScale(1, 1, (a.depthRatio + b.depthRatio) / 2)
                h:setColor(MARKER_COLOR[1], MARKER_COLOR[2], MARKER_COLOR[3], MARKER_COLOR[4])
                h.visible = (sActiveTab == 'preview')
                table.insert(tMannequinHandles.bones, h)
            end
        end
    end
end

-- Switches between the 4 tabs, toggling visibility of whichever objects belong to each so they
-- never render simultaneously (2D always draws over 3D in this engine, which used to hide the
-- mannequin behind the photo when both were visible at once).
function setActiveTab(tab)
    sActiveTab = tab
    local markVisible = (tab == 'mark')
    local skeletonVisible = (tab == 'preview')
    local meshVisible = (tab == 'mesh' or tab == 'textured')

    if tImages.front.tex then tImages.front.tex.visible = markVisible end
    if tImages.side.tex then tImages.side.tex.visible = markVisible end
    for _, pair in pairs(tMarkerDotHandles) do
        if pair.front then pair.front.visible = markVisible end
        if pair.side then pair.side.visible = markVisible end
    end
    for _, ln in ipairs(tSkeletonLineHandles) do ln.visible = markVisible end
    for _, h in ipairs(tMannequinHandles.spheres) do h.visible = skeletonVisible end
    for _, h in ipairs(tMannequinHandles.bones) do h.visible = skeletonVisible end

    if tMeshPreviewHandle then
        tMeshPreviewHandle.visible = meshVisible
        if tab == 'textured' and tImages.front.path then
            tMeshPreviewHandle:setColor(tImages.front.path)
        elseif tab == 'mesh' then
            tMeshPreviewHandle:setColor(MESH_PREVIEW_COLOR[1], MESH_PREVIEW_COLOR[2], MESH_PREVIEW_COLOR[3], MESH_PREVIEW_COLOR[4])
        end
    end
end

------------------------------------------------------------------------------------------------------------------
-- 2D click picking
------------------------------------------------------------------------------------------------------------------

local function findMarkerDotUnderWorldPoint(wx, wy)
    local bestKey, bestDist = nil, nil
    for _, def in ipairs(tMarkerDefs) do
        local m = tMarkers[def.key]
        if m then
            for _, which in ipairs({ 'front', 'side' }) do
                local p = m[which]
                if p then
                    local dwx, dwy = imagePixelsToWorld(which, p.px, p.py)
                    local dist = math.sqrt((dwx - wx) ^ 2 + (dwy - wy) ^ 2)
                    local hitRadius = math.max(m.radius * tImages[which].scale, 10)
                    if dist <= hitRadius and (not bestDist or dist < bestDist) then
                        bestKey, bestDist = def.key, dist
                    end
                end
            end
        end
    end
    return bestKey
end

-- bLiveDrag: while true, skips the full rebuild (which destroys/recreates every 2D dot, every 2D
-- skeleton line, every 3D primitive AND the ~1000-vertex mesh preview -- doing that on literally
-- every mousemove event during a drag was the cause of the reported stutter/lag) and instead just
-- repositions the dragged dot itself. The full rebuild still runs once, on release (onTouchUp).
function tryPlaceMarkerAt(wx, wy, key, bLiveDrag)
    if not key then return false end

    local source, px, py = nil, nil, nil
    local fpx, fpy = worldToImagePixels('front', wx, wy)
    if fpx then source, px, py = 'front', fpx, fpy end
    if not source then
        local spx, spy = worldToImagePixels('side', wx, wy)
        if spx then source, px, py = 'side', spx, spy end
    end
    if not source then return false end

    local m = tMarkers[key]
    if not m then
        local def = findMarkerDef(key)
        m = { radius = def.defaultRadius, depthRatio = def.defaultDepthRatio }
        tMarkers[key] = m
    end
    m[source] = { px = px, py = py }

    if bLiveDrag then
        local pair = tMarkerDotHandles[key]
        if pair and pair[source] then
            pair[source]:setPos(wx, wy)
        end
    else
        rebuildMannequinPreview()
    end
    return true
end

function advanceArmedMarker()
    for _, def in ipairs(tMarkerDefs) do
        local m = tMarkers[def.key]
        if not m or not m.front then
            sArmedMarker = def.key
            return
        end
    end
    sArmedMarker = nil
end

------------------------------------------------------------------------------------------------------------------
-- Input: on the "mark" tab, left-click marks/drags a 2D marker dot on the reference image(s). On
-- the "preview" tab, left-drag orbits the 3D mannequin camera -- the standard editor convention
-- (scene_editor3d.lua also uses left-orbit), safe to use here too since marking and orbiting now
-- live on separate tabs and never compete for the same click. WASD pans, wheel dollies.
------------------------------------------------------------------------------------------------------------------

function onTouchDown(key, x, y)
    if tImGui.GetWantCaptureMouse() then return end
    mouseLastX, mouseLastY = x, y
    if key ~= 0 then return end

    if sActiveTab ~= 'mark' then
        isOrbiting = true
        return
    end

    local wx, wy = mbm.to2dw(x, y)
    local hitKey = findMarkerDotUnderWorldPoint(wx, wy)
    if hitKey then
        tDragKey = hitKey
        return
    end
    if sArmedMarker then
        if tryPlaceMarkerAt(wx, wy, sArmedMarker) then
            advanceArmedMarker()
        end
    end
end

function onTouchMove(key, x, y)
    if tImGui.GetWantCaptureMouse() then
        mouseLastX, mouseLastY = x, y
        return
    end
    if sActiveTab ~= 'mark' and isOrbiting then
        bCameraAutoFramed = true
        cam3d.azimuth   = cam3d.azimuth   - (x - mouseLastX) * 0.005
        cam3d.elevation = cam3d.elevation + (y - mouseLastY) * 0.005
        cam3d.elevation = math.max(-math.pi * 0.49, math.min(math.pi * 0.49, cam3d.elevation))
    elseif sActiveTab == 'mark' and tDragKey then
        local wx, wy = mbm.to2dw(x, y)
        tryPlaceMarkerAt(wx, wy, tDragKey, true)
    end
    mouseLastX, mouseLastY = x, y
end

function onTouchUp(key, x, y)
    if key == 0 then
        isOrbiting = false
        if tDragKey then
            -- Drag finished -- catch up the skeleton lines, 3D primitives and mesh preview with
            -- one full rebuild now (skipped on every intermediate move for performance).
            rebuildMannequinPreview()
        end
        tDragKey = nil
    end
end

function onTouchZoom(zoom)
    if tImGui.GetWantCaptureMouse() then return end
    if sActiveTab == 'mark' then return end
    bCameraAutoFramed = true
    local oldDistance = cam3d.distance
    local newDistance = math.max(10, oldDistance * (1.0 - zoom * 0.15))
    local deltaDistance = oldDistance - newDistance

    local caz, saz = math.cos(cam3d.azimuth), math.sin(cam3d.azimuth)
    local cel, sel = math.cos(cam3d.elevation), math.sin(cam3d.elevation)
    local dirX, dirY, dirZ = cel * saz, sel, cel * caz

    local x1, y1, z1 = mbm.to3d(mouseLastX, mouseLastY, 100)
    local x2, y2, z2 = mbm.to3d(mouseLastX, mouseLastY, 1000)
    local rx, ry, rz = x2 - x1, y2 - y1, z2 - z1
    local rlen = math.sqrt(rx * rx + ry * ry + rz * rz)
    if rlen > 1e-6 then
        rx, ry, rz = rx / rlen, ry / rlen, rz / rlen
        cam3d.fx = cam3d.fx + deltaDistance * (dirX + rx)
        cam3d.fy = cam3d.fy + deltaDistance * (dirY + ry)
        cam3d.fz = cam3d.fz + deltaDistance * (dirZ + rz)
    end
    cam3d.distance = newDistance
end

function onKeyDown(key)
    if sActiveTab == 'mark' then return end
    if key == mbm.getKeyCode('W') or key == mbm.getKeyCode('up') then
        tCam3dMove.forward = 1
    elseif key == mbm.getKeyCode('S') or key == mbm.getKeyCode('down') then
        tCam3dMove.forward = -1
    elseif key == mbm.getKeyCode('A') or key == mbm.getKeyCode('left') then
        tCam3dMove.right = -1
    elseif key == mbm.getKeyCode('D') or key == mbm.getKeyCode('right') then
        tCam3dMove.right = 1
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('W') or key == mbm.getKeyCode('up')
        or key == mbm.getKeyCode('S') or key == mbm.getKeyCode('down') then
        tCam3dMove.forward = 0
    elseif key == mbm.getKeyCode('A') or key == mbm.getKeyCode('left')
        or key == mbm.getKeyCode('D') or key == mbm.getKeyCode('right') then
        tCam3dMove.right = 0
    end
end

------------------------------------------------------------------------------------------------------------------
-- Image loading
------------------------------------------------------------------------------------------------------------------

function loadImage(which)
    local f = mbm.openFile(sLastImagePath, 'png', 'jpg', 'jpeg', 'bmp', 'tga')
    if type(f) == 'table' then f = f[1] end
    if not f then return end
    sLastImagePath = f

    local tex = texture:new('2dw', 0, 0)
    if not tex:load(f) then
        tUtil.showMessage('Falha ao carregar imagem: ' .. f)
        return
    end
    -- Empirically confirmed reliable for a plain `texture` object (unlike `mesh`, which is
    -- physics-driven per scene_editor3d.lua's own getAABB note) -- see docs/mannequin-editor-plan.md.
    local w, h = tex:getAABB(true)
    if not w or w <= 0 then
        tUtil.showMessage('Nao foi possivel obter o tamanho da imagem.')
        tex:destroy()
        return
    end
    local scale = computeDisplayScale(w, h)

    if which == 'front' then
        if tImages.front.tex then tImages.front.tex:destroy() end
        local originX, originY = 0, 0
        tex:setScale(scale, scale, 1)
        tex:setPos(originX + w * scale * 0.5, originY + h * scale * 0.5)
        tImages.front = { path = f, tex = tex, w = w, h = h, scale = scale, originX = originX, originY = originY }
        tex.visible = (sActiveTab == 'mark')
        -- The 2D camera does not default to framing whatever was just loaded -- center it on the
        -- image so it's actually visible without the user having to pan first.
        camera2d:setPos(originX + w * scale * 0.5, originY + h * scale * 0.5)

        -- Auto-populate a full default T-pose skeleton (DEFAULT_TEMPLATE) so marking becomes
        -- "drag each point onto the right spot" instead of "click 14 points from scratch" --
        -- only if nothing is marked yet, so reloading a corrected photo doesn't clobber work.
        local hasAnyMarker = next(tMarkers) ~= nil
        if not hasAnyMarker then
            for _, def in ipairs(tMarkerDefs) do
                local frac = DEFAULT_TEMPLATE[def.key]
                tMarkers[def.key] = {
                    front = { px = frac[1] * w, py = frac[2] * h },
                    radius = def.defaultRadius,
                    depthRatio = def.defaultDepthRatio,
                }
            end
        end
    else
        if tImages.side.tex then tImages.side.tex:destroy() end
        -- Placed to the right of the front image (in DISPLAY units) with a fixed gap, so the two
        -- never overlap in world space regardless of either image's resolution.
        local originX = tImages.front.originX + tImages.front.w * tImages.front.scale + 60
        local originY = 0
        tex:setScale(scale, scale, 1)
        tex:setPos(originX + w * scale * 0.5, originY + h * scale * 0.5)
        tImages.side.path, tImages.side.tex, tImages.side.w, tImages.side.h = f, tex, w, h
        tImages.side.scale, tImages.side.originX, tImages.side.originY = scale, originX, originY
        tex.visible = (sActiveTab == 'mark')

        -- Side markers had no visible starting point at all (loading the side image alone did
        -- nothing) -- auto-place them on the vertical centerline (a genuine unknown -- depth
        -- can't be guessed -- so centered is the least-wrong default), at the SAME proportional
        -- height as each marker's current front position (not the static template fraction, so
        -- this respects any front adjustments already made) for at least a visible, draggable
        -- starting point.
        for _, def in ipairs(tMarkerDefs) do
            local m = tMarkers[def.key]
            if m and m.front and not m.side then
                m.side = { px = w * 0.5, py = (m.front.py / tImages.front.h) * h }
            end
        end
    end
    rebuildMannequinPreview()
end

------------------------------------------------------------------------------------------------------------------
-- JSON handoff export (consumed by Marco 3's Blender script)
------------------------------------------------------------------------------------------------------------------

local function jsonStr(s)
    s = tostring(s or '')
    s = s:gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\n', '\\n'):gsub('\r', '\\r'):gsub('\t', '\\t')
    return '"' .. s .. '"'
end

-- Builds the exported mesh independently of the live preview's `shape` handles (which the engine
-- transforms internally) -- computes the same world-space vertices by hand, using the identical
-- rotation formula verified for rebuildMannequinPreview's setAngle(0,0,theta) call, so both stay
-- geometrically consistent. Only bone cylinders are exported (joint spheres are a preview aid,
-- not part of the mesh Blender will build).
local function buildExportMesh()
    local tVerts, tSubsets = {}, {}
    for childKey, parentKey in pairs(PARENT_OF) do
        local a, b = tJoints[parentKey], tJoints[childKey]
        if a and b then
            local dx, dy = b.x - a.x, b.y - a.y
            local height = math.sqrt(dx * dx + dy * dy + (b.z - a.z) * (b.z - a.z))
            if height > 0.01 then
                local theta = math.atan(-dx, dy)
                local depth = (a.depthRatio + b.depthRatio) / 2
                local localVerts = unitCylinderVerts(b.radius, a.radius, height, 10)
                local baseIndex = #tVerts
                local nVerts = #localVerts / 3
                local indices = {}
                for i = 1, nVerts do
                    local lx, ly, lz = localVerts[i*3-2], localVerts[i*3-1], localVerts[i*3]
                    local wx = lx * math.cos(theta) - ly * math.sin(theta) + a.x
                    local wy = lx * math.sin(theta) + ly * math.cos(theta) + a.y
                    local wz = lz * depth + a.z

                    local axisZ = a.z + (ly / height) * (b.z - a.z)
                    local owner = (ly > height * 0.5) and childKey or parentKey

                    local u, v, uvSource
                    if wz >= axisZ or not tImages.side.tex then
                        uvSource = 'front'
                        u = clamp01(wx / math.max(1, tImages.front.w))
                        v = clamp01(1 - wy / math.max(1, tImages.front.h))
                    else
                        uvSource = 'side'
                        local scx = sideCenterlineX() or 0
                        local sidePixelX = scx + wz / tImages.side.facing
                        u = clamp01(sidePixelX / math.max(1, tImages.side.w))
                        v = clamp01(1 - wy / math.max(1, tImages.side.h))
                    end

                    table.insert(tVerts, { x = wx, y = wy, z = wz, u = u, v = v, uvSource = uvSource, owner = owner })
                    table.insert(indices, baseIndex + i)
                end
                table.insert(tSubsets, { name = 'bone_' .. parentKey .. '_' .. childKey, owner = childKey, indices = indices })
            end
        end
    end
    return tVerts, tSubsets
end

-- Builds the "Mesh 3D"/"Mesh Texturizada" tabs' preview: the same continuous geometry+UVs that
-- exportJson() writes out, as ONE shape (not the many small primitive spheres/cylinders of the
-- "Manequim 3D" tab) -- a closer look at what the actual exported mesh will look like, in mini-mbm
-- before ever touching Blender.
function rebuildMeshPreview()
    if tMeshPreviewHandle then
        tMeshPreviewHandle:destroy()
        tMeshPreviewHandle = nil
    end
    local tVerts = buildExportMesh()
    if #tVerts == 0 then return end

    local vertsFlat, uvsFlat = {}, {}
    for _, v in ipairs(tVerts) do
        table.insert(vertsFlat, v.x); table.insert(vertsFlat, v.y); table.insert(vertsFlat, v.z)
        table.insert(uvsFlat, v.u); table.insert(uvsFlat, v.v)
    end

    -- Vertex data changes every rebuild -- unique nickname required (same shape:create() cache
    -- pitfall as the bone cylinders in rebuildMannequinPreview).
    iMeshPreviewRebuildGen = iMeshPreviewRebuildGen + 1
    local h = shape:new('3d', 0, 0, 0)
    h:create(vertsFlat, uvsFlat, 'mannequin_mesh_preview_' .. iMeshPreviewRebuildGen)
    if sActiveTab == 'textured' and tImages.front.path then
        h:setColor(tImages.front.path)
    else
        h:setColor(MESH_PREVIEW_COLOR[1], MESH_PREVIEW_COLOR[2], MESH_PREVIEW_COLOR[3], MESH_PREVIEW_COLOR[4])
    end
    h.visible = (sActiveTab == 'mesh' or sActiveTab == 'textured')
    tMeshPreviewHandle = h
end

sLastExportPath = 'mannequin.json'

-- Writes the current mannequin (joints + exported mesh) to `path` as JSON. Shared by the
-- user-facing "Exportar JSON..." button and the internal Marco 3 Blender-build flow (which writes
-- to its own temp path, no dialog). Returns ok, names, tVerts, tSubsets (counts for the status
-- message) or ok=false, errMsg.
function writeMannequinJson(path)
    resolveJoints()

    local names = {}
    for name in pairs(tJoints) do table.insert(names, name) end
    if #names == 0 then
        return false, 'Marque ao menos um ponto antes de exportar.'
    end
    table.sort(names)

    local f = io.open(path, 'w')
    if not f then
        return false, 'Falha ao criar arquivo: ' .. path
    end

    f:write('{\n  "schemaVersion": 1,\n  "sourceImages": {\n')
    f:write(string.format('    "front": { "path": %s, "width": %d, "height": %d }',
        jsonStr(tImages.front.path), tImages.front.w, tImages.front.h))
    if tImages.side.tex then
        f:write(',\n')
        f:write(string.format('    "side": { "path": %s, "width": %d, "height": %d, "facing": %d }',
            jsonStr(tImages.side.path), tImages.side.w, tImages.side.h, tImages.side.facing))
    end
    f:write('\n  },\n  "joints": [\n')
    for i, name in ipairs(names) do
        local j = tJoints[name]
        f:write(string.format('    { "name": %s, "parent": %s, "x": %.3f, "y": %.3f, "z": %.3f, "radius": %.3f }',
            jsonStr(j.name), j.parent and jsonStr(j.parent) or 'null', j.x, j.y, j.z, j.radius))
        f:write(i < #names and ',\n' or '\n')
    end
    f:write('  ],\n  "mesh": {\n    "vertices": [\n')

    local tVerts, tSubsets = buildExportMesh()
    for i, v in ipairs(tVerts) do
        f:write(string.format('      { "x": %.3f, "y": %.3f, "z": %.3f, "u": %.4f, "v": %.4f, "uvSource": %s, "owner": %s }',
            v.x, v.y, v.z, v.u, v.v, jsonStr(v.uvSource), jsonStr(v.owner)))
        f:write(i < #tVerts and ',\n' or '\n')
    end
    f:write('    ],\n    "subsets": [\n')
    for i, s in ipairs(tSubsets) do
        f:write(string.format('      { "name": %s, "owner": %s, "indices": [%s] }',
            jsonStr(s.name), jsonStr(s.owner), table.concat(s.indices, ', ')))
        f:write(i < #tSubsets and ',\n' or '\n')
    end
    f:write('    ]\n  }\n}\n')
    f:close()

    return true, names, tVerts, tSubsets
end

function exportJson()
    -- mbm.saveFile(defaultNameOrPath, extensionFilter) -- first arg is the SUGGESTED FILENAME
    -- shown in the dialog (not a title), second is a bare extension or glob (see font_maker.lua/
    -- particle_editor.lua call sites) -- passing a "title" string here previously produced a
    -- literal file named "<title>.<filter>".
    local path = mbm.saveFile(sLastExportPath, 'json')
    if not path then return end

    local ok, a, b, c = writeMannequinJson(path)
    if not ok then
        tUtil.showMessage(a)
        return
    end
    sLastExportPath = path
    sStatusMessage = string.format('Exportado: %d joints, %d vertices, %d subsets -> %s', #a, #b, #c, path)
    tUtil.showMessage(sStatusMessage)
end

------------------------------------------------------------------------------------------------------------------
-- Marco 3: build a real FBX via headless Blender (editor/blender_mannequin_build.py)
------------------------------------------------------------------------------------------------------------------

-- Self-locating, same idiom as mesh_debug.lua's getEditorDir() -- works regardless of which
-- script required/dofile'd this one, since it reads THIS file's own chunk source path.
local function getEditorDir()
    local src = debug.getinfo(1, 'S').source or ''
    if src:sub(1, 1) == '@' then
        local path = src:sub(2)
        local dir = path:match('^(.*)[/\\]')
        return dir or '.'
    end
    return '.'
end

local function getOSTempDir()
    return os.getenv('TMPDIR') or os.getenv('TEMP') or os.getenv('TMP') or '/tmp'
end

local function blenderBuildCoroutine(jsonPath, outputFbx, dbgLog, cancelFile)
    local st = tBlenderBuildState
    local exporterPath = getEditorDir() .. '/blender_mannequin_build.py'
    local cmd = tBlender.buildMannequinCmd(jsonPath, outputFbx, exporterPath,
        { cancelFile = cancelFile, debugSteps = st.bDebugSteps })
    if not cmd then
        st.sStatus = 'Blender nao encontrado. Verifique se esta instalado e no PATH.'
        st.bStatusOk = false
        st.bBuilding = false
        return
    end

    os.remove(outputFbx)
    os.remove(dbgLog)
    tBlender.launchCmdAsync(cmd, dbgLog)

    local startTime = os.time()
    local lastActivityTime = startTime
    local lastLogSize = -1
    while true do
        if st.bAbortRequested then
            if cancelFile ~= '' then
                local f = io.open(cancelFile, 'w')
                if f then f:write('cancel\n'); f:close() end
            end
            st.sStatus = 'Cancelado.'
            st.bStatusOk = false
            break
        end

        if tBlender.fileExists(outputFbx) then
            st.sStatus = 'FBX gerado: ' .. outputFbx
            st.bStatusOk = true
            break
        end

        local logSize = 0
        local lf = io.open(dbgLog, 'rb')
        if lf then
            logSize = lf:seek('end')
            lf:close()
        end
        if logSize ~= lastLogSize then
            lastLogSize = logSize
            lastActivityTime = os.time()
        end

        if os.time() - lastActivityTime >= st.iTimeoutSecs then
            local errLog = io.open(dbgLog, 'r')
            local tail = ''
            if errLog then tail = errLog:read('*a') or ''; errLog:close() end
            st.sStatus = 'Tempo esgotado esperando o Blender.\n' .. tail:sub(-800)
            st.bStatusOk = false
            break
        end

        coroutine.yield()
    end
    st.bBuilding = false
end

function startBlenderBuild()
    local st = tBlenderBuildState
    if st.bBuilding then return end

    local outputFbx = mbm.saveFile(sLastFbxExportPath, 'fbx')
    if not outputFbx then return end
    sLastFbxExportPath = outputFbx

    local tmpDir = getOSTempDir()
    local jsonPath = tmpDir .. '/mannequin_build_input.json'
    local ok, err = writeMannequinJson(jsonPath)
    if not ok then
        tUtil.showMessage(err)
        return
    end

    st.bOpen = true
    st.bOpenPopup = true
    st.bBuilding = true
    st.bAbortRequested = false
    st.sStatus = ''
    st.bStatusOk = true
    st.sOutputFbx = outputFbx

    local dbgLog = tmpDir .. '/mannequin_build_debug.log'
    local cancelFile = tmpDir .. '/mannequin_build_cancel'
    os.remove(cancelFile)
    st.sCancelFile = cancelFile
    st.co = coroutine.create(function() blenderBuildCoroutine(jsonPath, outputFbx, dbgLog, cancelFile) end)
end

function showBlenderBuildDialog()
    local st = tBlenderBuildState
    if not st.bOpen then return end

    if st.bOpenPopup then
        tImGui.OpenPopup('mannequin_blender_build_modal')
        st.bOpenPopup = false
    end

    local isOpen = tImGui.BeginPopupModal('Exportar via Blender###mannequin_blender_build_modal', false, 0)
    if not isOpen then return end

    if st.bBuilding then
        if st.co and coroutine.status(st.co) == 'suspended' then
            local ok, err = coroutine.resume(st.co)
            if not ok then
                st.bBuilding = false
                st.sStatus = tostring(err)
                st.bStatusOk = false
            end
        end
        tImGui.Text('Construindo armature + mesh no Blender...')
        if tImGui.Button('Cancelar') then
            st.bAbortRequested = true
        end
    else
        tImGui.TextWrapped(st.sStatus)
        if st.bStatusOk then
            sStatusMessage = st.sStatus
        end
        if tImGui.Button('Fechar') then
            st.bOpen = false
            tImGui.CloseCurrentPopup()
        end
    end

    tImGui.EndPopup()
end

------------------------------------------------------------------------------------------------------------------
-- GUI
------------------------------------------------------------------------------------------------------------------

function drawMarkTab()
    tImGui.Text('Imagens')
    if tImGui.Button('Carregar Imagem Frontal...') then loadImage('front') end
    tImGui.Text(tImages.front.path and tUtil.getShortName(tImages.front.path) or '(nenhuma)')

    if tImGui.Button('Carregar Imagem Lateral (opcional)...') then loadImage('side') end
    tImGui.Text(tImages.side.path and tUtil.getShortName(tImages.side.path) or '(nenhuma)')

    if tImages.side.tex then
        tImGui.Text('Lado da imagem lateral:')
        if tImGui.Button('Direita##sideFacingR') then tImages.side.facing = 1; rebuildMannequinPreview() end
        tImGui.SameLine()
        if tImGui.Button('Esquerda##sideFacingL') then tImages.side.facing = -1; rebuildMannequinPreview() end
        tImGui.SameLine()
        tImGui.Text(tImages.side.facing == 1 and '(Direita)' or '(Esquerda)')
    end

    tImGui.Separator()
    tImGui.TextWrapped('Um manequim padrao ja aparece assim que voce carrega a imagem frontal -- '
        .. 'arraste cada ponto magenta ate a posicao certa. Clique-esquerdo arrasta; nao precisa '
        .. 'clicar em "Marcar" a nao ser que tenha removido um ponto.')
    for _, def in ipairs(tMarkerDefs) do
        local m = tMarkers[def.key]
        local status = '-'
        if m and m.front and m.side then status = 'F+S'
        elseif m and m.front then status = 'F' end

        local prefix = (sArmedMarker == def.key) and '> ' or ''
        if tImGui.Button(prefix .. def.label .. ' [' .. status .. ']##arm_' .. def.key) then
            sArmedMarker = def.key
        end
        if m then
            -- Read-only, refreshed every frame from the actual data -- lets you see a drag
            -- register immediately instead of only seeing the dot move on the image.
            if m.front then
                tImGui.Text(string.format('  Posicao (frontal): x=%.1f y=%.1f', m.front.px, m.front.py))
            end
            if m.side then
                tImGui.Text(string.format('  Posicao (lateral): x=%.1f y=%.1f', m.side.px, m.side.py))
            end
            tUtil.pushResponsiveItemWidth(120)
            local rc, rv = tImGui.DragFloat('Raio##radius_' .. def.key, m.radius, 0.5, 1, 300)
            if rc then m.radius = rv; rebuildMannequinPreview() end
            tUtil.pushResponsiveItemWidth(120)
            local dc, dv = tImGui.DragFloat('Profundidade##depth_' .. def.key, m.depthRatio, 0.02, 0.1, 3.0)
            if dc then m.depthRatio = dv; rebuildMannequinPreview() end
            if tImGui.Button('Remover##rm_' .. def.key) then
                tMarkers[def.key] = nil
                if sArmedMarker == def.key then sArmedMarker = nil end
                rebuildMannequinPreview()
            end
        end
        tImGui.Separator()
    end
end

-- Shared by the 3 camera-driven tabs (preview/mesh/textured) -- they only differ in which
-- geometry setActiveTab() makes visible, not in what controls are drawn here.
function drawCameraPreviewTab(sHint)
    tImGui.Text('Camera 3D: clique-esquerdo arrasta = orbita, WASD = mover, roda = zoom')
    tUtil.drawOrbitGizmo(cam3d, { size = 90 })
    if sHint then
        tImGui.Separator()
        tImGui.TextWrapped(sHint)
    end
    tImGui.Separator()

    if tImGui.Button('Exportar JSON...') then exportJson() end
    tImGui.SameLine()
    if tImGui.Button('Exportar via Blender (FBX)...') then startBlenderBuild() end
    if sStatusMessage ~= '' then
        tImGui.TextWrapped(sStatusMessage)
    end
end

function drawMainPanel()
    tUtil.setInitialWindowPositionLeft('Mannequin Editor', 0, 0, 300, 340, 900)
    tImGui.Begin('Mannequin Editor', true, 0)

    if tImGui.BeginTabBar('##MannequinTabBar', 0) then
        if tImGui.BeginTabItem('Marcacao (2D)') then
            setActiveTab('mark')
            drawMarkTab()
            tImGui.EndTabItem()
        end
        if tImGui.BeginTabItem('Manequim 3D') then
            setActiveTab('preview')
            drawCameraPreviewTab('Esqueleto de primitivas (esferas+cilindros) -- rascunho rapido, nao e a mesh final.')
            tImGui.EndTabItem()
        end
        if tImGui.BeginTabItem('Mesh 3D') then
            setActiveTab('mesh')
            drawCameraPreviewTab('Geometria continua real (a mesma que sera exportada no JSON), sem textura.')
            tImGui.EndTabItem()
        end
        if tImGui.BeginTabItem('Mesh Texturizada') then
            setActiveTab('textured')
            drawCameraPreviewTab('Mesma mesh, com a foto frontal aplicada via UV.')
            tImGui.EndTabItem()
        end
        tImGui.EndTabBar()
    end

    tImGui.End()
end

------------------------------------------------------------------------------------------------------------------
-- Engine callbacks
------------------------------------------------------------------------------------------------------------------

function onInitScene()
    camera3d = mbm.getCamera('3d')
    camera3d:setFar(20000)
    camera2d = mbm.getCamera('2d')
    mbm.setLightEnabled('3d', true)
    mbm.setAmbientLight('3d', 0.35, 0.35, 0.4)
    mbm.setDirectionalLight('3d', 0, -1, 0.3, 1.0, 0.98, 0.9)
    applyCam3d(cam3d)
end

function onLoop(delta)
    drawMainPanel()
    showBlenderBuildDialog()
    updateCam3dKeyboardMovement(delta)
    applyCam3d(cam3d)
end
