local started = nil

local function fail(message)
    error("[skeletal-sharing-smoke] " .. message)
end

function onInitScene()
    started = mbm.getTimeRun()
    mbm.addPath("src/test-lib")

    local mesh_a = mesh:new("3d")
    local mesh_b = mesh:new("3d")
    local static_mesh = mesh:new("3d")
    if not mesh_a:load("Lorekeeper-walk.msh") then fail("mesh_a load failed") end
    if not mesh_b:load("Lorekeeper-walk.msh") then fail("mesh_b load failed") end
    if not static_mesh:load("Crate.msh") then fail("static mesh load failed") end

    local report = mesh_a:getSkeletalSharingCompatibility(mesh_b)
    if not report.compatible or report.reason ~= "compatible" or report.boneCount <= 0 then
        fail("compatible report mismatch")
    end

    report = mesh_a:getSkeletalSharingCompatibility(static_mesh)
    if report.compatible or report.reason ~= "missing_skeleton" then
        fail("missing skeleton report mismatch")
    end
end

function onLoop(delta)
    if started and mbm.getTimeRun() - started > 0.2 then
        mbm.quit()
    end
end
