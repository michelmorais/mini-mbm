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

    gimp_cli_wrapper.lua

    Standalone Lua module that wraps the GIMP CLI (Script-Fu batch mode) to
    extract layers from PSD (Photoshop) files and export them as PNGs.

    Unlike the ImageMagick wrapper, GIMP provides:
      - True group/layer hierarchy via gimp-item-get-children
      - Canvas offsets (position of each layer on the PSD canvas)
      - Reliable per-layer visibility flags
      - Correct rendering of blend modes and smart objects

    GIMP is launched ONCE per PSD. A dynamically-written .scm Script-Fu file
    is used so no installation step is needed.

    Usage from texture_packer.lua:
        tGimp = require 'gimp_cli_wrapper'
        tGimp.detectGimp()
        tGimp.getPsdLayerInfo('/path/to/file.psd')
        tGimp.buildExportScript('/path/to/file.psd', jobs, '/tmp/meta.lua')

    Module-level state (readable after calls):
        tGimp.gimp        -- {found, path, version, major}
        tGimp.customPath  -- user-supplied executable path

]]--

local M = {}

-- ─── Module state ────────────────────────────────────────────────────────────

M.gimp       = nil   -- cached {found=bool, path=string, version=string, major=int}
M.customPath = nil   -- user-supplied executable path (overrides auto-detection)

-- ─── OS detection ────────────────────────────────────────────────────────────

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

-- ─── GIMP detection ──────────────────────────────────────────────────────────

local PROBE_EXES = {
    linux   = { "gimp", "gimp-2.10", "gimp-3.0", "gimp-3", "gimp-3.2" },
    macos   = { "gimp", "gimp-2.10", "gimp-3.0", "gimp-3", "gimp-3.2" },
    -- Console variants are listed first on Windows: they are compiled with the console
    -- subsystem so --version output is reliably captured by io.popen.  The plain
    -- gimp-3.exe (GUI subsystem) is also included as it is the MSIX-signed entry point
    -- that actually runs from the WindowsApps VFS path without "Access denied".
    windows = {
        "gimp-console.exe", "gimp-console-3.exe", "gimp-console-3.2.exe",
        "gimp-3.exe", "gimp-3.2.exe",
        "gimp.exe", "gimp-2.10.exe", "gimp-3.0.exe",
    },
}

local WINDOWS_GIMP_CANDIDATES = {
    -- GIMP 3.2+ traditional installer paths (prefer console variants for stdout capture)
    "C:\\Program Files\\GIMP 3\\bin\\gimp-console-3.2.exe",
    "C:\\Program Files\\GIMP 3\\bin\\gimp-console-3.exe",
    "C:\\Program Files\\GIMP 3\\bin\\gimp-console.exe",
    "C:\\Program Files\\GIMP 3\\bin\\gimp-3.2.exe",
    "C:\\Program Files\\GIMP 3\\bin\\gimp-3.exe",
    "C:\\Program Files\\GIMP 3\\bin\\gimp-3.0.exe",
    -- GIMP 2.x traditional installer paths
    "C:\\Program Files\\GIMP 2\\bin\\gimp-2.10.exe",
    "C:\\Program Files (x86)\\GIMP 2\\bin\\gimp-2.10.exe",
}

local function tryDetectExe(exe)
    local f = io.popen(wrapCmd(shellQuote(exe) .. " --version") .. " 2>&1")
    local time_stamp = os.date("%Y-%m-%d %H:%M:%S")
    if bPrintDebugCliWrapper then
        print("[gimp_cli] " .. time_stamp .." trying to detect GIMP with command: " .. shellQuote(exe) .. " --version" .. "result: " .. tostring(f))
    end
    if not f then return nil end
    local output = f:read("*a")
    f:close()
    -- "GNU" is a proper noun and is never translated in any locale.
    -- GIMP 2.x English:    "GNU Image Manipulation Program version X.Y.Z"
    -- GIMP 3.x Portuguese: "Programa de manipulação de imagem do GNU versão X.Y.Z"
    -- GIMP 3.x Spanish:    "Programa de manipulación de imágenes de GNU versión X.Y.Z"
    -- All translated outputs still contain "GNU" and a version number.
    -- Avoid false-positives: require BOTH "GNU" AND a digit-dot sequence.
    if output and output:find("GNU") and output:find("%d+%.%d+") then
        -- Extract version after any word meaning "version" in common locales,
        -- or fall back to the first X.Y.Z triplet in the output.
        -- Note: "versão" is UTF-8; Lua patterns work on bytes, so we match
        -- "vers" followed by non-space chars, then a space, then the number.
        local version = output:match("[Vv]ersion%s+([%d%.]+)")
                     or output:match("vers%S*%s+([%d%.]+)")
                     or output:match("(%d+%.%d+%.%d+)")
                     or ""
        local major   = tonumber(version:match("^(%d+)")) or 2
        return { found = true, path = exe, version = version, major = major }
    end
    return nil
end

