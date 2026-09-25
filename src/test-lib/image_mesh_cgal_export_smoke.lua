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


-- Export geometry or attributed OBJ for external experiments; never saves the project.
package.path='editor/?.lua;'..package.path
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local task,started
local function test()
    assert(api.openProject(assert(os.getenv('MBM_CGAL_PROJECT'),'Set MBM_CGAL_PROJECT')))
    api.select(api.state.project.regions[1].id)
    local E=api.state
    E.values.simplify=false; assert(api.applyProperties())
    api.setEditMode(false);api.rebuild()
    while E.meshTask do coroutine.yield() end
    assert(E.previewPath,E.status)
    local d=meshDebug:new();assert(d:load(E.previewPath))
    local objPath=os.getenv('MBM_CGAL_OBJ')
    if objPath then
        local vertices,faces=dofile('editor/mesh_cgal_obj.lua').export(d,objPath)
        print(string.format('CGAL_OBJ_EXPORT_OK vertices=%d triangles=%d',vertices,faces))
        return
    end
    local vertices,faces,positions={},{},{}
    local rawCount=0
    for subset=1,d:getTotalSubset(1) do
        local remap={}
        local count=d:getTotalVertex(1,subset);rawCount=rawCount+count
        for i,v in ipairs(d:getVertex(1,subset,1,count)) do
            local key=string.format('%.17g %.17g %.17g',v.x+0.0,v.y+0.0,v.z+0.0)
            if not positions[key] then vertices[#vertices+1]=key;positions[key]=#vertices end
            remap[i]=positions[key]-1
        end
        local indices=d:getIndex(1,subset)
        if not indices or #indices==0 then indices={};for i=1,count do indices[i]=i end end
        for i=1,#indices,3 do faces[#faces+1]=string.format('3 %d %d %d',remap[indices[i]],remap[indices[i+1]],remap[indices[i+2]]) end
    end
    local output=assert(os.getenv('MBM_CGAL_OFF'),'Set MBM_CGAL_OFF to a disposable output file')
    local existing=io.open(output,'rb')
    if existing then existing:close();error('Output already exists; choose a new file') end
    local f=assert(io.open(output,'w'))
    f:write('OFF\n',#vertices,' ',#faces,' 0\n',table.concat(vertices,'\n'),'\n',table.concat(faces,'\n'),'\n');f:close()
    print(string.format('CGAL_EXPORT_OK vertices=%d welded=%d triangles=%d',rawCount,#vertices,#faces))
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop(delta)
    loop(delta)
    if coroutine.status(task)=='dead' then mbm.quit();return end
    local ok,e=pcall(function() local success,message=coroutine.resume(task);assert(success,message) end)
    if not ok then print('CGAL_EXPORT_FAIL '..tostring(e));mbm.quit() end
    if mbm.getTimeRun()-started>50 then print('CGAL_EXPORT_FAIL timeout');mbm.quit() end
end
