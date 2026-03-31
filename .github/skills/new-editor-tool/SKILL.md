---
name: new-editor-tool
description: "Step-by-step workflow for creating a new Lua-based editor tool in mini-mbm. Use when: adding a new editor to the editor/ directory, creating an ImGui-backed interactive tool, wiring a new editor into the launcher menu, using editor_utils.lua / lang/language.lua patterns, saving/loading editor state as Lua code."
---

# New Editor Tool — mini-mbm

## When to Use

- Creating a new Lua script in `editor/` that becomes a full editor tool
- Adding a new entry to the launcher's application menu
- Building any ImGui-backed interactive tool (asset browser, converter, visualizer, etc.)
- Adding localized strings to `editor/lang/language.lua`
- Understanding how existing editors are structured (`sprite_maker.lua`, `scene_editor2d.lua`, etc.)

---

## Architecture Overview

Editor tools are **standalone Lua scripts** loaded by the `mini-mbm` executable (Lua mode, `-DUSE_LUA=1`).  
They are **not** loadable as `require` modules — the engine calls them as the initial scene script.

```
editor/
├── <your_tool>.lua        ← your new editor script
├── editor_utils.lua       ← shared utility library (auto-required by editor_utils)
├── lang/
│   └── language.lua       ← shared localization module (auto-required by editor_utils)
├── shaders/
│   └── shader_cfg.lua     ← shared shader definitions (loaded manually if needed)
├── sprite_maker.lua       ← reference implementation
├── scene_editor2d.lua     ← reference implementation
└── ...
```

The launcher (e.g. `platform-linux/main-lua.cpp`) presents a selection dialog listing all registered editor scripts. To appear in that list your script must be **added to the `default_applications` array** — see Step 5.

---

## Lifecycle Callbacks

The engine calls these Lua globals in your script:

| Function | When called |
|---|---|
| `onInitScene()` | Once when the scene is first loaded |
| `loop(delta)` | Every frame — **all ImGui calls must happen here** |
| `onTouchDown(key,x,y)` | Mouse button / touch press |
| `onTouchMove(key,x,y)` | Mouse / touch move |
| `onTouchUp(key,x,y)` | Mouse button / touch release |
| `onTouchZoom(zoom)` | Scroll wheel (+1/-1) |
| `onKeyDown(key)` | Keyboard key press |
| `onKeyUp(key)` | Keyboard key release |

**Important**: There is no `onLogicScene`. The per-frame callback is `loop(delta)`.  
All ImGui rendering and state logic lives inside `loop(delta)`.

---

## Minimum Required Structure