-- Windows: use 'where.exe' to locate an executable on PATH
-- (also resolves app-execution aliases created by Windows Store installs).
local function whereExe(exe)
    local f = io.popen('where "' .. exe .. '" 2>nul')
    if not f then return nil end
    local line = f:read("*l")
    f:close()
    if line and line ~= "" and not line:lower():find("could not find") then
        return line:match("^%s*(.-)%s*$")
    end
    return nil
end

-- Windows: query MSIX package metadata via PowerShell and locate the GIMP exe.
-- For Windows Store MSIX installs, the GUI-subsystem gimp-3.exe uses AllocConsole /
-- AttachConsole which writes directly to the console window, bypassing the stdout pipe
-- created by io.popen.  Running --version therefore yields empty pipe output even
-- though the text is visible on screen.  We work around this by:
--   1. Reading the GIMP version from the MSIX PackageManager ($p.Version).
--   2. Verifying exe existence on disk with io.open rather than running the exe.
--   3. For console-subsystem builds (gimp-console-*.exe) we still attempt --version
--      capture first, since those write to the pipe correctly.
-- Returns {found=bool, path=string, version=string, major=int} or nil.
local function detectWindowsStoreGimp()
    -- One PowerShell call outputs two lines: InstallLocation then Version.
    local psCmd = 'powershell -NoProfile -NonInteractive -Command ' ..
        '"$p = Get-AppxPackage -Name \"*GIMP*\" | Select-Object -First 1; ' ..
        'if ($p) { $p.InstallLocation; $p.Version }" 2>nul'
    local f = io.popen(psCmd)
    if not f then return nil end
    local out = f:read("*a")
    f:close()

    local loc, pkgVer
    for line in out:gmatch("[^\r\n]+") do
        line = line:match("^%s*(.-)%s*$")
        if line ~= "" and line:lower() ~= "null" then
            if not loc then loc = line
            elseif not pkgVer then pkgVer = line end
        end
    end
    if not loc or loc == "" then return nil end

    -- MSIX package version is "A.B.C.D"; normalise to "A.B.C" for display.
    local pkgVersion = (pkgVer or ""):match("^(%d+%.%d+%.%d+)") or (pkgVer or "")
    local pkgMajor   = tonumber((pkgVer or ""):match("^(%d+)")) or 3

    -- Prefer App Execution Alias stubs. These are zero-byte reparse-point files
    -- (mode -a---l in PowerShell) that io.open cannot open, but where.exe resolves
    -- them correctly since Windows adds the alias dir to PATH for MSIX packages.
    -- They invoke GIMP through the proper MSIX activation context (no Access Denied).
    for _, name in ipairs({
        "gimp-console-3.2.exe", "gimp-console-3.exe", "gimp-console.exe",
        "gimp-3.exe",
    }) do
        local aliasPath = whereExe(name)
        if aliasPath then
            if name:find("console") then
                local r = tryDetectExe(aliasPath)
                if r then return r end
            end
            -- GUI subsystem (gimp-3.exe) or console build with uncapturable output:
            -- use the package version from PackageManager metadata.
            if bPrintDebugCliWrapper then
                print("[gimp_cli] Windows Store GIMP (alias+meta): " .. aliasPath .. " v=" .. pkgVersion)
            end
            return { found = true, path = aliasPath, version = pkgVersion, major = pkgMajor }
        end
    end

    -- Fallback: VFS path (readable but execution-denied for regular users on most setups).
    local binDir = loc .. "\\VFS\\ProgramFilesX64\\GIMP\\bin"
    for _, name in ipairs({
        "gimp-console-3.2.exe", "gimp-console-3.exe", "gimp-console.exe",
        "gimp-3.exe",
    }) do
        local exePath = binDir .. "\\" .. name
        local probe = io.open(exePath, "rb")
        if probe then
            probe:close()
            if name:find("console") then
                local r = tryDetectExe(exePath)
                if r then return r end
            end
            if bPrintDebugCliWrapper then
                print("[gimp_cli] Windows Store GIMP (pkg meta): " .. exePath .. " v=" .. pkgVersion)
            end
            return { found = true, path = exePath, version = pkgVersion, major = pkgMajor }
        end
    end
    return nil
end

-- Windows: enumerate C:\Program Files\WindowsApps for a Store-installed GIMP.
-- NOTE: This usually returns nothing for standard (non-admin) users because
-- WindowsApps denies LIST_DIRECTORY.  Kept as a last-resort fallback.
local function findWindowsStoreGimpPaths()
    local results = {}
    local base = "C:\\Program Files\\WindowsApps"
    local f = io.popen('cmd /c dir /b /ad "' .. base .. '\\GIMP*" 2>nul')
    if not f then return results end
    for line in f:lines() do
        line = line:match("^%s*(.-)%s*$")
        if line and line ~= "" then
            local binDir = base .. "\\" .. line .. "\\VFS\\ProgramFilesX64\\GIMP\\bin"
            for _, exe in ipairs({
                "gimp-console-3.2.exe", "gimp-console-3.exe", "gimp-console.exe",
                "gimp-3.exe", "gimp-3.2.exe", "gimp-3.0.exe", "gimp-2.10.exe",
            }) do
                table.insert(results, binDir .. "\\" .. exe)
            end
        end
    end
    f:close()
    return results
