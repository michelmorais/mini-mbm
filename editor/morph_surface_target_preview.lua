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
local tTargets=require 'morph_surface_targets'

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

local function releaseSurface(preview,field)
    if not preview or not preview[field] then return end
    local surface=preview[field]
    dpCall(function() surface:onRender(nil) end)
    surface.visible=false
    dpCall(function() surface:destroy() end)
    preview[field]=nil
end

local function releaseSurfaces(preview,field)
    local surfaces=preview and preview[field]
    if type(surfaces)~='table' then return end
    for _,surface in ipairs(surfaces) do
        dpCall(function() surface:onRender(nil) end)
        surface.visible=false
        dpCall(function() surface:destroy() end)
    end
    preview[field]=nil
end

local function clearBufferCallbacks(preview)
    for _,target in ipairs(preview.bufferTargets or {}) do
        dpCall(function() target.surface:onRender(nil) end)
    end
    preview.bufferTargets={}
    preview.bufferApplied=false
end

function M.destroy(state)
    local preview=state.targetPreview
    if not preview then return end
    clearBufferCallbacks(preview)
    releaseSurface(preview,'surface')
    releaseSurfaces(preview,'plainSurfaces')
    preview.plainSurface=nil
    if state.mesh and preview.meshHidden then
        state.mesh.visible=preview.meshWasVisible~=false
    end
    preview.targetId=nil
    preview.weight=nil
    preview.meshWasVisible=nil
    preview.meshHidden=false
    preview.shaderEnabled=nil
    preview.bufferDirty=false
    preview.bufferTargets={}
end

function M.setVisible(state,shaderEnabled)
    local preview=state.targetPreview
    if not preview or not preview.surface or not preview.plainSurface or
            type(preview.plainSurfaces)~='table' then return false end
    shaderEnabled=shaderEnabled==true
    local modeChanged=preview.shaderEnabled~=shaderEnabled
    preview.shaderEnabled=shaderEnabled
    if not preview.meshHidden then
        preview.meshWasVisible=state.mesh and state.mesh.visible~=false or true
        preview.meshHidden=true
    end
    if state.mesh then state.mesh.visible=false end
    if modeChanged then clearBufferCallbacks(preview) end
    preview.surface.visible=shaderEnabled and preview.meshWasVisible or false
    for _,surface in ipairs(preview.plainSurfaces) do
        surface.visible=(not shaderEnabled) and preview.meshWasVisible or false
    end
    if modeChanged then preview.bufferDirty=true end
    return true
end

