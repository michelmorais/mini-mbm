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

    inkscape_cli_wrapper.lua

    Standalone Lua module that wraps the inkscape CLI to:
      - Parse SVG files and detect <g> groups at a given depth level
      - Rasterize the whole SVG or individual groups to PNG files

    Usage from texture_packer.lua:
        tInkscape = require 'inkscape_cli_wrapper'
        tInkscape.detectInkscape()
        tInkscape.parseSvgGroupsAtDepth('/path/to/file.svg', 2)
        tInkscape.importGroups('/path/to/file.svg', {'g1060','g1064'}, 256, 256)

    Module-level state (readable after calls):
        tInkscape.inkscape     -- {found, is_v1, version}
        tInkscape.tGroups      -- [{id, displayName}] from last parseSvgGroupsAtDepth()
        tInkscape.tLastResults -- [{ok, outputPath, message}] from last import call

]]--

local M = {}

-- ─── Module state ────────────────────────────────────────────────────────────

M.inkscape     = nil   -- cached {found=bool, is_v1=bool, version=string, path=string}
M.customPath   = nil   -- user-supplied executable path (overrides auto-detection)
M.tGroups      = {}    -- [{id=string, displayName=string}]
M.tLastResults = {}    -- [{ok=bool, outputPath=string, message=string}]

-- ─── OS detection ────────────────────────────────────────────────────────────

-- Returns "windows", "macos", "linux", "android", or "ios".
-- Uses mbm.get('os') which returns the capitalised platform name at runtime.
function M.getOS()
    local s = (mbm and mbm.get('os') or ''):lower()
    if s == '' then
        -- Fallback when running outside the engine (e.g. unit tests).
        return package.config:sub(1, 1) == '\\' and 'windows' or 'linux'
    end
    return s
end

-- ─── Shell quoting & command wrapping ────────────────────────────────────────

