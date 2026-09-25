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
local A=require 'mesh_audit'
assert(A.setPath('/tmp/audit worker',false));assert(A.getPath()=='/tmp/audit worker')
local json='{"schema_version":1,"operation":"mesh_audit","status":"completed","vertices":4,"triangles":2,"valid_polygon_mesh":true,"self_intersections_status":"not_requested","has_self_intersections":null}'
local r=A.decodeReport(json)
assert(r.vertices==4 and r.has_self_intersections==nil and r.self_intersections_status=='not_requested')
for _,bad in ipairs{json..'os.execute("bad")',json:gsub('"vertices":4','"vertices":0/0'),json:gsub('"schema_version":1','"schema_version":2'),json:gsub('"vertices":4','"vertices":4,"vertices":5'),'return {}','{"schema_version":1}'} do
 assert(not pcall(A.decodeReport,bad))
end
for _,number in ipairs{'01','+1','1.','1e','-1','1.5'} do
 assert(not pcall(A.decodeReport,json:gsub('"vertices":4','"vertices":'..number)))
end
assert(not pcall(A.decodeReport,json:gsub('not_requested','completed')))
assert(not pcall(A.decodeReport,json:gsub('not_requested','unknown')))
local time=0
local calls,processes={},{}
mbm={getTimeRun=function()return time end,executeProcessAsync=function(spec)
 calls[#calls+1]=spec
 local p={running=true}
 function p:isRunning()return self.running end
 function p:getExitCode()return self.exit or 0 end
 function p:cancel()self.cancelled=true;self.running=false;return true end
 function p:destroy()self.destroyed=true end
 processes[#processes+1]=p;return p
end}
local job=assert(A.startFile('/tmp/source.obj',{selfIntersections=false}))
assert(calls[1].arguments[#calls[1].arguments]=='--quiet')
assert(calls[1].executable=='/tmp/audit worker' and calls[1].arguments[4]=='--skip-self-intersections')
assert(job:getStatus().state=='running')
local path=calls[1].arguments[3];local f=assert(io.open(path,'w'));f:write(json);f:close()
processes[1].running=false
assert(job:getStatus().report.triangles==2 and job.json==json)
assert(job:getStatus().state=='completed' and processes[1].destroyed and not io.open(path))
job:destroy()
job=assert(A.startFile('/tmp/source.obj'));job:cancel();job:cancel()
assert(job:getStatus().state=='cancelled' and processes[2].cancelled and processes[2].destroyed)
job=assert(A.startFile('/tmp/source.obj',{timeout=1}));time=2
assert(job:getStatus().state=='failed' and processes[3].cancelled)
job=assert(A.startFile('/tmp/source.obj'));A.shutdown();A.shutdown();assert(job:getStatus().state=='cancelled')
print('MESH AUDIT API OK: protocol / completion / cancellation / timeout / cleanup')

local UI=require 'mesh_audit_ui'
local state={}
assert(UI.start(state,'/tmp/first.obj'))
local process=processes[#processes]
UI.draw(state,'/tmp/second.obj','test',{TreeNode=function()return false end},function(k)return k end,false)
assert(process.cancelled and process.destroyed and not state.job and not state.status)
local count=#calls
for _=1,5 do UI.update();UI.draw(state,'/tmp/second.obj','test',{TreeNode=function()return false end},function(k)return k end,false) end
assert(#calls==count,'idle panel launched another audit')
state.status={state='completed',report={}};state.json='old report'
UI.draw(state,'/tmp/third.obj','test',{TreeNode=function()return false end},function(k)return k end,false)
assert(not state.status and not state.json)
UI.shutdown()
print('MESH AUDIT UI OK: source change / cancellation / stale report / idle')

local verbose=assert(A.startFile('/tmp/source.obj',{printJson=true}))
for _,arg in ipairs(calls[#calls].arguments) do assert(arg~='--quiet') end
verbose:cancel()
print('MESH AUDIT JSON OUTPUT OK: default quiet / explicit terminal output')
