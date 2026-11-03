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
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Dynamic viewport preview for generic morph targets.
]]--

local M={}
local SHADER_NAME='morph_target_overlay.ps'

local function dpCall(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then print('[morph_surface_target_preview] '..tostring(result[2])) end
    return table.unpack(result,1,result.n)
end

local function clampWeight(value)
    return math.max(0,math.min(1,tonumber(value) or 1))
end

local function ensureShader()
    if mbm.existShader(SHADER_NAME) then return true end
    local code
    if mbm.get('USE_METAL') then
        code=[=[
        fragment float4 frag_main(VOut in [[stage_in]])
        {
            float selected=clamp(in.uv.x,0.0f,1.0f);
            float3 color=mix(float3(0.12f,0.15f,0.20f),float3(0.05f,0.85f,1.00f),selected);
            return float4(color,mix(0.35f,0.92f,selected));
        }
        ]=]
    elseif mbm.get('USE_DIRECTX11') then
        code=[[
        struct PSInput
        {
            float4 position : SV_POSITION;
            float2 texCoord : TEXCOORD0;
        };

        float4 main(PSInput input) : SV_TARGET
        {
            float selected=saturate(input.texCoord.x);
            float3 color=lerp(float3(0.12,0.15,0.20),float3(0.05,0.85,1.00),selected);
            return float4(color,lerp(0.35,0.92,selected));
        }
        ]]
    elseif mbm.get('USE_DIRECTX9') then
        code=[[
        float4 main(float2 texCoord : TEXCOORD0) : COLOR0
        {
            float selected=saturate(texCoord.x);
            float3 color=lerp(float3(0.12,0.15,0.20),float3(0.05,0.85,1.00),selected);
            return float4(color,lerp(0.35,0.92,selected));
        }
        ]]
    else
        code=[[
        precision mediump float;
        varying vec2 vTexCoord;

        void main()
        {
            float selected=clamp(vTexCoord.x,0.0,1.0);
            vec3 color=mix(vec3(0.12,0.15,0.20),vec3(0.05,0.85,1.00),selected);
            gl_FragColor=vec4(color,mix(0.35,0.92,selected));
        }
        ]]
    end
    local ok,result=dpCall(function()
        return mbm.addShader({name=SHADER_NAME,code=code,var={},min={},max={}})
    end)
    return ok and result~=false
end

local function releaseSurface(preview)
    if not preview or not preview.surface then return end
    dpCall(function() preview.surface:onRender(nil) end)
    preview.surface.visible=false
    dpCall(function() preview.surface:destroy() end)
    preview.surface=nil
end

function M.destroy(state)
    local preview=state.targetPreview
    if not preview then return end
    releaseSurface(preview)
    if state.mesh and preview.meshHidden then
        state.mesh.visible=preview.meshWasVisible~=false
    end
    preview.targetId=nil
    preview.weight=nil
    preview.meshWasVisible=nil
    preview.meshHidden=false
    preview.visible=false
    preview.bufferDirty=false
    preview.bufferApplied=false
end

function M.setVisible(state,visible)
    local preview=state.targetPreview
    if not preview or not preview.surface then return false end
    visible=visible==true
    preview.visible=visible
    if visible then
        if not preview.meshHidden then
            preview.meshWasVisible=state.mesh and state.mesh.visible~=false or true
            preview.meshHidden=true
        end
        if state.mesh then state.mesh.visible=false end
        preview.surface.visible=preview.meshWasVisible
    else
        if preview.meshHidden and state.mesh then
            state.mesh.visible=preview.meshWasVisible~=false
            preview.meshHidden=false
        end
        preview.surface.visible=false
    end
    return true
end

local function updateDynamicBuffer(state,_,vertex,uv)
    local preview=state.targetPreview
    if not preview or not preview.surface then return end
    local weight=preview.weight or 1
    local vertices=state.topology and state.topology.vertices or {}
    for index,point in ipairs(vertices) do
        local entry=vertex[index]
        if entry then
            local delta=preview.deltas[index]
            entry.x=point.x+(delta and delta.x or 0)*weight
            entry.y=point.y+(delta and delta.y or 0)*weight
            entry.z=point.z+(delta and delta.z or 0)*weight
        end
    end
    vertex.name='vertex'
    if uv then uv.name='uv' end
    preview.bufferApplied=true
    return vertex,uv
end

function M.update(state)
    local preview=state.targetPreview
    if not preview or not preview.surface then return end
    if preview.bufferApplied then
        dpCall(function() preview.surface:onRender(nil) end)
        preview.bufferApplied=false
    end
    if not preview.bufferDirty then return end
    preview.bufferDirty=false
    local ok=dpCall(function()
        return preview.surface:onRender(function(shape,vertex,uv)
            return updateDynamicBuffer(state,shape,vertex,uv)
        end)
    end)
    if not ok then preview.bufferDirty=false end
end

function M.setWeight(state,weight)
    local preview=state.targetPreview
    if not preview or not preview.surface then return false end
    local clamped=clampWeight(weight)
    if math.abs((preview.weight or -1)-clamped)<=1e-6 then return false end
    preview.weight=clamped
    preview.bufferDirty=true
    return true
end

function M.build(state,target,weight)
    if not state.mesh or not state.topology or not target then
        return false,'missing_preview_input'
    end
    local vertices=state.topology.vertices or {}
    local triangles=state.topology.triangles or {}
    if #vertices==0 or #triangles==0 then return false,'missing_surface_geometry' end
    local deltas={}
    local maximum=0
    for _,delta in ipairs(target.deltas or {}) do
        local index=math.floor(tonumber(delta.index) or 0)
        if vertices[index] then
            local value={x=tonumber(delta.x) or 0,y=tonumber(delta.y) or 0,z=tonumber(delta.z) or 0}
            deltas[index]=value
            maximum=math.max(maximum,math.sqrt(value.x*value.x+value.y*value.y+value.z*value.z))
        end
    end
    local clampedWeight=clampWeight(weight)
    local coords,indices,uvs={},{},{}
    for index,point in ipairs(vertices) do
        local delta=deltas[index]
        coords[#coords+1]=point.x+(delta and delta.x or 0)*clampedWeight
        coords[#coords+1]=point.y+(delta and delta.y or 0)*clampedWeight
        coords[#coords+1]=point.z+(delta and delta.z or 0)*clampedWeight
        local magnitude=delta and math.sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z) or 0
        uvs[#uvs+1]=maximum>1e-10 and math.min(1,magnitude/maximum) or 0
        uvs[#uvs+1]=1
    end
    for _,triangle in ipairs(triangles) do
        indices[#indices+1]=triangle[1]
        indices[#indices+1]=triangle[2]
        indices[#indices+1]=triangle[3]
    end
    local surface=shape:new('3d',0,0,0.0001)
    local created=dpCall(function()
        return surface:createDynamicIndexed(coords,indices,uvs,
            'morph_target_overlay_'..tostring(mbm.getTimeRun()))
    end)
    if not created then
        dpCall(function() surface:destroy() end)
        return false,'create_overlay_failed'
    end
    local shaderLoaded=false
    local shaderOk,shader=dpCall(function() return surface:getShader() end)
    if ensureShader() and shaderOk and shader then
        local loadOk,loaded=dpCall(function() return shader:load(SHADER_NAME,nil) end)
        shaderLoaded=loadOk and loaded~=false
    end
    if not shaderLoaded then surface:setColor(0.1,0.7,1,0.8) end
    surface.alwaysRender=true
    surface.alwaysOnTop=true
    surface.alwaysOnTopPriority=3
    surface.visible=false

    local previous=state.targetPreview
    local hasPrevious=previous and previous.surface
    local meshWasVisible=hasPrevious and previous.visible and previous.meshWasVisible~=nil and
        previous.meshWasVisible or state.mesh.visible~=false
    local previewVisible=hasPrevious and previous.visible~=nil and previous.visible or
        state.targetPreviewVisible~=false
    M.destroy(state)
    state.targetPreview={surface=surface,targetId=target.id,weight=clampedWeight,deltas=deltas,
        meshWasVisible=meshWasVisible,meshHidden=false,visible=false,bufferDirty=false,
        bufferApplied=false}
    M.setVisible(state,previewVisible)
    return true
end

return M
