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
local G = require 'gui_asset_paths'
local root = os.tmpname()
os.remove(root)
assert(os.execute('mkdir -p "'..root..'/raw" "'..root..'/assets"'))
local project = root..'/raw/gui.lua'
local asset = root..'/assets/icon.spt'
local f=assert(io.open(asset,'w')); f:write('fixture'); f:close()
local engine={addPath=function() end, getAllPaths=function() return {root..'/assets'} end}
assert(G.saved(asset,project,engine)=='../assets/icon.spt')
local options={sExtraScript=root..'/assets/code.lua',sCurrentScriptExecution=''}
assert(G.options(options,project,engine).sExtraScript=='../assets/code.lua')
assert(options.sExtraScript==root..'/assets/code.lua')
f=assert(io.open(project,'w'))
f:write('local tScene={}\n')
G.write(f,project,{{fileName=asset}},engine)
f:write("tScene.tAllMeshInfo={{fileName='../assets/icon.spt'}}\n")
f:write("tScene.resolved=_guiAssetPath(tScene.tAllMeshInfo[1].fileName)\nreturn tScene")
f:close()
local scene=G.load(project,engine)
assert(scene.tAllMeshInfo[1].fileName==asset)
-- Move and rename the whole project, preserving its internal layout.
assert(os.rename(root,root..'-moved'))
root=root..'-moved';project=root..'/raw/gui.lua';asset=root..'/assets/icon.spt'
scene=G.load(project,engine)
assert(scene.tAllMeshInfo[1].fileName==asset)
f=assert(io.open(project));local source=f:read('*a');f:close()
assert(not source:find(root,1,true))
local windows=assert(load(source,[[@D:\Games\Renamed\raw\gui.lua]],'t',
    setmetatable({mbm=engine},{__index=_G})))()
assert(P.normalize(windows.resolved)=='D:/Games/Renamed/assets/icon.spt')
-- Legacy absolute paths survive a different OS and renamed project root.
f=assert(io.open(project,'w'))
f:write([[mbm.addPath('C:/old/Original/assets')
return {tAllMeshInfo={{fileName='icon.spt'}}}]])
f:close()
assert(G.load(project,engine).tAllMeshInfo[1].fileName==asset)
os.remove(project);os.remove(asset)
assert(os.execute('rmdir "'..root..'/raw" "'..root..'/assets" "'..root..'"'))
print('GUI asset relocation: PASS')
if arg[1] then
    scene=G.load(arg[1],engine)
    for _,info in ipairs(scene.tAllMeshInfo) do
        assert(info.fileName:sub(1,1)=='#' or P.exists(info.fileName),info.fileName)
    end
    print('Legacy GUI fixture resolved: '..#scene.tAllMeshInfo..' elements')
end
