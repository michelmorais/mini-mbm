local started = nil
local source = nil
local follower = nil
local sharing_disabled = false

local function fail(message)
    error("[skeletal-sharing-smoke] " .. message)
end

function onInitScene()
    started = mbm.getTimeRun()
    mbm.addPath("src/test-lib")

    local mesh_a = mesh:new("3d")
    source = mesh_a
    local mesh_b = mesh:new("3d")
    follower = mesh_b
    local mesh_c = mesh:new("3d")
    local static_mesh = mesh:new("3d")
    if not mesh_a:load("Lorekeeper-walk.msh") then fail("mesh_a load failed") end
    if not mesh_b:load("Lorekeeper-walk.msh") then fail("mesh_b load failed") end
    if not mesh_c:load("Lorekeeper-walk.msh") then fail("mesh_c load failed") end
    if not static_mesh:load("Crate.msh") then fail("static mesh load failed") end

    local report = mesh_a:getSkeletalSharingCompatibility(mesh_b)
    if not report.compatible or report.reason ~= "compatible" or report.boneCount <= 0 then
        fail("compatible report mismatch")
    end

    report = mesh_a:getSkeletalSharingCompatibility(static_mesh)
    if report.compatible or report.reason ~= "missing_skeleton" then
        fail("missing skeleton report mismatch")
    end

    if mesh_b:enableSkeletalPoseSharing(mesh_b) then
        fail("self skeletal pose sharing must be rejected")
    end
    if mesh_b:enableSkeletalPoseSharing(static_mesh) then
        fail("incompatible skeletal pose sharing must be rejected")
    end

    if not mesh_b:enableSkeletalPoseSharing(mesh_a) then
        fail("compatible skeletal pose sharing enable failed")
    end
    if mesh_c:enableSkeletalPoseSharing(mesh_b) then
        fail("follower chains must be rejected")
    end
    if mesh_a:enableSkeletalPoseSharing(mesh_c) then
        fail("a source with followers must not become a follower")
    end
    local sharing = mesh_b:getSkeletalPoseSharing()
    if not sharing.enabled or sharing.active or sharing.reason ~= "source_pose_inactive" then
        fail("sharing should be enabled but inactive before source pose evaluation")
    end

    local clip = mesh_a:getSkeletalAnimationName(1)
    if not clip or not mesh_a:playSkeletalAnimation(clip) then
        fail("source skeletal animation play failed")
    end
    sharing = mesh_b:getSkeletalPoseSharing()
    if not sharing.enabled or not sharing.active or sharing.reason ~= "active" then
        fail("sharing should become active after source pose evaluation")
    end

end

function onLoop(delta)
    local elapsed = started and mbm.getTimeRun() - started or 0
    if follower and not sharing_disabled and elapsed > 0.1 then
        local sharing = follower:getSkeletalPoseSharing()
        if not sharing.enabled or not sharing.active or sharing.reason ~= "active" then
            fail("sharing must remain active while rendering")
        end
        if not follower:disableSkeletalPoseSharing() then
            fail("sharing disable failed")
        end
        sharing = follower:getSkeletalPoseSharing()
        if sharing.enabled or sharing.active or sharing.reason ~= "disabled" then
            fail("sharing query after disable mismatch")
        end
        sharing_disabled = true
    end
    if elapsed > 0.2 then
        mbm.quit()
    end
end
