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

-- Optional visual acceptance fixture for the 4x3 reference atlas from the plan.
-- Set MBM_IMAGE_MESH_SOURCE to its local path. Outputs go to /tmp/image-mesh-stage2-export.
package.path='editor/?.lua;'..package.path
local Model=require 'image_mesh_model'
local IO=require 'image_mesh_io'
local objects,started={},nil
local function run()
    started=mbm.getTimeRun()
    local source=assert(os.getenv('MBM_IMAGE_MESH_SOURCE'),'Set MBM_IMAGE_MESH_SOURCE')
    local image=assert(mbm.loadTexture(source)); assert(image.width==1344 and image.height==768,'Expected reference atlas dimensions')
    local project=Model.new(source,image.width,image.height)
    project.defaults.columns=32; project.defaults.rows=32; project.defaults.relief=6
    local xs={41,375,716,1064}; local ys={27,282,537}
    mbm.setColor(0.08,0.10,0.13); mbm.setLightEnabled('3d',true)
    mbm.setAmbientLight('3d',0.35,0.35,0.35); mbm.setDirectionalLight('3d',0.4,-0.5,1,0.7,0.7,0.7)
    for row=1,3 do for col=1,4 do
        local r=Model.add(project,'rectangle',xs[col],ys[row],232,210)
        local asset,report=mbm.generateImageMesh(source,Model.options(project,r)); assert(asset,report)
        local path='/tmp/image-mesh-stage2-export/reference_'..r.id..'.msh'
        assert(asset:save(path,false,false,true))
        local obj=mesh:new('3d'); assert(obj:load(path)); obj:setPos((col-2.5)*100,(2-row)*100,0); objects[#objects+1]=obj
    end end
    -- Save the exact crop setup as a project that the new editor can reopen.
    tImGui=require 'ImGui'; local util=require 'editor_utils'
    IO.save(project,'/tmp/image-mesh-stage2-export/reference.imesh',util.save)
    local camera=mbm.getCamera('3d'); camera:setPos(230,150,-730); camera:setFocus(0,0,0)
    print('IMAGE MESH WALL: 12 EXPORTED / LOADED MODULES OK')
end
function onInitScene()
    local ok,err=pcall(run); if not ok then print('IMAGE MESH WALL FAIL: '..tostring(err)); started=nil; mbm.quit() end
end
function onLoop(delta) if started and mbm.getTimeRun()-started>10 then mbm.quit() end end
