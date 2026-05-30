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

    imagick_cli_wrapper.lua

    Standalone Lua module that wraps the ImageMagick CLI to extract
    individual layers from PSD (Photoshop) files and export them as PNGs.

    Usage from texture_packer.lua:
        tImageMagick = require 'imagick_cli_wrapper'
        tImageMagick.detectImageMagick()
        tImageMagick.countPsdLayers('/path/to/file.psd')
        tImageMagick.buildLayerCmd('/path/to/file.psd', 0, '/tmp/layer0.png')

    Module-level state (readable after calls):
        tImageMagick.imagick    -- {found, path, version}
        tImageMagick.customPath -- user-supplied executable path

]]--

local M = {}

-- ─── Module state ────────────────────────────────────────────────────────────

M.imagick    = nil   -- cached {found=bool, path=string, version=string}
M.customPath = nil   -- user-supplied executable path (overrides auto-detection)

-- ─── OS detection ────────────────────────────────────────────────────────────

-- Returns "windows", "macos", "linux", "android", or "ios".
function M.getOS()
    local s = (mbm and mbm.get('os') or ''):lower()
    if s == '' then
        return package.config:sub(1, 1) == '\\' and 'windows' or 'linux'
    end
    return s
end

-- ─── Shell quoting ────────────────────────────────────────────────────────────

local function shellQuote(s)
    return '"' .. s:gsub('"', '\\"') .. '"'
end

local function wrapCmd(cmd)
    if M.getOS() == "windows" then
        return 'cmd /c "' .. cmd .. '"'
    end
    return cmd
end

-- ─── ImageMagick detection ───────────────────────────────────────────────────

-- On Windows, ImageMagick 7+ uses `magick` as the unified entry point.
-- On Linux/macOS, `convert` is the traditional entry point for IM 6 and
-- also available in IM 7 as a compatibility shim.
-- We probe both so that both IM 6 and IM 7 on all platforms are detected.
local PROBE_EXES = {
    linux   = { "convert", "magick" },
    macos   = { "convert", "magick" },
    windows = { "magick", "convert" },
}

local WINDOWS_IMAGICK_CANDIDATES = {
    "C:\\Program Files\\ImageMagick-7\\magick.exe",
    "C:\\Program Files\\ImageMagick-6\\convert.exe",
    "C:\\Program Files (x86)\\ImageMagick-7\\magick.exe",
    "C:\\Program Files (x86)\\ImageMagick-6\\convert.exe",
}

local function tryDetectExe(exe)
    local f = io.popen(wrapCmd(shellQuote(exe) .. " --version") .. " 2>&1")
    if not f then return nil end
    local output = f:read("*a")
    f:close()
    if output and output:find("ImageMagick") then
        local version = output:match("ImageMagick%s+([%d%.%-]+)") or ""
        return { found = true, path = exe, version = version }
    end
    return nil
end

-- Detects whether ImageMagick is installed.
-- Probes (in order): M.customPath → platform-appropriate exe names on PATH
-- → Windows common install dirs.
-- Caches the result in M.imagick; subsequent calls return the cached value.
-- Returns: {found=bool, path=string, version=string}
function M.detectImageMagick()
    if M.imagick ~= nil then return M.imagick end

    -- 1. User-supplied custom path takes priority.
    if M.customPath and M.customPath ~= "" then
        local r = tryDetectExe(M.customPath)
        if r then M.imagick = r; return M.imagick end
    end

    -- 2. Try platform-appropriate executables on PATH.
    local os_name = M.getOS()
    local probes  = PROBE_EXES[os_name] or PROBE_EXES.linux
    for _, exe in ipairs(probes) do
        local r = tryDetectExe(exe)
        if r then M.imagick = r; return M.imagick end
    end

    -- 3. On Windows, probe common installation directories.
    if os_name == "windows" then
        for _, candidate in ipairs(WINDOWS_IMAGICK_CANDIDATES) do
            local probe = io.open(candidate, "r")
            if probe then
                probe:close()
                local r = tryDetectExe(candidate)
                if r then M.imagick = r; return M.imagick end
            end
        end
    end

    M.imagick = { found = false, path = "", version = "" }
    return M.imagick
end

-- Force a fresh detection (discards the cache).
function M.resetDetection()
    M.imagick = nil
end

-- Set a user-supplied executable path and reset the detection cache.
function M.setCustomPath(path)
    M.customPath = path
    M.imagick    = nil
end

-- ─── Path helpers ─────────────────────────────────────────────────────────────

-- Returns just the filename stem (no directory, no extension).
--   "/a/b/char.psd"  →  "char"
function M.getFileBaseStem(path)
    local name = path:match("[/\\]([^/\\]+)$") or path
    return name:match("(.+)%.[^%.]+$") or name
end

-- Returns the directory portion of a path (with trailing slash).
--   "/a/b/char.psd"  →  "/a/b/"
--   "char.psd"       →  "./"
function M.getFileDir(path)
    return path:match("(.*[/\\])") or "./"
end

-- ─── PSD layer introspection ──────────────────────────────────────────────────

-- Internal helper: returns the correct identify command prefix.
-- IM 7 unified binary: `magick identify`; IM 6: standalone `identify` binary.
local function getIdentifyExe(exePath)
    if exePath:match("[/\\]?magick$") or exePath:match("[/\\]?magick%.exe$")
       or exePath == "magick" then
        return shellQuote(exePath) .. " identify"
    else
        return "identify"
    end
end

