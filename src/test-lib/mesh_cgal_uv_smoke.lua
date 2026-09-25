--[[---------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------]]


-- Attributed OBJ -> engine save/reload and textured comparison, without editor mutation.
local Obj=dofile('editor/mesh_cgal_obj.lua')
tImGui=require 'ImGui'
local objects,paths,start,extent={}, {},nil,nil
local function tests()
    local source=Obj.read(assert(os.getenv('MBM_CGAL_SOURCE')))
    local result=Obj.read(assert(os.getenv('MBM_CGAL_RESULT')))
    local originals={}
    local function key(p) return string.format('%.9g:%.9g:%.9g',p.x,p.y,p.z) end
    for _,group in ipairs(source.groups) do
        originals[group.name]=originals[group.name] or {}
        for _,v in ipairs(group.vertices) do
            local list=originals[group.name][key(v)] or {};originals[group.name][key(v)]=list
            list[#list+1]={u=v.u,v=v.v}
        end
    end
    for _,group in ipairs(result.groups) do
        assert(originals[group.name],'material changed')
        for _,v in ipairs(group.vertices) do
            local found=false
            for _,uv in ipairs(originals[group.name][key(v)] or {}) do
                if math.abs(v.u-uv.u)<1e-6 and math.abs(v.v-uv.v)<1e-6 then found=true;break end
            end
            assert(found,'UV did not match a source corner on this material')
        end
    end
    mbm.setColor(.08,.09,.1)
    local lo={x=math.huge,y=math.huge,z=math.huge};local hi={x=-math.huge,y=-math.huge,z=-math.huge}
    for _,p in ipairs(source.positions) do for _,a in ipairs({'x','y','z'}) do lo[a]=math.min(lo[a],p[a]);hi[a]=math.max(hi[a],p[a]) end end
    extent=math.max(hi.x-lo.x,hi.y-lo.y,hi.z-lo.z)
    for i,data in ipairs({source,result}) do
        local d=Obj.asset(data)
        local path='/tmp/mbm-cgal-uv-preview-'..tostring(os.time())..'-'..i..'.msh';paths[i]=path
        assert(d:save(path,false,false,true))
        local restored=meshDebug:new();assert(restored:load(path));assert(restored:check())
        assert(restored:getTotalSubset(1)==#data.groups)
        for subset,group in ipairs(data.groups) do
            local list=restored:getVertex(1,subset,1,restored:getTotalVertex(1,subset))
            assert(#list==#group.vertices)
            for n,v in ipairs(list) do
                assert(math.abs(v.u-group.vertices[n].u)<1e-6 and math.abs(v.v-group.vertices[n].v)<1e-6,'UV changed on save/reload')
            end
            assert(restored:getTexture(1,subset)==d:getTexture(1,subset),'texture changed on save/reload')
        end
        local object=mesh:new('3d');assert(meshDebug:loadMeshPreview(object,path));objects[i]=object
        object:setPos((i-1.5)*extent*1.3-(lo.x+hi.x)/2,-(lo.y+hi.y)/2,-(lo.z+hi.z)/2)
        object.alwaysRender=true
    end
    local camera=mbm.getCamera('3d');camera:setFar(extent*20);camera:setFocus(0,0,0)
    print('CGAL_UV_RENDER_OK source corner UVs / material identity / engine save-reload / texture paths')
end
function onInitScene()
    start=mbm.getTimeRun()
    local ok,err=pcall(tests)
    if not ok then print('CGAL_UV_RENDER_FAIL '..tostring(err));mbm.quit() end
end
function onLoop()
    local elapsed=mbm.getTimeRun()-start
    if extent then
        local camera=mbm.getCamera('3d')
        camera:setPos(0,-extent*.6,(elapsed<3 and 1 or -1)*extent*3.4)
    end
    local open=tImGui.Begin('CGAL UV comparison',false,0)
    if open then tImGui.Text('Left: original | Right: CGAL with UVs | Unlit, source texture') end
    tImGui.End()
    if elapsed>6 then
        for _,object in ipairs(objects) do object:destroy() end
        for _,path in ipairs(paths) do meshDebug:fakeRelease(path);os.remove(path) end
        print('CGAL_UV_CLEANUP_OK');mbm.quit()
    end
end