```lua
--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by <Your Name>  <your@email.com>                                                               |
| ...license boilerplate...                                                                                              |
|------------------------------------------------------------------------------------------------------------------------|

   <Tool Name> Editor

   This is a script based on mbm engine.

   <One-line description of what this tool does.>

   More info at: https://mbm-documentation.readthedocs.io/en/latest/
]]--

tImGui        = require "ImGui"
tUtil         = require "editor_utils"
-- tLang is auto-populated by editor_utils (require "lang.language")

function onInitScene()
    camera2d                = mbm.getCamera("2d")
    bEnableMoveWorld        = false
    bEnableMoveWindow       = false
    ImGuiWindowFlags_NoMove = tImGui.Flags('ImGuiWindowFlags_NoMove')

    -- Standard origin lines (red = X axis, green = Y axis)
    tLineCenterX = line:new("2dw", 0, 0, 50)
    tLineCenterY = line:new("2dw", 0, 0, 50)
    tLineCenterX:add({-9999999, 0, 9999999, 0})
    tLineCenterY:add({0, -9999999, 0, 9999999})
    tLineCenterX:setColor(1, 0, 0)
    tLineCenterY:setColor(0, 1, 0)

    -- Standard alpha-pattern background (checkerboard to show transparency)
    local sTextureFileName = tUtil.createAlphaPattern(1024, 768, 32,
                                 {r=240, g=240, b=240}, {r=125, g=125, b=125})
    if sTextureFileName then
        local iW, iH      = mbm.getSizeScreen()
        tex_alpha_pattern  = texture:new('2dw')
        tex_alpha_pattern:load(sTextureFileName)
        tex_alpha_pattern:setSize(iW, iH)
        tex_alpha_pattern.z       = 99
        tex_alpha_pattern.visible = false
    end

    -- Window title keys (use string keys; ImGui uses them as unique IDs)
    tWindowsTitle = {
        title_main_panel  = "title_main_panel",
        title_properties  = "title_properties",
    }

    -- Welcome overlay
    tUtil.sMessageOverlay = tLang.L("welcome_my_tool")   -- add key to language.lua
end

function loop(delta)
    showMainMenu()
    showMainPanel()
    tUtil.showOverlayMessage()
    -- keep alpha pattern centered on camera
    if tex_alpha_pattern and tex_alpha_pattern.visible then
        tex_alpha_pattern:setPos(camera2d.x, camera2d.y)
    end
end

-- ─── Main Menu Bar ───────────────────────────────────────────────────────────
function showMainMenu()
    if tImGui.BeginMainMenuBar() then
        if tImGui.BeginMenu(tLang.L("menu_file")) then
            local pressed = tImGui.MenuItem(tLang.L("menu_quit"), "Ctrl+Q")
            if pressed then mbm.quit() end
            tImGui.EndMenu()
        end
        if tImGui.BeginMenu(tLang.L("menu_options")) then
            local pressed, checked = tImGui.MenuItem(tLang.L("move_windows"), nil, bEnableMoveWindow)
            if pressed then bEnableMoveWindow = checked end
            tImGui.Separator()
            if tImGui.BeginMenu(tLang.L("menu_language")) then
                if tImGui.MenuItem(tLang.L("lang_english")) then
                    tLang.setLanguage("en")
                end
                if tImGui.MenuItem(tLang.L("lang_portuguese_br")) then
                    tLang.setLanguage("pt_br")
                end
                tImGui.EndMenu()
            end
            tImGui.EndMenu()
        end
        tImGui.EndMainMenuBar()
    end
end

-- ─── Main Panel ──────────────────────────────────────────────────────────────
function showMainPanel()
    local flags = bEnableMoveWindow and 0 or ImGuiWindowFlags_NoMove
    tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_main_panel, 0, 0, 300)
    local is_opened, closed = tImGui.Begin(tWindowsTitle.title_main_panel, true, flags)
    if is_opened then
        tImGui.Text("Hello from my tool!")
    end
    tImGui.End()
end

-- ─── Input ───────────────────────────────────────────────────────────────────
function onTouchDown(key, x, y)
    isClickedMouseLeft = (key == 0)
    camera2d.mx = x
    camera2d.my = y
end

function onTouchMove(key, x, y)
    if bEnableMoveWorld and isClickedMouseLeft and not tImGui.IsAnyWindowHovered() then
        local px = (camera2d.mx - x) * camera2d.sx
        local py = (camera2d.my - y) * camera2d.sy
        camera2d.mx = x
        camera2d.my = y
        camera2d:setPos(camera2d.x + px, camera2d.y - py)
    end
end

function onTouchUp(key, x, y)
    isClickedMouseLeft = false
    camera2d.mx = x
    camera2d.my = y
end

function onTouchZoom(zoom)
    -- override for scroll-to-zoom behavior
end

function onKeyDown(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = true
    elseif keyControlPressed then
        if key == mbm.getKeyCode('Q') then mbm.quit() end
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = false
    end
end
```

---

## ImGui API Reference (via `tImGui`)

All ImGui calls go through the `tImGui` global, e.g. `tImGui.Begin(...)`.

### Window Management

```lua
-- Create a window. Returns is_opened, closed_clicked.
local is_opened, closed = tImGui.Begin("Title", closeable, flags)
tImGui.End()

-- Constrain / position a window (call before Begin, repeated each frame until stable)
tImGui.SetNextWindowPos({x=..., y=...}, tImGui.Flags('ImGuiCond_Always'))
tImGui.SetNextWindowSize({x=..., y=...}, tImGui.Flags('ImGuiCond_Always'))
tImGui.SetNextWindowSizeConstraints(minSize, maxSize)

-- Check if the cursor is over any ImGui window
tImGui.IsAnyWindowHovered()   -- use to suppress world mouse interaction
```

### Menu Bar (inside a window with ImGuiWindowFlags_MenuBar)

```lua
if tImGui.BeginMenuBar() then
    if tImGui.BeginMenu("Menu Name") then
        local pressed, checked = tImGui.MenuItem("Item Label", "Ctrl+X")
        local pressed, checked = tImGui.MenuItem("Toggle", nil, currentBool)
        tImGui.Separator()
        tImGui.EndMenu()
    end
    tImGui.EndMenuBar()
end
```