end

-- Windows: look for GIMP app execution alias stubs in %LOCALAPPDATA%\Microsoft\WindowsApps\.
-- Windows Store MSIX apps (e.g. GIMP from the Microsoft Store) register lightweight alias
-- stubs here.  These stubs invoke the app through the proper MSIX activation context;
-- running the physical exe under C:\Program Files\WindowsApps\...\VFS\... bypasses that
-- context and typically produces no usable output (access denied or empty stdout).
local function findWindowsAppExecutionAliases()
    local results = {}
    local localAppData = os.getenv("LOCALAPPDATA") or ""
    if localAppData == "" then return results end
    local aliasDir = localAppData .. "\\Microsoft\\WindowsApps"
    for _, name in ipairs({
        "gimp-console.exe", "gimp-console-3.exe", "gimp-console-3.2.exe",
        "gimp-3.exe", "gimp-3.2.exe",
        "gimp.exe", "gimp-3.0.exe", "gimp-2.10.exe",
    }) do
        table.insert(results, aliasDir .. "\\" .. name)
    end
    return results
end

-- Detects whether GIMP is installed.
-- Probes (in order): M.customPath → platform-appropriate exe names on PATH
-- → Windows common install dirs.
-- Caches the result in M.gimp; subsequent calls return the cached value.
-- Returns: {found=bool, path=string, version=string, major=int}
function M.detectGimp()
    if M.gimp ~= nil then return M.gimp end

    if M.customPath and M.customPath ~= "" then
        local r = tryDetectExe(M.customPath)
        if r then M.gimp = r; return M.gimp end
    end

    local os_name = M.getOS()
    local probes  = PROBE_EXES[os_name] or PROBE_EXES.linux
    for _, exe in ipairs(probes) do
        local r = tryDetectExe(exe)
        if r then M.gimp = r; return M.gimp end
    end

    if os_name == "windows" then
        -- 1. Windows Store MSIX package: read version from PackageManager metadata and
        --    verify exe existence on disk.  MSIX GUI builds (gimp-3.exe) use AllocConsole
        --    which writes version output directly to the console, bypassing the io.popen
        --    pipe, so --version capture is unreliable for this install type.
        local storeResult = detectWindowsStoreGimp()
        if storeResult then M.gimp = storeResult; return M.gimp end
        -- 2. App execution aliases (%LOCALAPPDATA%\Microsoft\WindowsApps):
        --    MSIX Store apps register alias stubs here that invoke GIMP through
        --    the proper MSIX activation context.  Running the physical exe under
        --    C:\Program Files\WindowsApps\...\VFS\... does not work reliably.
        for _, candidate in ipairs(findWindowsAppExecutionAliases()) do
            local probe = io.open(candidate, "r")
            if probe then
                probe:close()
                local r = tryDetectExe(candidate)
                if r then M.gimp = r; return M.gimp end
            end
        end
        -- 3. 'where.exe' finds executables on PATH; on Windows 10/11 this also
        --    resolves App Execution Aliases when they are enabled by the user.
        for _, exe in ipairs({
            "gimp-console.exe", "gimp-console-3.exe", "gimp-console-3.2.exe",
            "gimp-3.exe", "gimp-3.2.exe",
            "gimp.exe", "gimp-3.0.exe", "gimp-2.10.exe",
        }) do
            local path = whereExe(exe)
            if path then
                local r = tryDetectExe(path)
                if r then M.gimp = r; return M.gimp end
            end
        end
        -- 4. Traditional installer paths (GIMP 2/3 via official .exe installer).
        for _, candidate in ipairs(WINDOWS_GIMP_CANDIDATES) do
            local probe = io.open(candidate, "r")
            if probe then
                probe:close()
                local r = tryDetectExe(candidate)
                if r then M.gimp = r; return M.gimp end
            end
        end
        -- 5. Dynamic WindowsApps dir enumeration (requires admin; usually a no-op).
        for _, candidate in ipairs(findWindowsStoreGimpPaths()) do
            local r = tryDetectExe(candidate)
            if r then M.gimp = r; return M.gimp end
        end
    end

    M.gimp = { found = false, path = "", version = "", major = 0 }
    return M.gimp
end

function M.resetDetection()
    M.gimp = nil
end

function M.setCustomPath(path)
    M.customPath = path
    M.gimp       = nil
end

-- ─── Path helpers ─────────────────────────────────────────────────────────────

function M.getFileBaseStem(path)
    local name = path:match("[/\\]([^/\\]+)$") or path
    return name:match("(.+)%.[^%.]+$") or name
end

function M.getFileDir(path)
    return path:match("(.*[/\\])") or "./"
end

-- ─── Temp directory ───────────────────────────────────────────────────────────

local function tmpDir()
    return os.getenv("TMPDIR") or os.getenv("TEMP") or os.getenv("TMP") or "/tmp"
end

-- ─── Script-Fu helpers ───────────────────────────────────────────────────────

