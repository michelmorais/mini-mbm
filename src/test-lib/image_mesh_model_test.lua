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
local M=require 'image_mesh_model'
local IO=require 'image_mesh_io'
local p=M.new('/tmp/source.png',1344,768)
M.validate(p)
local ids=M.grid(p,{columns=4,rows=3,marginX=40,marginY=27,gapX=100,gapY=36})
assert(#ids==12 and #p.regions==12)
M.validate(p)
local r=p.regions[1]; r.overrides.relief=4
assert(M.options(p,r).relief==4 and M.options(p,r).depth==20)
p.defaults.depth=30; assert(M.options(p,r).depth==30)
local h=M.history(); M.commit(h,p); local copy=M.copy(p); copy.regions[1].x=41
local old=M.undo(h,copy); assert(old.regions[1].x==40)
assert(M.undo(h,old,true).regions[1].x==41)
local polygon=M.fromPoints(p,{{x=0,y=0},{x=40,y=0},{x=40,y=20},{x=20,y=20},{x=20,y=40},{x=0,y=40}})
assert(polygon.w==41 and polygon.h==41)
assert(M.contains(M.outline(polygon),10,30)); assert(not M.contains(M.outline(polygon),30,30))
local ellipse=M.add(p,'ellipse',10,10,60,30); assert(#M.outline(ellipse)==48)
M.validate(p)
local bad=M.copy(p); bad.regions[1].x=1344; assert(not pcall(M.validate,bad))
bad=M.copy(p); bad.version=2; assert(not pcall(M.validate,bad))
bad=M.copy(p); bad.regions[1].overrides.columns=0; assert(not pcall(M.validate,bad))
assert(IO.relative('/tmp/source.png','/tmp/project.imesh')=='source.png')
assert(IO.resolve('source.png','/tmp/project.imesh')=='/tmp/source.png')
assert(IO.exportName({id=12,name='../evil/name'}):match('^012_') and not IO.exportName({id=12,name='../evil/name'}):find('/'))
for _,kind in ipairs({'rectangle','circle','ellipse','triangle','regular'}) do
    local r=M.primitive(p,kind,40,30,6,-100,10000)
    M.validate(p)
    assert(r.x==0 and r.y+r.h==p.image.height)
    if kind=='circle' then assert(r.w==r.h and M.options(p,r).height==M.options(p,r).width) end
    if kind=='triangle' then assert(#r.contour==3) end
    if kind=='regular' then assert(#r.contour==6) end
end
local count=#p.regions
assert(not pcall(M.primitive,p,'regular',40,30,2))
assert(not pcall(M.primitive,p,'rectangle',0,30,6))
assert(not pcall(M.primitive,p,'circle',900,30,6))
assert(#p.regions==count)
local wide=M.primitive(p,'regular',336,192,10)
local o=M.options(p,wide); assert(math.abs(o.width/o.height-335/191)<1e-6)
wide.overrides.preserveAspect=false; wide.overrides.height=123
assert(M.options(p,wide).height==123)
p.defaults.preserveAspect=nil; wide.overrides={}; M.validate(p)
assert(M.options(p,wide).preserveAspect,'legacy project must preserve proportions')
local before=M.copy(wide); local points=M.outline(before)
M.movePoint(wide,before,1,points[1].x,before.y-20)
local moved=M.outline(wide); assert(wide.y==before.y-20 and moved[1].y==before.y-20)
for i=2,#points do assert(math.abs(points[i].x-moved[i].x)<1e-6 and math.abs(points[i].y-moved[i].y)<1e-6) end
M.validate(p)
wide.overrides.maxVertices=1000; wide.overrides.maxTriangles=1
assert(M.options(p,wide).maxTriangles==2000,'editor triangle budget is not derived')
local legacy=M.copy(p)
for k in pairs(M.grooveDefaults) do legacy.defaults[k]=nil end
M.validate(legacy)
assert(not M.options(legacy,legacy.regions[1]).followImage)
local badGrooves=M.copy(p); badGrooves.defaults.grooveTransition=0
assert(not pcall(M.validate,badGrooves))
print('IMAGE MESH MODEL OK')

local legacySimplify=M.copy(p)
for key in pairs(M.simplifyDefaults) do legacySimplify.defaults[key]=nil end
M.validate(legacySimplify)
local options=M.options(legacySimplify,legacySimplify.regions[1])
assert(options.simplify==false and options.simplifyRatio==0.9 and options.simplifyDetails and options.simplifyBoundary==0)
for _,bad in ipairs({{simplifyRatio=0},{simplifyRatio=1},{simplifyBoundary=-1},{simplifyBoundary=1},{simplify=1},{simplifyDetails=0}}) do
    assert(not pcall(M.validateOptions,bad,false))
end
print('IMAGE MESH SIMPLIFICATION SETTINGS / LEGACY PROJECT OK')

local remeshDefaults=M.settings({})
assert(remeshDefaults.remesh==false and remeshDefaults.remeshEdgeLengthFraction==.03)
assert(remeshDefaults.remeshIterations==3 and remeshDefaults.remeshFeatureAngle==45)
for _,bad in ipairs({{remesh=1},{remeshEdgeLengthFraction=0},{remeshEdgeLengthFraction=.251},
    {remeshIterations=0},{remeshIterations=1.5},{remeshFeatureAngle=-1},{remeshFeatureAngle=181}}) do
    assert(not pcall(M.validateOptions,bad,false))
end
print('IMAGE MESH REMESH SETTINGS / VALIDATION OK')

-- Saved options migrate as data without exposing removed native algorithms.
local function encode(value)
    if type(value)=='table' then
        local fields={}
        for key,item in pairs(value) do fields[#fields+1]='['..encode(key)..']='..encode(item) end
        return '{'..table.concat(fields,',')..'}'
    end
    if type(value)=='string' then return string.format('%q',value) end
    return tostring(value)
end
local savedProject=M.new('/tmp/source.png',65,65)
M.add(savedProject,'rectangle',0,0,65,65)
savedProject.defaults.simplifyMode='coplanar'
savedProject.defaults.curvedSimplifyMode='specific'
savedProject.defaults.planarReduceBoundaries=true
savedProject.regions[1].overrides={simplifyMode='coplanar_qem',curvedSimplify=true,curvedSimplifyMode='coplanar',planarReduceBoundaries=false}
savedProject.presets={{name='Existing',settings=M.copy(savedProject.defaults)}}
local filename='/tmp/ime-simplify-migration.imesh'
local bytes='return '..encode(savedProject)
local file=assert(io.open(filename,'wb'));assert(file:write(bytes));file:close()
local migrated=IO.load(filename)
assert(migrated.defaults.simplifyMode=='cgal' and migrated.defaults.planarReduceBoundaries==nil)
assert(migrated.defaults.curvedSimplifyMode==nil and migrated.presets[1].settings.simplifyMode=='cgal')
assert(migrated.regions[1].overrides.simplifyMode=='cgal' and migrated.regions[1].overrides.curvedSimplify==false)
assert(migrated.regions[1].overrides.planarReduceBoundaries==nil)
file=assert(io.open(filename,'rb'));assert(file:read('*a')==bytes);file:close();os.remove(filename)
for _,mode in ipairs{'coplanar','coplanar_qem'} do assert(not pcall(M.validateOptions,{simplifyMode=mode},false)) end
M.validateOptions({simplifyMode='qem'},false);M.validateOptions({simplifyMode='cgal'},false)
M.validateOptions({simplifyMode='cgal_qem'},false)
print('IMAGE MESH SIMPLIFICATION MIGRATION / READ-ONLY INPUT / CURRENT MODES OK')

-- Independent method checkboxes represent all four combinations independently.
local modes=require 'mesh_simplify_modes'
for _,mode in ipairs{'none','qem','cgal','cgal_qem'} do
 M.validateOptions({simplifyMode=mode},false)
 for _,method in ipairs{'cgal','qem'} do
  local other=method=='cgal' and 'qem' or 'cgal'
  for _,enabled in ipairs{false,true} do
   local result=modes.setEnabled(mode,method,enabled)
   assert(modes.enabled(result,method)==enabled)
   assert(modes.enabled(result,other)==modes.enabled(mode,other))
  end
 end
end
print('SIMPLIFICATION INDEPENDENT CHECKBOX STATES OK')

-- Checkbox returns the new value (one result), including on idle frames.
local oldImGui,oldLang=tImGui,tLang
tLang={L=function(key)return key end}
for _,initial in ipairs{'qem','cgal','none','remesh'} do
 for _,checked in ipairs{false,true} do
  tImGui={Checkbox=function()return checked end,IsItemHovered=function()return false end}
  local expected=checked and 'remesh' or (initial=='remesh' and 'none' or initial)
  assert(modes.remeshCheckbox(initial,'test')==expected)
 end
end
tImGui,tLang=oldImGui,oldLang
print('REMESH CHECKBOX TOGGLE / IDLE OK')