### Top-Level Menu Bar

```lua
if tImGui.BeginMainMenuBar() then
    -- same as above
    tImGui.EndMainMenuBar()
end
```

### Common Widgets

```lua
tImGui.Text("label")
tImGui.TextColored({r=1,g=0,b=0,a=1}, "red text")
tImGui.Separator()
tImGui.SameLine()
tImGui.Spacing()
tImGui.NewLine()
tImGui.Dummy({x=w, y=h})

-- Buttons — return true when clicked
if tImGui.Button("Label", {x=120, y=0}) then ... end
if tImGui.SmallButton("Label") then ... end

-- Checkbox — returns pressed(bool), checked(bool)
local pressed, checked = tImGui.Checkbox("Label", currentBool)
if pressed then myBool = checked end

-- Slider
local changed, newVal = tImGui.SliderFloat("Label", currentFloat, min, max)
local changed, newVal = tImGui.SliderInt("Label", currentInt, min, max)

-- Drag
local changed, newVal = tImGui.DragFloat("Label", val, speed, min, max, "%.2f")
local changed, newVal = tImGui.DragInt("Label", val, speed, min, max)

-- Input
local changed, newStr = tImGui.InputText("Label", currentStr, bufferSize)
local changed, newVal = tImGui.InputFloat("Label", currentFloat)
local changed, newVal = tImGui.InputInt("Label", currentInt)

-- Combo (dropdown)
local changed, newIndex = tImGui.Combo("Label", currentIndex, {"Option A","Option B","Option C"})

-- Color picker
local changed, newColor = tImGui.ColorEdit4("Label", {r=r,g=g,b=b,a=a})

-- Progress bar  (value 0..1)
tImGui.ProgressBar(fraction, {x=-1, y=0}, "optional overlay text")

-- Radio buttons
if tImGui.RadioButton("Option A", currentIndex == 1) then currentIndex = 1 end

-- Selectable
local selected, _ = tImGui.Selectable("Item", isSelected)
```

### Collapsing / Tree

```lua
if tImGui.CollapsingHeader("Section") then
    -- content
end

if tImGui.TreeNode("Node Label") then
    -- child content
    tImGui.TreePop()
end
```

### Popups / Modals

```lua
-- Trigger once:
tImGui.OpenPopup("popup_id")

-- Inside loop():
local is_open, _ = tImGui.BeginPopupModal("popup_id", false,
                       tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize'))
if is_open then
    tImGui.Text("Are you sure?")
    tImGui.Separator()
    if tImGui.Button("OK", {x=120,y=0}) then
        -- do work
        tImGui.CloseCurrentPopup()
    end
    tImGui.SameLine()
    if tImGui.Button("Cancel", {x=120,y=0}) then
        tImGui.CloseCurrentPopup()
    end
    tImGui.EndPopup()
end
```

### Context Menu (right-click popup)

```lua
if tImGui.BeginPopupContextItem("item_id") then
    if tImGui.MenuItem("Delete") then ... end
    tImGui.EndPopup()
end
```

### Tabs

```lua
if tImGui.BeginTabBar("TabBar##id") then
    if tImGui.BeginTabItem("Tab One") then
        -- content
        tImGui.EndTabItem()
    end
    if tImGui.BeginTabItem("Tab Two") then
        tImGui.EndTabItem()
    end
    tImGui.EndTabBar()
end
```

### Tables

```lua
if tImGui.BeginTable("##table", 3, tImGui.Flags('ImGuiTableFlags_Borders','ImGuiTableFlags_RowBg')) then
    tImGui.TableSetupColumn("Name")
    tImGui.TableSetupColumn("Value")
    tImGui.TableSetupColumn("Type")
    tImGui.TableHeadersRow()
    for _, row in ipairs(data) do
        tImGui.TableNextRow()
        tImGui.TableNextColumn(); tImGui.Text(row.name)
        tImGui.TableNextColumn(); tImGui.Text(row.value)
        tImGui.TableNextColumn(); tImGui.Text(row.type)
    end
    tImGui.EndTable()
end
```

### Images (texture previews)

