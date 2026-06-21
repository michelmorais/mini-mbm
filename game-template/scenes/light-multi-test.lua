--[[
--------------------------------------------------------------------------------
| mini-mbm manual scene: multi-light 2dw validation                            |
|                                                                              |
| Preferred asset: src/test-lib/box.spt                                        |
| Fallback asset in this repo: src/test-lib/box.spt                            |
|                                                                              |
| Run with:                                                                    |
|   ./mini-mbm game-template/scenes/light-multi-test.lua                       |
--------------------------------------------------------------------------------
]]--

local script_dir = ...
if script_dir then
    mbm.addPath(script_dir)
    mbm.addPath(script_dir .. "/../../src/test-lib")
end
mbm.addPath("/home/michel/mini-mbm/src/test-lib")
local color = {r=37/255,g=37/255,b=37/255}
mbm.setColor(color.r,color.g,color.b)

local TARGET = "2dw"
local PREFERRED_SPRITE = "box.spt"
local FALLBACK_SPRITE = "box.spt"
local MOVE_SPEED = 220
local LIGHT_MARKER_RADIUS = 12
local LIGHT_RING_SEGMENTS = 40

local sprite_test = nil
local camera2d = nil
local held_keys = {}
local light_markers = {}
local light_rings = {}
local KEY_LEFT = 0
local KEY_RIGHT = 0
local KEY_UP = 0
local KEY_DOWN = 0
local KEY_ESCAPE = 0
local KEY_SPACE = 0
local last_selection_signature = ""
local last_report_time = 0

local EQUAL_LIGHT_COLOR = {r = 1.0, g = 1.0, b = 1.0, a = 1.0}
local lights = {
    {position = {x = -220, y = -220, z = 10}, radius = 360, color = EQUAL_LIGHT_COLOR},
    {position = {x = 220,  y = -220, z = 10}, radius = 360, color = EQUAL_LIGHT_COLOR},
    {position = {x = -220, y =  220, z = 10}, radius = 360, color = EQUAL_LIGHT_COLOR},
    {position = {x = 220,  y =  220, z = 10}, radius = 360, color = EQUAL_LIGHT_COLOR},
}

local function clamp01(value)
    if value < 0 then
        return 0
    elseif value > 1 then
        return 1
    end
    return value
end

local function color_to_hex(color, alpha_scale)
    local scale = alpha_scale or 1.0
    local a = math.floor(clamp01((color.a or 1.0) * scale) * 255)
    local r = math.floor(clamp01(color.r or 0.0) * 255)
    local g = math.floor(clamp01(color.g or 0.0) * 255)
    local b = math.floor(clamp01(color.b or 0.0) * 255)
    return string.format("#%02X%02X%02X%02X", a, r, g, b)
end

