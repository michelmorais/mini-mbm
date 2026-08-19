local character = nil
local start_time = nil
local checked = false

local function fail(message)
    error("skeletal root motion smoke failed: " .. message)
end

function onInitScene()
    start_time = mbm.getTimeRun()
    mbm.addPath("src/test-lib")
    character = mesh:new("3d")
    if not character:load("Lorekeeper-walk.msh") then
        fail("could not load Lorekeeper-walk.msh")
    end
    local clip_name = character:getSkeletalAnimationName(1)
    if not clip_name then
        fail("missing skeletal clip")
    end
    if character:getAutomaticSkeletalRootMotionBone() ~= nil then
        fail("automatic root motion must start disabled")
    end
    if not character:playSkeletalAnimation(clip_name) then
        fail("could not play skeletal clip")
    end
    if not character:enableAutomaticSkeletalRootMotion("mixamorig:Hips") then
        fail("could not enable root motion")
    end
    local config = character:getAutomaticSkeletalRootMotionBone()
    if not config or config.name ~= "mixamorig:Hips" or type(config.boneId) ~= "string" or
            config.applyRotation ~= false then
        fail("root motion query returned invalid configuration")
    end
end

function onLoop()
    if not checked then
        local delta = character:getSkeletalRootMotionDelta("mixamorig:Hips", "world")
        if delta then
            local pos = character:getPos()
            local root = character:getSkeletalBoneTransform("mixamorig:Hips", "model")
            if not pos or not root then
                fail("missing position or neutralized root transform")
            end
            if math.abs(delta.translation.x) + math.abs(delta.translation.y) +
                    math.abs(delta.translation.z) <= 0.000001 then
                fail("raw root motion delta was zero")
            end
            if math.abs(pos.x) + math.abs(pos.y) + math.abs(pos.z) <= 0.000001 then
                fail("automatic root motion did not move the mesh")
            end
            if not character:disableAutomaticSkeletalRootMotion() then
                fail("could not disable root motion")
            end
            if character:getAutomaticSkeletalRootMotionBone() ~= nil then
                fail("root motion remained enabled after disable")
            end
            checked = true
            mbm.quit()
        end
    end
    if start_time and (mbm.getTimeRun() - start_time) > 5 then
        fail("timed out waiting for root motion delta")
    end
end