```lua
-- texInfo is a texture userdata returned by mbm.loadTexture()
local size = {x=128, y=128}
local uv0  = {x=0, y=0}
local uv1  = {x=1, y=1}
tImGui.Image(texInfo, size, uv0, uv1)
if tImGui.ImageButton("btn_id##unique", texInfo, size, uv0, uv1) then
    -- clicked
end
```

### Style / Colors

```lua
tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r=0,g=0.6,b=0,a=1})
-- ... widgets ...
tImGui.PopStyleColor(1)   -- pass count of pushes

tImGui.PushStyleVar(tImGui.Flags('ImGuiStyleVar_FramePadding'), {x=4, y=4})
tImGui.PopStyleVar(1)
```

### Flags Helper

```lua
-- Combine one or more ImGui flag names into an integer:
local flags = tImGui.Flags('ImGuiWindowFlags_MenuBar', 'ImGuiWindowFlags_NoMove')
local cond  = tImGui.Flags('ImGuiCond_Always')
```

### Layout Width

```lua
tImGui.PushItemWidth(180)
-- ... widgets at 180px wide ...
tImGui.PopItemWidth()

tImGui.SetNextItemWidth(120)  -- one widget only
```

### ImDrawList (immediate geometry)

```lua
-- Pixel-space geometry drawn over all windows:
tImGui.SetImDrawListToForeground()
tImGui.AddLine({x=x1,y=y1}, {x=x2,y=y2}, 0xFFFFFFFF, 1.0)
tImGui.AddRect({x=xMin,y=yMin}, {x=xMax,y=yMax}, 0xFF0000FF)
tImGui.AddRectFilled({x=xMin,y=yMin}, {x=xMax,y=yMax}, 0x8800FFFF)
tImGui.AddCircle({x=cx,y=cy}, radius, 0xFFFFFFFF)
```

---

## editor_utils.lua API (`tUtil`)

`editor_utils.lua` is loaded via `require "editor_utils"`. It also `require "lang.language"` globally as `tLang`.

| Function | Purpose |
|---|---|
| `tUtil.setInitialWindowPositionLeft(title,x,y,width,maxWidth)` | Anchor window to left on first 3 frames |
| `tUtil.setInitialWindowPositionRight(title,x,y,width,maxWidth)` | Anchor window to right on first 3 frames |
| `tUtil.setInitialWindowPositionDown(title,xStart,yPct,xRight)` | Anchor window to bottom on first 3 frames |
| `tUtil.showTextureAssets(title,tList,x,y,bMove)` | Reusable texture-picker panel; returns `closed_clicked` |
| `tUtil.createAlphaPattern(w,h,blockSize,color1,color2)` | Creates checkerboard texture; returns file path or nil |
| `tUtil.showOverlayMessage()` | Render `tUtil.sMessageOverlay` as a screen overlay |
| `tUtil.showMessage(msg, time)` | Timed status message (defaults 3 seconds) |
| `tUtil.showMessageWarn(msg, time)` | Timed warning message in orange |
| `tUtil.getShortName(path, quote)` | Basename from a path string |
| `tUtil.getBaseFileName(path)` | Strip directory, keep filename |
| `tUtil.getExtension(path)` | Extract file extension |
| `tUtil.hasSupportedImageExtension(name)` | Returns true for png/jpg/bmp/gif/tga/… |
| `tUtil.hasSupportedMeshExtension(name)` | Returns true for spt/msh/fnt/tile/ptl |
| `tUtil.loadInfoImagesToTable(files, tOut)` | Load texture metadata into `{file_name,width,height,alpha,id}` list |
| `tUtil.getMeshFilesFromFolder(dir)` | Return list of mesh file paths in a directory |
| `tUtil.save(name, value, tOut, onSaveUserData)` | Serialize a Lua value/table as source code lines into `tOut` |
| `tUtil.onNewAnyWindowsHovered()` | Returns a tracker object; call `:addThisWindow()` / `:IsAnyWindowHovered()` |
| `tUtil.newInstance(w,h,ew,eh,scriptPath)` | Spawn a new engine instance running `scriptPath` |
| `tUtil.deepCopyTable(tbl)` | Full recursive copy |
| `tUtil.onAddMeshToEditor(file,center,mode,label)` | Load any asset into the editor viewport |
| `tUtil.setShapeToMesh(tObj)` | Attach a physics-preview shape to a mesh object |

### Standard welcome message

```lua
tUtil.sMessageOverlay = 'Welcome to My Tool!\n\nFirst add a file from the menu!'
```

