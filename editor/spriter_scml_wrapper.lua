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

    spriter_scml_wrapper.lua

    Standalone Lua module that parses BrashMonkey Spriter .scml files and bakes
    skeletal mainline keys into Sprite Maker frame data.

    The Sprite Maker binary format stores mesh frames and frame-range
    animations, not Spriter bones.  This wrapper therefore resolves the Spriter
    bone hierarchy per mainline key and returns one baked frame per key, with
    each visible image object represented as a textured quad.

    Usage from sprite_maker.lua:
        tSpriterScml = require 'spriter_scml_wrapper'
        local result = tSpriterScml.import('/path/to/player.scml')

]]--

local M = {}

M.tLastResult = nil

-- File/path helpers

local function readFile(path)
    local f = io.open(path, "rb")
    if not f then return nil end
    local content = f:read("*a")
    f:close()
    return content
end

local function normalizePath(path)
    return (path or ""):gsub("\\", "/")
end

local function getDirName(path)
    local s = normalizePath(path)
    return s:match("^(.*)/[^/]*$") or "."
end

local function joinPath(a, b)
    a = normalizePath(a)
    b = normalizePath(b)
    if b:match("^/") or b:match("^%a:[/]") then
        return b
    end
    if a:sub(-1) == "/" then
        return a .. b
    end
    return a .. "/" .. b
end

local function addUnique(list, seen, value)
    if not seen[value] then
        seen[value] = true
        table.insert(list, value)
    end
end

-- XML parser

local function decodeXml(s)
    if not s then return s end
    return (s:gsub("&quot;", '"')
             :gsub("&apos;", "'")
             :gsub("&lt;", "<")
             :gsub("&gt;", ">")
             :gsub("&amp;", "&"))
end

local function parseAttrs(s)
    local attrs = {}
    for key, quote, value in s:gmatch("([%w_:%-]+)%s*=%s*(['\"])(.-)%2") do
        attrs[key] = decodeXml(value)
    end
    return attrs
end

local function parseTag(raw)
    local tag = raw:match("^%s*(.-)%s*$") or raw
    if tag:sub(1, 1) == "/" or tag:sub(1, 1) == "!" or tag:sub(1, 1) == "?" then
        return nil
    end
    local selfClosing = tag:match("/%s*$") ~= nil
    if selfClosing then
        tag = tag:gsub("/%s*$", "")
    end
    local name, rest = tag:match("^([%w_:%-]+)%s*(.*)$")
    if not name then return nil end
    return { name = name, attrs = parseAttrs(rest or ""), children = {} }, selfClosing
end

