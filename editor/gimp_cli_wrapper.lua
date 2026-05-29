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
    linux   = { "gimp", "gimp-2.10", "gimp-3.0" },
    macos   = { "gimp", "gimp-2.10", "gimp-3.0" },
    windows = { "gimp-2.10.exe", "gimp-3.0.exe" },
}

local WINDOWS_GIMP_CANDIDATES = {
    "C:\\Program Files\\GIMP 2\\bin\\gimp-2.10.exe",
    "C:\\Program Files\\GIMP 3\\bin\\gimp-3.0.exe",
    "C:\\Program Files (x86)\\GIMP 2\\bin\\gimp-2.10.exe",
}

local function tryDetectExe(exe)
    local f = io.popen(wrapCmd(shellQuote(exe) .. " --version") .. " 2>&1")
    if not f then return nil end
    local output = f:read("*a")
    f:close()
    if output and (output:find("GIMP") or output:find("gimp")) then
        local version = output:match("version%s+([%d%.]+)") or ""
        local major   = tonumber(version:match("^(%d+)")) or 2
        return { found = true, path = exe, version = version, major = major }
    end
    return nil
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
        for _, candidate in ipairs(WINDOWS_GIMP_CANDIDATES) do
            local probe = io.open(candidate, "r")
            if probe then
                probe:close()
                local r = tryDetectExe(candidate)
                if r then M.gimp = r; return M.gimp end
            end
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

    local scm = string.format([[
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

    local f = io.open(scriptPath, "w")
    if not f then return nil end
    f:write(scm)
    f:close()

    local cmd = string.format(
        "%s -i -b %s -b %s",
        shellQuote(M.gimp and M.gimp.path or "gimp"),
        shellQuote("(load " .. scmStr(scriptPath) .. ")"),
        shellQuote("(gimp-quit 0)")
    )

    return { scriptPath = scriptPath, metaPath = metaPath, cmd = cmd }
end

-- Parses the pipe-delimited metadata file written by the info Script-Fu.
-- Returns [{gimpId, name, offsetX, offsetY, width, height, visible, isGroup, groupPath}]
function M.parseMetaFile(metaPath)
    local layers = {}
    local f = io.open(metaPath, "r")
    if not f then return layers end
    for line in f:lines() do
        if line == "-- END" then break end
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
    f:close()
    return layers
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

-- Synchronous info call: build the script, run GIMP, parse the output.
-- Blocks for ~3-5 s on first call. Returns the same array as parseMetaFile.
function M.getPsdLayerInfo(psdPath)
    local g = M.gimp or M.detectGimp()
    if not g.found then return {} end

    local info = M.buildInfoScript(psdPath)
    if not info then return {} end

    -- Run synchronously (blocking).
    local f = io.popen(wrapCmd(info.cmd) .. " 2>&1")
    if f then f:read("*a"); f:close() end

    -- Poll up to 30 s for the meta file (GIMP may still be writing on slow machines).
    local deadline = os.time() + 30
    while not M.metaFileExists(info.metaPath) and os.time() < deadline do
        -- tiny busy-wait — acceptable since this is a one-off synchronous call
    end

    local layers = M.parseMetaFile(info.metaPath)
    os.remove(info.scriptPath)
    os.remove(info.metaPath)
    return layers
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
    local scriptPath = td .. "/gimp_export_" .. stem .. "_" .. tostring(os.time()) .. ".scm"

    -- Build the per-job export calls.
    local jobLines = {}
    for _, job in ipairs(jobs) do
        if job.isMerge then
            -- Merge-group: flatten all visible children of the group into a new image.
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
            -- Leaf layer: copy to a new image sized to the layer.
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

    local scm = string.format([[
(let* (
  (image (car (gimp-file-load RUN-NONINTERACTIVE %s %s)))
  (port  (open-output-file %s))
)
%s
  (display "-- END\n" port)
  (close-output-port port)
  (gimp-image-delete image)
)
]], scmStr(psdPath), scmStr(psdPath), scmStr(metaOutPath), table.concat(jobLines, "\n"))

    local f = io.open(scriptPath, "w")
    if not f then return nil end
    f:write(scm)
    f:close()

    local cmd = string.format(
        "%s -i -b %s -b %s",
        shellQuote(M.gimp and M.gimp.path or "gimp"),
        shellQuote("(load " .. scmStr(scriptPath) .. ")"),
        shellQuote("(gimp-quit 0)")
    )

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
