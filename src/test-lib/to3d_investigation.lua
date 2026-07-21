--[[
mbm.to3d / mbm.getPickRay / obj:collide investigation scene.

Context: docs/future_investigation.md ("RESOLVED" entry) documents two real bugs this scene found
and that got fixed as a direct result (MBM_VERSION 6.31.9): (1) platform-linux/main-lua.cpp and
platform-macos/main-lua.cpp hardcoded expectedwidth/height to 1920x1080 whenever -ew/-eh weren't
passed, silently leaving camera.scaleScreen2d wrong for the whole session on any --disable_select_
monitor launch at another resolution; (2) mbm.getPickRay and obj:collide's 3D branch skipped the
scaleScreen2d correction mbm.to3d applies, a real (now fixed) consistency gap in DEVICE::rayCast.
This scene stays in the repo as a live regression check for both, and as an interactive tool for
re-running the same kind of check by eye on a real desktop. See docs/lua-api.md section 2 for the
full depth-vs-distance semantics of mbm.to3d.

Run (from repo root, real desktop with a mouse):
    ./bin/debug/linux_x86/mini-mbm --scene src/test-lib/to3d_investigation.lua \
        --disable_select_monitor --nosplash -w 1024 -h 768

Interactive controls:
    Mouse move   - a red marker cube follows mbm.to3d(mouseX, mouseY, DEPTH) for the currently
                   selected DEPTH. Compare it by eye against the reference mesh sitting at that
                   exact distance (see keys 1/2/3) -- at screen center it should land exactly on
                   top of the mesh; moving toward the edges will visibly reveal how much "depth"
                   diverges from literal distance off-axis (see docs/lua-api.md section 2).
    1 / 2 / 3    - select DEPTH = 300 / 600 / 900, matching Crate.msh / base.msh / building_A.msh.
    UP / DOWN    - grow/shrink the frustum-slice depth (default 600). The 4 cyan/yellow markers
                   sit at mbm.to3d() of the screen's left/right/top/bottom edge at that depth --
                   i.e. they trace the actual frustum cross-section at that distance. This is the
                   "how deep/wide is the frustum" check.
    Left click   - runs obj:collide(x, y) (the real ray/AABB path, since MBM_VERSION 6.8.0)
                   against all three reference meshes and logs which one was hit. Compare this
                   against where the red marker visually is -- they should always agree now
                   (both go through the same scaleScreen2d-corrected DEVICE::rayCast); if they
                   ever disagree again that's a real regression of the 6.31.9 fix.
    ESC          - quit.

A headless numeric self-test also runs once at startup (no mouse needed, no window resize
required) -- see runSelfTest() and runRayConsistencyCheck() below -- and prints
TO3D_SELFTEST PASS/FAIL a few frames in, once the camera matrices from onInitScene's cam:setPos/
setFocus have actually been rebuilt by a rendered frame (see the frameCount gate below). It never
auto-quits on its own (this scene is meant to stay open for interactive use) -- for scripted/
headless verification just wrap the launch in `timeout -s KILL <n>` and grep the log for the
sentinel, per the engine-testing skill's Path B pattern. (Note: the bare `name=value` ->
mbm.getGlobal() CLI mechanism documented in that skill only applies to the LUA_MANAGER(argc,argv)
entry point, not to this binary's PARSE_launcher_ARGS + --scene path -- confirmed empirically,
worth fixing in the skill doc separately.)
]]--

local script_dir = ...
if script_dir then
    mbm.addPath(script_dir)
end

-- Flush every print() immediately -- stdout is fully buffered once redirected to a file/pipe,
-- so without this a killed/headless run can look like a silent hang with no log at all.
local rawPrint = print
print = function(...)
    rawPrint(...)
    io.stdout:flush()
end

-- ---------------------------------------------------------------------------
-- Colored cube marker helper -- same "default cube" pattern makeBoxShape3d()
-- uses in editor/physic_editor.lua (shape:create() from a flat vertex list,
-- no indices needed since the triangle list is already expanded).
-- ---------------------------------------------------------------------------
local function boxCorners(hw, hh, hd)
    return {
        {x=-hw,y=-hh,z= hd}, {x=-hw,y= hh,z= hd}, {x= hw,y= hh,z= hd}, {x= hw,y=-hh,z= hd},
        {x=-hw,y=-hh,z=-hd}, {x=-hw,y= hh,z=-hd}, {x= hw,y= hh,z=-hd}, {x= hw,y=-hh,z=-hd},
    }
end

