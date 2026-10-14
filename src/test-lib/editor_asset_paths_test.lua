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
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

]]--

package.path = 'editor/?.lua;' .. package.path
local P = require 'editor_asset_paths'
local IO = require 'image_mesh_io'
assert(P.normalize('C:\\Game\\raw\\..\\assets/a.msh') == 'C:/Game/assets/a.msh')
assert(P.directory('/scene.lua') == '/')
assert(P.directory('C:/scene.lua') == 'C:/')
assert(P.relative('/new/Game/assets/a.msh', '/new/Game/raw/scene.lua') == '../assets/a.msh')
assert(P.relative('D:/a.msh', 'C:/Game/scene.lua') == 'D:/a.msh')
assert(P.resolve('../assets/a.msh', '/renamed/raw/scene.lua') == '/renamed/assets/a.msh')
assert(P.resolve('#FFFFFFFF', '/renamed/raw/scene.lua') == '#FFFFFFFF')
assert(P.resolve('', '/renamed/raw/scene.lua') == '')
local old = '/home/user/Game/assets/a.msh'
local current = '/Users/user/Game/assets/a.msh'
local project = '/Users/user/Game/raw/scene.lua'
assert(P.resolve(old, project, function(p) return p == current end) == current)
assert(P.resolve(old, project, function(p) return p == old or p == current end) == old)
assert(P.resolve(old, project, function() return false end) == old)
assert(P.resolve('C:/old/Game/assets/a.msh', project, function(p) return p == current end) == current)
-- Two matching ancestors with existing files: do not select an arbitrary asset.
assert(P.resolve(old, '/new/Game/Game/raw/scene.lua', function(p)
    return p == '/new/Game/assets/a.msh' or p == '/new/Game/Game/assets/a.msh'
end) == old)
assert(IO.relative(current, project) == '../assets/a.msh')
assert(IO.resolve('../assets/a.msh', project) == current)
local state = {tMeshOffsets={[old]={y=7}}, tPlacedMeshInfo={{fileName=old}},
    sMeshSetFolder='/home/user/Game/assets', tOptionsEditor={sCurrentScriptExecution=''}}