-- Returns the total frame count reported by ImageMagick for the PSD.
-- Returns 0 on any failure.
function M.countPsdLayers(psdPath)
    local im = M.imagick or M.detectImageMagick()
    if not im.found then return 0 end

    local identifyExe = getIdentifyExe(im.path)
    local cmd = wrapCmd(
        identifyExe .. " -format \"%n\\n\" " .. shellQuote(psdPath) .. "[0]"
    ) .. " 2>&1"

    local f = io.popen(cmd)
    if not f then return 0 end
    local line = f:read("*l")
    f:close()

    local n = tonumber(line)
    return n and math.max(0, n) or 0
end

-- Returns per-layer info for each frame in the PSD that has actual pixel data.
-- Runs `identify -format "%[label]|%w|%h\n"` to read the layer name and
-- bounding-box size for every frame.  Frames with zero dimensions (empty
-- merged composites, blank adjustment layers) are silently skipped.
--
-- Returns: [{index=int, displayName=string, width=int, height=int}]
function M.getPsdLayerInfo(psdPath)
    local im = M.imagick or M.detectImageMagick()
    if not im.found then return {} end

    local identifyExe = getIdentifyExe(im.path)
    -- Format per frame: "label|width|height".  | is safe because PSD layer
    -- names rarely contain pipes.
    local cmd = wrapCmd(
        identifyExe .. " -format \"%[label]|%w|%h\\n\" " .. shellQuote(psdPath)
    ) .. " 2>&1"

    local f = io.popen(cmd)
    if not f then return {} end
    local output = f:read("*a")
    f:close()

    local layers   = {}
    local frameIdx = 0
    for line in output:gmatch("[^\n\r]+") do
        -- Only process lines that are actual frame data in "label|w|h" format.
        -- IM 6 emits a stderr warning ("unknown image property %[label]") that
        -- appears in the output when we use 2>&1.  That warning line does NOT
        -- contain "|digits|digits", so it will not match and frameIdx will NOT
        -- be incremented for it — keeping all layer indices correct.
        local label, ws, hs = line:match("^([^|]*)|(%d+)|(%d+)")
        if ws and hs then
            local w = tonumber(ws) or 0
            local h = tonumber(hs) or 0
            -- Only expose frames that actually contain pixel data.
            if w > 0 and h > 0 then
                -- Use the PSD layer name when available; fall back to "Layer N".
                local name = (label and label:match("%S")) and label
                             or string.format("Layer %d", frameIdx)
                table.insert(layers, {
                    index       = frameIdx,
                    displayName = name,
                    width       = w,
                    height      = h,
                })
            end
            -- Increment only for real frame lines (not warning/error lines).
            frameIdx = frameIdx + 1
        end
    end
    return layers
end

-- ─── Layer command builder ────────────────────────────────────────────────────

-- Builds the ImageMagick CLI command to extract a single PSD layer to PNG.
--
-- psdPath           : path to the source PSD file
-- layerIndex        : 0-based frame index (as returned by getPsdLayerInfo)
-- outputPath        : destination PNG path
-- width, height     : (optional) desired output dimensions in pixels
-- keepAspectRatio   : (optional bool) when true, only one axis is fixed
-- keepAspectOnHeight: (optional bool) relevant when keepAspectRatio=true;
--                     if true: fix height, width is auto; otherwise fix width
--
-- Returns the command string, or nil if ImageMagick is not available.
function M.buildLayerCmd(psdPath, layerIndex, outputPath, width, height, keepAspectRatio, keepAspectOnHeight)
    local im = M.imagick or M.detectImageMagick()
    if not im.found then return nil end

    local exe    = shellQuote(im.path)
    -- The [N] suffix selects a specific frame/layer.
    local input  = shellQuote(psdPath .. "[" .. tostring(layerIndex) .. "]")
    local output = shellQuote(outputPath)

    -- Build the optional resize flag.
    local resizeFlag = ""
    if width and height and width > 0 and height > 0 then
        if keepAspectRatio then
            if keepAspectOnHeight then
                resizeFlag = string.format(" -resize x%d", height)
            else
                resizeFlag = string.format(" -resize %dx", width)
            end
        else
            resizeFlag = string.format(" -resize %dx%d!", width, height)
        end
    end

    local baseExe = im.path
    if baseExe:match("[/\\]?magick$") or baseExe:match("[/\\]?magick%.exe$")
       or baseExe == "magick" then
        return string.format("%s convert %s -alpha on%s %s", exe, input, resizeFlag, output)
    else
        return string.format("%s %s -alpha on%s %s", exe, input, resizeFlag, output)
    end
end

-- ─── Async helpers ────────────────────────────────────────────────────────────

-- Returns true only when the file at `path` is a complete, fully-written PNG.
-- Checks for the 12-byte IEND chunk at the end of the file, which ImageMagick
-- only writes after closing the file — making this race-condition-free.
function M.fileExists(path)
    local f = io.open(path, "rb")
    if not f then return false end
    local ok = f:seek("end", -12)
    if not ok then f:close(); return false end
    local tail = f:read(12)
    f:close()
    return tail ~= nil and tail:sub(5, 8) == "IEND"
end

-- Launches an ImageMagick command as a background (non-blocking) process.
function M.launchCmdAsync(cmd)
    if M.getOS() == "windows" then
        os.execute('cmd /c start /b "" ' .. cmd)
    else
        os.execute(cmd .. ' >/dev/null 2>&1 &')
    end
end

return M