local function updateDynamicBuffer(state,start,count,_,vertex,uv)
    local preview=state.targetPreview
    if not preview or not preview.surface then return end
    local weight=preview.weight or 1
    local vertices=state.topology and state.topology.vertices or {}
    for localIndex=1,count do
        local index=start+localIndex-1
        local point=vertices[index]
        local entry=vertex[localIndex]
        if entry then
            local delta=preview.deltas[index]
            local draft=state.targetEditDraft
            local selectionWeight=draft and draft.weights and draft.weights[index]
            if draft and selectionWeight and selectionWeight>0 then
                local old=delta and {x=delta.x,y=delta.y,z=delta.z} or {x=0,y=0,z=0}
                local transformed
                if draft.mode=='rotate' then
                    transformed=tTargets.rotatedDelta(point,old,draft.pivot,draft.basis)
                elseif draft.mode=='translate' then
                    transformed={x=old.x+draft.dx,y=old.y+draft.dy,z=old.z+draft.dz}
                end
                if transformed then
                    local blend=math.max(0,math.min(1,tonumber(selectionWeight) or 0))
                    delta={x=old.x+(transformed.x-old.x)*blend,
                        y=old.y+(transformed.y-old.y)*blend,
                        z=old.z+(transformed.z-old.z)*blend}
                end
            end
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
    if not preview or not preview.surface or not preview.plainSurface or
            type(preview.plainSurfaces)~='table' then return end
    if preview.bufferApplied then clearBufferCallbacks(preview) end
    if not preview.bufferDirty then return end
    preview.bufferDirty=false
    local targets=preview.shaderEnabled and {preview.shaderTarget} or preview.plainTargets
    preview.bufferTargets={}
    for _,target in ipairs(targets or {}) do
        local ok=dpCall(function()
            return target.surface:onRender(function(shape,vertex,uv)
                return updateDynamicBuffer(state,target.start,target.count,shape,vertex,uv)
            end)
        end)
        if ok then preview.bufferTargets[#preview.bufferTargets+1]=target end
    end
    if #preview.bufferTargets==0 then
        preview.bufferDirty=false
    end
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
    local coords,indices,heatmapUvs={},{},{}
    for index,point in ipairs(vertices) do
        local delta=deltas[index]
        coords[#coords+1]=point.x+(delta and delta.x or 0)*clampedWeight
        coords[#coords+1]=point.y+(delta and delta.y or 0)*clampedWeight
        coords[#coords+1]=point.z+(delta and delta.z or 0)*clampedWeight
        local magnitude=delta and math.sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z) or 0
        heatmapUvs[#heatmapUvs+1]=maximum>1e-10 and math.min(1,magnitude/maximum) or 0
        heatmapUvs[#heatmapUvs+1]=1
    end
    for _,triangle in ipairs(triangles) do
        indices[#indices+1]=triangle[1]
        indices[#indices+1]=triangle[2]
        indices[#indices+1]=triangle[3]
    end
    local uniqueName='morph_target_preview_'..tostring(mbm.getTimeRun())
    local function createSurface(withShader,surfaceCoords,surfaceIndices,surfaceUvs,
            nameSuffix,texture)
        local surface=shape:new('3d',0,0,0.0001)
        local created=dpCall(function()
            return surface:createDynamicIndexed(surfaceCoords,surfaceIndices,surfaceUvs,
                uniqueName..nameSuffix)
        end)
        if not created then
            dpCall(function() surface:destroy() end)
            return nil
        end
        if withShader then
            local shaderLoaded=false
            local shaderOk,shader=dpCall(function() return surface:getShader() end)
            if ensureShader() and shaderOk and shader then
                local loadOk,loaded=dpCall(function() return shader:load(SHADER_NAME,nil) end)
                shaderLoaded=loadOk and loaded~=false
            end
            if not shaderLoaded then surface:setColor(0.1,0.7,1,0.8) end
        else
            local textured=false
            if texture then
                local textureOk,textureApplied=dpCall(function()
                    return surface:setTexture(texture)
                end)
                textured=textureOk and textureApplied~=false
            end
            if not textured then
                -- Keep the target geometry opaque and inspectable without the displacement shader.
                surface:setColor(0.72,0.82,0.9,1)
            end
        end
        surface.alwaysRender=true
        surface.alwaysOnTop=true
        surface.alwaysOnTopPriority=3
        surface.visible=false
        return surface
    end
    local surface=createSurface(true,coords,indices,heatmapUvs,'_shader',nil)
    local topologySubsets=state.topology.subsets or {}
    if #topologySubsets==0 then
        topologySubsets={{start=1,count=#vertices,indices=indices,
            texture=state.topology.sourceTexture}}
    end
    local plainSurfaces,plainTargets={},{}
    for subsetIndex,subset in ipairs(topologySubsets) do
        local subsetStart=math.max(1,math.floor(tonumber(subset.start) or 1))
        local subsetCount=math.max(0,math.floor(tonumber(subset.count) or 0))
        local subsetCoords,subsetUvs={},{}
        local sourceUv=state.topology.uvs or {}
        for localIndex=1,subsetCount do
            local index=subsetStart+localIndex-1
            subsetCoords[#subsetCoords+1]=coords[index*3-2] or 0
            subsetCoords[#subsetCoords+1]=coords[index*3-1] or 0
            subsetCoords[#subsetCoords+1]=coords[index*3] or 0
            subsetUvs[#subsetUvs+1]=sourceUv[index*2-1] or 0
            subsetUvs[#subsetUvs+1]=sourceUv[index*2] or 0
        end
        local plain=createSurface(false,subsetCoords,subset.indices or {},subsetUvs,
            '_plain_'..tostring(subsetIndex),subset.texture)
        if not plain then
            if surface then dpCall(function() surface:destroy() end) end
            for _,createdSurface in ipairs(plainSurfaces) do
                dpCall(function() createdSurface:destroy() end)
            end
            return false,'create_overlay_failed'
        end
        plainSurfaces[#plainSurfaces+1]=plain
        plainTargets[#plainTargets+1]={surface=plain,start=subsetStart,count=subsetCount}
    end
    if not surface or #plainSurfaces==0 then
        if surface then dpCall(function() surface:destroy() end) end
        for _,createdSurface in ipairs(plainSurfaces) do
            dpCall(function() createdSurface:destroy() end)
        end
        return false,'create_overlay_failed'
    end

    local previous=state.targetPreview
    local hasPrevious=previous and previous.surface
    local meshWasVisible=hasPrevious and previous.meshWasVisible~=nil and
        previous.meshWasVisible or state.mesh.visible~=false
    local previewShaderEnabled=hasPrevious and previous.shaderEnabled~=nil and previous.shaderEnabled or
        state.targetPreviewVisible~=false
    M.destroy(state)
    state.targetPreview={surface=surface,plainSurface=plainSurfaces[1],plainSurfaces=plainSurfaces,
        shaderTarget={surface=surface,start=1,count=#vertices},plainTargets=plainTargets,
        targetId=target.id,weight=clampedWeight,deltas=deltas,meshWasVisible=meshWasVisible,
        meshHidden=false,shaderEnabled=false,bufferDirty=false,bufferApplied=false,bufferTargets={}}
    M.setVisible(state,previewShaderEnabled)
    return true
end

return M
