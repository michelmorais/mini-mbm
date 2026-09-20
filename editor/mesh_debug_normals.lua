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

-- Shared policy for vertex, subset and all-frame normal operations.
local M={}
local function finite(n) return type(n)=='number' and n==n and math.abs(n)<math.huge end
function M.select(v,g,mode)
    if mode~=2 then
        local x,y,z=v.nx,v.ny,v.nz
        if finite(x) and finite(y) and finite(z) then
            local scale=math.max(math.abs(x),math.abs(y),math.abs(z))
            if scale>0 then
                local sx,sy,sz=x/scale,y/scale,z/scale
                local length=math.sqrt(sx*sx+sy*sy+sz*sz)
                local nx,ny,nz=sx/length,sy/length,sz/length
                if not g or nx*g.x+ny*g.y+nz*g.z>0 then
                    if math.abs(scale*length-1)<=.001 then return nil end
                    return nx,ny,nz -- preserve direction, repair length only
                end
            end
        end
    end
    if g and finite(g.x) and finite(g.y) and finite(g.z) then
        if v.nx==g.x and v.ny==g.y and v.nz==g.z then return nil end
        return g.x,g.y,g.z
    end
end
function M.draw(gui,lang,state,id)
    gui.Text(lang.L('normal_method'))
    gui.PushItemWidth(-1)
    local changed,mode=gui.Combo('##'..id,state.normalMethod or 1,
        {lang.L('normal_method_repair'),lang.L('normal_method_uniform')},-1)
    gui.PopItemWidth()
    if changed then state.normalMethod=mode end
    gui.TextWrapped(lang.L((state.normalMethod or 1)==1 and 'normal_repair_help' or 'normal_uniform_help'))
end
return M