### Standard background color

`editor_utils.lua` sets `mbm.setColor(37/255, 37/255, 37/255)` (dark grey) automatically on load.

---

## lang/language.lua API (`tLang`)

`tLang` is the module returned by `require "lang.language"` (auto-populated by `editor_utils`).

```lua
tLang.L("key")                  -- get localized string for current language
tLang.setLanguage("en")         -- switch to English
tLang.setLanguage("pt_br")      -- switch to Brazilian Portuguese
```

### Adding New Strings

Edit `editor/lang/language.lua`. Add your keys to **both** the `M.en` and `M.pt_br` tables:

```lua
-- In M.en:
welcome_my_tool     = "Welcome to My Tool!",
my_tool_load        = "Load File",
my_tool_save        = "Save File",
my_tool_process     = "Process",

-- In M.pt_br:
welcome_my_tool     = "Bem-vindo ao Meu Editor!",
my_tool_load        = "Carregar Arquivo",
my_tool_save        = "Salvar Arquivo",
my_tool_process     = "Processar",
```

---

## File Dialogs (native OS dialogs via `mbm`)

```lua
-- Open file dialog — returns path string or nil
local path = mbm.openFile(lastPath, "ext1", "ext2", "ext3")

-- Save file dialog — returns path string or nil
local path = mbm.saveFile(lastPath, "ext")

-- Folder picker — returns path string or nil
local folder = mbm.openFolder("Choose Folder", ".")

-- List files in a directory recursively
local tEntries = mbm.listFiles(folderPath, true)  -- [{path,name,isDir}, ...]
```

---

## Save / Load Editor State

Editors save their state as **regeneratable Lua source code** using `tUtil.save()`.

### Saving

```lua
function onSaveEditor(sFileName)
    local sFile = sFileName or mbm.saveFile(sLastEditorFileName, "myeditor")
    if not sFile then return end
    sLastEditorFileName = sFile
    local tOut = {}
    -- serialize each piece of state
    tUtil.save("myState.someValue",  myState.someValue,  tOut)
    tUtil.save("myState.tOptions",   myState.tOptions,   tOut)
    -- write to file
    local f = io.open(sFile, "w")
    if f then
        for _, line in ipairs(tOut) do
            f:write(line .. "\n")
        end
        f:close()
        tUtil.showMessage("Saved: " .. tUtil.getShortName(sFile))
    end
end
```

### Loading

```lua
function onLoadEditor(sFileName)
    local sFile = sFileName or mbm.openFile(sLastEditorFileName, "myeditor")
    if not sFile then return end
    sLastEditorFileName = sFile
    local fn, err = loadfile(sFile)
    if fn then
        fn()   -- executes the saved Lua code, restoring globals
        tUtil.showMessage("Loaded: " .. tUtil.getShortName(sFile))
    else
        tUtil.showMessageWarn("Load failed: " .. tostring(err))
    end
end
```

---

## Window Positioning Patterns

Use `tUtil.setInitialWindowPosition*` **before** `tImGui.Begin()`. These functions push  
position/size constraints for the first 3 frames only, then let the user resize freely.

```lua
-- Right panel (width=300 from right edge, offset 0 from corners)
tUtil.setInitialWindowPositionRight("WindowTitle", 0, 0, 300)
local is_opened, closed = tImGui.Begin("WindowTitle", true, flags)

-- Left panel (width=280 anchored to x=0)
tUtil.setInitialWindowPositionLeft("WindowTitle", 0, 0, 280)

-- Bottom panel (starting at x=leftPanelWidth, height=25% of screen)
tUtil.setInitialWindowPositionDown("WindowTitle", 280, 0.25, 300)
```

---

## Alpha-Pattern (Checkerboard) Background

Used to visualize transparent images/sprites:

```lua
-- In onInitScene():
local sTexFile = tUtil.createAlphaPattern(1024, 768, 32,
                     {r=240, g=240, b=240}, {r=125, g=125, b=125})
if sTexFile then
    local iW, iH      = mbm.getSizeScreen()
    tex_alpha_pattern  = texture:new('2dw')
    tex_alpha_pattern:load(sTexFile)
    tex_alpha_pattern:setSize(iW, iH)
    tex_alpha_pattern.z       = 99
    tex_alpha_pattern.visible = false  -- toggle via menu option
end

-- In loop(delta): keep it centered on camera
tex_alpha_pattern:setPos(camera2d.x, camera2d.y)
```

