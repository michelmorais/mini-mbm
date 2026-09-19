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

-- Rectangular proof-of-concept preview. Input/output are supplied through environment.
-- MBM_IMAGE_MESH_SOURCE is required. Crop values default to the entire image.
local object,started
local function runPreview()
    started=mbm.getTimeRun()
    local source=assert(os.getenv('MBM_IMAGE_MESH_SOURCE'),'Set MBM_IMAGE_MESH_SOURCE')
    local output=os.getenv('MBM_IMAGE_MESH_OUTPUT') or '/tmp/mbm_image_mesh_preview.msh'
    local function number(name,default) return tonumber(os.getenv('MBM_IMAGE_MESH_'..name)) or default end
    local asset,report=mbm.generateImageMesh(source,{
        x=number('X',0),y=number('Y',0),cropWidth=number('WIDTH',0),cropHeight=number('HEIGHT',0),
        width=240,height=240,depth=50,relief=number('RELIEF',12),columns=64,rows=64,
        lockBorder=true,borderWidth=0.05,
    })
    assert(asset,report)
    assert(asset:save(output,false,false,true))
    mbm.setColor(0.08,0.10,0.13)
    mbm.setLightEnabled('3d',true)
    mbm.setAmbientLight('3d',0.35,0.35,0.35)
    mbm.setDirectionalLight('3d',0.4,-0.5,1,0.85,0.85,0.85)
    local camera=mbm.getCamera('3d'); camera:setPos(250,160,-560); camera:setFocus(0,0,0)
    object=mesh:new('3d'); assert(object:load(output))
    print('IMAGE MESH PREVIEW OK: '..output..' ('..report.vertices..' vertices, '..report.triangles..' triangles)')
end
function onInitScene()
    local ok,err=pcall(runPreview)
    if not ok then print('IMAGE MESH PREVIEW FAIL: '..tostring(err)); started=nil; mbm.quit() end
end
function onLoop(delta)
    if started and mbm.getTimeRun()-started>=10 then mbm.quit() end
end
