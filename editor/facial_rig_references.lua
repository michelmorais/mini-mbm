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
| LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  |
| OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                                                          |
|------------------------------------------------------------------------------------------------------------------------|

   Inspect and migrate named target references in authored expressions and facial clips.
]]--

local M={}

function M.inspect(project,name)
    local report={poses={},clips={},keyCount=0}
    for _,pose in ipairs(project and project.expressions and project.expressions.poses or {}) do
        if pose.weights and pose.weights[name]~=nil then report.poses[#report.poses+1]=pose.name end
    end
    for _,clip in ipairs(project and project.clips or {}) do
        local count=0
        for _,key in ipairs(clip.keys or {}) do
            if key.weights and key.weights[name]~=nil then count=count+1 end
        end
        if count>0 then
            report.clips[#report.clips+1]={name=clip.name,count=count}
            report.keyCount=report.keyCount+count
        end
    end
    return report
end

function M.rename(project,oldName,newName)
    if oldName==newName then return end
    for _,pose in ipairs(project and project.expressions and project.expressions.poses or {}) do
        if pose.weights and pose.weights[oldName]~=nil then
            pose.weights[newName]=pose.weights[oldName]
            pose.weights[oldName]=nil
        end
    end
    for _,clip in ipairs(project and project.clips or {}) do
        for _,key in ipairs(clip.keys or {}) do
            if key.weights and key.weights[oldName]~=nil then
                key.weights[newName]=key.weights[oldName]
                key.weights[oldName]=nil
            end
        end
    end
end

function M.remove(project,name)
    for _,pose in ipairs(project and project.expressions and project.expressions.poses or {}) do
        if pose.weights then pose.weights[name]=nil end
    end
    for _,clip in ipairs(project and project.clips or {}) do
        for _,key in ipairs(clip.keys or {}) do
            if key.weights then key.weights[name]=nil end
        end
    end
end

return M