local function boxTriangleFaces(c)
    local a,b,cc,d,e,f,g,h = c[1],c[2],c[3],c[4],c[5],c[6],c[7],c[8]
    return {
        {a,b,cc},{a,cc,d}, -- front
        {h,g,f},{h,f,e},   -- back
        {e,f,b},{e,b,a},   -- left
        {d,cc,g},{d,g,h},  -- right
        {b,f,g},{b,g,cc},  -- top
        {e,a,d},{e,d,h},   -- bottom
    }
end

local cubeCount = 0
local function makeCube(cx, cy, cz, size, r, g, b, a)
    cubeCount = cubeCount + 1
    local hw = size * 0.5
    local corners = boxCorners(hw, hw, hw)
    local faces = boxTriangleFaces(corners)
    local verts = {}
    for _, tri in ipairs(faces) do
        for _, p in ipairs(tri) do
            table.insert(verts, p.x); table.insert(verts, p.y); table.insert(verts, p.z)
        end
    end
    local sh = shape:new('3d', cx, cy, cz)
    sh:create(verts, nil, 'to3d_investigation_cube_' .. cubeCount)
    sh:setColor(r or 1, g or 1, b or 1, a or 1)
    return sh
end

-- Scale a loaded mesh so its largest AABB dimension is targetSize -- keeps the scene readable
-- regardless of each fixture mesh's native scale (base.msh / building_A.msh / Crate.msh are not
-- modeled at the same scale).
local function fitScale(obj, targetSize)
    local w, h, d = obj:getAABB(true)
    local maxDim = math.max(w or 0, h or 0, d or 0)
    if maxDim < 1e-4 then maxDim = 1 end
    local s = targetSize / maxDim
    obj:setScale(s, s, s)
end

-- ---------------------------------------------------------------------------
-- State
-- ---------------------------------------------------------------------------
local refs = {}             -- {obj, name, dist} -- on-axis (x=0,y=0), camera looks down +Z from origin
local marker                -- red, follows the mouse at the selected depth
local edgeMarkers = {}       -- cyan/yellow, trace the frustum cross-section at frustumDepth
local hudFont                -- MUST be kept referenced for the scene's lifetime -- font's Lua
                              -- __gc (onDestroyFontLua, font-lua.cpp) deletes the FONT_DRAW AND
                              -- every TEXT_DRAW child it created via :add() the moment nothing in
                              -- Lua references the font userdata anymore. A `local fnt` scoped
                              -- only inside onInitScene() gets GC'd out from under hudLines[i]
                              -- (still-referenced children!) some frames later, turning the next
                              -- hudLines[i].text = ... into a dangling-pointer write -- confirmed
                              -- with gdb (SIGSEGV in TEXT_DRAW::setText -> renderText ->
                              -- ANIMATION_MANAGER::getIndexAnimation on a freed `this`). Keep the
                              -- font itself alive anywhere a text object it created is still used.
local hudLines = {}
local lastMouseX, lastMouseY = nil, nil
local depthIndex = 1
local frustumDepth = 600
local screenW, screenH = 0, 0
local KEY_1, KEY_2, KEY_3, KEY_UP, KEY_DOWN, KEY_ESC = 0,0,0,0,0,0

local function updateFrustumEdgeMarkers()
    local cx, cy = screenW * 0.5, screenH * 0.5
    local xMin = {mbm.to3d(0, cy, frustumDepth)}
    local xMax = {mbm.to3d(screenW, cy, frustumDepth)}
    local yMin = {mbm.to3d(cx, 0, frustumDepth)}
    local yMax = {mbm.to3d(cx, screenH, frustumDepth)}
    edgeMarkers[1]:setPos(xMin[1], xMin[2], xMin[3])
    edgeMarkers[2]:setPos(xMax[1], xMax[2], xMax[3])
    edgeMarkers[3]:setPos(yMin[1], yMin[2], yMin[3])
    edgeMarkers[4]:setPos(yMax[1], yMax[2], yMax[3])
end

-- ---------------------------------------------------------------------------
-- Headless numeric self-checks -- re-verify, on THIS machine's build, the claims
-- docs/lua-api.md section 2 and docs/future_investigation.md make about to3d/getPickRay.
-- ---------------------------------------------------------------------------

