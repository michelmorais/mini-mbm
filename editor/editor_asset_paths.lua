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

-- Paths are anchored to the project file, never to the process working directory.
-- Legacy absolute paths are recovered only through matching ancestor directory
-- names, preserving the entire suffix. No recursive scans or basename guessing.
local M = {}
local function absolute(path)
    return path:sub(1,1) == '/' or path:match('^%a:/') ~= nil
end
function M.normalize(path)
    path = path:gsub('\\', '/')
    local root = path:match('^%a:/') or (path:sub(1,2) == '//' and '//')
        or (path:sub(1,1) == '/' and '/') or ''
    local parts = {}
    for part in path:sub(#root+1):gmatch('[^/]+') do
        if part == '..' and #parts > 0 and parts[#parts] ~= '..' then
            table.remove(parts)
        elseif part ~= '.' and (part ~= '..' or root == '') then
            parts[#parts+1] = part
        end
    end
    local result = root .. table.concat(parts, '/')
    return result ~= '' and result or '.'
end
function M.directory(path)
    path = M.normalize(path)
    local dir = path:match('^(.*)/[^/]*$')
    if dir == '' then return '/' end
    if dir and dir:match('^%a:$') then return dir .. '/' end
    return dir or '.'
end
function M.exists(path)
    local file = io.open(path, 'rb')
    if not file then
        -- Windows does not allow io.open on directories. Renaming to itself
        -- probes existence without changing the directory or its contents.
        return os.rename(path, path) == true
    end
    file:close()
    return true
end
function M.relative(path, projectPath)
    if path == '' or path:sub(1,1) == '#' then return path end
    path = M.normalize(path)
    local base = M.directory(projectPath)
    if not absolute(path) or not absolute(base) then return path end
    -- Different drives/shares cannot be expressed relative to each other.
    local function root(p) return p:match('^(%a:)/') or p:match('^(//[^/]+/[^/]+)') or '/' end
    if root(path) ~= root(base) then return path end
    local a, b = {}, {}
    for p in path:gmatch('[^/]+') do a[#a+1] = p end
    for p in base:gmatch('[^/]+') do b[#b+1] = p end
    local i = 1
    while a[i] and a[i] == b[i] do i = i + 1 end
    local out = {}
    for _ = i, #b do out[#out+1] = '..' end
    for j = i, #a do out[#out+1] = a[j] end
    return #out > 0 and table.concat(out, '/') or '.'
end
function M.resolve(path, projectPath, exists)
    if path == '' or path:sub(1,1) == '#' then return path end
    path = M.normalize(path)
    local base = M.directory(projectPath)
    if not absolute(path) then path = M.normalize(base .. '/' .. path) end
    exists = exists or M.exists
    if exists(path) then return path end
    local candidates = {}
    -- Match Ratworks/assets/foo against each current project ancestor named
    -- Ratworks. Multiple existing matches are ambiguous and remain unresolved.
    local ancestor = base
    while ancestor ~= '.' and ancestor ~= '' do
        local name = ancestor:match('([^/]+)$')
        if not name then break end
        local prefix = ''
        for part in path:gmatch('[^/]+') do
            prefix = prefix .. '/' .. part
            if part == name then
                local suffix = ('/' .. path:gsub('^/+', '')):sub(#prefix+1)
                local candidate = M.normalize(ancestor .. suffix)
                if exists(candidate) then candidates[candidate] = true end
            end
        end
        local parent = M.directory(ancestor)
        if parent == ancestor then break end
        ancestor = parent
    end
    local found
    for candidate in pairs(candidates) do
        if found then return path end
        found = candidate
    end
    return found or path
end
-- Recover absent legacy assets only when their containing directory can be
-- located unambiguously. The missing filename remains visible to the user.
function M.asset(path, projectPath)
    local resolved = M.resolve(path, projectPath)
    if resolved == '' or resolved:sub(1,1) == '#' or M.exists(resolved) then return resolved end
    local parent = M.directory(resolved)
    local moved = M.resolve(parent, projectPath)
    if moved ~= parent then return moved .. '/' .. resolved:match('[^/]+$') end
    return resolved
end
function M.saved(path, projectPath)
    local resolved = M.asset(path, projectPath)
    if resolved ~= '' and resolved:sub(1,1) ~= '#' and not M.exists(resolved)
        and not M.exists(M.directory(resolved)) then
        return resolved -- Keep unresolved external references explicit, not ../../home/...
    end
    return M.relative(resolved, projectPath)
end
-- Find the enclosing assets tree; do not capture the engine's process-wide paths.
function M.searchPaths(projectPath, paths, meshes, folder)
    local root = M.directory(projectPath)
    while not M.exists(root .. '/assets') do
        local parent = M.directory(root)
        if parent == root or parent == '.' or root == '.' then
            root = M.directory(projectPath)
            break
        end
        root = parent
    end
    local result, seen = {}, {}
    local function add(path)
        path = M.normalize(path)
        if path ~= '' and not seen[path] then
            seen[path] = true
            result[#result+1] = M.relative(path, projectPath)
        end
    end
    for _, path in ipairs(paths) do
        path = M.asset(path, projectPath)
        if path == root or path:sub(1,#root+1) == root .. '/' then add(path) end
    end
    for _, mesh in ipairs(meshes) do add(M.directory(M.asset(mesh.fileName, projectPath))) end
    if folder and folder ~= '' then add(M.asset(folder, projectPath)) end
    return result
end
-- Embedded in game exports: no dependency on editor modules or the launch CWD.
function M.exportResolver()
    return [=[
local source = debug.getinfo(1, 'S').source
assert(source:sub(1,1) == '@', 'Scene must be loaded from a file')
local sceneFile = source:sub(2):gsub('\\', '/')
local sceneDir = sceneFile:match('^(.*)/[^/]*$') or '.'
local function assetPath(path)
    if path == '' or path:sub(1,1) == '#' or path:match('^/') or path:match('^%a:[/\\]') then return path end
    return sceneDir .. '/' .. path
end
for _, path in ipairs(tScene3d.tAssetPaths) do mbm.addPath(assetPath(path)) end
local offsets = {}
for path, value in pairs(tScene3d.tMeshOffsets) do offsets[assetPath(path)] = value end
tScene3d.tMeshOffsets = offsets
for _, info in ipairs(tScene3d.tPlacedMeshInfo) do info.fileName = assetPath(info.fileName) end
]=]
end
-- Convert only schema-defined asset fields, including path-keyed mesh offsets.
function M.scene(scene, convert)
    local offsets = {}
    for path, value in pairs(scene.tMeshOffsets or {}) do
        local resolved = convert(path)
        assert(offsets[resolved] == nil, 'Conflicting mesh paths: ' .. resolved)
        offsets[resolved] = value
    end
    scene.tMeshOffsets = offsets
    for _, info in ipairs(scene.tPlacedMeshInfo or {}) do info.fileName = convert(info.fileName) end
    if scene.sMeshSetFolder then scene.sMeshSetFolder = convert(scene.sMeshSetFolder) end
    local options = scene.tOptionsEditor
    if options and options.sCurrentScriptExecution then
        options.sCurrentScriptExecution = convert(options.sCurrentScriptExecution)
    end
    return scene
end
-- Capture old save-file addPath calls without changing the live mbm table.
-- This is a compatibility environment, not a sandbox for untrusted Lua scripts.
function M.loadScene(projectPath, engine)
    local paths = {}
    local proxy = setmetatable({addPath = function(path) paths[#paths+1] = path end}, {__index = engine})
    local env = setmetatable({mbm = proxy}, {__index = _G})
    local chunk, err = loadfile(projectPath, 't', env)
    assert(chunk, err)
    local scene = chunk()
    assert(type(scene) == 'table', 'Invalid scene file')
    local cache = {}
    local function resolve(path)
        if cache[path] == nil then cache[path] = M.asset(path, projectPath) end
        return cache[path]
    end
    M.scene(scene, resolve)
    for _, path in ipairs(scene.tAssetPaths or paths) do engine.addPath(resolve(path)) end
    engine.addPath(M.directory(projectPath))
    return scene
end
return M
