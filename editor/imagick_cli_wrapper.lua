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

-- ─── PSD layer count ──────────────────────────────────────────────────────────

-- Returns the number of frames/layers in the PSD file.
-- Uses `identify -format "%n\n"` which prints the frame count.
-- Returns 0 on any failure.
function M.countPsdLayers(psdPath)
    local im = M.imagick or M.detectImageMagick()
    if not im.found then return 0 end

    -- Use the detected exe; for `magick` (IM 7) we call `magick identify`.
    local exe       = im.path
    local identifyCmd
    if exe:match("[/\\]?magick$") or exe:match("[/\\]?magick%.exe$")
       or exe == "magick" then
        identifyCmd = shellQuote(exe) .. " identify"
    else
        -- `convert` / legacy IM 6: `identify` is a separate binary on PATH;
        -- fall back to it, or try `convert -identify` which also works.
        identifyCmd = "identify"
    end

    local cmd = wrapCmd(
        identifyCmd .. " -format \"%n\\n\" " .. shellQuote(psdPath) .. "[0]"
    ) .. " 2>&1"

    local f = io.popen(cmd)
    if not f then return 0 end
    local line = f:read("*l")
    f:close()

    local n = tonumber(line)
    return n and math.max(0, n) or 0
end

-- ─── Layer command builder ────────────────────────────────────────────────────

-- Builds the ImageMagick CLI command to extract a single PSD layer to PNG.
--
-- psdPath    : path to the source PSD file
-- layerIndex : 0-based layer index (e.g. 0 = first layer / merged composite)
-- outputPath : destination PNG path
--
-- Returns the command string, or nil if ImageMagick is not available.
function M.buildLayerCmd(psdPath, layerIndex, outputPath)
    local im = M.imagick or M.detectImageMagick()
    if not im.found then return nil end

    local exe = shellQuote(im.path)
    -- The [N] suffix on the input path selects a specific frame/layer.
    local input = shellQuote(psdPath .. "[" .. tostring(layerIndex) .. "]")
    local output = shellQuote(outputPath)

    -- `convert` / `magick convert` both accept: convert input[N] output.png
    -- For IM 7 `magick`, we use `magick convert` for clarity.
    local baseExe = im.path
    if baseExe:match("[/\\]?magick$") or baseExe:match("[/\\]?magick%.exe$")
       or baseExe == "magick" then
        return string.format("%s convert %s %s", exe, input, output)
    else
        return string.format("%s %s %s", exe, input, output)
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