-- Escape a path string for embedding inside a Scheme string literal.
local function scmStr(s)
    -- Backslashes must be doubled; double-quotes escaped.
    s = s:gsub("\\", "\\\\")
    s = s:gsub('"', '\\"')
    return '"' .. s .. '"'
end

-- ─── Info phase ──────────────────────────────────────────────────────────────

-- Builds a Script-Fu .scm file that opens the PSD, traverses all layers
-- recursively, and writes a pipe-delimited metadata file.
--
-- Each line in the output file has the format:
--   GIMPID|name|offsetX|offsetY|width|height|visible|isGroup|groupPath
--
-- The file ends with the sentinel line:   -- END
--
-- Returns {scriptPath, metaPath, cmd}
function M.buildInfoScript(psdPath)
    local td         = tmpDir()
    local stem       = M.getFileBaseStem(psdPath)
    local scriptPath = td .. "/gimp_info_" .. stem .. "_" .. tostring(os.time()) .. ".scm"
    local metaPath   = td .. "/gimp_info_" .. stem .. "_" .. tostring(os.time()) .. ".lua"

    local gimp3 = (M.gimp and M.gimp.major or 2) >= 3
    local scm

    if gimp3 then
        -- GIMP 3.x: Script-Fu's plug-in-script-fu-eval has a broken IMAGE type
        -- bridge — gimp-file-load returns GObject cells that legacy PDB functions
        -- reject as "non-numeric", and key helpers (gimp-image-get-id,
        -- gimp-image-list, open-append-file) are simply unbound.  Switch to
        -- Python-Fu which uses GObject Introspection and works correctly.
        scriptPath = td .. "/gimp_info_" .. stem .. "_" .. tostring(os.time()) .. ".py"
        local function pyStr(s)
            s = s:gsub("\\", "/")   -- forward slashes work on Windows in Python
            s = s:gsub("'", "\\'")  -- escape single quotes
            return "'" .. s .. "'"
        end
        scm = string.format([[
import gi
gi.require_version('Gimp', '3.0')
from gi.repository import Gimp, Gio

def get_offsets(layer):
    o = layer.get_offsets()
    if hasattr(o, 'offset_x'):
        return o.offset_x, o.offset_y
    return int(o[0]), int(o[1])

def traverse(layer, group_path, out):
    name     = layer.get_name()
    ox, oy   = get_offsets(layer)
    w        = layer.get_width()
    h        = layer.get_height()
    visible  = 1 if layer.get_visible() else 0
    is_group = 1 if layer.is_group() else 0
    layer_id = layer.get_id()
    my_path  = name if not group_path else (group_path + '/' + name)
    out.write('{}|{}|{}|{}|{}|{}|{}|{}|{}\n'.format(
        layer_id, name, ox, oy, w, h, visible, is_group, group_path))
    if is_group:
        for child in layer.get_children():
            traverse(child, my_path, out)

image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(%s))
with open(%s, 'w', encoding='utf-8') as out:
    out.write('IMAGE|{}|{}\n'.format(image.get_width(), image.get_height()))
    for layer in image.get_layers():
        traverse(layer, '', out)
    out.write('-- END\n')
image.delete()
]], pyStr(psdPath), pyStr(metaPath))
    else
        -- GIMP 2.x: all PDB returns are wrapped in lists (need car for single values).
        -- gimp-image-get-layers / gimp-item-get-children return (count #(ids...)).
        scm = string.format([[
(let* (
  (image (car (gimp-file-load RUN-NONINTERACTIVE %s %s)))
  (port  (open-output-file %s))
)
  (define (traverse item-id group-path)
    (let* (
      (name     (car (gimp-item-get-name item-id)))
      (offsets  (gimp-drawable-offsets item-id))
      (ox       (car offsets))
      (oy       (cadr offsets))
      (w        (car (gimp-drawable-width item-id)))
      (h        (car (gimp-drawable-height item-id)))
      (visible  (car (gimp-item-get-visible item-id)))
      (is-group (car (gimp-item-is-group item-id)))
      (my-path  (if (string=? group-path "") name (string-append group-path "/" name)))
    )
      (display
        (string-append
          (number->string item-id) "|"
          name "|"
          (number->string ox) "|"
          (number->string oy) "|"
          (number->string w) "|"
          (number->string h) "|"
          (if (= visible TRUE) "1" "0") "|"
          (if (= is-group TRUE) "1" "0") "|"
          group-path
          "\n"
        )
        port
      )
      (when (= is-group TRUE)
        (let ((children (gimp-item-get-children item-id)))
          (for-each
            (lambda (child-id) (traverse child-id my-path))
            (vector->list (cadr children))
          )
        )
      )
    )
  )
  (let* ((top-layers (gimp-image-get-layers image)))
    (display
      (string-append
        "IMAGE|"
        (number->string (car (gimp-image-width image))) "|"
        (number->string (car (gimp-image-height image))) "\n"
      )
      port
    )
    (for-each
      (lambda (item-id) (traverse item-id ""))
      (vector->list (cadr top-layers))
    )
  )
  (display "-- END\n" port)
  (close-output-port port)
  (gimp-image-delete image)
)
]], scmStr(psdPath), scmStr(psdPath), scmStr(metaPath))
    end

    local f = io.open(scriptPath, "w")
    if not f then return nil end
    f:write(scm)
    f:close()

    -- GIMP 3.x uses Python-Fu (python-fu-eval); Gimp.quit(0) is in the script.
    -- GIMP 2.x uses Script-Fu with a second -b arg for (gimp-quit 0).
    local cmd
    if gimp3 then
        local pyPath = scriptPath:gsub("\\", "/"):gsub("'", "\\'")
        cmd = string.format(
            "%s -i --batch-interpreter python-fu-eval -b %s --quit",
            shellQuote(M.gimp and M.gimp.path or "gimp"),
            shellQuote("exec(open('" .. pyPath .. "').read())")
        )
    else
        cmd = string.format(
            "%s -i -b %s -b %s",
            shellQuote(M.gimp and M.gimp.path or "gimp"),
            shellQuote("(load " .. scmStr(scriptPath) .. ")"),
            shellQuote("(gimp-quit 0)")
        )
    end

    return { scriptPath = scriptPath, metaPath = metaPath, cmd = cmd }
end

-- Parses the pipe-delimited metadata file written by the info Script-Fu.
-- Returns layers, psdWidth, psdHeight
-- layers: [{gimpId, name, offsetX, offsetY, width, height, visible, isGroup, groupPath}]
function M.parseMetaFile(metaPath)
    local layers = {}
    local psdW, psdH = 0, 0
    local f = io.open(metaPath, "r")
    if not f then return layers, psdW, psdH end
    for line in f:lines() do
        if line == "-- END" then break end
        -- IMAGE dimensions header (written as first line by buildInfoScript)
        local iw, ih = line:match("^IMAGE|(%d+)|(%d+)$")
        if iw then
            psdW = tonumber(iw) or 0
            psdH = tonumber(ih) or 0
        else
            local id, name, ox, oy, w, h, vis, isGrp, grpPath =
                line:match("^(%d+)|([^|]*)|(-?%d+)|(-?%d+)|(%d+)|(%d+)|([01])|([01])|(.*)")
            if id then
                table.insert(layers, {
                    gimpId    = tonumber(id),
                    name      = name,
                    offsetX   = tonumber(ox),
                    offsetY   = tonumber(oy),
                    width     = tonumber(w),
                    height    = tonumber(h),
                    visible   = (vis == "1"),
                    isGroup   = (isGrp == "1"),
                    groupPath = grpPath or "",
                })
            end
        end
    end
    f:close()
    return layers, psdW, psdH
end

-- Checks whether the metadata file is complete (sentinel present).
function M.metaFileExists(path)
    local f = io.open(path, "r")
    if not f then return false end
    -- Read all content and look for the sentinel.
    local content = f:read("*a")
    f:close()
    return content ~= nil and content:find("-- END", 1, true) ~= nil
end

-- Returns the number of successfully exported images written so far
-- (counts non-sentinel, non-empty lines in the meta file).
function M.countExportedSoFar(path)
    local f = io.open(path, "r")
    if not f then return 0 end
    local n = 0
    for line in f:lines() do
        if line == "-- END" then break end
        if line ~= "" then n = n + 1 end
    end
    f:close()
    return n
end

-- Synchronous info call: build the script, run GIMP, parse the output.
-- Blocks for ~3-5 s on first call. Returns the same array as parseMetaFile.
function M.getPsdLayerInfo(psdPath)
    local g = M.gimp or M.detectGimp()
    if bPrintDebugCliWrapper then
        print("[gimp_cli] detectGimp: found=" .. tostring(g.found) .. " path=" .. tostring(g.path) .. " version=" .. tostring(g.version))
    end
    if not g.found then
        print("[gimp_cli] GIMP not found, aborting layer info scan")
        return {}
    end

    local info = M.buildInfoScript(psdPath)
    if not info then
        print("[gimp_cli] buildInfoScript returned nil (could not write temp .scm)")
        return {}
    end
    if bPrintDebugCliWrapper then
        print("[gimp_cli] script: " .. info.scriptPath)
        print("[gimp_cli] meta:   " .. info.metaPath)
        print("[gimp_cli] cmd:    " .. info.cmd)
    end

    -- Run synchronously (blocking). Capture and print GIMP's stdout/stderr.
    local fullCmd = wrapCmd(info.cmd) .. " 2>&1"
    if bPrintDebugCliWrapper then
        print("[gimp_cli] running: " .. fullCmd)
    end
    local f = io.popen(fullCmd)
    local gimpOutput = ""
    if f then
        gimpOutput = f:read("*a") or ""
        f:close()
    end
    if bPrintDebugCliWrapper then
        if gimpOutput ~= "" then
            print("[gimp_cli] GIMP output:\n" .. gimpOutput)
        else
            print("[gimp_cli] GIMP produced no output")
        end
    end

    -- io.popen + read("*a") already blocked until GIMP exited, so the meta
    -- file is either there now or never.  Give the filesystem a 2-second grace
    -- period for slow network/antivirus flushes, then give up immediately.
    local deadline = os.time() + 2
    while not M.metaFileExists(info.metaPath) and os.time() < deadline do end

    if not M.metaFileExists(info.metaPath) then
        if bPrintDebugCliWrapper then
            print("[gimp_cli] GIMP exited without producing the meta file (script error)")
            print("[gimp_cli] Script kept for inspection: " .. info.scriptPath)
        end
        if bPrintDebugCliWrapper then
            local sf = io.open(info.scriptPath, "r")
            if sf then
                print("[gimp_cli] --- generated script ---")
                print(sf:read("*a"))
                print("[gimp_cli] --- end script ---")
                sf:close()
            end
        end
        return {}
    end
    if bPrintDebugCliWrapper then
        print("[gimp_cli] meta file ready: " .. info.metaPath)
    end

    -- Print first few lines of the meta file for inspection.
    if bPrintDebugCliWrapper then
        local dbg = io.open(info.metaPath, "r")
        if dbg then
            local n = 0
            for line in dbg:lines() do
                print("[gimp_cli] meta[" .. n .. "]: " .. line)
                n = n + 1
                if n >= 5 then print("[gimp_cli] meta[...]: (truncated)"); break end
            end
            dbg:close()
        end
    end

    local layers, psdW, psdH = M.parseMetaFile(info.metaPath)
    if bPrintDebugCliWrapper then
        print("[gimp_cli] parsed " .. #layers .. " layer(s), PSD canvas " .. psdW .. "x" .. psdH)
    end
    os.remove(info.scriptPath)
    os.remove(info.metaPath)
    return layers, psdW, psdH
end

-- ─── Export phase ─────────────────────────────────────────────────────────────

-- Builds a Script-Fu .scm file that re-opens the PSD and exports each
-- requested layer (or merged group) as a PNG.
--
-- jobs: [{gimpId=int, outputPath=string, width=int, height=int, isMerge=bool}]
--
-- After all exports the script writes a pipe-delimited result metadata file:
--   outputPath|name|offsetX|offsetY
-- ending with sentinel:   -- END
--
-- Returns {scriptPath, metaPath, cmd}
function M.buildExportScript(psdPath, jobs, metaOutPath)
    local td         = tmpDir()
    local stem       = M.getFileBaseStem(psdPath)
    local gimp3 = (M.gimp and M.gimp.major or 2) >= 3
    local scriptPath
    if gimp3 then
        scriptPath = td .. "/gimp_export_" .. stem .. "_" .. tostring(os.time()) .. ".py"
    else
        scriptPath = td .. "/gimp_export_" .. stem .. "_" .. tostring(os.time()) .. ".scm"
    end

    local function pyStr(s)
        s = s:gsub("\\", "/")
        s = s:gsub("'", "\\'")
        return "'" .. s .. "'"
    end

    -- Build the per-job export calls.
    local jobLines = {}
    for _, job in ipairs(jobs) do
        if gimp3 then
            -- GIMP 3.x Python: new_from_drawable flattens group layers automatically,
            -- so merge and regular layers use the same export_item() code path.
            table.insert(jobLines, string.format(
                "    export_item(Gimp.Item.get_by_id(%d), %s, %d, %d, out)\n",
                job.gimpId, pyStr(job.outputPath), job.width, job.height))
        else
            if job.isMerge then
                table.insert(jobLines, string.format([[
  (let* (
    (grp-layer  %d)
    (new-img    (car (gimp-image-new %d %d RGB)))
    (flat-layer (car (gimp-layer-new-from-drawable grp-layer new-img)))
  )
    (gimp-image-insert-layer new-img flat-layer 0 -1)
    (when (and (> %d 0) (> %d 0))
      (gimp-layer-scale flat-layer %d %d FALSE)
      (gimp-image-resize-to-layers new-img)
    )
    (file-png-save RUN-NONINTERACTIVE new-img flat-layer %s %s 0 9 1 1 1 1 1)
    (display (string-append %s "|" (car (gimp-item-get-name grp-layer)) "|"
      (number->string (car (gimp-drawable-offsets grp-layer))) "|"
      (number->string (cadr (gimp-drawable-offsets grp-layer))) "\n") port)
    (gimp-image-delete new-img)
  )
]], job.gimpId, job.width, job.height,
    job.width, job.height, job.width, job.height,
    scmStr(job.outputPath), scmStr(M.getFileBaseStem(job.outputPath)),
    scmStr(job.outputPath)))
            else
                table.insert(jobLines, string.format([[
  (let* (
    (layer      %d)
    (lw         (car (gimp-drawable-width layer)))
    (lh         (car (gimp-drawable-height layer)))
    (new-img    (car (gimp-image-new lw lh RGB)))
    (new-layer  (car (gimp-layer-new-from-drawable layer new-img)))
  )
    (gimp-image-insert-layer new-img new-layer 0 -1)
    (when (and (> %d 0) (> %d 0))
      (gimp-layer-scale new-layer %d %d FALSE)
      (gimp-image-resize-to-layers new-img)
    )
    (file-png-save RUN-NONINTERACTIVE new-img new-layer %s %s 0 9 1 1 1 1 1)
    (display (string-append %s "|" (car (gimp-item-get-name layer)) "|"
      (number->string (car (gimp-drawable-offsets layer))) "|"
      (number->string (cadr (gimp-drawable-offsets layer))) "\n") port)
    (gimp-image-delete new-img)
  )
]], job.gimpId,
    job.width, job.height, job.width, job.height,
    scmStr(job.outputPath), scmStr(M.getFileBaseStem(job.outputPath)),
    scmStr(job.outputPath)))
            end
        end
    end

    local scm
    if gimp3 then
        scm = string.format([[
import gi
gi.require_version('Gimp', '3.0')
from gi.repository import Gimp, Gio
import os

def get_offsets(item):
    o = item.get_offsets()
    if hasattr(o, 'offset_x'):
        return o.offset_x, o.offset_y
    return int(o[0]), int(o[1])

def save_as_png(img, drawable, path):
    file_obj = Gio.File.new_for_path(path)
    # GIMP 3.2+: Gimp.file_save(run_mode, image, file, options)
    # Drawable argument was removed from the signature in GIMP 3.2.
    if hasattr(Gimp, 'file_save'):
        Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, img, file_obj, None)
        return
    # Fallback: Gimp.file_overwrite (older GIMP 3.x builds)
    if hasattr(Gimp, 'file_overwrite'):
        Gimp.file_overwrite(Gimp.RunMode.NONINTERACTIVE, img, file_obj, None)
        return
    raise RuntimeError('No PNG save method; attrs: ' + str([a for a in dir(Gimp) if any(k in a.lower() for k in ('file', 'export', 'save'))]))

def export_item(item, out_path, width, height, out):
    os.makedirs(os.path.dirname(out_path) or '.', exist_ok=True)
    ox, oy = get_offsets(item)
    name = item.get_name()
    lw = item.get_width()
    lh = item.get_height()
    new_img = Gimp.Image.new(lw, lh, Gimp.ImageBaseType.RGB)
    new_layer = Gimp.Layer.new_from_drawable(item, new_img)
    new_img.insert_layer(new_layer, None, -1)
    if width > 0 and height > 0:
        new_layer.scale(width, height, False)
        new_img.resize_to_layers()
    save_as_png(new_img, new_layer, out_path)
    new_img.delete()
    out.write('{}|{}|{}|{}\n'.format(out_path, name, ox, oy))
    out.flush()  # flush after each item so async progress polling works

image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(%s))
with open(%s, 'w', encoding='utf-8') as out:
%s    out.write('-- END\n')
image.delete()
]], pyStr(psdPath), pyStr(metaOutPath), table.concat(jobLines, ""))
    else
        local outerLoad = string.format(
            "(let* (\n  (image (car (gimp-file-load RUN-NONINTERACTIVE %s %s)))",
            scmStr(psdPath), scmStr(psdPath))
        scm = string.format([[
%s
  (port  (open-output-file %s))
)
%s
  (display "-- END\n" port)
  (close-output-port port)
  (gimp-image-delete image)
)
]], outerLoad, scmStr(metaOutPath), table.concat(jobLines, "\n"))
    end

    local f = io.open(scriptPath, "w")
    if not f then return nil end
    f:write(scm)
    f:close()

    -- GIMP 3.x uses Python-Fu (python-fu-eval); GIMP 2.x uses Script-Fu.
    local cmd
    if gimp3 then
        local pyPath = scriptPath:gsub("\\", "/"):gsub("'", "\\'")
        cmd = string.format(
            "%s -i --batch-interpreter python-fu-eval -b %s --quit",
            shellQuote(M.gimp and M.gimp.path or "gimp"),
            shellQuote("exec(open('" .. pyPath .. "').read())")
        )
    else
        cmd = string.format(
            "%s -i -b %s -b %s",
            shellQuote(M.gimp and M.gimp.path or "gimp"),
            shellQuote("(load " .. scmStr(scriptPath) .. ")"),
            shellQuote("(gimp-quit 0)")
        )
    end

    return { scriptPath = scriptPath, metaPath = metaOutPath, cmd = cmd }