local function parseXml(content)
    local root = { name = "#document", attrs = {}, children = {} }
    local stack = { root }
    local pos = 1
    local len = #content

    while pos <= len do
        local tagStart = content:find("<", pos, true)
        if not tagStart then break end

        if content:sub(tagStart, tagStart + 3) == "<!--" then
            local e = content:find("-->", tagStart + 4, true)
            pos = e and (e + 3) or (len + 1)
        elseif content:sub(tagStart, tagStart + 8) == "<![CDATA[" then
            local e = content:find("]]>", tagStart + 9, true)
            pos = e and (e + 3) or (len + 1)
        else
            local tagEnd = content:find(">", tagStart + 1, true)
            if not tagEnd then break end
            local raw = content:sub(tagStart + 1, tagEnd - 1)
            if raw:match("^%s*/") then
                if #stack > 1 then table.remove(stack) end
            elseif raw:match("^%s*[!?]") then
                -- processing instruction / doctype
            else
                local node, selfClosing = parseTag(raw)
                if node then
                    local parent = stack[#stack]
                    table.insert(parent.children, node)
                    if not selfClosing then
                        table.insert(stack, node)
                    end
                end
            end
            pos = tagEnd + 1
        end
    end

    return root
end

local function childNodes(node, name)
    local out = {}
    if not node or not node.children then return out end
    for _, child in ipairs(node.children) do
        if child.name == name then
            table.insert(out, child)
        end
    end
    return out
end

local function firstChild(node, name)
    if not node or not node.children then return nil end
    for _, child in ipairs(node.children) do
        if child.name == name then return child end
    end
    return nil
end

local function toNum(value, default)
    local n = tonumber(value)
    if n == nil then return default end
    return n
end

local function toInt(value, default)
    local n = tonumber(value)
    if n == nil then return default end
    return math.floor(n)
end

local function cloneAttrs(attrs)
    local out = {}
    for k, v in pairs(attrs or {}) do out[k] = v end
    return out
end

-- SCML structural parser

function M.parse(scmlPath)
    local content = readFile(scmlPath)
    if not content then
        return nil, "could not read file"
    end

    local document = parseXml(content)
    local root = firstChild(document, "spriter_data")
    if not root then
        return nil, "missing spriter_data root"
    end

    local baseDir = getDirName(scmlPath)
    local data = {
        path = normalizePath(scmlPath),
        baseDir = baseDir,
        generator = root.attrs.generator or "",
        generatorVersion = root.attrs.generator_version or "",
        folders = {},
        folderList = {},
        entities = {},
        texturePaths = {},
        warnings = {},
    }
    local textureSeen = {}

    for _, folderNode in ipairs(childNodes(root, "folder")) do
        local folder = {
            id = toInt(folderNode.attrs.id, #data.folderList),
            name = folderNode.attrs.name or "",
            files = {},
            fileList = {},
        }
        for _, fileNode in ipairs(childNodes(folderNode, "file")) do
            local relPath = normalizePath(fileNode.attrs.name or "")
            local file = {
                id = toInt(fileNode.attrs.id, #folder.fileList),
                name = relPath,
                path = joinPath(baseDir, relPath),
                width = toNum(fileNode.attrs.width, 0),
                height = toNum(fileNode.attrs.height, 0),
                pivot_x = toNum(fileNode.attrs.pivot_x, 0),
                pivot_y = toNum(fileNode.attrs.pivot_y, 1),
            }
            folder.files[file.id] = file
            table.insert(folder.fileList, file)
            addUnique(data.texturePaths, textureSeen, file.path)
        end
        data.folders[folder.id] = folder
        table.insert(data.folderList, folder)
    end

    for _, entityNode in ipairs(childNodes(root, "entity")) do
        local entity = {
            id = toInt(entityNode.attrs.id, #data.entities),
            name = entityNode.attrs.name or "",
            animations = {},
        }

        for _, animNode in ipairs(childNodes(entityNode, "animation")) do
            local anim = {
                id = toInt(animNode.attrs.id, #entity.animations),
                name = animNode.attrs.name or ("animation_" .. tostring(#entity.animations + 1)),
                length = toNum(animNode.attrs.length, 0),
                interval = toNum(animNode.attrs.interval, nil),
                mainline = {},
                timelines = {},
            }

            local mainlineNode = firstChild(animNode, "mainline")
            if mainlineNode then
                for _, keyNode in ipairs(childNodes(mainlineNode, "key")) do
                    local key = {
                        id = toInt(keyNode.attrs.id, #anim.mainline),
                        time = toNum(keyNode.attrs.time, 0),
                        boneRefs = {},
                        objectRefs = {},
                    }
                    for _, refNode in ipairs(keyNode.children or {}) do
                        if refNode.name == "bone_ref" then
                            table.insert(key.boneRefs, cloneAttrs(refNode.attrs))
                        elseif refNode.name == "object_ref" then
                            table.insert(key.objectRefs, cloneAttrs(refNode.attrs))
                        end
                    end
                    table.insert(anim.mainline, key)
                end
            end

            for _, timelineNode in ipairs(childNodes(animNode, "timeline")) do
                local timeline = {
                    id = toInt(timelineNode.attrs.id, #anim.timelines),
                    name = timelineNode.attrs.name or "",
                    objectType = timelineNode.attrs.object_type or "sprite",
                    keys = {},
                }
                for _, keyNode in ipairs(childNodes(timelineNode, "key")) do
                    local child = firstChild(keyNode, "object") or firstChild(keyNode, "bone")
                    if child then
                        local key = {
                            id = toInt(keyNode.attrs.id, #timeline.keys),
                            time = toNum(keyNode.attrs.time, 0),
                            spin = toInt(keyNode.attrs.spin, 1),
                            type = child.name,
                            attrs = cloneAttrs(child.attrs),
                        }
                        table.insert(timeline.keys, key)
                    end
                end
                anim.timelines[timeline.id] = timeline
            end

            table.sort(anim.mainline, function(a, b)
                if a.time == b.time then return a.id < b.id end
                return a.time < b.time
            end)
            table.insert(entity.animations, anim)
        end

        table.insert(data.entities, entity)
    end

    return data
end

-- Transform baking

local function transformFromAttrs(attrs, file)
    attrs = attrs or {}
    file = file or {}
    return {
        x = toNum(attrs.x, 0),
        y = toNum(attrs.y, 0),
        angle = toNum(attrs.angle, 0),
        scale_x = toNum(attrs.scale_x, 1),
        scale_y = toNum(attrs.scale_y, 1),
        a = toNum(attrs.a, 1),
        pivot_x = toNum(attrs.pivot_x, file.pivot_x or 0),
        pivot_y = toNum(attrs.pivot_y, file.pivot_y or 1),
    }
end

local function combineTransform(parent, child)
    if not parent then return child end
    local angle = math.rad(parent.angle)
    local cosA = math.cos(angle)
    local sinA = math.sin(angle)
    local x = child.x * parent.scale_x
    local y = child.y * parent.scale_y
    return {
        x = parent.x + x * cosA - y * sinA,
        y = parent.y + x * sinA + y * cosA,
        angle = parent.angle + child.angle,
        scale_x = parent.scale_x * child.scale_x,
        scale_y = parent.scale_y * child.scale_y,
        a = parent.a * child.a,
        pivot_x = child.pivot_x,
        pivot_y = child.pivot_y,
    }
end

local function transformPoint(t, x, y)
    local sx = x * t.scale_x
    local sy = y * t.scale_y
    local angle = math.rad(t.angle)
    local cosA = math.cos(angle)
    local sinA = math.sin(angle)
    return {
        x = t.x + sx * cosA - sy * sinA,
        y = t.y + sx * sinA + sy * cosA,
    }
end

local function timelineKey(anim, timelineId, keyId)
    local timeline = anim.timelines[toInt(timelineId, -1)]
    if not timeline then return nil end
    local want = toInt(keyId, -1)
    for _, key in ipairs(timeline.keys) do
        if key.id == want then return key, timeline end
    end
    return nil, timeline
end

local function resolveFile(data, attrs)
    local folder = data.folders[toInt(attrs.folder, -1)]
    if not folder then return nil end
    return folder.files[toInt(attrs.file, -1)]
end

local function makeQuad(file, tr)
    local left = -tr.pivot_x * file.width
    local right = (1 - tr.pivot_x) * file.width
    local bottom = -tr.pivot_y * file.height
    local top = (1 - tr.pivot_y) * file.height

    local p1 = transformPoint(tr, left, bottom)
    local p2 = transformPoint(tr, left, top)
    local p3 = transformPoint(tr, right, bottom)
    local p4 = transformPoint(tr, right, top)

    return {
        vertices = { p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y },
        uv = { 0, 1, 0, 0, 1, 1, 1, 0 },
        index = { 1, 2, 3, 3, 2, 4 },
    }
end

local function addBounds(bounds, vertices)
    for i = 1, #vertices, 2 do
        local x = vertices[i]
        local y = vertices[i + 1]
        bounds.minX = math.min(bounds.minX, x)
        bounds.minY = math.min(bounds.minY, y)
        bounds.maxX = math.max(bounds.maxX, x)
        bounds.maxY = math.max(bounds.maxY, y)
    end
end

local function averageFrameTime(anim)
    if anim.interval and anim.interval > 0 then
        return anim.interval / 1000
    end
    if #anim.mainline > 1 then
        local firstTime = anim.mainline[1].time or 0
        local lastTime = anim.mainline[#anim.mainline].time or anim.length or 0
        local duration = math.max(lastTime - firstTime, 0)
        if duration > 0 then
            return duration / math.max(#anim.mainline - 1, 1) / 1000
        end
    end
    if anim.length and anim.length > 0 and #anim.mainline > 0 then
        return anim.length / #anim.mainline / 1000
    end
    return 0.1
end

local function animationName(entity, anim, entityCount)
    local name = anim.name or ("anim_" .. tostring(anim.id))
    if entityCount > 1 and entity.name and entity.name ~= "" then
        name = entity.name .. "_" .. name
    end
    if #name > 32 then
        name = name:sub(1, 32)
    end
    return name
end

function M.bake(data)
    local result = {
        texturePaths = data.texturePaths or {},
        frames = {},
        animations = {},
        warnings = data.warnings or {},
        width = 1,
        height = 1,
    }

    local bounds = {
        minX = 999999999,
        minY = 999999999,
        maxX = -999999999,
        maxY = -999999999,
    }

    for _, entity in ipairs(data.entities or {}) do
        for _, anim in ipairs(entity.animations or {}) do
            local frameStart = #result.frames + 1

            for _, mainKey in ipairs(anim.mainline or {}) do
                local boneWorldByRef = {}
                local boneRefById = {}
                local parts = {}

                for _, ref in ipairs(mainKey.boneRefs) do
                    boneRefById[toInt(ref.id, #boneRefById)] = ref
                end

                local function resolveBoneRef(refId)
                    if boneWorldByRef[refId] then
                        return boneWorldByRef[refId]
                    end
                    local ref = boneRefById[refId]
                    if not ref then return nil end
                    local key = timelineKey(anim, ref.timeline, ref.key)
                    if key and key.type == "bone" then
                        local localTr = transformFromAttrs(key.attrs)
                        local parentTr = nil
                        if ref.parent ~= nil then
                            parentTr = resolveBoneRef(toInt(ref.parent, -1))
                        end
                        boneWorldByRef[refId] = combineTransform(parentTr, localTr)
                    end
                    return boneWorldByRef[refId]
                end

                for _, ref in ipairs(mainKey.boneRefs) do
                    resolveBoneRef(toInt(ref.id, -1))
                end

                for _, ref in ipairs(mainKey.objectRefs) do
                    local key = timelineKey(anim, ref.timeline, ref.key)
                    if key and key.type == "object" then
                        local file = resolveFile(data, key.attrs)
                        if file then
                            local localTr = transformFromAttrs(key.attrs, file)
                            local parentTr = nil
                            if ref.parent ~= nil then
                                parentTr = boneWorldByRef[toInt(ref.parent, -1)]
                            end
                            local worldTr = combineTransform(parentTr, localTr)
                            if worldTr.a > 0 then
                                local quad = makeQuad(file, worldTr)
                                addBounds(bounds, quad.vertices)
                                table.insert(parts, {
                                    texturePath = file.path,
                                    fileName = file.name,
                                    zIndex = toInt(ref.z_index, #parts),
                                    alpha = worldTr.a,
                                    vertices = quad.vertices,
                                    uv = quad.uv,
                                    index = quad.index,
                                })
                            end
                        else
                            table.insert(result.warnings, string.format(
                                "missing file folder=%s file=%s in animation %s",
                                tostring(key.attrs.folder), tostring(key.attrs.file), tostring(anim.name)))
                        end
                    end
                end

                table.sort(parts, function(a, b)
                    if a.zIndex == b.zIndex then return a.fileName < b.fileName end
                    return a.zIndex < b.zIndex
                end)

                if #parts > 0 then
                    table.insert(result.frames, {
                        entityName = entity.name,
                        animationName = anim.name,
                        time = mainKey.time or 0,
                        parts = parts,
                    })
                end
            end

            local frameStop = #result.frames
            if frameStop >= frameStart then
                table.insert(result.animations, {
                    name = animationName(entity, anim, #(data.entities or {})),
                    frameStart = frameStart,
                    frameStop = frameStop,
                    frameTime = averageFrameTime(anim),
                })
            end
        end
    end

    if bounds.minX <= bounds.maxX and bounds.minY <= bounds.maxY then
        local centerX = (bounds.minX + bounds.maxX) * 0.5
        local centerY = (bounds.minY + bounds.maxY) * 0.5
        result.width = math.max(bounds.maxX - bounds.minX, 1)
        result.height = math.max(bounds.maxY - bounds.minY, 1)
        for _, frame in ipairs(result.frames) do
            for _, part in ipairs(frame.parts) do
                for i = 1, #part.vertices, 2 do
                    part.vertices[i] = part.vertices[i] - centerX
                    part.vertices[i + 1] = part.vertices[i + 1] - centerY
                end
            end
        end
    end

    return result
end

function M.import(scmlPath)
    local data, err = M.parse(scmlPath)
    if not data then
        M.tLastResult = { ok = false, message = err or "parse failed" }
        return M.tLastResult
    end
    local baked = M.bake(data)
    baked.ok = #baked.frames > 0
    baked.message = baked.ok and "ok" or "no frames found"
    baked.path = data.path
    baked.generator = data.generator
    baked.generatorVersion = data.generatorVersion
    M.tLastResult = baked
    return baked
end

return M