-- Wraps a file-system path in double-quotes for a shell command.
-- Does NOT double backslashes (unlike Lua's %q), so Windows paths work.
local function shellQuote(s)
    return '"' .. s:gsub('"', '\\"') .. '"'
end

-- On Windows, cmd.exe mis-parses a command that starts with a quoted path
-- unless the whole thing is wrapped in an extra pair of outer quotes.
-- Prefix with 'cmd /c "..."' to handle that correctly.
local function wrapCmd(cmd)
    if M.getOS() == "windows" then
        return 'cmd /c "' .. cmd .. '"'
    end
    return cmd
end

-- ─── Inkscape detection ───────────────────────────────────────────────────────

-- Internal helper: try running `exe --version` and parse output.
-- Returns {found, is_v1, version, path} or nil if the exe is not usable.
local function tryDetectExe(exe)
    local f = io.popen(wrapCmd(shellQuote(exe) .. " --version") .. " 2>&1")
    if not f then return nil end
    local output = f:read("*a")
    f:close()
    if output and output:find("Inkscape") then
        local version = output:match("Inkscape%s+([%d%.]+)") or ""
        local major   = tonumber(version:match("^(%d+)")) or 0
        return { found = true, is_v1 = (major >= 1), version = version, path = exe }
    end
    return nil
end

-- Candidate paths to probe on Windows when `inkscape` is not on PATH.
local WINDOWS_INKSCAPE_CANDIDATES = {
    "C:\\Program Files\\Inkscape\\bin\\inkscape.exe",
    "C:\\Program Files (x86)\\Inkscape\\bin\\inkscape.exe",
    "C:\\Program Files\\Inkscape\\inkscape.exe",
}

-- Detects whether inkscape is installed and which version family it belongs to.
-- Probes (in order): M.customPath → PATH → Windows common install dirs.
-- Caches the result in M.inkscape; subsequent calls return the cached value.
-- Returns: {found=bool, is_v1=bool, version=string, path=string}
function M.detectInkscape()
    if M.inkscape ~= nil then return M.inkscape end

    -- 1. User-supplied custom path takes priority.
    if M.customPath and M.customPath ~= "" then
        local r = tryDetectExe(M.customPath)
        if r then M.inkscape = r; return M.inkscape end
    end

    -- 2. Try `inkscape` on the system PATH.
    local r = tryDetectExe("inkscape")
    if r then M.inkscape = r; return M.inkscape end

    -- 3. On Windows, probe common installation directories.
    if M.getOS() == "windows" then
        for _, candidate in ipairs(WINDOWS_INKSCAPE_CANDIDATES) do
            local probe = io.open(candidate, "r")
            if probe then
                probe:close()
                r = tryDetectExe(candidate)
                if r then M.inkscape = r; return M.inkscape end
            end
        end
    end

    M.inkscape = { found = false, is_v1 = false, version = "", path = "" }
    return M.inkscape
end

-- Force a fresh detection (discards the cache).
function M.resetDetection()
    M.inkscape = nil
end

-- Set a user-supplied inkscape executable path and reset the detection cache.
-- Call this when the user browses for the executable manually.
function M.setCustomPath(path)
    M.customPath = path
    M.inkscape   = nil
end

-- ─── Path helpers ─────────────────────────────────────────────────────────────

-- Returns the file path without its extension.
--   "/a/b/char.svg"  →  "/a/b/char"
function M.getSvgStem(svgPath)
    return svgPath:match("(.+)%.[^%.]+$") or svgPath
end

-- Returns just the filename stem (no directory, no extension).
--   "/a/b/char.svg"  →  "char"
function M.getFileBaseStem(svgPath)
    local name = svgPath:match("[/\\]([^/\\]+)$") or svgPath
    return name:match("(.+)%.[^%.]+$") or name
end

-- ─── SVG group parser ─────────────────────────────────────────────────────────

-- Parses an SVG file and collects all <g> elements at a specific XML depth.
--
-- depth: integer ≥ 1
--   depth=1 → direct children of the <svg> root  (e.g. Inkscape layers)
--   depth=2 → their children                      (sub-groups / shapes)
--
-- Only <g> elements with an 'id' attribute are returned; inkscape needs an id
-- for --export-id. Unnamed groups are silently skipped.
--
-- Populates M.tGroups and returns it.
-- Returns: [{id=string, displayName=string}]
function M.parseSvgGroupsAtDepth(svgPath, targetDepth)
    M.tGroups = {}

    local f = io.open(svgPath, "r")
    if not f then return M.tGroups end
    local content = f:read("*a")
    f:close()

    -- Locate the end of the opening <svg ...> tag so we start scanning inside it.
    local svgTagOpen = content:find("<svg[%s>]")
    if not svgTagOpen then return M.tGroups end
    local svgTagClose = content:find(">", svgTagOpen)
    if not svgTagClose then return M.tGroups end

    local pos   = svgTagClose + 1
    local len   = #content
    local depth = 0

    while pos <= len do
        local tagStart = content:find("<", pos, true)
        if not tagStart then break end

        -- ── XML comment <!-- ... --> ──────────────────────────────────────────
        if content:sub(tagStart, tagStart + 3) == "<!--" then
            local e = content:find("-->", tagStart + 4, true)
            pos = e and (e + 3) or (len + 1)

        -- ── CDATA section <![CDATA[ ... ]]> ───────────────────────────────────
        elseif content:sub(tagStart, tagStart + 8) == "<![CDATA[" then
            local e = content:find("]]>", tagStart + 9, true)
            pos = e and (e + 3) or (len + 1)

        -- ── Processing instruction <?...?> or DOCTYPE <!...> ─────────────────
        elseif content:sub(tagStart, tagStart + 1) == "<?" or
               content:sub(tagStart, tagStart + 1) == "<!" then
            local e = content:find(">", tagStart + 2, true)
            pos = e and (e + 1) or (len + 1)

        -- ── Closing tag </tag> ────────────────────────────────────────────────
        elseif content:sub(tagStart, tagStart + 1) == "</" then
            local e = content:find(">", tagStart + 2, true)
            depth = depth - 1
            if depth < 0 then break end   -- exited </svg>
            pos = e and (e + 1) or (len + 1)

        -- ── Opening or self-closing tag ───────────────────────────────────────
        else
            local tagEnd = content:find(">", tagStart + 1, true)
            if not tagEnd then break end

            local tagContent  = content:sub(tagStart, tagEnd)
            -- A self-closing tag ends with '/>' (ignoring trailing whitespace).
            local selfClosing = tagContent:match("/>%s*$") ~= nil

            if not selfClosing then
                depth = depth + 1
                if depth == targetDepth then
                    -- Only interested in <g elements at this depth.
                    local tagName = tagContent:match("^<([%w:%-_]+)")
                    if tagName == "g" then
                        local id = tagContent:match('id="([^"]+)"')
                                or tagContent:match("id='([^']+)'")
                        if id then
                            table.insert(M.tGroups, { id = id, displayName = id })
                        end
                        -- Groups without id are silently skipped:
                        -- inkscape requires --export-id=<id> for element export.
                    end
                end
            end
            pos = tagEnd + 1
        end
    end

    return M.tGroups
end

-- Finds all direct <g> children whose immediate parent id is in parentIdSet.
--
-- parentIdSet : a hash table  { ["gtile1"] = true, ["gtile2"] = true, ... }
--              Use any group IDs previously returned by parseSvgGroupsAtDepth.
--
-- The function walks the entire SVG XML while tracking a stack of open ancestor
-- element IDs ("" for non-<g> elements).  A <g> is collected only when the
-- top of that stack matches a key in parentIdSet.
--
-- Returns: [ {id=string, displayName=string, parentId=string} ]
function M.parseChildrenOfGroups(svgPath, parentIdSet)
    local results = {}

    local f = io.open(svgPath, "r")
    if not f then return results end
    local content = f:read("*a")
    f:close()

    local svgTagOpen  = content:find("<svg[%s>]")
    if not svgTagOpen  then return results end
    local svgTagClose = content:find(">", svgTagOpen)
    if not svgTagClose then return results end

    local pos   = svgTagClose + 1
    local len   = #content
    local stack = {}   -- stack[i] = group id (or "" for non-<g> elements)

    while pos <= len do
        local tagStart = content:find("<", pos, true)
        if not tagStart then break end

        if content:sub(tagStart, tagStart + 3) == "<!--" then
            local e = content:find("-->", tagStart + 4, true)
            pos = e and (e + 3) or (len + 1)

        elseif content:sub(tagStart, tagStart + 8) == "<![CDATA[" then
            local e = content:find("]]", tagStart + 9, true)
            pos = e and (e + 3) or (len + 1)

        elseif content:sub(tagStart, tagStart + 1) == "<?" or
               content:sub(tagStart, tagStart + 1) == "<!" then
            local e = content:find(">", tagStart + 2, true)
            pos = e and (e + 1) or (len + 1)

        elseif content:sub(tagStart, tagStart + 1) == "</" then
            -- Closing tag: pop one ancestor off the stack.
            local e = content:find(">", tagStart + 2, true)
            if #stack > 0 then table.remove(stack) end
            pos = e and (e + 1) or (len + 1)

        else
            local tagEnd = content:find(">", tagStart + 1, true)
            if not tagEnd then break end
            local tagContent  = content:sub(tagStart, tagEnd)
            local selfClosing = tagContent:match("/>%s*$") ~= nil

            if not selfClosing then
                local tagName = tagContent:match("^<([%w:%-_]+)")
                if tagName == "g" then
                    local id = tagContent:match('id="([^"]+)"')
                           or  tagContent:match("id='([^']+)'")
                    -- Collect this group if its direct parent is a selected container.
                    local parentId = stack[#stack]
                    if parentId and parentIdSet[parentId] and id then
                        table.insert(results, {
                            id          = id,
                            displayName = id,
                            parentId    = parentId,
                        })
                    end
                    table.insert(stack, id or "")
                else
                    -- Non-<g> opening tag: push placeholder to keep the stack balanced.
                    table.insert(stack, "")
                end
            end
            pos = tagEnd + 1
        end
    end

    return results
end

-- ─── Inkscape command builder ─────────────────────────────────────────────────

-- Builds the inkscape CLI command string for rasterizing an SVG (or a group
-- within it) to a PNG file.
--
-- svgPath         : path to the source SVG
-- outputPath      : path for the output PNG
-- width           : output width in pixels (always honoured)
-- height          : output height in pixels (ignored when keepAspectRatio=true)
-- groupId         : (optional) id of a <g> element to export; nil = whole SVG
-- keepAspectRatio : (optional bool, default false)
--                   When true, only width is passed to inkscape; the height is
--                   derived automatically to preserve the source aspect ratio.
--
-- Returns the command string, or nil if inkscape is not available.
function M.buildCmd(svgPath, outputPath, width, height, groupId, keepAspectRatio, keepAspectOnHeight)
    local ink = M.inkscape or M.detectInkscape()
    if not ink.found then return nil end

    -- Build the dimension flags based on the aspect-ratio mode.
    local sizeFlags
    if keepAspectRatio then
        if keepAspectOnHeight then
            -- Fix height, let inkscape derive width automatically.
            if ink.is_v1 then
                sizeFlags = string.format("--export-height=%d", height)
            else
                sizeFlags = string.format("-h %d", height)
            end
        else
            -- Fix width, let inkscape derive height automatically.
            if ink.is_v1 then
                sizeFlags = string.format("--export-width=%d", width)
            else
                sizeFlags = string.format("-w %d", width)
            end
        end
    else
        if ink.is_v1 then
            sizeFlags = string.format("--export-width=%d --export-height=%d", width, height)
        else
            sizeFlags = string.format("-w %d -h %d", width, height)
        end
    end

    local exe = shellQuote(ink.path)

    if ink.is_v1 then
        -- inkscape 1.x command-line syntax
        if groupId then
            return string.format(
                "%s %s --export-id=%s --export-area-drawing"
                .. " --export-type=png %s --export-filename=%s",
                exe, shellQuote(svgPath), groupId, sizeFlags, shellQuote(outputPath))
        else
            return string.format(
                "%s %s --export-type=png %s --export-filename=%s",
                exe, shellQuote(svgPath), sizeFlags, shellQuote(outputPath))
        end
    else
        -- inkscape 0.9x legacy syntax
        if groupId then
            return string.format(
                "%s -z -i %s --export-area-drawing %s -e %s %s",
                exe, groupId, sizeFlags, shellQuote(outputPath), shellQuote(svgPath))
        else
            return string.format(
                "%s -z %s -e %s %s",
                exe, sizeFlags, shellQuote(outputPath), shellQuote(svgPath))
        end
    end
end

-- ─── Command runner ───────────────────────────────────────────────────────────

-- Executes a shell command and verifies the output file was created.
-- Uses an io.open probe rather than relying on the exit code, which is
-- not always reliable across inkscape versions.
--
-- Returns: {ok=bool, message=string}
function M.runCmd(cmd, outputPath)
    local f      = io.popen(wrapCmd(cmd) .. " 2>&1")
    local output = f and f:read("*a") or ""
    if f then f:close() end

    local probe = io.open(outputPath, "r")
    if probe then
        probe:close()
        return { ok = true, message = "ok" }
    end

    -- Return the first non-empty line of stderr as the error message.
    local errLine = output:match("[^\n]+") or "inkscape command failed"
    return { ok = false, message = errLine }
end

-- ─── High-level import helpers ────────────────────────────────────────────────

-- Rasterizes the whole SVG as a single PNG.
-- outputDir : (optional) directory for the output file;
--             nil = same directory as the SVG
--
-- Stores result in M.tLastResults.
-- Returns: {ok=bool, outputPath=string, message=string}
function M.importSingle(svgPath, width, height, outputDir)
    M.tLastResults = {}
    local outputPath
    if outputDir then
        outputPath = outputDir .. "/" .. M.getFileBaseStem(svgPath) .. ".png"
    else
        outputPath = M.getSvgStem(svgPath) .. ".png"
    end
    local cmd        = M.buildCmd(svgPath, outputPath, width, height, nil)
    local result
    if not cmd then
        result = { ok = false, outputPath = outputPath, message = "inkscape not found" }
    else
        result            = M.runCmd(cmd, outputPath)
        result.outputPath = outputPath
    end
    table.insert(M.tLastResults, result)
    return result
end

-- Rasterizes each selected group as a separate PNG.
-- outputDir    : (optional) directory for the output files;
--               nil = same directory as the SVG
-- tSelectedIds : list of id strings, e.g. {"g1060", "g1064"}
--
-- Stores all results in M.tLastResults.
-- Returns: [{ok, outputPath, message}]
function M.importGroups(svgPath, tSelectedIds, width, height, outputDir)
    M.tLastResults = {}
    local stem
    if outputDir then
        stem = outputDir .. "/" .. M.getFileBaseStem(svgPath)
    else
        stem = M.getSvgStem(svgPath)
    end
    for _, id in ipairs(tSelectedIds) do
        local outputPath = stem .. "_" .. id .. ".png"
        local cmd        = M.buildCmd(svgPath, outputPath, width, height, id)
        local result
        if not cmd then
            result = { ok = false, outputPath = outputPath, message = "inkscape not found" }
        else
            result            = M.runCmd(cmd, outputPath)
            result.outputPath = outputPath
        end
        table.insert(M.tLastResults, result)
    end
    return M.tLastResults
end

-- ─── Async helpers (used by the coroutine-based importer) ─────────────────────

-- Returns true only when the file at `path` is a complete, fully-written PNG.
-- A valid PNG always ends with the 12-byte IEND chunk
-- (4-byte zero length + 4-byte "IEND" + 4-byte CRC).
-- Checking for IEND is race-condition-free: the marker is only present once
-- inkscape has finished writing the entire file and closed it.
function M.fileExists(path)
    local f = io.open(path, "rb")
    if not f then return false end
    local ok = f:seek("end", -12)
    if not ok then f:close(); return false end
    local tail = f:read(12)
    f:close()
    return tail ~= nil and tail:sub(5, 8) == "IEND"
end

-- Launches an inkscape command as a background (non-blocking) process.
-- On Unix  : appends ' >/dev/null 2>&1 &' to detach inkscape.
-- On Windows: runs via 'start /b' to avoid blocking the frame loop.
function M.launchCmdAsync(cmd)
    if M.getOS() == "windows" then
        -- 'start /b' launches the process in the background without blocking,
        -- so the coroutine can yield between frames and the progress bar updates.
        -- The empty "" is a required window-title placeholder for the start command.
        os.execute('cmd /c start /b "" ' .. cmd)
    else
        -- Unix: detach to background, suppress output.
        os.execute(cmd .. ' >/dev/null 2>&1 &')
    end
end

-- ─── Adobe Illustrator helpers ────────────────────────────────────────────────

-- Returns true if `path` has a .ai extension (case-insensitive).
function M.isAiFile(path)
    return path:lower():sub(-3) == ".ai"
end

-- Converts an Adobe Illustrator (.ai) file to a plain SVG using Inkscape.
-- This is a synchronous operation (via io.popen) intended to run once before
-- the SVG group-parsing and export pipeline is entered.
--
-- aiPath        : full path to the source .ai file
-- outputSvgPath : destination .svg path for the converted file
--
-- Returns true on success (the output file exists), false otherwise.
function M.convertAiToSvg(aiPath, outputSvgPath)
    local ink = M.inkscape or M.detectInkscape()
    if not ink.found then return false end

    local exe = shellQuote(ink.path)
    local cmd
    if ink.is_v1 then
        -- Inkscape 1.x
        cmd = string.format(
            "%s %s --export-type=svg --export-filename=%s",
            exe, shellQuote(aiPath), shellQuote(outputSvgPath))
    else
        -- Inkscape 0.9x
        cmd = string.format(
            "%s %s --export-plain-svg=%s",
            exe, shellQuote(aiPath), shellQuote(outputSvgPath))
    end

    local f = io.popen(wrapCmd(cmd) .. " 2>&1")
    if f then f:read("*a"); f:close() end

    local probe = io.open(outputSvgPath, "r")
    if probe then probe:close(); return true end
    return false
end

return M