end

-- ─── Export result reader ─────────────────────────────────────────────────────

-- Reads the pipe-delimited export metadata file produced by buildExportScript.
-- Returns [{outputPath, name, offsetX, offsetY}]
function M.loadExportMeta(metaPath)
    local results = {}
    local f = io.open(metaPath, "r")
    if not f then return results end
    for line in f:lines() do
        if line == "-- END" then break end
        local outPath, name, ox, oy = line:match("^([^|]+)|([^|]*)|(-?%d+)|(-?%d+)")
        if outPath then
            table.insert(results, {
                outputPath = outPath,
                name       = name,
                offsetX    = tonumber(ox),
                offsetY    = tonumber(oy),
            })
        end
    end
    f:close()
    return results
end

-- ─── Async launch ────────────────────────────────────────────────────────────

function M.launchExportAsync(cmd)
    if M.getOS() == "windows" then
        os.execute('cmd /c start /b "" ' .. cmd)
    else
        os.execute(cmd .. ' >/dev/null 2>&1 &')
    end
end

-- ─── Synchronous export (with full output capture for debugging) ──────────────

-- Runs the export script synchronously (blocking), captures GIMP stdout/stderr,
-- and logs everything. Returns true if the meta sentinel was written.
function M.runExportSync(info)
    -- Log the script content so failures can be diagnosed.
    if bPrintDebugCliWrapper then
        print("[gimp_cli] export script: " .. info.scriptPath)
        print("[gimp_cli] export meta:   " .. info.metaPath)
        print("[gimp_cli] export cmd:    " .. info.cmd)
    end
    if bPrintDebugCliWrapper then
        local sf = io.open(info.scriptPath, "r")
        if sf then
            print("[gimp_cli] --- export script content ---")
            print(sf:read("*a"))
            print("[gimp_cli] --- end export script ---")
            sf:close()
        end
    end

    local fullCmd = wrapCmd(info.cmd) .. " 2>&1"
    if bPrintDebugCliWrapper then
        print("[gimp_cli] running export: " .. fullCmd)
    end
    local f = io.popen(fullCmd)
    local gimpOutput = ""
    if f then
        gimpOutput = f:read("*a") or ""
        f:close()
    end
    if bPrintDebugCliWrapper then
        if gimpOutput ~= "" then
            print("[gimp_cli] GIMP export output:\n" .. gimpOutput)
        else
            print("[gimp_cli] GIMP export produced no output")
        end
    end

    -- Brief grace period for filesystem flush.
    local deadline = os.time() + 2
    while not M.metaFileExists(info.metaPath) and os.time() < deadline do end

    if not M.metaFileExists(info.metaPath) then
        print("[gimp_cli] GIMP export exited without producing the meta file")
        print("[gimp_cli] Script kept for inspection: " .. info.scriptPath)
        return false
    end

    -- Log first few result lines.
    if bPrintDebugCliWrapper then
        local dbg = io.open(info.metaPath, "r")
        if dbg then
            local n = 0
            for line in dbg:lines() do
                print("[gimp_cli] export meta[" .. n .. "]: " .. line)
                n = n + 1
                if n >= 5 then print("[gimp_cli] export meta[...]: (truncated)"); break end
            end
            dbg:close()
        end
    end
    return true