local function build_circle_points(radius, segments)
    local points = {}
    for i = 0, segments do
        local angle = (math.pi * 2.0 * i) / segments
        points[#points + 1] = math.cos(angle) * radius
        points[#points + 1] = math.sin(angle) * radius
    end
    return points
end

local function create_light_debug_visuals()
    for i = 1, #lights do
        local light = lights[i]

        local marker = shape:new(TARGET, light.position.x, light.position.y, -20)
        marker:create("circle", 1)
        marker:setScale(LIGHT_MARKER_RADIUS, LIGHT_MARKER_RADIUS, 1)
        marker:setTexture(color_to_hex(light.color, 1.0))
        marker.alwaysRender = true
        light_markers[#light_markers + 1] = marker

        local ring = line:new(TARGET, light.position.x, light.position.y, -21)
        ring:add(build_circle_points(light.radius, LIGHT_RING_SEGMENTS))
        ring:setColor(
            math.floor(clamp01(light.color.r) * 255),
            math.floor(clamp01(light.color.g) * 255),
            math.floor(clamp01(light.color.b) * 255),
            255)
        ring.alwaysRender = true
        light_rings[#light_rings + 1] = ring
    end
end

local function resolve_sprite_file()
    local exists, full_path = mbm.existFile(PREFERRED_SPRITE)
    if exists then
        return PREFERRED_SPRITE, full_path
    end
    exists, full_path = mbm.existFile(FALLBACK_SPRITE)
    if exists then
        return FALLBACK_SPRITE, full_path
    end
    return nil, nil
end

local function get_sprite_center()
    return {
        x = sprite_test and sprite_test.x or 0,
        y = sprite_test and sprite_test.y or 0,
        z = sprite_test and sprite_test.z or 0,
    }
end

local function get_sprite_half_extents()
    if not sprite_test or not sprite_test.getSize then
        return {x = 96, y = 96, z = 8}
    end

    local width, height, depth = sprite_test:getSize(true)
    width = tonumber(width) or 0
    height = tonumber(height) or 0
    depth = tonumber(depth) or 0

    if width <= 0 and height <= 0 and depth <= 0 then
        return {x = 96, y = 96, z = 8}
    end

    return {
        x = math.max(width * 0.5, 0.0),
        y = math.max(height * 0.5, 0.0),
        z = math.max(depth * 0.5, 0.0),
    }
end

local function get_selection_signature(selected)
    local parts = {}
    for i = 1, #selected do
        parts[#parts + 1] = tostring(selected[i].sourceIndex)
    end
    return table.concat(parts, ",")
end

local function report_selected_lights(reason)
    if not sprite_test then
        return
    end
    local selected = mbm.getSelectedPointLights(TARGET, get_sprite_center(), get_sprite_half_extents())
    local summary = {}
    for i = 1, #selected do
        local entry = selected[i]
        local color = entry.color or (entry.pointLight and entry.pointLight.color) or {r = 0, g = 0, b = 0}
        local radius = entry.radius or (entry.pointLight and entry.pointLight.radius) or 0
        local inside_center = entry.distanceToObjectCenter <= radius and "yes" or "no"
        local center_attenuation = 0
        if radius > 0 then
            local ratio = math.min(entry.distanceToObjectCenter / radius, 1.0)
            center_attenuation = (1.0 - ratio)
            center_attenuation = center_attenuation * center_attenuation
        end
        summary[#summary + 1] = string.format(
            "#%d src=%d dist=%.2f radius=%.2f inside-center=%s center-atten=%.3f color=(%.2f,%.2f,%.2f)",
            i,
            entry.sourceIndex,
            entry.distanceToObjectCenter,
            radius,
            inside_center,
            center_attenuation,
            color.r,
            color.g,
            color.b)
    end
    print("info", "white",
          string.format("%s | selected=%d | %s", reason, #selected, table.concat(summary, " | ")))
    last_selection_signature = get_selection_signature(selected)
end

local function configure_lights()
    local supported = mbm.getSupportedMaxLights(TARGET)
    local requested = math.min(#lights, supported)
    mbm.setLightEnabled(TARGET, true)
    mbm.setAmbientLight(TARGET, {r = 0.10, g = 0.10, b = 0.12, a = 1.0})
    mbm.setRequestedMaxLights(TARGET, requested)
    mbm.setLightSelectionMode(TARGET, "per_object_nearest")
    mbm.clearPointLights(TARGET)
    for i = 1, #lights do
        local light = lights[i]
        mbm.addPointLight(TARGET, light.position, light.radius, light.color)
    end
    print("info", "green",
          string.format("2dw supported=%d requested=%d totalPointLights=%d mode=equal-power", supported, requested, #lights))
end

function onInitScene()
    KEY_LEFT = mbm.getKeyCode("LEFT")
    KEY_RIGHT = mbm.getKeyCode("RIGHT")
    KEY_UP = mbm.getKeyCode("UP")
    KEY_DOWN = mbm.getKeyCode("DOWN")
    KEY_ESCAPE = mbm.getKeyCode("ESC")
    KEY_SPACE = mbm.getKeyCode("SPACE")

    local color = {r=37/255,g=37/255,b=37/255}
    mbm.setColor(color.r, color.g, color.b)
    camera2d = mbm.getCamera("2d")
    camera2d:setPos(0, 0)

    configure_lights()
    create_light_debug_visuals()

    local sprite_file, full_path = resolve_sprite_file()
    if not sprite_file then
        error("No sprite available. Expected enemy-1-normal_map_plus_normals.spt or src/test-lib/box.spt")
    end

    sprite_test = sprite:new(TARGET, 0, 0)
    if not sprite_test:load(sprite_file) then
        error("Failed to load sprite: " .. sprite_file)
    end
    sprite_test:setScale(2.0, 2.0)

    print("info", "yellow", string.format("Loaded sprite %s (%s)", sprite_file, full_path or "path unresolved"))
    print("info", "yellow", "Use arrow keys to move the sprite. Press SPACE to dump selected point lights.")
    print("info", "yellow", "This scene uses four matched white lights for balance testing.")
    report_selected_lights("init")
end

function onLoop(delta)
    if not sprite_test then
        return
    end

    if held_keys[KEY_LEFT] then sprite_test.x = sprite_test.x - MOVE_SPEED * delta end
    if held_keys[KEY_RIGHT] then sprite_test.x = sprite_test.x + MOVE_SPEED * delta end
    if held_keys[KEY_UP] then sprite_test.y = sprite_test.y + MOVE_SPEED * delta end
    if held_keys[KEY_DOWN] then sprite_test.y = sprite_test.y - MOVE_SPEED * delta end

    last_report_time = last_report_time + delta
    if last_report_time >= 0.20 then
        last_report_time = 0
        local selected = mbm.getSelectedPointLights(TARGET, get_sprite_center(), get_sprite_half_extents())
        local signature = get_selection_signature(selected)
        if signature ~= last_selection_signature then
            report_selected_lights("selection changed")
        end
    end
end

function onKeyDown(key)
    held_keys[key] = true
    if key == KEY_ESCAPE then
        mbm.quit()
    elseif key == KEY_SPACE then
        report_selected_lights("manual dump")
    end
end

function onKeyUp(key)
    held_keys[key] = nil
end

function onTouchDown(key, x, y)
end

function onTouchUp(key, x, y)
end

function onTouchMove(key, x, y)
end