-- Camera sits at world origin looking straight down +Z with no roll, so at screen center the
-- ray direction is exactly (0,0,1): to3d(centerX, centerY, D) must equal the camera position
-- plus D along that axis, i.e. exactly (0,0,D) -- matching each reference object's own position.
-- NOTE: this only holds when camera.scaleScreen2d == 1 (the common case -- see
-- docs/future_investigation.md). transformeScreen2dToWorld3d_scaled (device-common.cpp) scales
-- the raw screen pixel by scaleScreen2d BEFORE dividing by the actual backbuffer size, so once
-- an app opts into design-resolution scaling via -ew/-eh (scaleScreen2d != 1 on purpose), the
-- TRUE screen-center pixel no longer lands on the forward axis by construction -- that is a
-- property of to3d's own formula, not a bug this scene is checking for. Skip the assertion (but
-- still print the numbers) whenever that's the case.
local function runSelfTest()
    local realW, realH = mbm.getRealSizeScreen()
    local sizeW, sizeH = mbm.getSizeScreen()
    local scaleIsOne = math.abs(realW / sizeW - 1.0) < 1e-4 and math.abs(realH / sizeH - 1.0) < 1e-4
    local cx, cy = screenW * 0.5, screenH * 0.5
    local EPS = 0.5
    local allPass = true
    for _, r in ipairs(refs) do
        local wx, wy, wz = mbm.to3d(cx, cy, r.dist)
        local err = math.sqrt(wx*wx + wy*wy + (wz - r.dist) * (wz - r.dist))
        local pass = (not scaleIsOne) or (err < EPS)
        allPass = allPass and pass
        print('info', pass and 'green' or 'red', string.format(
            '[selftest] %-16s depth=%-4d to3d=(%.3f,%.3f,%.3f) expected=(0,0,%d) err=%.4f %s',
            r.name, r.dist, wx, wy, wz, r.dist, err,
            scaleIsOne and (pass and 'PASS' or 'FAIL') or 'SKIP (scaleScreen2d != 1)'))
    end
    return allPass
end

