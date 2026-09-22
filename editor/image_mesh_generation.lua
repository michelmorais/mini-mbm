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

local M={}
function M.cancel(E)
 if E.imageJob then E.imageJob:cancel();E.generationCancelling=true end
 if E.simplifyAsset then E.simplifyCancelRequested=true;E.simplifyAsset:cancelSimplify() end
end
function M.generate(E,path,options)
 E.generationCancelled=nil;E.generationCancelling=nil
 local job,err=mbm.startImageMesh(path,options)
 if not job then return nil,err end
 E.imageJob=job
 -- Let the GUI render its progress/cancel controls before collecting even a fast result.
 coroutine.yield()
 while true do
  local status=job:getStatus();E.generationProgress=status.progress;E.generationStage=status.stage
  if status.state~='running' then
   E.imageJob=nil;E.generationProgress=nil;E.generationStage=nil;E.generationCancelling=nil
   if status.state=='cancelled' then
    E.generationCancelled=true;E.batch=nil;E.statisticsRequested=nil
    return nil,'ime_generation_cancelled'
   end
   if status.state=='completed' then return job:takeResult() end
   return nil,status.error or 'Image mesh generation failed'
  end
  coroutine.yield()
 end
end
function M.panel(E)
 if not E.imageJob and not E.simplifyAsset then return end
 local open=tImGui.Begin(tLang.L('ime_generation_title'),false,E.flags.auto)
 if open then
  tImGui.Text(E.simplifyAsset and tLang.L('simplify_geometry') or tLang.L('ime_generation_'..(E.generationStage or 'decode')))
  tImGui.ProgressBar(E.simplifyProgress or E.generationProgress or 0,{x=300,y=0})
  tImGui.TextWrapped(tLang.L('ime_generation_help'))
  if E.generationCancelling or E.simplifyCancelRequested then tImGui.Text(tLang.L('ime_generation_cancelling'))
  elseif tImGui.Button(tLang.L('ime_cancel')) then M.cancel(E) end
 end
 tImGui.End()
end
return M
