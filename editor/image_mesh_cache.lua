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

-- One completed statistics mesh, already oriented for the editor.
-- Exporters never receive this mutable asset: portable export may rewrite UVs.
local TextureAliases=require 'image_mesh_texture_aliases'
local M={}
function M.clear(E)
 local c=E.generatedMesh
 if c and c.originalPath then os.remove(c.originalPath) end
 E.generatedMesh=nil
end
function M.begin(E,id)
 M.clear(E)
 E.generatedMesh={id=id,revision=E.revision}
end
function M.original(E,asset,textureNamespace)
 local c=assert(E.generatedMesh)
 c.originalPath=tUtil.getTemporaryFilePath('.msh')
 assert(TextureAliases.savePreview(asset,c.originalPath,textureNamespace),tLang.L('ime_export_failed'))
end
function M.finish(E,asset,report)
 local c=assert(E.generatedMesh);c.asset=asset;c.report=report
end
function M.get(E,id)
 local c=E.generatedMesh
 if c and c.id==id and c.revision==E.revision and c.asset then return c end
end
return M
