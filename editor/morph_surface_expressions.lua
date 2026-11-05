--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to     |
| permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Generic named morph expressions composed from target weights.
]]--

local M={}

local function finite(value,default)
    local number=tonumber(value)
    if not number or number~=number or number==math.huge or number==-math.huge then
        return default or 0
    end
    return number
end

local function clampWeight(value)
    return math.max(0,math.min(1,finite(value,0)))
end

local function trim(value)
    return (type(value)=='string' and value or ''):match('^%s*(.-)%s*$')
end

local function copyWeights(weights)
    local result={}
    for _,entry in ipairs(type(weights)=='table' and weights or {}) do
        if type(entry)=='table' then
            local id=trim(entry.targetId)
            local weight=clampWeight(entry.weight)
            if id~='' and weight>1e-8 then
                result[#result+1]={targetId=id,targetName=trim(entry.targetName),weight=weight}
            end
        end
    end
    table.sort(result,function(left,right) return left.targetId<right.targetId end)
    return result
end

function M.new(name,id)
    return {id=trim(id),name=trim(name),weights={}}
end

function M.normalize(expression)
    if type(expression)~='table' then return nil,'invalid_expression' end
    expression.id=trim(expression.id)
    expression.name=trim(expression.name)
    expression.weights=copyWeights(expression.weights)
    return expression
end

function M.copyWeights(expression)
    return copyWeights(expression and expression.weights)
end

function M.getWeight(expression,targetId)
    targetId=trim(targetId)
    for _,entry in ipairs(expression and expression.weights or {}) do
        if entry.targetId==targetId then return entry.weight end
    end
    return 0
end

function M.setWeight(expression,targetId,targetName,weight)
    if type(expression)~='table' then return false end
    targetId=trim(targetId)
    if targetId=='' then return false end
    local value=clampWeight(weight)
    local weights=expression.weights or {}
    expression.weights=weights
    for index,entry in ipairs(weights) do
        if entry.targetId==targetId then
            if value<=1e-8 then
                table.remove(weights,index)
            else
                entry.targetName=trim(targetName)
                entry.weight=value
            end
            table.sort(weights,function(left,right) return left.targetId<right.targetId end)
            return true
        end
    end
    if value>1e-8 then
        weights[#weights+1]={targetId=targetId,targetName=trim(targetName),weight=value}
        table.sort(weights,function(left,right) return left.targetId<right.targetId end)
    end
    return true
end

function M.activeCount(expression)
    return #(expression and expression.weights or {})
end

function M.inspect(expression,targets,statusFn)
    local targetById={}
    for _,target in ipairs(type(targets)=='table' and targets or {}) do
        if type(target)=='table' and trim(target.id)~='' then targetById[target.id]=target end
    end
    local missing,stale,active=0,0,0
    for _,entry in ipairs(expression and expression.weights or {}) do
        local target=targetById[entry.targetId]
        if not target then
            missing=missing+1
        else
            local status=statusFn and statusFn(target) or 'ready'
            if status~='ready' then stale=stale+1 end
            if entry.weight and entry.weight>1e-8 then active=active+1 end
        end
    end
    return {missing=missing,stale=stale,active=active}
end

function M.compose(expression,targets,statusFn)
    local targetById={}
    for _,target in ipairs(type(targets)=='table' and targets or {}) do
        if type(target)=='table' and trim(target.id)~='' then targetById[target.id]=target end
    end
    local deltas={}
    local missing,stale,active=0,0,0
    for _,entry in ipairs(expression and expression.weights or {}) do
        local target=targetById[entry.targetId]
        if not target then
            missing=missing+1
        else
            local status=statusFn and statusFn(target) or 'ready'
            if status~='ready' then
                stale=stale+1
            else
                local weight=clampWeight(entry.weight)
                if weight>1e-8 then
                    active=active+1
                    for _,delta in ipairs(target.deltas or {}) do
                        local index=tonumber(delta.index)
                        if index and index>=1 then
                            local result=deltas[index]
                            if not result then
                                result={x=0,y=0,z=0}
                                deltas[index]=result
                            end
                            result.x=result.x+(tonumber(delta.x) or 0)*weight
                            result.y=result.y+(tonumber(delta.y) or 0)*weight
                            result.z=result.z+(tonumber(delta.z) or 0)*weight
                        end
                    end
                end
            end
        end
    end
    return {id=expression and expression.id or 'expression_preview',
        name=expression and expression.name or 'expression_preview',deltas=deltas,
        missing=missing,stale=stale,active=active}
end

return M