end

-- ─── Sidecar writers ─────────────────────────────────────────────────────────

-- Writes a Lua-table sidecar file: return { {name=…, offsetX=…, …}, … }
-- entries: [{outputPath, name, offsetX, offsetY}]
function M.writeLuaMeta(path, entries)
    local f = io.open(path, "w")
    if not f then return false end
    f:write("return {\n")
    for _, e in ipairs(entries) do
        -- Escape backslashes and quotes inside strings.
        local function esc(s)
            return s:gsub("\\", "\\\\"):gsub('"', '\\"')
        end
        f:write(string.format(
            '    { name="%s", outputPath="%s", offsetX=%d, offsetY=%d },\n',
            esc(e.name or ""), esc(e.outputPath or ""),
            e.offsetX or 0, e.offsetY or 0
        ))
    end
    f:write("}\n")
    f:close()
    return true
end

-- Writes a JSON sidecar file: [{…}, …]
-- entries: [{outputPath, name, offsetX, offsetY}]
function M.writeJsonMeta(path, entries)
    local f = io.open(path, "w")
    if not f then return false end

    -- Minimal JSON string escaper — no external lib needed.
    local function jsonStr(s)
        s = tostring(s or "")
        s = s:gsub('\\', '\\\\')
        s = s:gsub('"',  '\\"')
        s = s:gsub('\n', '\\n')
        s = s:gsub('\r', '\\r')
        s = s:gsub('\t', '\\t')
        return '"' .. s .. '"'
    end

    f:write("[\n")
    for i, e in ipairs(entries) do
        f:write(string.format(
            '    { "name": %s, "outputPath": %s, "offsetX": %d, "offsetY": %d }',
            jsonStr(e.name), jsonStr(e.outputPath),
            e.offsetX or 0, e.offsetY or 0
        ))
        if i < #entries then f:write(",") end
        f:write("\n")
    end
    f:write("]\n")
    f:close()
    return true
end

-- ─── Cleanup ─────────────────────────────────────────────────────────────────

function M.cleanupTempFiles(paths)
    for _, p in ipairs(paths) do
        if p then os.remove(p) end
    end
end

-- ─── PNG completion check (same pattern as imagick_cli_wrapper) ───────────────

-- Returns true only when the file at `path` is a complete, fully-written PNG.
function M.fileExists(path)
    local f = io.open(path, "rb")
    if not f then return false end
    local ok = f:seek("end", -12)
    if not ok then f:close(); return false end
    local tail = f:read(12)
    f:close()
    return tail ~= nil and tail:sub(5, 8) == "IEND"
end

return M
