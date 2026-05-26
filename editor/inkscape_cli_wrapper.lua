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

M.inkscape     = nil   -- cached {found=bool, is_v1=bool, version=string}
M.tGroups      = {}    -- [{id=string, displayName=string}]
M.tLastResults = {}    -- [{ok=bool, outputPath=string, message=string}]

-- ─── Inkscape detection ───────────────────────────────────────────────────────

-- Detects whether inkscape is installed and which version family it belongs to.
-- Caches the result in M.inkscape; subsequent calls return the cached value.
-- Returns: {found=bool, is_v1=bool, version=string}
function M.detectInkscape()
    if M.inkscape ~= nil then return M.inkscape end
    local f = io.popen("inkscape --version 2>&1")
    if not f then
        M.inkscape = { found = false, is_v1 = false, version = "" }
        return M.inkscape
    end
    local output = f:read("*a")
    f:close()
    if output and output:find("Inkscape") then
        local version = output:match("Inkscape%s+([%d%.]+)") or ""
        local major   = tonumber(version:match("^(%d+)")) or 0
        M.inkscape = { found = true, is_v1 = (major >= 1), version = version }
    else
        M.inkscape = { found = false, is_v1 = false, version = "" }
    end
    return M.inkscape
end

-- Force a fresh detection (discards the cache).
function M.resetDetection()
    M.inkscape = nil
end

-- ─── Path helpers ─────────────────────────────────────────────────────────────

-- Returns the file path without its extension.
--   "/a/b/char.svg"  →  "/a/b/char"
function M.getSvgStem(svgPath)
    return svgPath:match("(.+)%.[^%.]+$") or svgPath
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

-- ─── Inkscape command builder ─────────────────────────────────────────────────

-- Builds the inkscape CLI command string for rasterizing an SVG (or a group
-- within it) to a PNG file.
--
-- svgPath    : path to the source SVG
-- outputPath : path for the output PNG
-- width      : output width in pixels
-- height     : output height in pixels
-- groupId    : (optional) id of a <g> element to export; nil = whole SVG
--
-- Returns the command string, or nil if inkscape is not available.
function M.buildCmd(svgPath, outputPath, width, height, groupId)
    local ink = M.inkscape or M.detectInkscape()
    if not ink.found then return nil end

    if ink.is_v1 then
        -- inkscape 1.x command-line syntax
        if groupId then
            return string.format(
                "inkscape %q --export-id=%s --export-area-drawing"
                .. " --export-type=png --export-width=%d --export-height=%d"
                .. " --export-filename=%q",
                svgPath, groupId, width, height, outputPath)
        else
            return string.format(
                "inkscape %q --export-type=png"
                .. " --export-width=%d --export-height=%d"
                .. " --export-filename=%q",
                svgPath, width, height, outputPath)
        end
    else
        -- inkscape 0.9x legacy syntax
        if groupId then
            return string.format(
                "inkscape -z -i %s --export-area-drawing -w %d -h %d -e %q %q",
                groupId, width, height, outputPath, svgPath)
        else
            return string.format(
                "inkscape -z -w %d -h %d -e %q %q",
                width, height, outputPath, svgPath)
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
    local f      = io.popen(cmd .. " 2>&1")
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
-- Output file: <svgStem>.png  (same directory as the SVG)
--
-- Stores result in M.tLastResults.
-- Returns: {ok=bool, outputPath=string, message=string}
function M.importSingle(svgPath, width, height)
    M.tLastResults = {}
    local outputPath = M.getSvgStem(svgPath) .. ".png"
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
-- Output files: <svgStem>_<groupId>.png  (same directory as the SVG)
-- tSelectedIds: list of id strings, e.g. {"g1060", "g1064"}
--
-- Stores all results in M.tLastResults.
-- Returns: [{ok, outputPath, message}]
function M.importGroups(svgPath, tSelectedIds, width, height)
    M.tLastResults = {}
    local stem = M.getSvgStem(svgPath)
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

return M
