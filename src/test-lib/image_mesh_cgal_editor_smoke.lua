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



package.path='editor/?.lua;'..package.path
local Model=require 'image_mesh_model'
local IO=require 'image_mesh_io'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local Cgal=require 'mesh_cgal'
local init,loop=onInitScene,onLoop
local task,started
local function await() while api.state.meshTask do coroutine.yield() end end
local function test()
 assert(Cgal.setPath(assert(os.getenv('MBM_CGAL_EXECUTABLE')),false))
 local E=api.state
 assert(api.openProject(assert(os.getenv('MBM_CGAL_PROJECT'))));api.select(E.project.regions[1].id)
 E.values.simplify=true;E.values.simplifyMode='cgal';E.values.planarAngle=5;E.values.planarTolerance=.01
 assert(api.applyProperties());assert(api.saveProject('/tmp/cgal-integrated.imesh'))
 local saved=IO.load('/tmp/cgal-integrated.imesh');assert(Model.options(saved,saved.regions[1]).simplifyMode=='cgal')
 api.setEditMode(false);api.rebuild();await()
 assert(E.report and E.report.simplification and E.report.simplification.backend=='cgal',E.status)
 assert(E.report.sourceTriangles==12570 and E.report.triangles==6882,'unexpected triangle count '..tostring(E.report.triangles))
 local builds=E.builds;for _=1,5 do coroutine.yield() end;assert(E.builds==builds,'idle rebuilt')
 api.exportOne('/tmp/cgal-integrated.msh');await()
 local d=meshDebug:new();assert(d:load('/tmp/cgal-integrated.msh'));assert(d:check())
 print('IMAGE CGAL EDITOR SMOKE OK '..E.report.sourceTriangles..' -> '..E.report.triangles)
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop(delta)
 loop(delta)
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,e=coroutine.resume(task);if not ok then print('IMAGE CGAL EDITOR SMOKE FAIL '..tostring(e));mbm.quit() end
 if mbm.getTimeRun()-started>60 then print('IMAGE CGAL EDITOR SMOKE FAIL timeout');mbm.quit() end
end
