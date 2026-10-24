--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
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
|-------------------------------------------------------------------------------------------------------------------------|

   Facial rig binding plan.

   Builds a deterministic, read-only assignment plan from validated facial roles, masks, and
   generated bones. It deliberately does not mutate mesh data or create weights.
]]--

local M={}

local POLICIES={
    main_face={method='facial_skeleton',bones={'face_root','eye.L','eye.R','nose','jaw','mouth','brow.L','brow.R'}},
    left_eye={method='rigid',bones={'eye.L'}},
    right_eye={method='rigid',bones={'eye.R'}},
    upper_teeth={method='rigid',bones={'face_root'}},
    lower_teeth={method='rigid',bones={'jaw','face_root'}},
    tongue={method='rigid',bones={'jaw','face_root'}},
    brows={method='bilateral',bones={'brow.L','brow.R'}},
    nose={method='rigid',bones={'nose'}},
    mouth={method='facial_mouth',bones={'mouth','jaw'}},
    hair={method='opt_in',bones={}},
    accessory={method='opt_in',bones={}}
}

local function hasBone(rig,name)
    for _,bone in ipairs(rig and rig.bones or {}) do
        if bone.id==name then return true end
    end
    return false
end

local function copyAvailableBones(policy,rig)
    local result={}
    for _,name in ipairs(policy and policy.bones or {}) do
        if hasBone(rig,name) then result[#result+1]=name end
    end
    return result
end

function M.build(project,topology,rig,review)
    local plan={version=1,valid=true,readyCount=0,blockedCount=0,entries={},errors={},warnings={},
        vertexCount=topology and topology.vertexCount or project and project.meshVertexCount or 0}
    if not rig or rig.valid~=true then
        plan.valid=false
        plan.errors[#plan.errors+1]='canonical rig description is not valid'
    end
    for _,reviewEntry in ipairs(review and review.roles or {}) do
        local policy=POLICIES[reviewEntry.name] or {method='opt_in',bones={}}
        local entry={index=reviewEntry.index,name=reviewEntry.name,object=reviewEntry.object or '',
            mode=reviewEntry.mode,method=policy.method,bones=copyAvailableBones(policy,rig),
            vertexCount=reviewEntry.maskVertexCount or 0,status='blocked',reasons={}}
        for _,issue in ipairs(reviewEntry.issues or {}) do entry.reasons[#entry.reasons+1]=issue end
        if entry.mode=='unassigned' then
            entry.reasons[#entry.reasons+1]='role has no object or vertex mask'
        elseif entry.mode=='object' then
            entry.reasons[#entry.reasons+1]='object vertices are not available in the merged MSH topology'
        elseif #entry.bones==0 then
            entry.reasons[#entry.reasons+1]='the selected policy has no generated bone'
        elseif entry.vertexCount==0 then
            entry.reasons[#entry.reasons+1]='the role mask has no vertices'
        elseif #entry.reasons==0 and policy.method~='opt_in' then
            entry.status='ready'
            plan.readyCount=plan.readyCount+1
        end
        if entry.status~='ready' then
            plan.blockedCount=plan.blockedCount+1
            plan.valid=false
        end
        plan.entries[#plan.entries+1]=entry
    end
    if #plan.entries==0 then
        plan.valid=false
        plan.errors[#plan.errors+1]='no enabled facial roles are available'
    end
    if review and review.overlapCount>0 then
        plan.valid=false
        plan.errors[#plan.errors+1]='vertex masks overlap across enabled roles'
    end
    return plan
end

function M.policy(roleName)
    local source=POLICIES[roleName]
    if not source then return nil end
    local result={method=source.method,bones={}}
    for _,bone in ipairs(source.bones) do result.bones[#result.bones+1]=bone end
    return result
end

return M
