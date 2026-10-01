--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
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
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

]]--

-- Exercise the engine launcher, not a manual call to the editor's initializer.
package.path='editor/?.lua;'..package.path
function onInitScene()
    local ok,err=pcall(function()
        __onLoadScene('editor/normal_map_editor.lua')
        assert(__t_my_class==nil,'Global-callback editor returned a class-style scene')
        local editorLoop=assert(onLoop)
        local started=mbm.getTimeRun()
        local frames=0
        onLoop=function(delta)
            local drawn,message=pcall(editorLoop,delta)
            if not drawn then
                print('NORMAL MAP LAUNCHER SMOKE FAIL: '..tostring(message));mbm.quit();return
            end
            frames=frames+1
            if mbm.getTimeRun()-started>3 then
                assert(frames>1)
                print('NORMAL MAP LAUNCHER SMOKE PASS');mbm.quit()
            end
        end
    end)
    if not ok then print('NORMAL MAP LAUNCHER SMOKE FAIL: '..tostring(err));mbm.quit() end
end