-- mbm.getPickRay's normalized direction should agree with the direction reconstructed by
-- sampling mbm.to3d at two different depths through the same off-center pixel (docs/lua-api.md
-- section 2's own recommended technique), and its origin should always be the camera position.
local function runRayConsistencyCheck()
    local px, py = screenW * 0.25, screenH * 0.75
    local ox, oy, oz, dx, dy, dz = mbm.getPickRay(px, py)
    local x1, y1, z1 = mbm.to3d(px, py, 100)
    local x2, y2, z2 = mbm.to3d(px, py, 300)
    local ddx, ddy, ddz = x2 - x1, y2 - y1, z2 - z1
    local len = math.sqrt(ddx*ddx + ddy*ddy + ddz*ddz)
    if len > 1e-6 then ddx, ddy, ddz = ddx/len, ddy/len, ddz/len end
    local dot = ddx*dx + ddy*dy + ddz*dz
    local originErr = math.sqrt(ox*ox + oy*oy + oz*oz)
    local pass = dot > 0.999 and originErr < 0.5
    print('info', pass and 'green' or 'red', string.format(
        '[selftest] getPickRay dir=(%.4f,%.4f,%.4f) vs to3d-derived dir=(%.4f,%.4f,%.4f) dot=%.6f origin=(%.3f,%.3f,%.3f) %s',
        dx, dy, dz, ddx, ddy, ddz, dot, ox, oy, oz, pass and 'PASS' or 'FAIL'))
    return pass
end

-- ---------------------------------------------------------------------------
function onInitScene()
    mbm.setColor(18, 18, 28)
    screenW, screenH = mbm.getRealSizeScreen()

    KEY_1    = mbm.getKeyCode('1')
    KEY_2    = mbm.getKeyCode('2')
    KEY_3    = mbm.getKeyCode('3')
    KEY_UP   = mbm.getKeyCode('UP')
    KEY_DOWN = mbm.getKeyCode('DOWN')
    KEY_ESC  = mbm.getKeyCode('ESC')

    local cam = mbm.getCamera('3d')
    cam:setPos(0, 0, 0)
    cam:setFocus(0, 0, 1)
    cam:setUp(0, 1, 0)
    cam:setNear(1)
    cam:setFar(2000) -- default is only 1000 (docs/lua-api.md section 4); farthest ref sits at 900

    local defs = {
        {file = 'Crate.msh',      dist = 300},
        {file = 'base.msh',       dist = 600},
        {file = 'building_A.msh', dist = 900},
    }
    for i, d in ipairs(defs) do
        local m = mesh:new('3d', 0, 0, d.dist)
        local ok = m:load(d.file)
        if ok then
            fitScale(m, 150)
        else
            print('error', 'red', 'failed to load ' .. d.file)
        end
        refs[i] = {obj = m, name = d.file, dist = d.dist}
    end

    marker = makeCube(0, 0, 0, 20, 1, 0, 0, 1) -- red: follows the mouse

    edgeMarkers[1] = makeCube(0, 0, 0, 15, 0, 1, 1, 1) -- cyan: left/right frustum edge
    edgeMarkers[2] = makeCube(0, 0, 0, 15, 0, 1, 1, 1)
    edgeMarkers[3] = makeCube(0, 0, 0, 15, 1, 1, 0, 1) -- yellow: top/bottom frustum edge
    edgeMarkers[4] = makeCube(0, 0, 0, 15, 1, 1, 0, 1)
    -- NOT calling updateFrustumEdgeMarkers() here yet -- see the frameCount gate in onLoop below.

    hudFont = font:new('Font-test-no-shader-50.fnt')
    for i = 1, 5 do
        hudLines[i] = hudFont:add('', '2ds', 10, 10 + (i - 1) * 20)
        hudLines[i]:setScale(0.5, 0.5, 0.5)
    end
    hudLines[2].text = '[1/2/3] select depth=300/600/900   [UP/DOWN] frustum-slice depth'
    hudLines[3].text = '[click] obj:collide ray/AABB test against the three reference meshes'
    hudLines[4].text = '[ESC] quit'

    print('info', 'green', 'onInitScene complete -- screen ' .. screenW .. 'x' .. screenH)
end

-- CAMERA::updateCam() (camera.cpp) -- which rebuilds matrixView/matrixProj from cam:setPos/
-- setFocus -- only runs once per *rendered* frame (core-manager-opengl_es.cpp), not immediately
-- when the Lua setters are called. Calling mbm.to3d/getPickRay in the same onInitScene() that
-- just moved the camera reads the PREVIOUS frame's (default) matrices -- confirmed empirically
-- (origin came back as the engine's default camera position, not the one just set). So the
-- self-test and the initial edge-marker placement are deferred here until a couple of frames
-- have actually rendered.
local frameCount = 0
local selfTestDone = false

function onLoop(delta)
    frameCount = frameCount + 1
    if not selfTestDone and frameCount >= 3 then
        selfTestDone = true
        local realW, realH = mbm.getRealSizeScreen()
        local sizeW, sizeH = mbm.getSizeScreen()
        print('info', 'blue', string.format(
            '[selftest] getRealSizeScreen=(%.1f,%.1f) getSizeScreen=(%.1f,%.1f) implied scaleScreen2d=(%.4f,%.4f)',
            realW, realH, sizeW, sizeH, realW / sizeW, realH / sizeH))
        updateFrustumEdgeMarkers()
        local selfTestPass = runSelfTest()
        local rayPass = runRayConsistencyCheck()
        if selfTestPass and rayPass then
            print('info', 'green', 'TO3D_SELFTEST PASS')
        else
            print('error', 'red', 'TO3D_SELFTEST FAIL')
        end
    end

    if lastMouseX and lastMouseY then
        local depth = refs[depthIndex] and refs[depthIndex].dist or 300
        local wx, wy, wz = mbm.to3d(lastMouseX, lastMouseY, depth)
        marker:setPos(wx, wy, wz)
        hudLines[1].text = string.format(
            'mouse=(%.0f,%.0f) depth=%d (%s)  to3d=(%.1f,%.1f,%.1f)  frustumDepth=%d',
            lastMouseX, lastMouseY, depth,
            refs[depthIndex] and refs[depthIndex].name or '?',
            wx, wy, wz, frustumDepth)
    end
end

function onKeyDown(key)
    if key == KEY_ESC then
        mbm.quit()
    elseif key == KEY_1 then
        depthIndex = 1
    elseif key == KEY_2 then
        depthIndex = 2
    elseif key == KEY_3 then
        depthIndex = 3
    elseif key == KEY_UP then
        frustumDepth = frustumDepth + 50
        updateFrustumEdgeMarkers()
    elseif key == KEY_DOWN then
        frustumDepth = math.max(50, frustumDepth - 50)
        updateFrustumEdgeMarkers()
    end
end

function onTouchMove(key, x, y)
    lastMouseX, lastMouseY = x, y
end

function onTouchDown(key, x, y)
    if key ~= 0 then return end
    local hitAny = false
    for _, r in ipairs(refs) do
        local hit = r.obj:collide(x, y)
        if hit then
            hitAny = true
            print('info', 'green', string.format('[click] obj:collide hit %s at screen (%.0f,%.0f)', r.name, x, y))
        end
    end
    if not hitAny then
        print('info', 'yellow', string.format('[click] no reference mesh hit at screen (%.0f,%.0f)', x, y))
    end
    hudLines[5].text = hitAny and 'last click: HIT' or 'last click: no hit'
end
