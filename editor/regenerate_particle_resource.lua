-- Regenerate resource-particle.h from src/test-lib/particle.png
-- Run from project root: ./bin/debug/linux_x86/mini-mbm --scene editor/regenerate_particle_resource.lua --nosplash

local pathSep = package.config:sub(1,1)
-- Paths relative to project root (mini-mbm typically runs with cwd = project root)
local pngPath = "src" .. pathSep .. "test-lib" .. pathSep .. "particle.png"
local headerPath = "include" .. pathSep .. "static-resource" .. pathSep .. "resource-particle.h"

if mbm.generateImageResourceHeaderFromPng(pngPath, headerPath) then
    print("Successfully regenerated " .. headerPath)
else
    print("ERROR: Failed to regenerate resource-particle.h")
    print("  PNG: " .. pngPath)
    print("  Output: " .. headerPath)
end
mbm.quit()