---

## Shared Shader Definitions

To load the standard editor shaders (gradient, life bar, etc.): 

```lua
-- At top of onInitScene() or before first use:
mbm.include("shader_cfg.lua")   -- loads all shaders from editor/shaders/
```

Or add custom shaders inline:

```lua
local ok = mbm.addShader({
    name = "my_effect.ps",
    code = [[
        precision mediump float;
        uniform sampler2D sample0;
        uniform float my_param;
        varying vec2 vTexCoord;
        void main() {
            vec4 c = texture2D(sample0, vTexCoord);
            gl_FragColor = c * my_param;
        }
    ]],
    var = {my_param = {1.0}},
    min = {my_param = {0.0}},
    max = {my_param = {1.0}}
})
```

---

## Optional: Require Additional Plugins

Some editors need extra plugins:

```lua
tImGui  = require "ImGui"         -- always required for editor tools
tUtil   = require "editor_utils"  -- always required
sqlite3 = require "lsqlite3"      -- only for asset_packager
tTile   = require "tilemap"       -- only for tilemap_editor
```

---

## Step-by-Step: Creating a New Editor Tool

### Step 1 — Create `editor/<your_tool>.lua`

Use the minimum required structure above as a starting point.  
Name the file in `snake_case` matching the tool's purpose, e.g. `color_palette.lua`.

Follow these invariants taken from every existing editor:

1. **License block** at the top (MIT, matching the project)
2. `tImGui = require "ImGui"` on line 1 after the comment block
3. `tUtil = require "editor_utils"` on line 2 after ImGui
4. `tLang` is available automatically after `editor_utils` loads
5. `function onInitScene()` — all engine objects created here
6. `function loop(delta)` — all ImGui calls happen here, nothing else calls ImGui
7. Set `tUtil.sMessageOverlay` in `onInitScene()` to welcome text
8. Create `tLineCenterX` / `tLineCenterY` as the standard origin cross
9. Create `tex_alpha_pattern` with `tUtil.createAlphaPattern()` if you show textures
10. Cache all `tImGui.Flags(...)` results in `onInitScene()` (e.g. `ImGuiWindowFlags_NoMove`)
11. Use a `tWindowsTitle` table of string keys — do **not** hardcode ImGui window title strings inline

### Step 2 — Add Localization Keys

In `editor/lang/language.lua`, add your keys to **both** `M.en` and `M.pt_br`.  
Use the `tLang.L("key")` call everywhere instead of string literals.

### Step 3 — Wire into the Launcher (`main-lua.cpp`)

Each platform's `main-lua.cpp` contains a `mbm::APP_RUN default_applications[]` array.  
Add your tool to this array in **every platform** you want to support:

```cpp
// In platform-linux/main-lua.cpp (and platform-macos/, platform-msvs/mini-mbm/main.cpp, etc.)
mbm::APP_RUN default_applications[] = {
    {"Asset packager"     , STR_PT_BR_ASSET_PACKAGER,    "asset_packager.lua"},
    {"Font Maker"         , STR_PT_BR_FONT_MAKER,        "font_maker.lua"},
    // ... existing entries ...
    {"My Tool Name"       , "Meu Editor",                "my_tool.lua"},   // ← add here
    {"User specified"     , STR_PT_BR_USER_SPECIFIED,    "user_specified.lua"},  // keep last
};
```

You can also define a `STR_PT_BR_MY_TOOL` constant in `include/core_mbm/strings-pt-br.h` for consistency.

### Step 4 — Set the Editor Search Paths (Debug Mode)

In the debug block of `main-lua.cpp`, confirm the `editor/` directory is already in the path:

```cpp
#if defined _DEBUG
    mbm::add_path("/home/michel/mini-mbm/editor/");
    mbm::add_path("/home/michel/mini-mbm/editor/shaders");
#endif
```

No changes needed here — the paths are already configured.

### Step 5 — Launch Your Tool

**Via the launcher menu** (after adding to `default_applications`): just run `mini-mbm` and select your tool.

**Directly via CLI**:

```sh
./bin/debug/linux_x86/mini-mbm --scene editor/my_tool.lua --nosplash --showconsole
```

**From another Lua script** (spawn a second window):