P.scene(state, function(p) return P.resolve(p, project, function(q) return q == current end) end)
assert(state.tMeshOffsets[current].y == 7 and state.tPlacedMeshInfo[1].fileName == current)
assert(state.tOptionsEditor.sCurrentScriptExecution == '')
-- Exercise both generations of saved Lua files, including captured search paths.
local temp = os.tmpname()
local function checkFile(text, expected)
    local f = assert(io.open(temp, 'w')); f:write(text); f:close()
    local registered = {}
    local engine = {addPath=function(path) registered[#registered+1]=path end}
    local original = engine.addPath
    local scene = P.loadScene(temp, engine)
    assert(engine.addPath == original)
    assert(scene.tPlacedMeshInfo[1].fileName == expected)
    assert(scene.tMeshOffsets[expected].y == 7)
    assert(registered[1] == P.normalize(P.directory(temp)..'/assets'))
end
local expected = temp .. '.msh'
local assetFile = assert(io.open(expected, 'w')); assetFile:close()
local assetName = expected:match('[^/]+$')
local legacyExpected = P.normalize(P.directory(temp)..'/assets/a.msh')
checkFile([[mbm.addPath('assets')
return {tMeshOffsets={['assets/a.msh']={y=7}},tPlacedMeshInfo={{fileName='assets/a.msh'}}}]], legacyExpected)
checkFile([[return {tAssetPaths={'assets'},tMeshOffsets={['assets/a.msh']={y=7}},
tPlacedMeshInfo={{fileName='assets/a.msh'}}}]], legacyExpected)
os.remove(temp)
assert(loadfile('editor/scene_editor3d.lua'))
assert(loadfile('editor/image_mesh_editor.lua'))
local source = assert(io.open('editor/scene_editor3d.lua'))
local code = source:read('*a'); source:close()
local writer = assert(code:match('(function writeScene3d%b()%s*.-)\n%-%- Applies a loaded/'))
local function serialize(value)
    if type(value) == 'table' then
        local out = {'{'}
        for key, child in pairs(value) do out[#out+1] = '['..serialize(key)..']='..serialize(child)..',' end
        out[#out+1] = '}'
        return table.concat(out)
    end
    return type(value) == 'string' and string.format('%q', value) or tostring(value)
end
local env = setmetatable({assetPaths=P, mbm={getAllPaths=function() return {P.directory(temp)..'/assets'} end},
    tUtil={save=function(name, value, lines) lines[#lines+1]=name..'='..serialize(value) end},
    getHeader3d=function() return '' end, getSceneLoaderCode3d=function() return 'return tScene3d' end,
    tOptionsEditor={iIndexResolution=1,sCurrentScriptExecution='',fSceneCamPos={},fSceneCamFocus={}},
    tResolution={{x=800,y=600}}, tLightConfig={}, tMeshOffsets={[expected]={y=7}},
    tSceneObjects={}, tMapOptions={}, tLayers={}, tCamByTab={}, sMeshSetFolder=P.directory(expected),
    tPlacedMeshes={{fileName=expected,type='mesh',scale={x=1,y=1,z=1}}},
    resolvePlacedMeshWorldPos=function() return 1,2,3 end, formatLuaStringList=function() return '{}' end,
}, {__index=_G})
assert(load(writer, '@scene-writer-test', 't', env))()
assert(env.writeScene3d(temp, false, false))
local saved = assert(loadfile(temp))()
assert(saved.tPlacedMeshInfo[1].fileName == assetName)
assert(saved.tMeshOffsets[assetName].y == 7)
assert(env.tPlacedMeshes[1].fileName == expected, 'Saving mutated live state')
local reloaded = P.loadScene(temp, {addPath=function() end})
assert(reloaded.tPlacedMeshInfo[1].fileName == expected)
assert(reloaded.tMeshOffsets[expected].y == 7)
-- Export keeps its existing independent game-facing contract.
assert(env.writeScene3d(temp, false, true))
local exported = assert(loadfile(temp, 't', setmetatable({mbm={addPath=function() end}}, {__index=_G})))()
assert(exported.tPlacedMeshInfo[1].fileName == expected)
assert(exported.tMeshOffsets[expected].y == 7 and #exported.tAssetPaths > 0)
os.remove(temp)
os.remove(expected)
print('editor asset paths: PASS')
-- Optional real user fixture: read only, never save over it.
if arg[1] then
    local scene = P.loadScene(arg[1], {addPath=function() end})
    -- Run the production writer with real project data, redirecting only its
    -- destination so the user's project is never modified by the regression test.
    env.tMeshOffsets = scene.tMeshOffsets
    env.tPlacedMeshes = {}
    for _, info in ipairs(scene.tPlacedMeshInfo) do
        local mesh = {}
        for k,v in pairs(info) do mesh[k] = v end
        mesh.scale = {x=info.sx,y=info.sy,z=info.sz}
        env.tPlacedMeshes[#env.tPlacedMeshes+1] = mesh
    end
    env.resolvePlacedMeshWorldPos = function(mesh) return mesh.x,mesh.y,mesh.z end
    env.sMeshSetFolder = scene.sMeshSetFolder
    local base = P.directory(P.directory(arg[1]))
    env.mbm.getAllPaths = function() return {
        '/Users/michel/mini-mbm/editor', '/var/folders/cache/mbm_scene3d_thumbnails',
        base..'/assets/meshes', base..'/assets/textures', base..'/raw',
    } end
    env.io = {open=function() return io.open(temp, 'w') end}
    assert(env.writeScene3d(arg[1], false, false))
    local clean = assert(loadfile(temp))()
    for _, path in ipairs(clean.tAssetPaths) do
        assert(not path:find('mini%-mbm') and not path:find('var/folders'))
    end
    for path in pairs(clean.tMeshOffsets) do
        assert(path:sub(1,17) == '../assets/meshes/', path)
    end
    assert(env.writeScene3d(base..'/levels/scene.lua', false, true))
    local f = assert(io.open(temp)); local exportText = f:read('*a'); f:close()
    assert(not exportText:find('/Users/') and not exportText:find('/home/'))
    assert(not exportText:find('mini%-mbm') and not exportText:find('var/folders'))
    -- Different location, directory depth, project name, and Windows drive.
    for _, relocated in ipairs({'/opt/games/Renamed/levels/scene.lua', 'D:/Games/Renamed/levels/scene.lua'}) do
        local gameEnv = setmetatable({mbm={addPath=function() end}}, {__index=_G})
        local moved = assert(load(exportText, '@'..relocated, 't', gameEnv))()
        assert(#moved.tPlacedMeshInfo == #scene.tPlacedMeshInfo)
        for _, info in ipairs(moved.tPlacedMeshInfo) do
            assert(info.fileName:find(P.directory(relocated), 1, true) == 1)
            assert(moved.tMeshOffsets[info.fileName])
        end
    end
    os.remove(temp)
    print('Real project save/export relocation: PASS')
    local unique, count = {}, 0
    for _, info in ipairs(scene.tPlacedMeshInfo) do
        assert(P.exists(info.fileName), 'Unresolved mesh: '..info.fileName)
        unique[info.fileName] = true
    end
    local missingOffsets = 0
    for path in pairs(scene.tMeshOffsets) do
        if not P.exists(path) then missingOffsets = missingOffsets + 1 end
    end
    for _ in pairs(unique) do count = count+1 end
    assert(P.exists(scene.sMeshSetFolder))
    print(string.format('Real project: %d placements, %d unique assets resolved', #scene.tPlacedMeshInfo, count))
    print('Unused offsets with missing assets: '..missingOffsets)
end
