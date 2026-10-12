--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                        |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Scene Editor 3D wall-layer regression fixture.

]]--
-- Run from repository root with mini-mbm --scene this-file --disable_select_monitor --nosplash.
package.path = 'editor/?.lua;' .. package.path
dofile('editor/scene_editor3d.lua')
local initEditor = onInitScene
local started, tested, failure
local asset = 'src/test-lib/Crate.msh'
local function near(a, b) assert(math.abs(a - b) < 0.001, tostring(a) .. ' ~= ' .. tostring(b)) end
local function screenPoint(p)
    local w,h = mbm.getSizeScreen()
    local ox,oy,oz,fx,fy,fz = mbm.getPickRay(w/2,h/2)
    local o = {x=ox,y=oy,z=oz}
    local f = {x=fx,y=fy,z=fz}
    local len=math.sqrt(f.x*f.x+f.z*f.z)
    local r={x=f.z/len,y=0,z=-f.x/len}
    local u={x=f.y*r.z,y=f.z*r.x-f.x*r.z,z=-f.y*r.x}
    local vx,vy,vz = p.x-o.x,p.y-o.y,p.z-o.z
    local depth = vx*f.x+vy*f.y+vz*f.z
    local _,_,_,dx,dy,dz = mbm.getPickRay(w,h/2)
    local horizontal = (dx*r.x+dy*r.y+dz*r.z)/(dx*f.x+dy*f.y+dz*f.z)
    local _,_,_,ex,ey,ez = mbm.getPickRay(w/2,h)
    local vertical = (ex*u.x+ey*u.y+ez*u.z)/(ex*f.x+ey*f.y+ez*f.z)
    return w/2+(vx*r.x+vy*r.y+vz*r.z)/depth/horizontal*w/2,
        h/2+(vx*u.x+vy*u.y+vz*u.z)/depth/vertical*h/2
end

function onInitScene()
    initEditor()
    mbm.addPath('src/test-lib')
    started = mbm.getTimeRun()
    addLayer(true)
    tLayers[2].fY = 20
    tLayers[2].offset = {x = 30, z = -40}
    tMapOptions.iGridCountX, tMapOptions.iGridCountZ = 4, 3
    tMapOptions.fGridCellWidthX, tMapOptions.fGridCellDepthZ = 100, 80
    setActiveTab('layer')
    rebuildGridVisual()
end

