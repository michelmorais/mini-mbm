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

local Geometry=require 'image_mesh_holes_geometry'
local M={}
local function number(v,lo,hi) return type(v)=='number' and v==v and v>=lo and v<=hi end
function M.validate(nodes)
 if nodes==nil then return end
 assert(type(nodes)=='table' and #nodes<=32,'ime_curved_nodes_invalid')
 local targets,depth={}, {[0]=0}
 for i,n in ipairs(nodes) do
  assert(type(n)=='table' and number(n.parent,0,i-1) and n.parent%1==0,'ime_curved_nodes_invalid')
  assert(n.role=='target' or n.role=='region','ime_curved_nodes_invalid')
  assert(type(n.name)=='string' and #n.name<=128 and not n.name:find('%c'),'ime_curved_nodes_invalid')
  assert(number(n.thickness,.001,1000000),'ime_curved_nodes_invalid')
  assert(n.profile==nil or n.profile=='linear' or n.profile=='smooth' or n.profile=='bezier','ime_curved_profile_invalid')
  assert(n.bezier1==nil or number(n.bezier1,0,1),'ime_curved_profile_invalid')
  assert(n.bezier2==nil or number(n.bezier2,0,1),'ime_curved_profile_invalid')
  assert(n.bezier3==nil or number(n.bezier3,0,1),'ime_curved_profile_invalid')
  assert(n.bezier4==nil or number(n.bezier4,0,1),'ime_curved_profile_invalid')
  assert(n.bezierPoints==nil or n.bezierPoints==2 or n.bezierPoints==3 or n.bezierPoints==4,'ime_curved_profile_invalid')
  assert(n.shape=='polyline' or n.shape=='point' or n.shape=='line' or n.shape=='ellipse' or n.shape=='rectangle' or n.shape=='polygon','ime_curved_nodes_invalid')
  assert(#n>=1 and #n<=128 and (n.shape~='point' or #n==1) and (n.shape~='line' or #n==2) and (n.shape~='polyline' or #n>=2),'ime_curved_nodes_invalid')
  for _,p in ipairs(n) do assert(type(p)=='table' and number(p.x,0,1) and number(p.y,0,1),'ime_curved_nodes_invalid') end
  if n.shape~='point' and n.shape~='line' and n.shape~='polyline' then assert(#n>=3 and Geometry.simple(n),'ime_curved_nodes_invalid') end
  assert(n.role~='region' or (#n>=3 and n.shape~='polyline'),'ime_curved_nodes_invalid')
  assert(n.parent==0 or (#nodes[n.parent]>=3 and nodes[n.parent].shape~='polyline'),'ime_curved_nodes_terminal')
  depth[i]=depth[n.parent]+1;assert(depth[i]<=8,'ime_curved_nodes_invalid')
  if n.role=='target' then assert(not targets[n.parent],'ime_curved_nodes_target_exists');targets[n.parent]=i end
 end
end
function M.same(a,b)
 if type(a)~=type(b) then return false end
 if type(a)~='table' then return a==b end
 for k,v in pairs(a) do if not M.same(v,b[k]) then return false end end
 for k in pairs(b) do if a[k]==nil then return false end end
 return true
end
function M.bounds(points) return Geometry.bounds(points) end
function M.descendant(nodes,index,parent)
 while index>0 do if index==parent then return true end;index=nodes[index].parent end
 return parent==0
end
function M.shape(kind,x,y,rx,ry)
 if kind=='point' then return {{x=x,y=y}} end
 if kind=='line' or kind=='polyline' then return {{x=x,y=y-ry},{x=x,y=y+ry}} end
 if kind=='rectangle' then return {{x=x-rx,y=y-ry},{x=x+rx,y=y-ry},{x=x+rx,y=y+ry},{x=x-rx,y=y+ry}} end
 if kind=='polygon' then return {{x=x,y=y-ry},{x=x+rx,y=y+ry},{x=x-rx,y=y+ry}} end
 local points={}
 for i=0,31 do local a=i*math.pi/16;points[#points+1]={x=x+rx*math.cos(a),y=y+ry*math.sin(a)} end
 return points
end
function M.convert(region,options)
 local kind=options.curvedRadius>0 and 'ellipse' or 'point'
 local n=M.shape(kind,options.curvedX,options.curvedY,options.curvedRadius/options.width,options.curvedRadius/options.height)
 n.shape=kind;n.parent=0;n.role='target';n.thickness=options.curvedTarget;n.name='1'
 region.curvedNodes={n}
end
function M.add(region,parent,role,kind,thickness)
 local nodes=assert(region.curvedNodes,'ime_curved_nodes_invalid')
 assert(#nodes<32 and parent>=0 and parent<=#nodes,'ime_curved_nodes_invalid')
 assert(parent==0 or (#nodes[parent]>=3 and nodes[parent].shape~='polyline'),'ime_curved_nodes_terminal')
 if role=='target' then for _,n in ipairs(nodes) do assert(n.parent~=parent or n.role~='target','ime_curved_nodes_target_exists') end end
 assert(role~='region' or (kind~='point' and kind~='line' and kind~='polyline'),'ime_curved_nodes_invalid')
 local b=parent==0 and {x=.5,y=.5,rx=.5,ry=.5} or M.bounds(nodes[parent])
 local n=M.shape(kind,b.x,b.y,b.rx*.4,b.ry*.4)
 n.parent=parent;n.role=role;n.shape=kind;n.thickness=thickness;n.name=tostring(#nodes+1)
 nodes[#nodes+1]=n;return #nodes
end
-- Only invoked when adding an interior target, never from the draw loop.
function M.seedInterior(node,outer,holes)
 local best,score
 for y=1,39 do for x=1,39 do
  local cx,cy=x/40,y/40
  local candidate=node.shape=='point' and {{x=cx,y=cy}} or {{x=cx,y=cy-.005},{x=cx,y=cy+.005}}
  local distance=(cx-.5)^2+(cy-.5)^2
  if (not score or distance<score) and Geometry.openInside(outer,holes,candidate) then best=candidate;score=distance end
 end end
 assert(best,'ime_curved_edit_interior')
 for i,p in ipairs(best) do node[i]=p end
end
function M.remove(nodes,index)
 local remap,out={[0]=0},{}
 for i,n in ipairs(nodes) do if not M.descendant(nodes,i,index) then remap[i]=#out+1;out[#out+1]=n end end
 for _,n in ipairs(out) do n.parent=assert(remap[n.parent]) end
 return out
end
-- Event-time validation: return a reason instead of mutating or throwing.
local function cross(a,b,c) return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x) end
local function winding(points)
 local area=0
 for i,a in ipairs(points) do local b=points[i%#points+1];area=area+a.x*b.y-a.y*b.x end
 return area<0 and -1 or 1
end
function M.geometry(nodes,outer,interior,holes)
 if interior then
  if #nodes==0 then return true end
  if #nodes~=1 then return false,'edit_interior' end
  local n=nodes[1]
  if n.parent~=0 or n.role~='target' or (n.shape~='point' and n.shape~='line' and n.shape~='polyline') then return false,'edit_interior' end
  for _,p in ipairs(n) do if not number(p.x,0,1) or not number(p.y,0,1) then return false,'edit_bounds' end end
  if not Geometry.openInside(outer,holes,n) then return false,'edit_interior' end
  return true
 end
 for _,n in ipairs(nodes) do
  if n.shape=='polyline' then return false,'edit_interior_mode' end
  for _,p in ipairs(n) do
   if not number(p.x,0,1) or not number(p.y,0,1) then return false,'edit_bounds' end
  end
  if #n==2 and (n[1].x-n[2].x)^2+(n[1].y-n[2].y)^2<1e-12 then return false,'edit_line' end
  if #n>=3 then
   if not Geometry.simple(n) then return false,'edit_crossing' end
   if n.role=='target' then
    local sign=winding(n)
    for i,a in ipairs(n) do
     if sign*cross(a,n[i%#n+1],n[(i+1)%#n+1]) < -1e-10 then return false,'edit_convex' end
    end
   end
  end
  local parent=n.parent==0 and outer or nodes[n.parent]
  if not Geometry.canPlace(parent,{},n) then return false,'edit_inside' end
  if n.role=='target' then
   local sign=winding(parent)
   for _,p in ipairs(n) do for i,a in ipairs(parent) do
    if sign*cross(a,parent[i%#parent+1],p)<=1e-10 then return false,'edit_visibility' end
   end end
  end
 end
 for i,a in ipairs(nodes) do if a.role=='region' then
  for j=i+1,#nodes do local b=nodes[j]
   if b.role=='region' and not M.descendant(nodes,i,j) and not M.descendant(nodes,j,i) then
    if not Geometry.canPlace(outer,{a},b) then return false,'edit_overlap' end
   end
  end
 end end
 return true
end
function M.transform(nodes,index,key,value)
 if not number(value,0,1) then return false end
 local b=M.bounds(nodes[index]);local dx,dy,sx,sy=0,0,1,1
 if key=='x' then dx=value-b.x
 elseif key=='y' then dy=value-b.y
 elseif key=='rx' then if b.rx<=0 or value<=0 then return false end;sx=value/b.rx
 elseif key=='ry' then if b.ry<=0 or value<=0 then return false end;sy=value/b.ry end
 for i,n in ipairs(nodes) do if M.descendant(nodes,i,index) then
  for _,p in ipairs(n) do p.x=b.x+(p.x-b.x)*sx+dx;p.y=b.y+(p.y-b.y)*sy+dy end
 end end
 return true
end
function M.resizeProfile(node,count)
 assert(count==2 or count==3 or count==4,'ime_curved_profile_invalid')
 local old=node.bezierPoints or 2
 local values={[0]=0}
 for i=1,old do values[i]=node['bezier'..i] or (i==1 and 0 or 1) end
 values[old+1]=1
 if count>old then
  while old<count do
   local elevated={[0]=0,[old+2]=1}
   for i=1,old+1 do local w=i/(old+2);elevated[i]=w*values[i-1]+(1-w)*values[i] end
   values=elevated;old=old+1
  end
 elseif count<old then
  local reduced={}
  for i=1,count do
   local x=i*(old+1)/(count+1);local lo=math.floor(x);local w=x-lo
   reduced[i]=values[lo]*(1-w)+values[lo+1]*w
  end
  values=reduced
 end
 node.bezierPoints=count
 for i=1,count do node['bezier'..i]=values[i] end
end
function M.profileValue(node,t)
 if node.profile=='smooth' then return t*t*(3-2*t) end
 if node.profile=='bezier' then
  local count=node.bezierPoints or 2
  if count>2 then
   local values={[0]=0}
   for i=1,count do values[i]=node['bezier'..i] or (i==1 and 0 or 1) end
   values[count+1]=1
   for remaining=count+1,1,-1 do
    for i=0,remaining-1 do values[i]=values[i]+(values[i+1]-values[i])*t end
   end
   return values[0]
  end
  local u=1-t
  return 3*u*u*t*(node.bezier1 or 0)+3*u*t*t*(node.bezier2 or 1)+t*t*t
 end
 return t
end
function M.range(options)
 if options.curvedNodes==nil then return math.min(options.curvedEdge,options.curvedTarget),math.max(options.curvedEdge,options.curvedTarget) end
 local lo,hi=options.curvedEdge,options.curvedEdge
 for _,n in ipairs(options.curvedNodes) do if n.role=='target' then lo=math.min(lo,n.thickness);hi=math.max(hi,n.thickness) end end
 return lo,hi
end
return M
