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
|------------------------------------------------------------------------------------------------------------------------|

   Facial Rig Editor project model.
]]--

local M = {}

local ROLE_NAMES = {
    'main_face', 'left_eye', 'right_eye', 'upper_teeth', 'lower_teeth',
    'tongue', 'brows', 'nose', 'mouth', 'hair', 'accessory'
}

local LANDMARK_NAMES = {
    {'face_left', 'left', true}, {'face_right', 'right', true},
    {'face_top', 'midline', true}, {'face_bottom', 'midline', true},
    {'left_eye_center', 'left', true}, {'right_eye_center', 'right', true},
    {'left_eye_corner', 'left', false}, {'right_eye_corner', 'right', false},
    {'nose_bridge', 'midline', false}, {'nose_tip', 'midline', false},
    {'mouth_left', 'left', true}, {'mouth_right', 'right', true},
    {'upper_lip', 'midline', false}, {'lower_lip', 'midline', false},
    {'chin', 'midline', false}, {'jaw_hinge_left', 'left', false},
    {'jaw_hinge_right', 'right', false}, {'brow_left', 'left', false},
    {'brow_right', 'right', false}
}

local function copyPoint(point)
    return {x=tonumber(point and point.x) or 0,
        y=tonumber(point and point.y) or 0,
        z=tonumber(point and point.z) or 0}
end

local function defaultRoles()
    local result = {}
    for index, name in ipairs(ROLE_NAMES) do
        result[index] = {name=name, object='', enabled=name == 'main_face'}
    end
    return result
end

local function defaultLandmarks()
    local result = {}
    for index, entry in ipairs(LANDMARK_NAMES) do
        result[index] = {name=entry[1], side=entry[2], required=entry[3],
            position={x=0, y=0, z=0}, placed=false}
    end
    return result
end

function M.new()
    return {
        schemaVersion=1,
        meshPath='',
        meshVertexCount=0,
        roles=defaultRoles(),
        masks={},
        landmarks=defaultLandmarks(),
        selectedRole=1,
        selectedLandmark=1
    }
end

function M.roleNames()
    local result = {}
    for index, name in ipairs(ROLE_NAMES) do result[index] = name end
    return result
end

function M.normalize(project)
    if type(project) ~= 'table' then return nil, 'project must be a table' end
    project.schemaVersion=tonumber(project.schemaVersion) or 1
    project.meshPath=type(project.meshPath) == 'string' and project.meshPath or ''
    project.meshVertexCount=math.max(0, math.floor(tonumber(project.meshVertexCount) or 0))
    project.roles=type(project.roles) == 'table' and project.roles or defaultRoles()
    project.masks=type(project.masks) == 'table' and project.masks or {}
    project.landmarks=type(project.landmarks) == 'table' and project.landmarks or defaultLandmarks()
    project.selectedRole=math.max(1, math.floor(tonumber(project.selectedRole) or 1))
    project.selectedLandmark=math.max(1, math.floor(tonumber(project.selectedLandmark) or 1))
    for index, role in ipairs(project.roles) do
        if type(role) ~= 'table' then
            project.roles[index]={name=ROLE_NAMES[index] or ('role_' .. index), object='', enabled=true}
        else
            role.name=type(role.name) == 'string' and role.name ~= '' and role.name or
                (ROLE_NAMES[index] or ('role_' .. index))
            role.object=type(role.object) == 'string' and role.object or ''
            role.enabled=role.enabled ~= false
        end
    end
    for index, landmark in ipairs(project.landmarks) do
        if type(landmark) ~= 'table' then
            project.landmarks[index]=defaultLandmarks()[index]
        else
            landmark.name=type(landmark.name) == 'string' and landmark.name or ('landmark_' .. index)
            landmark.side=type(landmark.side) == 'string' and landmark.side or 'midline'
            landmark.required=landmark.required == true
            landmark.position=copyPoint(landmark.position)
            landmark.placed=landmark.placed == true
        end
    end
    return project
end

function M.addMask(project, name, roleIndex)
    local mask = {name=name or ('region_' .. (#project.masks + 1)), roleIndex=roleIndex or 1,
        vertices={}, topologySignature='', enabled=true}
    project.masks[#project.masks + 1] = mask
    return mask
end

function M.validate(project, meshLoaded)
    local report = {ok=true, errors={}, warnings={}, roleCount=0, maskCount=0,
        landmarkCount=0, placedLandmarkCount=0}
    if type(project) ~= 'table' then
        report.ok=false
        report.errors[#report.errors + 1]='project is missing'
        return report
    end
    if not meshLoaded or project.meshPath == '' then
        report.ok=false
        report.errors[#report.errors + 1]='mesh is not loaded'
    end
    local names={}
    for _, role in ipairs(project.roles or {}) do
        if role.enabled then
            report.roleCount=report.roleCount+1
            if role.name == '' then
                report.ok=false
                report.errors[#report.errors + 1]='enabled role has no name'
            elseif names[role.name] then
                report.ok=false
                report.errors[#report.errors + 1]='duplicate role: ' .. role.name
            else
                names[role.name]=true
            end
            if role.name == 'main_face' and role.object == '' then
                report.warnings[#report.warnings + 1]='main_face has no object assignment'
            end
        end
    end
    report.maskCount=#(project.masks or {})
    for _, mask in ipairs(project.masks or {}) do
        if type(mask.name) ~= 'string' or mask.name == '' then
            report.ok=false
            report.errors[#report.errors + 1]='mask has no name'
        end
        if type(mask.vertices) ~= 'table' then
            report.ok=false
            report.errors[#report.errors + 1]='mask has invalid vertex list'
        end
    end
    for _, landmark in ipairs(project.landmarks or {}) do
        report.landmarkCount=report.landmarkCount+1
        if landmark.placed then
            report.placedLandmarkCount=report.placedLandmarkCount+1
        elseif landmark.required then
            report.ok=false
            report.errors[#report.errors + 1]='required landmark not placed: ' .. tostring(landmark.name)
        end
    end
    return report
end

function M.save(project, path, serializer)
    if type(serializer) ~= 'function' then return false, 'serializer is missing' end
    local lines={'-- mini-mbm facial rig project', 'local facialProject = {}'}
    serializer('facialProject', project, lines)
    lines[#lines + 1]='return facialProject'
    local file, errorMessage = io.open(path, 'w')
    if not file then return false, errorMessage end
    file:write(table.concat(lines, '\n'), '\n')
    file:close()
    return true
end

function M.load(path)
    local chunk, errorMessage = loadfile(path)
    if not chunk then return nil, errorMessage end
    local ok, project = pcall(chunk)
    if not ok then return nil, project end
    return M.normalize(project)
end

return M