local function runTests()
    local wall = tLayers[2]
    assert(tLayers[1].bWall == false and wall.bWall == true)
    assert(#tGridLines == 9, 'odd row count')
    local x, y, z = gridCellToWorld(1, 2, wall)
    near(x, 130); near(y, 180); near(z, -40)
    x, y, z = gridCellToWorld(1, 2, tLayers[1])
    near(x, 100); near(y, 0); near(z, 160)
    for _, mode in ipairs({'Orthogonal', 'Isometric'}) do
        tMapOptions.sMapType = mode
        for _, layer in ipairs(tLayers) do
            for _, stagger in ipairs({{false,false}, {true,false}, {false,true}, {true,true}}) do
                layer.bHalfOffsetX, layer.bHalfOffsetZ = table.unpack(stagger)
                for cx = -2, 2 do
                    for cv = -2, 2 do
                        x, y, z = gridCellToWorld(cx, cv, layer)
                        local a, b = worldToGridCell(x, y, z, layer)
                        assert(a == cx and b == cv, 'cell round trip')
                    end
                end
            end
            layer.bHalfOffsetX, layer.bHalfOffsetZ = false, false
        end
    end
    tMapOptions.sMapType = 'Orthogonal'
    tMapOptions.sSnapScaleMode = 'full_fit'
    local measured = {setScale = function() end, getAABB = function() return 50, 160, 20 end}
    near(computeSnapScale(measured, wall), 0.5)
    near(computeSnapScale(measured, tLayers[1]), 2)
    tMapOptions.sSnapScaleMode = 'bigger_only'
    near(computeSnapScale(measured, wall), 0.5)
    near(computeSnapScale(measured, tLayers[1]), 1)
    tMapOptions.sSnapScaleMode = 'none'
    local floorSX, floorSY = screenPoint({x = 100, y = 0, z = 80})
    x, y, z = screenToWorldOnLayerPlane(floorSX, floorSY, tLayers[1])
    near(x, 100); near(y, 0); near(z, 80)
    tMeshSetEntries = {{fileName = asset, type = 'mesh'}}
    sMeshSelectedForPlacement = asset
    local sx, sy = screenPoint({x = 130, y = 100, z = -40})
    x, y, z = screenToWorldOnLayerPlane(sx, sy, wall)
    near(x, 130); near(y, 100); near(z, -40)
    tryPlaceMeshAt(sx, sy)
    assert(#tPlacedMeshes == 1 and tPlacedMeshes[1].tObj)
    near(tPlacedMeshes[1].tObj.y, 100)
    near(tPlacedMeshes[1].tObj.z, -40)
    tryPlaceMeshAt(sx, sy)
    assert(#tPlacedMeshes == 1, 'overwrite same wall cell')
    tPlacedMeshes[1].isSelected = true
    copySelectedMeshes()
    tCopyBuffer[1].dy = -80
    pasteCopiedMeshes()
    assert(#tPlacedMeshes == 2, 'paste onto another wall row')
    near(tPlacedMeshes[2].tObj.y, 20); near(tPlacedMeshes[2].tObj.z, -40)
    fillActiveLayerWithMesh(asset)
    assert(#tPlacedMeshes == 12, 'wall fill count')
    for _, placed in ipairs(tPlacedMeshes) do near(placed.tObj.z, -40) end

    local snapshots = captureAllPlacedMeshWorldPositions()
    tMapOptions.sMapType = 'Free'
    applyPlacedMeshWorldPositions(snapshots)
    resyncAllPlacedMeshes()
    tryPlaceMeshAt(sx, sy)
    local placed = tPlacedMeshes[#tPlacedMeshes]
    near(placed.tObj.y, 100); near(placed.tObj.z, -40)
    assert(isWorldPosWithinGridBounds(130, 100, 9000, wall), 'free bounds ignore normal')
    assert(not isWorldPosWithinGridBounds(130, 9000, -40, wall), 'free bounds use Y')
    for _, item in ipairs(tPlacedMeshes) do item.isSelected = item == placed end
    copySelectedMeshes()
    tCopyBuffer[1].dy = -25
    pasteCopiedMeshes()
    near(tPlacedMeshes[#tPlacedMeshes].tObj.y, 75)
    near(tPlacedMeshes[#tPlacedMeshes].tObj.z, -40)
    removePlacedMesh(#tPlacedMeshes)
    setLayerWall(2, false)
    near(placed.tObj.y, 20); near(placed.tObj.z, 40)
    setLayerWall(2, true)
    near(placed.tObj.y, 100); near(placed.tObj.z, -40)

    for _, exporting in ipairs({false, true}) do
        local path = os.tmpname()
        assert(writeScene3d(path, false, exporting))
        local scene = assert(loadfile(path))()
        os.remove(path)
        near(scene.tPlacedMeshInfo[#tPlacedMeshes].y, 100)
        near(scene.tPlacedMeshInfo[#tPlacedMeshes].z, -40)
        if exporting then
            assert(not scene.tLayers, 'export excludes authoring state')
        else
            assert(scene.tLayers[2].bWall and not scene.tLayers[1].bWall)
            -- A missing plane flag represents an old floor layer.
            scene.tLayers[1].bWall = nil
            applyLoadedScene3d(scene)
            assert(not tLayers[1].bWall and tLayers[2].bWall)
            near(tPlacedMeshes[#tPlacedMeshes].tObj.y, 100)
        end
    end
    setActiveTab('layer')
    iSelectedLayer = 2
    rebuildGridVisual()
    -- No vertex uploads or grid reconstruction during idle editor frames.
    for _, ln in ipairs(tGridLines) do ln.set = function() error('idle grid upload') end end
    print('SCENE3D WALL TESTS OK')
end

function onLoop(delta)
    if not tested and mbm.getTimeRun() - started > 0.5 then
        tested = true
        local ok, err = pcall(runTests)
        if not ok then failure = err; print('SCENE3D WALL FAIL: ' .. tostring(err)) end
    end
    if not failure then
        applyCam3d(cam3d)
        tImGui.SetNextWindowPos({x = 0, y = 0}, tImGui.Flags('ImGuiCond_Always'))
        tImGui.SetNextWindowSize({x = 310, y = 600}, tImGui.Flags('ImGuiCond_Always'))
        tImGui.Begin('Wall layers', false, 0)
        drawLayerTab(290)
        tImGui.End()
    end
    if mbm.getTimeRun() - started > 5 then
        if not failure then print('SCENE3D WALL SMOKE OK') end
        mbm.quit()
    end
end