```lua
tUtil.newInstance(1280, 720, 1280, 720, "editor/my_tool.lua")
```

---

## Common Patterns from Existing Tools

### Keyboard Shortcut Handler

```lua
function onKeyDown(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = true
    elseif keyControlPressed then
        if     key == mbm.getKeyCode('S') then onSaveEditor()
        elseif key == mbm.getKeyCode('O') then onOpenFile()
        elseif key == mbm.getKeyCode('N') then onNewFile()
        end
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then
        keyControlPressed = false
    end
end
```

### Window Move Toggle (all editors have this)

```lua
-- In onInitScene():
bEnableMoveWindow = false

-- In loop() flag calculation:
local flags = bEnableMoveWindow and tImGui.Flags('ImGuiWindowFlags_MenuBar')
           or tImGui.Flags('ImGuiWindowFlags_MenuBar', 'ImGuiWindowFlags_NoMove')

-- In options menu:
local pressed, checked = tImGui.MenuItem(tLang.L("move_windows"), nil, bEnableMoveWindow)
if pressed then bEnableMoveWindow = checked end
```

### World Pan with Mouse Drag

```lua
-- In onInitScene():
bEnableMoveWorld = true

-- In onTouchMove():
if bEnableMoveWorld and isClickedMouseLeft and not tImGui.IsAnyWindowHovered() then
    local px = (camera2d.mx - x) * camera2d.sx
    local py = (camera2d.my - y) * camera2d.sy
    camera2d.mx = x
    camera2d.my = y
    camera2d:setPos(camera2d.x + px, camera2d.y - py)
end
```

### Scroll Zoom

```lua
function onTouchZoom(zoom)
    if not tImGui.IsAnyWindowHovered() then
        fViewScale = fViewScale + zoom * 0.1
        fViewScale = math.max(0.1, math.min(10.0, fViewScale))
    end
end
```

### Confirm/Delete Modal

```lua
-- State variables (in onInitScene):
bPendingDelete  = false
sPendingMessage = ''

-- Trigger (e.g. from a menu item):
bPendingDelete  = true
sPendingMessage = 'Delete this item?'
tImGui.OpenPopup("confirm_delete")

-- In loop():
local is_open, _ = tImGui.BeginPopupModal("confirm_delete", false,
                       tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize'))
if is_open then
    tImGui.Text(sPendingMessage)
    tImGui.Separator()
    if tImGui.Button(tLang.L("ok"), {x=120,y=0}) then
        doDelete()
        tImGui.CloseCurrentPopup()
        bPendingDelete = false
    end
    tImGui.SetItemDefaultFocus()
    tImGui.SameLine()
    if tImGui.Button(tLang.L("cancel"), {x=120,y=0}) then
        tImGui.CloseCurrentPopup()
        bPendingDelete = false
    end
    tImGui.EndPopup()
end
```

### Loading Textures for Display

```lua
-- In editor_utils.lua pattern:
tTexturesToEditor = {}

-- Load from file dialog:
local tFiles = {mbm.openFile(lastPath, table.unpack(tUtil.supported_images))}
tTexturesToEditor = tUtil.loadInfoImagesToTable(tFiles, tTexturesToEditor)
-- Each entry: {file_name, width, height, alpha, id, base_file_name}
--   id = texture userdata that can be passed to tImGui.Image()/ImageButton()
```

---

## Checklist

Before committing a new editor tool, verify:

- [ ] MIT license block at the top
- [ ] `tImGui = require "ImGui"` and `tUtil = require "editor_utils"` at the top
- [ ] All text uses `tLang.L("key")` — no hardcoded English strings
- [ ] New strings added to both `M.en` and `M.pt_br` in `language.lua`
- [ ] `tUtil.sMessageOverlay` set in `onInitScene()`
- [ ] `loop(delta)` contains all ImGui calls
- [ ] `tUtil.showOverlayMessage()` called at end of `loop(delta)`
- [ ] `ImGuiWindowFlags_NoMove` cached in `onInitScene()`, not recomputed per frame
- [ ] `tWindowsTitle` table used for all ImGui window IDs
- [ ] Mouse input guards `tImGui.IsAnyWindowHovered()` before world interaction
- [ ] Added to `default_applications[]` in all relevant `main-lua.cpp` files
- [ ] Editor tested on Linux debug build (`-DUSE_ALL=1 -DCMAKE_BUILD_TYPE=Debug`)
